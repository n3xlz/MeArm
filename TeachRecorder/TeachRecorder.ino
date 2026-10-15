/* ============================================================
 *  TeachRecorder —— 示教记录器（任务二点位标定工具）
 * ------------------------------------------------------------
 *  用途：把机械臂【用手 / 摇杆摆到某个物理位置】，一键记下这一
 *        刻四个舵机的角度；摆一个记一个。最后串口打印出可以
 *        直接粘贴进 SEQ_A / SEQ_B / SEQ_C 的 C 数组。
 *
 *  —— 工作流程（照着做一遍就懂）——
 *    1) 用摇杆把机械臂摆到第 1 个位置（例如"取物位上方"）
 *    2) 按【左摇杆键】记下第 1 个点（串口会回显角度）
 *    3) 摆到第 2 个位置，再按一次，记下第 2 个点
 *    4) …一直摆到整个动作结束
 *    5) 串口发 L → 打印出整段 C 数组，整段复制粘贴进 Mission.ino
 *
 *  —— 任务二的 9 个位置，照这个顺序摆 ——
 *      1 取物位上方  2 降到取物位  3 合爪夹紧   4 抬起
 *      5 转到放置位上方  6 降到放置位  7 张爪松开  8 抬起  9 回中
 *    （照这个顺序摆完 9 个，直接就是一份完整的 SEQ_X）
 *
 *  —— 串口命令 ——
 *    R  记录当前姿态（Record）
 *    D  删除最后一条（Delete）
 *    X  清空全部（Clear）
 *    L  列出全部（打印成可直接粘贴的 C 数组）
 *    P  回放一遍（验证记录对不对）
 *    I  回中（四轴回 90）
 *    O  爪张开        S  爪闭合
 *    b/r/f/c <值>     单轴直接设角度，例如 b90
 *    M  摇杆开关（关掉后只认串口，防止摆位时误碰）
 *    ?  打印当前状态
 *
 *  —— 摇杆按键（复用两个摇杆自带的按压开关）——
 *    左摇杆 单击 = 记录一点      左摇杆 双击 = 删除最后一条
 *    右摇杆 单击 = 回放          右摇杆 双击 = 回中
 *
 *  ⚠ 单击要等约 0.4 秒才响应，这是区分单双击的代价，不是 bug。
 *  ⚠ 本程序是【独立工具】，不改动 MeArm.ino（比赛程序）。
 * ============================================================ */

#include <Servo.h>
Servo base, rArm, fArm, claw;

int basePos = 90;
int rArmPos = 90;
int fArmPos = 90;
int clawPos = 90;

const int baseMin = 0,  baseMax = 180;
const int rArmMin = 20, rArmMax = 110;
const int fArmMin = 40, fArmMax = 130;
const int clawMin = 25, clawMax = 110;

int DSD = 15;                 // 回放时每步延迟，越小动作越快

// ---- 摇杆 ----
bool joyOn = true;
const int JOY_MID_A0 = 507;   // 左摇杆：底座
const int JOY_MID_A1 = 529;   // 左摇杆：后臂
const int JOY_MID_A2 = 498;   // 右摇杆：前臂
const int JOY_MID_A3 = 536;   // 右摇杆：爪子
const int JOY_DEADZONE = 60;
int moveStep = 3;

// ---- 记录表 ----
#define MAX_POINTS 32
byte pts[MAX_POINTS][4];      // [第几个点][base, rArm, fArm, claw]
int  ptCount = 0;

// ---- 摇杆按键 ----
const byte BTN_PIN[2] = { 2, 4 };       // [0]=左摇杆键  [1]=右摇杆键

const unsigned long BTN_DEBOUNCE_MS = 25;
const unsigned long BTN_DOUBLE_MS   = 350;

struct BtnState {
  bool          lastRaw;
  bool          stable;
  unsigned long chgMs;
  byte          clicks;
  unsigned long relMs;
};
BtnState btn[2];


void setup() {
  base.attach(11); base.write(90); delay(200);
  rArm.attach(10); rArm.write(90); delay(200);
  fArm.attach(9);  fArm.write(90);  delay(200);
  claw.attach(5);  claw.write(90);  delay(200);

  for (byte i = 0; i < 2; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    btn[i].lastRaw = HIGH;
    btn[i].stable  = HIGH;
  }

  Serial.begin(9600);
  Serial.println(F("=== TeachRecorder Ready ==="));
  Serial.println(F("R=record D=del X=clear L=list P=play I=home ?=status"));
}


