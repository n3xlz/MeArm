// ============================================================
//  任务五 PC 端测试台
//
//  被测输入是 arduino-cli 预处理后的 Task1235.ino.cpp（= 真实源码 +
//  自动补上的函数原型），本地 inc/ 提供 Arduino / Servo 桩。
//  因此被测的是源码本体，不存在"手抄失真"。
//
//  不要直接运行本文件，请用同目录的 build.sh —— 它会先从最新 .ino
//  重新生成 Task1235.ino.cpp，再编译运行。
// ============================================================
#include <cstdarg>
#include "Arduino.h"

SerialStub  Serial;
unsigned long g_millis = 0;
int         g_pinLevel[32];

#include "Task1235.ino.cpp"

// ------------------------------------------------------------
static int g_fail = 0;
static void ok(bool cond, const char* fmt, ...) {
  char buf[512];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  if (cond) printf("    \u2713 %s\n", buf);
  else      { printf("    \u2717 %s   <-- FAIL\n", buf); g_fail++; }
}
static void section(const char* t) { printf("\n\033[1m%s\033[0m\n", t); }

// 逆解 -> 舵机角
struct IkRes { bool ok; int ab, ar, af; float t1, t2, t3; };
static IkRes ikOf(float x, float y, float z) {
  IkRes r{}; r.ok = false;
  r.ok = inverseKinematics(x, y, z, r.t1, r.t2, r.t3);
  if (r.ok) {
    r.ab = (int)th2servoBase(r.t1);
    r.ar = (int)th2servoRarm(r.t2);
    r.af = (int)th2servoFarm(r.t3);
  }
  return r;
}
static bool inR(int v, int lo, int hi) { return v >= lo && v <= hi; }

// 把当前 drawState 推到结束（模拟 loop 里的 drawTick 心跳）
static int pumpUntilNotRun(int maxIter = 5000) {
  int it = 0;
  while (drawState == DRAW_RUN && it < maxIter) {
    g_millis += DRAW_STEP_MS + 1;   // 时间必然走过一个间隔
    drawTick();
    it++;
  }
  return it;
}

// ============================================================
// T1 可达判据边界
// ============================================================
static void testReachBoundary() {
  section("T1  可达判据边界扫描");
  printf("     连杆: L1=%.0f  Lf=%.0f  D=%.0f  H=%.0f\n", ARM_L1, ARM_LF, ARM_D, ARM_H);
  printf("     理论区间 [|L1-Lf|, L1+Lf] = [%.0f, %.0f]\n",
         fabs(ARM_L1 - ARM_LF), ARM_L1 + ARM_LF);

  float firstOk = -1, lastOk = -1;
  int okCount = 0;
  for (float rho = 0; rho <= 260.0f; rho += 0.25f) {
    float x = ARM_D + rho, y = 0.0f, z = ARM_H;  // r=rho, z'=0  =>  ρ=rho
    float t1, t2, t3;
    if (inverseKinematics(x, y, z, t1, t2, t3)) {
      if (firstOk < 0) firstOk = rho;
      lastOk = rho; okCount++;
    }
  }
  printf("     实测可达 ρ ∈ [%.2f, %.2f]  (共 %d 个采样点)\n", firstOk, lastOk, okCount);
  ok(fabs(firstOk - 53.0f) < 0.5f, "下界 = %.2f（应为 53）", firstOk);
  ok(fabs(lastOk - 213.0f) < 0.5f, "上界 = %.2f（应为 213）", lastOk);

  // 越界必须被拒
  float t1, t2, t3;
  ok(!inverseKinematics(ARM_D + 52.0f, 0, ARM_H, t1, t2, t3), "ρ=52（低于下界）被正确拒绝");
  ok(!inverseKinematics(ARM_D + 214.0f, 0, ARM_H, t1, t2, t3), "ρ=214（高于上界）被正确拒绝");
}