void loop() {
  if (Serial.available() > 0) {
    cmd(Serial.read());
  }
  scanButtons();     // 摇杆按键：单击 / 双击
  joyCtrl();         // 摇杆：摆位
  updateServos();    // 把目标角度推向舵机
}


// ============================================================
//  串口命令
// ============================================================
void cmd(char c) {
  if (c == '\r' || c == '\n') return;

  // 单轴直接设角度：b90 / r80 / f100 / c25
  if (c == 'b' || c == 'r' || c == 'f' || c == 'c') {
    setAxis(c, Serial.parseInt());
    return;
  }

  switch (c) {
    case 'R': recordPoint(); break;
    case 'D': deleteLast();  break;
    case 'X': clearAll();    break;
    case 'L': listPoints();  break;
    case 'P': playPoints(); resetButtons(); break;
    case 'I': goHome();      resetButtons(); break;
    case 'O': clawPos = clawMax; break;
    case 'S': clawPos = clawMin; break;
    case 'M':
      joyOn = !joyOn;
      Serial.print(F("Joystick "));
      Serial.println(joyOn ? F("ON") : F("OFF"));
      break;
    case '?': status(); break;
    default:
      Serial.println(F("? unknown, try R/D/X/L/P/I/O/S/M/?"));
      break;
  }
}


// ============================================================
//  记录 / 管理
// ============================================================

// 记下当前四个舵机角度
void recordPoint() {
  if (ptCount >= MAX_POINTS) {
    Serial.println(F("! table full (MAX_POINTS)"));
    return;
  }
  pts[ptCount][0] = basePos;
  pts[ptCount][1] = rArmPos;
  pts[ptCount][2] = fArmPos;
  pts[ptCount][3] = clawPos;
  ptCount++;

  Serial.print(F("+REC #"));
  Serial.print(ptCount);
  Serial.print(F("  { "));
  Serial.print(basePos); Serial.print(F(", "));
  Serial.print(rArmPos); Serial.print(F(", "));
  Serial.print(fArmPos); Serial.print(F(", "));
  Serial.print(clawPos);
  Serial.println(F(" }"));
}

// 删除最后一条
void deleteLast() {
  if (ptCount == 0) { Serial.println(F("! empty")); return; }
  ptCount--;
  Serial.print(F("-DEL, points = "));
  Serial.println(ptCount);
}

// 清空
void clearAll() {
  ptCount = 0;
  Serial.println(F("+CLEAR all"));
}

// 打印成可以直接粘贴进 SEQ 的 C 数组
void listPoints() {
  Serial.println();
  Serial.print(F("// ---- points = "));
  Serial.print(ptCount);
  Serial.println(F(" ----"));
  if (ptCount == 0) {
    Serial.println(F("// (empty)"));
    return;
  }

  Serial.println(F("const byte SEQ_X[][4] = {"));
  for (int i = 0; i < ptCount; i++) {
    Serial.print(F("  { "));
    Serial.print((int)pts[i][0]); Serial.print(F(", "));
    Serial.print((int)pts[i][1]); Serial.print(F(", "));
    Serial.print((int)pts[i][2]); Serial.print(F(", "));
    Serial.print((int)pts[i][3]);
    Serial.print(F(" }"));
    if (i < ptCount - 1) Serial.print(',');
    Serial.print(F("   // "));
    Serial.println(i + 1);
  }
  Serial.println(F("};"));
}

// 回放一遍，验证记录
void playPoints() {
  if (ptCount == 0) { Serial.println(F("! empty")); return; }
  Serial.print(F("+PLAY "));
  Serial.print(ptCount);
  Serial.println(F(" points"));

  for (int i = 0; i < ptCount; i++) {
    moveAll(pts[i][0], pts[i][1], pts[i][2], pts[i][3]);
  }
  Serial.println(F("+PLAY DONE"));
}


// ============================================================
//  动作
// ============================================================