// ============================================================
// T2 基础图形：走真实 drawShape
// ============================================================
static void testShapes() {
  section("T2  基础图形（调用真实 drawShape）");
  struct { char id; const char* name; } shapes[] = {
    {'N', "N 字形"}, {'T', "三角形"}, {'Z', "Z 字形"}, {'V', "V 字形"}
  };

  for (auto& s : shapes) {
    drawState = DRAW_IDLE;
    Serial.clear();
    drawShape(s.id);

    printf("     [%s] 航点数=%d  起笔阶段 IK 失败=%s\n",
           s.name, wayN, Serial.has("+IK FAIL") ? "是 ⚠" : "否");

    ok(wayN >= 3 && wayN <= 4, "%s 航点数合法(%d)", s.name, wayN);

    int geoBad = 0, over = 0;
    for (int i = 0; i < wayN; i++) {
      IkRes r = ikOf(way[i].x, way[i].y, way[i].z);
      if (!r.ok) { geoBad++; printf("        P%d (%.0f,%.0f,%.0f) 几何不可达 ⚠\n",
                                 i, way[i].x, way[i].y, way[i].z); continue; }
      bool rng = inR(r.ab, baseMin, baseMax) && inR(r.ar, rArmMin, rArmMax) && inR(r.af, fArmMin, fArmMax);
      printf("        P%d (%.0f,%.0f,%.0f) -> base=%4d rArm=%4d fArm=%4d%s\n",
             i, way[i].x, way[i].y, way[i].z, r.ab, r.ar, r.af,
             rng ? "   OK" : "   超出当前舵机行程(待标定)");
      if (!rng) over++;
    }
    ok(geoBad == 0, "%s 全部顶点几何可达（ρ 落在 53~213）", s.name);
    printf("        => %d/%d 个顶点超出【当前】舵机行程\n", over, wayN);
  }
}

// ============================================================
// T3 直线 + 插补步数
// ============================================================
static void testLine() {
  section("T3  直线绘制（'1' 命令路径）");
  drawState = DRAW_IDLE;
  Serial.clear();
  const float x1 = 70, y1 = -30, x2 = 130, y2 = 30;
  drawLine(x1, y1, x2, y2);

  printf("     起点 (%.0f,%.0f)  终点 (%.0f,%.0f)\n", x1, y1, x2, y2);
  printf("     装载航点数=%d  航点=(%.0f,%.0f,%.0f)  状态=%d\n",
         wayN, way[0].x, way[0].y, way[0].z, (int)drawState);
  ok(wayN == 1, "drawLine 只装载 1 个航点");
  ok(fabs(way[0].x - x2) < 0.01f && fabs(way[0].y - y2) < 0.01f, "航点坐标 = 终点");
  ok(fabs(way[0].z - penZDown) < 0.01f, "航点 z = penZDown(%.0f)", penZDown);
  ok(drawState == DRAW_RUN, "状态进入 DRAW_RUN");
  ok(!Serial.has("+IK FAIL"), "起笔/下笔阶段没有 IK 失败");

  float dist = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  printf("     直线长度=%.1f mm，每步 %.1f mm，预期 ≈ %d 步\n",
         dist, DRAW_STEP_MM, (int)(dist / DRAW_STEP_MM) + 1);

  int steps = pumpUntilNotRun();
  printf("     实际推进 %d 次 drawTick 后状态=%d\n", steps, (int)drawState);
  ok(drawState == DRAW_DONE, "画完自动进入 DRAW_DONE");
  ok(steps >= (int)(dist / DRAW_STEP_MM), "推进步数 %d ≥ 理论 %d", steps, (int)(dist / DRAW_STEP_MM));
  if (Serial.has("+Angle out of range"))
    printf("     -> 出现角度超限报警：当前占位标定(A0=90/σ=1)下的必然结果（非代码缺陷），见 T9\n");
  else
    printf("     -> 全程无角度超限\n");
}

// ============================================================
// T4 状态机：暂停 / 继续 / 取消
// ============================================================
static void testStateMachine() {
  section("T4  状态机（暂停 / 继续 / 取消）");
  drawState = DRAW_IDLE;
  drawLine(70, -30, 130, 30);

  // 先走两步
  for (int i = 0; i < 2; i++) { g_millis += DRAW_STEP_MS + 1; drawTick(); }
  float px = curX, py = curY;
  printf("     走 2 步后：cur=(%.1f, %.1f)  状态=%d\n", px, py, (int)drawState);

  // 暂停
  Serial.clear(); drawPause();
  ok(drawState == DRAW_PAUSE, "drawPause() -> DRAW_PAUSE");
  ok(Serial.has("+DRAW PAUSE"), "打印 +DRAW PAUSE");
  // 暂停期间推进时间，loop 不会调 drawTick
  g_millis += 5000;
  printf("     暂停 5 秒后：cur=(%.1f, %.1f)  （应原地不动）\n", curX, curY);
  ok(fabs(curX - px) < 0.01f && fabs(curY - py) < 0.01f, "暂停期间末端位置不变");

  // 重复暂停应被拒
  Serial.clear(); drawPause();
  ok(Serial.has("+Not drawing, cannot pause"), "重复暂停被拒绝并提示");

  // 继续
  Serial.clear(); drawResume();
  ok(drawState == DRAW_RUN, "drawResume() -> DRAW_RUN");
  ok(Serial.has("+DRAW RESUME"), "打印 +DRAW RESUME");

  // 继续后能画完
  int steps = pumpUntilNotRun();
  printf("     继续后推进 %d 步 -> 状态=%d\n", steps, (int)drawState);
  ok(drawState == DRAW_DONE, "继续后能正常画完");

  // 重复继续应被拒
  Serial.clear(); drawResume();
  ok(Serial.has("+Not paused, cannot resume"), "未暂停时 resume 被拒绝");

  // 取消
  drawLine(70, -30, 130, 30);
  g_millis += DRAW_STEP_MS + 1; drawTick();
  Serial.clear(); drawCancel();
  ok(drawState == DRAW_IDLE, "drawCancel() -> DRAW_IDLE");
  ok(wayN == 0 && wayI == 0, "取消后航点被清空");
  ok(Serial.has("+DRAW CANCEL"), "打印 +DRAW CANCEL");

  // 未画时暂停
  Serial.clear(); drawPause();
  ok(Serial.has("+Not drawing, cannot pause"), "空闲时 pause 被拒绝");
}

// ============================================================
// T5 Catmull-Rom：曲线必须过控制点
// ============================================================
static void testCatmullRom() {
  section("T5  Catmull-Rom 平滑曲线");
  float P[5][3] = {
    { 0.0f,   0.0f, 10.0f},
    {10.0f,  20.0f, 15.0f},
    {20.0f, -10.0f, 20.0f},
    {30.0f,  15.0f, 25.0f},
    {40.0f,   0.0f, 30.0f}
  };
  for (int i = 0; i < 5; i++) for (int k = 0; k < 3; k++) teachXYZ[i][k] = P[i][k];

  float maxErr = 0;
  for (int k = 0; k < 5; k++) {
    float ox, oy, oz;
    catmullRom((float)k / 4.0f, ox, oy, oz);
    float e = sqrtf((ox - P[k][0]) * (ox - P[k][0]) +
                    (oy - P[k][1]) * (oy - P[k][1]) +
                    (oz - P[k][2]) * (oz - P[k][2]));
    printf("     t=%.2f -> 曲线点 (%.3f, %.3f, %.3f)   控制点 P%d (%.0f, %.0f, %.0f)   误差 %.2e\n",
           k / 4.0f, ox, oy, oz, k, P[k][0], P[k][1], P[k][2], e);
    if (e > maxErr) maxErr = e;
  }
  ok(maxErr < 1e-3f, "曲线过全部 5 个控制点（最大误差 %.2e）", maxErr);

  // 端点外推：t 略超 1 不应崩
  float ox, oy, oz;
  catmullRom(1.0f, ox, oy, oz);
  ok(fabs(ox - P[4][0]) < 1e-3f, "t=1 落在最后一点");
  catmullRom(0.0f, ox, oy, oz);
  ok(fabs(ox - P[0][0]) < 1e-3f, "t=0 落在第一点");

  // 曲线是否平滑（相邻采样点距离不应突变）
  float prevx = 0, prevy = 0, prevz = 0, maxSeg = 0, minSeg = 1e9f;
  for (int i = 0; i <= 200; i++) {
    float t = i / 200.0f;
    catmullRom(t, ox, oy, oz);
    if (i > 0) {
      float d = sqrtf((ox - prevx) * (ox - prevx) + (oy - prevy) * (oy - prevy) + (oz - prevz) * (oz - prevz));
      if (d > maxSeg) maxSeg = d;
      if (d < minSeg) minSeg = d;
    }
    prevx = ox; prevy = oy; prevz = oz;
  }
  printf("     相邻采样点间距: min=%.3f  max=%.3f  (比值 %.1f)\n", minSeg, maxSeg, maxSeg / minSeg);
  printf("     注：Catmull-Rom 用参数 t 均匀采样，不是按弧长，间距天然不均匀\n");
  ok(maxSeg / minSeg < 10.0f, "曲线无异常突跳（最大/最小间距比 %.1f < 10）", maxSeg / minSeg);
}