// 四个关节同时插值走完（和 Mission.ino 的同名函数一个思路）
void moveAll(int tb, int tr, int tf, int tc) {
  tb = constrain(tb, baseMin, baseMax);
  tr = constrain(tr, rArmMin, rArmMax);
  tf = constrain(tf, fArmMin, fArmMax);
  tc = constrain(tc, clawMin, clawMax);

  int cb = base.read(), cr = rArm.read(), cf = fArm.read(), cc = claw.read();

  int d1 = abs(tb - cb), d2 = abs(tr - cr), d3 = abs(tf - cf), d4 = abs(tc - cc);
  int steps = d1;
  if (d2 > steps) steps = d2;
  if (d3 > steps) steps = d3;
  if (d4 > steps) steps = d4;
  if (steps == 0) return;

  for (int s = 1; s <= steps; s++) {
    base.write(cb + (tb - cb) * s / steps);
    rArm.write(cr + (tr - cr) * s / steps);
    fArm.write(cf + (tf - cf) * s / steps);
    claw.write(cc + (tc - cc) * s / steps);
    delay(DSD);
  }

  basePos = tb; rArmPos = tr; fArmPos = tf; clawPos = tc;
}

void goHome() {
  Serial.println(F("+HOME"));
  moveAll(90, 90, 90, 90);
}

void setAxis(char name, int v) {
  switch (name) {
    case 'b': basePos = constrain(v, baseMin, baseMax); break;
    case 'r': rArmPos = constrain(v, rArmMin, rArmMax); break;
    case 'f': fArmPos = constrain(v, fArmMin, fArmMax); break;
    case 'c': clawPos = constrain(v, clawMin, clawMax); break;
  }
  Serial.print(F("+set ")); Serial.print(name);
  Serial.print(' '); Serial.println(v);
}

void status() {
  Serial.println();
  Serial.print(F("now    = { "));
  Serial.print(basePos); Serial.print(F(", "));
  Serial.print(rArmPos); Serial.print(F(", "));
  Serial.print(fArmPos); Serial.print(F(", "));
  Serial.print(clawPos); Serial.println(F(" }"));
  Serial.print(F("points = ")); Serial.println(ptCount);
  Serial.print(F("joy    = ")); Serial.println(joyOn ? F("ON") : F("OFF"));
  Serial.println();
}


// ============================================================
//  摇杆
// ============================================================
int joyDelta(int pin, int mid) {
  int v = analogRead(pin);
  if (v > mid + JOY_DEADZONE) return  moveStep;
  if (v < mid - JOY_DEADZONE) return -moveStep;
  return 0;
}

void joyCtrl() {
  if (!joyOn) return;
  basePos += joyDelta(A0, JOY_MID_A0); basePos = constrain(basePos, baseMin, baseMax);
  rArmPos += joyDelta(A1, JOY_MID_A1); rArmPos = constrain(rArmPos, rArmMin, rArmMax);
  fArmPos += joyDelta(A2, JOY_MID_A2); fArmPos = constrain(fArmPos, fArmMin, fArmMax);
  clawPos += joyDelta(A3, JOY_MID_A3); clawPos = constrain(clawPos, clawMin, clawMax);
}

void updateServos() {
  if (basePos != base.read()) { base.write(basePos); delay(10); }
  if (rArmPos != rArm.read()) { rArm.write(rArmPos); delay(10); }
  if (fArmPos != fArm.read()) { fArm.write(fArmPos); delay(10); }
  if (clawPos != claw.read()) { claw.write(clawPos); delay(10); }
}


// ============================================================
//  摇杆按键：单击 / 双击
//    左单击 = 记录    左双击 = 删除
//    右单击 = 回放    右双击 = 回中
// ============================================================
void scanButtons() {
  unsigned long now = millis();

  for (byte i = 0; i < 2; i++) {
    bool raw = digitalRead(BTN_PIN[i]);

    if (raw != btn[i].lastRaw) {
      btn[i].lastRaw = raw;
      btn[i].chgMs   = now;
    }

    if (now - btn[i].chgMs >= BTN_DEBOUNCE_MS && raw != btn[i].stable) {
      btn[i].stable = raw;
      if (raw == LOW) btn[i].clicks++;
      else            btn[i].relMs = now;
    }

    if (btn[i].clicks > 0 && btn[i].stable == HIGH &&
        now - btn[i].relMs >= BTN_DOUBLE_MS) {
      byte n = btn[i].clicks;
      btn[i].clicks = 0;

      if (i == 0 && n == 1) {
        recordPoint();
      } else if (i == 0) {
        deleteLast();
      } else if (i == 1 && n == 1) {
        playPoints();
        resetButtons();
      } else {
        goHome();
        resetButtons();
      }
    }
  }
}

void resetButtons() {
  for (byte i = 0; i < 2; i++) {
    btn[i].lastRaw = HIGH;
    btn[i].stable  = HIGH;
    btn[i].clicks  = 0;
    btn[i].chgMs   = millis();
    btn[i].relMs   = millis();
  }
}