// ============================================================
// T6 正解 -> 逆解 往返一致性
// ============================================================
static void testRoundTrip() {
  section("T6  正解 → 逆解 → 正解 位置往返一致性");
  auto fkFromTheta = [](float t1, float t2, float t3, float& ox, float& oy, float& oz) {
    float R = ARM_L1 * cosf(t2) + ARM_LF * cosf(t2 + t3);
    ox = (ARM_D + R) * cosf(t1);
    oy = (ARM_D + R) * sinf(t1);
    oz = ARM_H + ARM_L1 * sinf(t2) + ARM_LF * sinf(t2 + t3);
  };

  int cases = 0, posOk = 0, baseOk = 0;
  float worstPos = 0;
  for (int b = 30; b <= 150; b += 30)
    for (int r = 30; r <= 100; r += 10)
      for (int f = 50; f <= 130; f += 20) {
        base.write(b); rArm.write(r); fArm.write(f);
        float x, y, z;
        forwardKinematics(x, y, z);
        float t1, t2, t3;
        if (!inverseKinematics(x, y, z, t1, t2, t3)) continue;
        float x2, y2, z2;
        fkFromTheta(t1, t2, t3, x2, y2, z2);
        float e = sqrtf((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y) + (z2 - z) * (z2 - z));
        cases++;
        if (e < 0.05f) posOk++;
        if (e > worstPos) worstPos = e;
        if (abs((int)lroundf(th2servoBase(t1)) - b) <= 1) baseOk++;
      }
  printf("     采样 %d 组姿态\n", cases);
  ok(posOk == cases, "位置往返一致 %d/%d，最大误差 %.4f mm", posOk, cases, worstPos);
  printf("     底座角 θ1 还原 %d/%d（±1°，解耦正确）\n", baseOk, cases);
  printf("     注：逆解固定取【肘上解】，θ2/θ3 与原姿态不同属正常 —— 末端位置一致即可\n");
}

// ============================================================
// T7 五点示教流程
// ============================================================
static void testTeachFlow() {
  section("T7  五点示教（记录 -> 折线 / 曲线）");
  teachClear();
  ok(teachCount == 0, "teachClear() 清空计数");

  int poses[5][4] = {
    { 90,  90,  90, 90},
    { 90,  90, 120, 90},
    {120,  90, 120, 90},
    {120,  90,  90, 90},
    { 90,  90,  90, 90}
  };
  for (int i = 0; i < 5; i++) {
    basePos = poses[i][0]; rArmPos = poses[i][1]; fArmPos = poses[i][2]; clawPos = poses[i][3];
    teachRecord();
  }
  ok(teachCount == 5, "记录 5 个示教点后 teachCount=5");

  // 再记一个应被拒绝
  Serial.clear(); teachRecord();
  ok(teachCount == 5 && Serial.has("! teach full"), "第 6 个点被拒绝（上限 5）");

  // 折线模式
  drawState = DRAW_IDLE;
  Serial.clear();
  runTeach(false);
  printf("     折线模式：航点数=%d  状态=%d\n", wayN, (int)drawState);
  ok(wayN == 5, "折线模式装载 5 个航点");
  ok(drawState == DRAW_RUN, "状态 = DRAW_RUN");

  int bad = 0, geoBad = 0;
  for (int i = 0; i < teachCount; i++) {
    printf("        T%d 姿态(%d,%d,%d) -> 末端 (%.1f, %.1f, %.1f)\n",
           i + 1, teachPts[i][0], teachPts[i][1], teachPts[i][2],
           teachXYZ[i][0], teachXYZ[i][1], teachXYZ[i][2]);
    IkRes r = ikOf(teachXYZ[i][0], teachXYZ[i][1], teachXYZ[i][2]);
    if (!r.ok) { geoBad++; continue; }
    if (!inR(r.ab, baseMin, baseMax) || !inR(r.ar, rArmMin, rArmMax) || !inR(r.af, fArmMin, fArmMax)) bad++;
  }
  ok(geoBad == 0, "5 个示教点正解后的坐标几何可达");
  printf("        => %d/5 超出【当前】舵机行程（占位标定下属预期，见 T9）\n", bad);

  int steps = pumpUntilNotRun();
  printf("     折线画完用了 %d 步，状态=%d\n", steps, (int)drawState);
  ok(drawState == DRAW_DONE, "折线能画完");

  // 曲线模式
  drawState = DRAW_IDLE;
  Serial.clear();
  runTeach(true);
  printf("     曲线模式：drawCurve=%d  drawDT=%.5f  总步数≈%d  状态=%d\n",
         (int)drawCurve, drawDT, (int)(1.0f / drawDT), (int)drawState);
  ok(drawCurve, "曲线模式打开 drawCurve");
  ok(drawDT > 0 && drawDT <= 1.0f, "步长 drawDT 合法");
  ok((int)(1.0f / drawDT) >= 20, "曲线步数 ≥ 20");

  // 诊断：曲线上有没有越出工作空间的点
  int badT = -1; float bx = 0, by = 0, bz = 0, brho = 0;
  for (int i = 0; i <= 1000; i++) {
    float t = i / 1000.0f, ox, oy, oz, q1, q2, q3;
    catmullRom(t, ox, oy, oz);
    if (!inverseKinematics(ox, oy, oz, q1, q2, q3)) {
      badT = i; bx = ox; by = oy; bz = oz;
      float rr = sqrtf(ox * ox + oy * oy) - ARM_D;
      brho = sqrtf(rr * rr + (oz - ARM_H) * (oz - ARM_H));
      break;
    }
  }
  if (badT < 0) printf("     诊断：曲线 1001 个采样点全部几何可达\n");
  else printf("     诊断：t=%.3f 处曲线点 (%.1f, %.1f, %.1f) 越界，ρ=%.1f（允许 53~213）\n",
              badT / 1000.0f, bx, by, bz, brho);

  steps = pumpUntilNotRun(20000);
  printf("     曲线画完用了 %d 步，状态=%d%s\n", steps, (int)drawState,
         drawState == DRAW_DONE ? "" : "  （途中遇到不可达点，drawTick 自动 drawCancel）");
  if (badT < 0) ok(drawState == DRAW_DONE, "曲线能画完");
  else          ok(drawState == DRAW_IDLE, "曲线遇不可达点后正确转入 CANCEL/IDLE（未卡死）");
}

// ============================================================
// T8 标定敏感性：笔尖高度变化时还可达吗
// ============================================================
static void testCalibSensitivity() {
  section("T8  标定敏感性（penZDown 变化对可达性的影响）");
  float saved = penZDown;
  const float zs[] = {0, 10, 20, 30, 40, 50, 60, 70};

  printf("     %-10s %-12s %-12s %-12s\n", "penZDown", "N字", "三角", "V字");
  for (float z : zs) {
    penZDown = z;
    int bad[3] = {0, 0, 0};
    char ids[3] = {'N', 'T', 'V'};
    for (int s = 0; s < 3; s++) {
      drawState = DRAW_IDLE;
      drawShape(ids[s]);
      for (int i = 0; i < wayN; i++) {
        IkRes r = ikOf(way[i].x, way[i].y, way[i].z);
        if (!r.ok || !inR(r.ab, baseMin, baseMax) || !inR(r.ar, rArmMin, rArmMax) || !inR(r.af, fArmMin, fArmMax))
          bad[s]++;
      }
    }
    printf("     %-10.0f %-12s %-12s %-12s\n",
           z,
           bad[0] ? "不可达 ⚠" : "OK",
           bad[1] ? "不可达 ⚠" : "OK",
           bad[2] ? "不可达 ⚠" : "OK");
  }
  penZDown = saved;

  // 当前占位值下结论
  drawState = DRAW_IDLE;
  Serial.clear();
  drawShape('N');
  ok(!Serial.has("+IK FAIL"), "当前 penZDown=%.0f 下 N 字起笔不失败", penZDown);
}

// ============================================================
// T9 几何需求：任务五的图形点到底需要多大的肘角？（纯几何，与标定无关）
// ============================================================
static void testGeometryDemand() {
  section("T9  图形点对肘角的需求（纯几何，与标定无关）");
  printf("     %-18s %-9s %-11s\n", "点 (x, y, z)", "ρ(mm)", "所需 θ3(°)");
  struct { float x, y; } pts[] = {
    { 70, -30}, { 70, 30}, {130, -30}, {130, 30}, {100, 35}, {100, -30}
  };
  for (auto& p : pts) {
    float x = p.x, y = p.y, z = penZDown;
    float r  = sqrtf(x * x + y * y) - ARM_D;
    float zp = z - ARM_H;
    float rho = sqrtf(r * r + zp * zp);
    float c3 = (rho * rho - ARM_L1 * ARM_L1 - ARM_LF * ARM_LF) / (2.0f * ARM_L1 * ARM_LF);
    if (c3 > 1) c3 = 1;
    if (c3 < -1) c3 = -1;
    float th3 = -degrees(acosf(c3));
    printf("     (%6.0f,%5.0f,%4.0f)  %7.1f   %8.1f\n", x, y, z, rho, th3);
  }

  // ρ 随 θ3 的变化：ρ² = L1² + Lf² + 2·L1·Lf·cos θ3
  auto rhoOf = [](float thDeg) {
    float c = cosf(radians(thDeg));
    return sqrtf(ARM_L1 * ARM_L1 + ARM_LF * ARM_LF + 2.0f * ARM_L1 * ARM_LF * c);
  };
  printf("\n     肘角 -> 可达半径 对照：\n");
  for (float th : {0.0f, -45.0f, -90.0f, -120.0f, -140.0f, -155.0f, -170.0f})
    printf("        θ3 = %6.0f°  ->  ρ = %6.1f mm\n", th, rhoOf(th));

  printf("\n     关键：fArm 软限位 40~130 只有 90° 行程；若传动比 1:1，\n");
  printf("     这 90° 只能覆盖 90° 宽的 θ3 区间：\n");
  printf("       · 区间取 [0°, -90°]    -> ρ ∈ [%.0f, %.0f]  （够不到半径 70~133 的图形）\n",
         rhoOf(-90.0f), rhoOf(0.0f));
  printf("       · 区间取 [-155°, -65°] -> ρ ∈ [%.0f, %.0f]  （能覆盖图形，但要求肘部反折 155°）\n",
         rhoOf(-155.0f), rhoOf(-65.0f));
  printf("     => 标定 A0_FARM 时必须实测：肘部到底能不能反折到 120°~155°。\n");
  printf("        若不能，就要放宽 fArm 软限位或引入前臂传动比（GAIN_FARM）。\n");
}

// ============================================================
// T10 按键：UI 模式隔离 + 任务五手势（真的模拟引脚电平）
// ============================================================
static void btnStep(unsigned long dt) { g_millis += dt; scanButtons(); }

// 走够消抖时间：chgMs 是在"看到电平变化的那一次调用"里才设的，
// 所以需要 4 次 10ms —— 第 1 次记时、到第 4 次才真的跨过 25ms 阈值。
static void settle() { btnStep(10); btnStep(10); btnStep(10); btnStep(10); }

static void pressButton(int idx, unsigned long holdMs = 40) {
  int pin = BTN_PIN[idx];
  g_pinLevel[pin] = LOW;
  settle();
  if (holdMs > 40) btnStep(holdMs - 40);
  g_pinLevel[pin] = HIGH;
  settle();
  btnStep(BTN_DOUBLE_MS + 20);                  // 越过双击窗口 -> 判定为单击
}

static void doubleClick(int idx) {
  int pin = BTN_PIN[idx];
  for (int k = 0; k < 2; k++) {
    g_pinLevel[pin] = LOW;  settle();
    g_pinLevel[pin] = HIGH; settle();
  }
  btnStep(BTN_DOUBLE_MS + 20);
}

static void longPress(int idx) {
  int pin = BTN_PIN[idx];
  g_pinLevel[pin] = LOW;
  settle();
  btnStep(BTN_LONG_MS + 50);          // 触发长按
  g_pinLevel[pin] = HIGH;
  settle();                           // 必须走完消抖让 stable 回到 HIGH，
  btnStep(50);                        // 否则下一次按下时 raw==stable、clicks 不会累加
}

static void testButtonModes() {
  section("T10 按键：UI 模式隔离 + 任务五手势");

  for (int i = 0; i < 32; i++) g_pinLevel[i] = HIGH;
  resetButtons();
  uiMode    = UI_TASK3;
  drawState = DRAW_IDLE;      // T8/T9 会在 DRAW_RUN 上留下状态，必须先归零
  teachClear();

  // 1) 任务三模式下左单击 —— 不应碰到示教功能
  Serial.clear(); pressButton(0);
  ok(!Serial.has("+Teach"), "Task3 模式：左单击不触发示教记点");

  // 2) 长按左键 -> 切到 Task5
  Serial.clear(); longPress(0);
  ok(uiMode == UI_TASK5, "长按左键切到 Task5 模式");
  ok(Serial.has("+UI MODE = Task5"), "串口回显 Task5 模式");

  // 3) 左单击 = 记点
  teachClear();
  Serial.clear(); pressButton(0);
  ok(Serial.has("+Teach #1"), "Task5：左单击记下第 1 个示教点");

  // 4) 不足 5 点时右单击 -> 拒绝
  Serial.clear(); pressButton(1);
  ok(Serial.has("! need 5 teach points"), "Task5：不足 5 点时拒绝启动绘制");

  // 5) 记满 5 点后右单击 -> 折线
  for (int i = 0; i < 4; i++) pressButton(0);
  ok(teachCount == 5, "记满 5 个示教点（teachCount=%d）", teachCount);
  Serial.clear(); pressButton(1);
  ok(Serial.has("+DRAW POLYLINE RUN"), "Task5：右单击启动折线绘制");
  ok(drawState == DRAW_RUN, "状态 = DRAW_RUN");

  // 6) 绘制中左单击 = 暂停
  g_millis += DRAW_STEP_MS + 1; drawTick();
  Serial.clear(); pressButton(0);
  ok(drawState == DRAW_PAUSE, "绘制中左单击 -> 暂停");
  ok(Serial.has("+DRAW PAUSE"), "串口回显 +DRAW PAUSE");

  // 7) 再左单击 = 继续
  Serial.clear(); pressButton(0);
  ok(drawState == DRAW_RUN, "再左单击 -> 继续");
  ok(Serial.has("+DRAW RESUME"), "串口回显 +DRAW RESUME");

  // 8) 左双击 = 取消
  Serial.clear(); doubleClick(0);
  ok(drawState == DRAW_IDLE, "绘制中左双击 -> 取消");
  ok(Serial.has("+DRAW CANCEL"), "串口回显 +DRAW CANCEL");

  // 9) 右双击 = 平滑曲线
  teachClear();
  for (int i = 0; i < 5; i++) pressButton(0);
  Serial.clear(); doubleClick(1);
  ok(Serial.has("+DRAW CURVE RUN"), "Task5：右双击启动平滑曲线");
  ok(drawCurve, "曲线模式已打开");
  drawCancel();

  // 10) 长按左键切回 Task3
  Serial.clear(); longPress(0);
  ok(uiMode == UI_TASK3, "长按左键切回 Task3 模式");

  // 11) 串口 'm' 也能切
  armDataCmd('m');
  ok(uiMode == UI_TASK5, "串口发 'm' 切到 Task5");
  armDataCmd('m');
  ok(uiMode == UI_TASK3, "再发 'm' 切回 Task3");

  // 12) 绘制态下任务三的手势不应生效（隔离验证）
  uiMode = UI_TASK5;
  teachClear();
  for (int i = 0; i < 5; i++) pressButton(0);
  pressButton(1);                       // 进入绘制
  Serial.clear();
  pressButton(1);                       // 绘制中右单击 = 无效手势
  ok(drawState == DRAW_RUN, "绘制中右单击不影响绘制状态");
  ok(Serial.has("+Task5 drawing:"), "绘制中按无效键有明确提示");
  drawCancel();
  uiMode = UI_TASK3;
}

// ============================================================
int main() {
  printf("\n============================================================\n");
  printf("  meArm 任务五 · PC 端自动化测试\n");
  printf("  被测源码: /Users/mac/Desktop/MeArm/Task1235/Task1235.ino\n");
  printf("============================================================\n");

  testReachBoundary();
  testShapes();
  testLine();
  testStateMachine();
  testCatmullRom();
  testRoundTrip();
  testTeachFlow();
  testCalibSensitivity();
  testGeometryDemand();
  testButtonModes();

  printf("\n============================================================\n");
  if (g_fail == 0) printf("  全部通过 ✅\n");
  else             printf("  失败 %d 项 ❌\n", g_fail);
  printf("============================================================\n");
  return g_fail ? 1 : 0;
}
