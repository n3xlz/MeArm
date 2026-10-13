#include <Servo.h>
Servo base, fArm, rArm, claw;

/*
base 底座;fore arm 前臂;
rarm 后臂;claw 爪子;
*/ 

int basePos = 90;
int rArmPos = 90;
int fArmPos = 90;
int clawPos = 90;

const int baseMin = 0;
const int baseMax = 180;
const int rArmMin = 20;
const int rArmMax = 110;
const int fArmMin = 40;
const int fArmMax = 130;
const int clawMin = 25;
const int clawMax = 110;

int DSD = 15; //Default Servo Delay 默认电机运动延迟时间

// ============================================================
//  摇杆设置 —— 摇杆和串口指令【同时有效】
//
//  "增量映射"：摇杆偏离中位，角度才加一点 / 减一点。
//    后果：摇杆居中 = 什么都不做 = 指令设的位置被保留住。
//
//  为什么不用"绝对映射"（摇杆在哪手臂在哪）？见 DEVLOG.md 2026-10-12。
// ============================================================
bool joyOn = true;            // 摇杆总开关，默认开。发 M 可以临时关掉

const int JOY_MID_A0 = 507;   // 左摇杆 左右
const int JOY_MID_A1 = 529;   // 左摇杆 前后
const int JOY_MID_A2 = 498;   // 右摇杆 前后
const int JOY_MID_A3 = 536;   // 右摇杆 左右

const int JOY_DEADZONE = 60;  // 死区：偏离中位不超过这个数就不动（防抖、防漂移）

// 摇杆每轮对角度改变多少度 —— 越大动得越快。
int moveStep = 3;

// 摇杆四个轴分别管哪个关节
//   A0 = 左摇杆 左右 -> base 底座
//   A1 = 左摇杆 前后 -> rArm 后臂
//   A2 = 右摇杆 前后 -> fArm 前臂
//   A3 = 右摇杆 左右 -> Claw 爪子


// ============================================================
//  任务二：A / B / C 自动夹取与放置
// ------------------------------------------------------------
//  一个"姿态" = 四个关节的角度，顺序固定为：{ base, rArm, fArm, claw }
//  一个"动作序列" = 若干个姿态，机械臂按顺序一个一个走过去。
//
//  claw：值大 = 张开（clawMax = 110）；值小 = 夹紧（clawMin = 25）
//
//  ⚠ 下面三个序列里的数字都是【示例值】—— 能让整条动作流程跑起来，
//     但抓不到真实的物体。必须按"标定流程"实测后替换成你自己的值。
//
// ============================================================

// —— 物体 A（三枚硬币）——
const byte SEQ_A[][4] = {
  {  20,  75, 110, 100 },   // 1 取物位上方（爪张开）
  {  20,  62,  95, 100 },   // 2 下降到取物位
  {  20,  62,  95,  25 },   // 3 合爪夹紧
  {  20,  75, 110,  25 },   // 4 抬起（保持夹紧）
  { 110,  75, 110,  25 },   // 5 转到放置位上方
  { 110,  62,  95,  25 },   // 6 下降到放置位
  { 110,  62,  95, 100 },   // 7 张爪松开
  { 110,  75, 110, 100 },   // 8 抬起
  {  90,  90,  90,  90 }    // 9 回中
};

// —— 物体 B（笔）——
const byte SEQ_B[][4] = {
  {  70,  80, 112, 100 },   // 1 取物位上方
  {  70,  68,  98, 100 },   // 2 下降到取物位
  {  70,  68,  98,  25 },   // 3 合爪夹紧
  {  70,  80, 112,  25 },   // 4 抬起
  { 160,  80, 112,  25 },   // 5 转到放置位上方
  { 160,  68,  98,  25 },   // 6 下降到放置位
  { 160,  68,  98, 100 },   // 7 张爪松开
  { 160,  80, 112, 100 },   // 8 抬起
  {  90,  90,  90,  90 }    // 9 回中
};

// —— 物体 C（自定义物体）——
const byte SEQ_C[][4] = {
  { 120,  85, 115, 100 },   // 1 取物位上方
  { 120,  72, 100, 100 },   // 2 下降到取物位
  { 120,  72, 100,  25 },   // 3 合爪夹紧
  { 120,  85, 115,  25 },   // 4 抬起
  {  10,  85, 115,  25 },   // 5 转到放置位上方
  {  10,  72, 100,  25 },   // 6 下降到放置位
  {  10,  72, 100, 100 },   // 7 张爪松开
  {  10,  85, 115, 100 },   // 8 抬起
  {  90,  90,  90,  90 }    // 9 回中
};


// ============================================================
//  任务三：4 个按键（机械臂侧）
// ------------------------------------------------------------
//  这 4 个功能有【两条触发路径】，最终都落到同一批函数上：
//
//    路径 A（当前在用） 复用两个摇杆自带的按压开关，用"单击 / 双击"
//                       组合出 4 个按键 —— 展开说明见下面那一块。
//    路径 B（接口预留） 电路组自制的按键遥控板，通过串口发 1 个字符。
//                       板子真做出来了就接这条路，机械臂侧不用改代码。
//
//  路径 B 的字符协议（下面四行要发给电路组同学）：
//
//    'N' 按键1  循环夹取：A -> B -> C -> A ... 每按一次做一个
//    'R' 按键2  录制：第一次按下开始，第二次按下结束并保存
//    'P' 按键3  播放：复现上一次录制的动作
//    'Z' 按键4  回中：四轴回到 90
//
//  已有的 'I' 保持不动（'Z' 与它等效）。为什么用字母而不是数字、
//  为什么另加 'Z' —— 见 DEVLOG.md 2026-10-13。
// ============================================================

// —— 录制参数 ——
// FRAME_MS  : 每多少毫秒存一帧。100ms = 每秒 10 帧
// MAX_FRAMES: 最多存多少帧。200 帧 × 100ms = 20 秒（任务要求 >10 秒）
// SUBSTEPS  : 播放时两帧之间插 5 小步，让动作看起来连续
// 每帧 4 个 byte，200 帧 = 800 字节 RAM。
#define FRAME_MS    100
#define MAX_FRAMES  200
#define SUBSTEPS    5

byte recFrames[MAX_FRAMES][4];   // 录下来的动作：[帧号][四轴]

bool recording = false;          // 是否正在录制
int  frameCount = 0;             // 已经录了多少帧
unsigned long recLastMs = 0;     // 上一帧的时间戳（配合 millis() 用）

int cycleIdx = 0;                // 按键1 循环到第几个物体：0=A 1=B 2=C


// ============================================================
//  任务三的"按键"从哪来 —— 路径 A：复用两个摇杆自带的按压开关
// ------------------------------------------------------------
//  不用额外按键板：把摇杆帽垂直往下按，"嗒"一下就是一个按键。
//  左右各一个，再用"按几下"组合出 4 种手势 = 任务要的 4 个按键：
//
//      左摇杆 单击  ->  按键1  循环夹取（A -> B -> C -> A）
//      右摇杆 单击  ->  按键2  录制 开始 / 结束
//      左摇杆 双击  ->  按键3  播放上一次录制
//      右摇杆 双击  ->  按键4  回中
//
//  ⚠ 单击按下后要约 0.4 秒才响应 —— 这是区分单双击的必然代价，
//     不是 bug。原因和两个时间参数见 DEVLOG.md 2026-10-13。
// ============================================================

// 两个摇杆按键的引脚（2026-10-12 实测确认）
const byte BTN_PIN[2] = { 2, 4 };            // [0]=左摇杆按键  [1]=右摇杆按键

const unsigned long BTN_DEBOUNCE_MS = 25;    // 消抖：电平要稳定这么久才算数
const unsigned long BTN_DOUBLE_MS   = 350;   // 双击窗口：两次点击间隔小于它算双击

// 每个按键的状态（判断单击 / 双击要用到的几个量）
struct BtnState {
  bool          lastRaw;   // 上一次读到的原始电平
  bool          stable;    // 消抖后确认的稳定电平（LOW = 按着）
  unsigned long chgMs;     // 电平开始变化的时刻（消抖计时用）
  byte          clicks;    // 已经累计了几次点击
  unsigned long relMs;     // 最后一次松手的时刻（双击窗口计时用）
};

BtnState btn[2];


void setup() {

  joyOn = true;
  base.attach(11);
  base.write(90);
  delay(200);

  rArm.attach(10);
  rArm.write(90);
  delay(200);

  fArm.attach(9);
  fArm.write(90);
  delay(200);

  claw.attach(5);
  claw.write(90);
  delay(200);
  
  // —— 摇杆按键初始化 ——
  // INPUT_PULLUP：引脚内部接上拉电阻，没按下读 HIGH、按下读 LOW，
  // 外面不用再焊电阻。
  for(byte i = 0; i < 2; i++) {
    pinMode(BTN_PIN[i], INPUT_PULLUP);
    btn[i].lastRaw = HIGH;
    btn[i].stable  = HIGH;
  }

  Serial.begin(9600);  //串口波特率
  Serial.println(F("Start!"));
  Serial.println(F("Command Ready / Joystick ON"));

}

void loop() {
  // ① 串口指令：随时可发，不受摇杆影响
  if(Serial.available() > 0) {
    char serialCmd = Serial.read();
    armDataCmd(serialCmd);
  }

  // ② 摇杆按键：识别单击 / 双击，识别到就触发对应功能。
  //    同样是非阻塞的 —— 只读电平 + 看 millis()，一次 delay 都不打
  scanButtons();

  // ③ 摇杆：每一轮都读。
  //    摇杆居中时它什么都不改，所以刚发完的指令位置会被原样保留下来
  joyCtrl();

  // ④ 录制采样：正在录的话，每 FRAME_MS 存一帧当前姿态
  //    非阻塞，所以录制期间摇杆照样能操控（按键2 要的就是这个）
  recordTick();

  // ⑤ 把四个目标角度推向舵机
  updateServos();

}

void armDataCmd(char serialCmd) {
  if(serialCmd == '\r' || serialCmd == '\n') return;
  int servoData;
  if(serialCmd == 'b' || serialCmd == 'r' || serialCmd == 'f' || serialCmd == 'c') {
    servoData = Serial.parseInt();
    servoCmd(serialCmd,servoData,DSD);
  } else if(serialCmd == 'x') {
    servoData = Serial.parseInt();
    servoCmd('b',servoData,DSD);
    servoData = Serial.parseInt();
    servoCmd('r',servoData,DSD);
    servoData = Serial.parseInt();
    servoCmd('f',servoData,DSD);
    char clear;
    while(Serial.available()>0) clear = Serial.read();
    Serial.println();
  } else {
    switch(serialCmd) {

      case 'O': //机械臂爪子张开
        Serial.println(F("Received Command:Claw Open"));
        clawPos = clawMax;
        servoCmd('c',clawPos,DSD);
        break;
      case 'S': //机械臂爪子关闭
        Serial.println(F("Received Command:Claw Close"));
        clawPos = clawMin;
        servoCmd('c',clawPos,DSD);
        break;
      case 'H': //提升机械臂整体运行速度
        Serial.println(F("Received Command:Speed Higher"));
        DSD -= 3;
        if(DSD <= 9) {
          DSD = 9;
          Serial.println(F("Servo Speed is Too High."));
        } else if(DSD >= 45) {
          DSD = 45;
          Serial.println(F("Servo Speed is Too Low."));
        }
        break;
      case 'L': //降低机械臂整体运行速度
        Serial.println(F("Received Command:Speed Lower"));
        DSD += 3;
        if(DSD <= 9) {
          DSD = 9;
          Serial.println(F("Servo Speed is Too High."));
        } else if(DSD >= 45) {
          DSD = 45;
          Serial.println(F("Servo Speed is Too Low."));
        }
        break;

      case 'M': //摇杆使能开关（关掉后手臂只认串口指令，防止演示时误碰摇杆）
        joyOn = !joyOn;
        Serial.print(F("Joystick "));
        Serial.println(joyOn ? F("ON") : F("OFF"));
        break;
      case '?':
        reportStatus();
        break;
      case 'I':
        armIniPos();
        break;

      case 'A':   // 任务二：夹取物体 A 并放置到另一个位置
        Serial.println(F("Received Command:Pick & Place Object A"));
        runSequence(SEQ_A, sizeof(SEQ_A) / sizeof(SEQ_A[0]));
        break;
      case 'B':   // 任务二：夹取物体 B 并放置到另一个位置
        Serial.println(F("Received Command:Pick & Place Object B"));
        runSequence(SEQ_B, sizeof(SEQ_B) / sizeof(SEQ_B[0]));
        break;
      case 'C':   // 任务二：夹取物体 C 并放置到另一个位置
        Serial.println(F("Received Command:Pick & Place Object C"));
        runSequence(SEQ_C, sizeof(SEQ_C) / sizeof(SEQ_C[0]));
        break;

      // ---- 任务三：4 按键遥控板发过来的四个指令 ----
      case 'N':   // 按键1：循环夹取，每一次按下轮换一个物体
        cyclePickPlace();
        break;
      case 'R':   // 按键2：录制，按一下开始，再按一下结束
        toggleRecord();
        break;
      case 'P':   // 按键3：播放上一次录制的动作
        playRecord();
        break;
      case 'Z':   // 按键4：回中（等同于已有的 I 命令）
        Serial.println(F("Received Command:Go Home"));
        armIniPos();
        break;

      default:
        Serial.println(F("Unknown Command."));

    }
  }
}

void reportStatus() {

  Serial.println();
  Serial.println();
  Serial.println(F("+ Robot-Arm Status Report +"));
  Serial.print(F("Claw Position:")); Serial.println(claw.read());
  Serial.print(F("Rear Arm Position:")); Serial.println(rArm.read());
  Serial.print(F("Front Arm Position:")); Serial.println(fArm.read());
  Serial.print(F("Base Position:")); Serial.println(base.read());
  Serial.println(F("++++++++++++++++++"));
  Serial.println(F(" "));

}

// ------------------------------------------------------------
//  joyDelta —— 读一个摇杆轴，返回"这一轮该给角度加多少"
//
//  pin : 模拟口（A0 ~ A3）
//  mid : 这【一根】轴的实测居中值（四根轴各不相同）
//
//  返回值只有三种：+moveStep、-moveStep、0
//  0 是关键 —— 它代表"摇杆没动，别碰这个轴"。
//  正是因为会返回 0，指令设好的位置才不会被覆盖。
// ------------------------------------------------------------
int joyDelta(int pin, int mid) {
  int v = analogRead(pin);
  if(v > mid + JOY_DEADZONE) return  moveStep;   // 往"大值"方向推
  if(v < mid - JOY_DEADZONE) return -moveStep;   // 往"小值"方向推
  return 0;                                      // 在死区内，不动
}

// ------------------------------------------------------------
//  joyCtrl —— 四个轴各自判断该不该动
//  注意是【累加】而不是赋值：只在摇杆确实被推偏时才加减一点。
//  （为什么必须改成累加，见 DEVLOG.md 2026-10-12）
// ------------------------------------------------------------
void joyCtrl() {
  if(!joyOn) return;         // 摇杆被关掉了，直接返回

  basePos += joyDelta(A0, JOY_MID_A0);   basePos = constrain(basePos, baseMin, baseMax);
  rArmPos += joyDelta(A1, JOY_MID_A1);   rArmPos = constrain(rArmPos, rArmMin, rArmMax);
  fArmPos += joyDelta(A2, JOY_MID_A2);   fArmPos = constrain(fArmPos, fArmMin, fArmMax);
  clawPos += joyDelta(A3, JOY_MID_A3);   clawPos = constrain(clawPos, clawMin, clawMax);
}

// ------------------------------------------------------------
//  updateServos —— 把四个"目标角度"推到舵机上
//
//  只有跟舵机当前角度不一样时才写，避免每轮重复发同样的脉冲。
//  摇杆居中时四个值都不变 → 一个字节都不发 → 手臂纹丝不动。
// ------------------------------------------------------------
void updateServos() {
  if(basePos != base.read()) { base.write(basePos); delay(10); }
  if(rArmPos != rArm.read()) { rArm.write(rArmPos); delay(10); }
  if(fArmPos != fArm.read()) { fArm.write(fArmPos); delay(10); }
  if(clawPos != claw.read()) { claw.write(clawPos); delay(10); }
}


void servoCmd(char servoName,int toPos,int servoDelay)
{

  Servo servo2go;

  Serial.println(F(" "));
  Serial.print(F("+Command: Servo "));
  Serial.print(servoName);
  Serial.print(F(" to "));
  Serial.print(toPos);
  Serial.print(F(" at servoDelay value "));
  Serial.print(servoDelay);
  Serial.println(F("."));
  Serial.println(F(" "));

  int fromPos;

  switch(servoName) {
    case 'b':
      if(toPos >= baseMin && toPos <= baseMax) {
        servo2go = base;
        fromPos = base.read();
        break;
      } else {
        Serial.println(F("+Warning: Base Servo Value Out Of Limit!"));
        return;
      }
    case 'r':
      if(toPos >= rArmMin && toPos <= rArmMax) {
        servo2go = rArm;
        fromPos = rArm.read();
        break;
      } else {
        Serial.println(F("+Warning: Rear Servo Value Out Of Limit!"));
        return;
      }
    case 'f':
      if(toPos >= fArmMin && toPos <= fArmMax) {
        servo2go = fArm;
        fromPos = fArm.read();
        break;
      } else {
        Serial.println(F("+Warning: Arm Servo Value Out Of Limit!"));
        return;
      }
    case 'c':
      if(toPos >= clawMin && toPos <= clawMax) {
        servo2go = claw;
        fromPos = claw.read();
        break;
      } else {
        Serial.println(F("+Warning: Claw Servo Value Out Of Limit!"));
        return;
      }
  }

  if(fromPos <= toPos) {
    for(int i = fromPos;i <= toPos;i++) {
      servo2go.write(i);
      delay(servoDelay);
      }
  } else {
    for(int i = fromPos;i >= toPos;i--) {
    servo2go.write(i);
    delay(servoDelay);
    }
  }

  // 动作结束后，把"目标角度"同步到舵机实际停的那个位置。
  // 不做这一步，摇杆下一轮就会从旧角度继续加减，把手臂猛地拽回去。
  switch(servoName) {
    case 'b': basePos = toPos; break;
    case 'r': rArmPos = toPos; break;
    case 'f': fArmPos = toPos; break;
    case 'c': clawPos = toPos; break;
  }
}

void armIniPos() {
  Serial.println(F("+Command:Restore Initial Position."));
  int robotIniPosArray[4][3] = {
    {'b',90,DSD},
    {'r',90,DSD},
    {'f',90,DSD},
    {'c',90,DSD}
  };

  for(int i = 0;i < 4;i++) {
    servoCmd(robotIniPosArray[i][0],robotIniPosArray[i][1],robotIniPosArray[i][2]);
  }
}


// ============================================================
//  任务二用到的两个函数
// ============================================================

// ------------------------------------------------------------
//  moveAll —— 四个关节【同时】走，各自按比例插值
//
//  与 servoCmd 的区别：servoCmd 是一个轴走完再走下一个（一顿一顿）；
//  moveAll 是四个轴一起动、同时到达（一气呵成）。
//
//  原理：先算出四个轴里最长的那段行程 = 总共要走多少步；
//        第 s 步时，每个轴都走到"自己行程的 s / 总量"处。
// ------------------------------------------------------------
void moveAll(int tb, int tr, int tf, int tc) {

  // 限位保护：填错数字超过范围时，先提醒你，再夹回边界
  if (tb < baseMin || tb > baseMax) Serial.println(F("+Warning: 序列里 base 角度超限，已夹到边界"));
  if (tr < rArmMin || tr > rArmMax) Serial.println(F("+Warning: 序列里 rArm 角度超限，已夹到边界"));
  if (tf < fArmMin || tf > fArmMax) Serial.println(F("+Warning: 序列里 fArm 角度超限，已夹到边界"));
  if (tc < clawMin || tc > clawMax) Serial.println(F("+Warning: 序列里 claw 角度超限，已夹到边界"));

  tb = constrain(tb, baseMin, baseMax);
  tr = constrain(tr, rArmMin, rArmMax);
  tf = constrain(tf, fArmMin, fArmMax);
  tc = constrain(tc, clawMin, clawMax);

  // 当前位置（read 读到的就是上一次 write 进去的角度）
  int cb = base.read();
  int cr = rArm.read();
  int cf = fArm.read();
  int cc = claw.read();

  // 四个轴各自的行程（取绝对值）
  int d1 = abs(tb - cb);
  int d2 = abs(tr - cr);
  int d3 = abs(tf - cf);
  int d4 = abs(tc - cc);

  // 最长的那一段决定总步数
  int steps = d1;
  if (d2 > steps) steps = d2;
  if (d3 > steps) steps = d3;
  if (d4 > steps) steps = d4;

  if (steps == 0) return;   // 已经在目标位置了，什么都不用做

  for (int s = 1; s <= steps; s++) {
    base.write(cb + (tb - cb) * s / steps);
    rArm.write(cr + (tr - cr) * s / steps);
    fArm.write(cf + (tf - cf) * s / steps);
    claw.write(cc + (tc - cc) * s / steps);
    delay(DSD);             // 每步延迟，DSD 越小动作越快（H / L 可调）
  }

  // 同步"目标角度"，理由同 servoCmd：
  // 序列跑完后，摇杆要能从终点继续接手，而不是从起点重新算。
  basePos = tb;
  rArmPos = tr;
  fArmPos = tf;
  clawPos = tc;
}


// ------------------------------------------------------------
//  runSequence —— 把一个动作序列从头到尾走完
//
//  seq   : 姿态数组，形状 [动作数][4]
//  steps : 数组里有几个动作
//
//  调用时用 sizeof(SEQ_A) / sizeof(SEQ_A[0]) 自动算动作数，
//  这样以后你往序列里加 / 删动作，都不用再回头改这个数字。
// ------------------------------------------------------------
void runSequence(const byte seq[][4], int steps) {
  for (int i = 0; i < steps; i++) {
    moveAll(seq[i][0], seq[i][1], seq[i][2], seq[i][3]);
  }
}


// ============================================================
//  任务三用的四个函数
// ============================================================

// ------------------------------------------------------------
//  cyclePickPlace —— 按键1（循环执行）
//
//  每按一次只做一个物体的取放，然后"指针"往下走一格：
//      第 1 次按 -> A    第 2 次按 -> B    第 3 次按 -> C
//      第 4 次按 -> A    如此循环
// ------------------------------------------------------------
void cyclePickPlace() {
  Serial.print(F("+Cycle Pick & Place #"));
  Serial.println(cycleIdx + 1);

  if(cycleIdx == 0) {
    runSequence(SEQ_A, sizeof(SEQ_A) / sizeof(SEQ_A[0]));
  } else if(cycleIdx == 1) {
    runSequence(SEQ_B, sizeof(SEQ_B) / sizeof(SEQ_B[0]));
  } else {
    runSequence(SEQ_C, sizeof(SEQ_C) / sizeof(SEQ_C[0]));
  }

  cycleIdx = (cycleIdx + 1) % 3;   // 0 -> 1 -> 2 -> 0
}


// ------------------------------------------------------------
//  toggleRecord —— 按键2（录制）
//
//  同一个函数被调用两次就是"开始"和"结束"：
//      第 1 次调用：recording 从 false 变 true  -> 开始录
//      第 2 次调用：recording 从 true  变 false -> 结束并保存
// ------------------------------------------------------------
void toggleRecord() {
  if(!recording) {
    recording  = true;
    frameCount = 0;                 // 重新录，旧数据丢掉
    recLastMs  = millis();
    joyOn      = true;              // 录制必须用摇杆操控，强制打开摇杆
    Serial.println(F("+REC START (use joystick)"));
  } else {
    recording = false;
    Serial.print(F("+REC END. frames = "));
    Serial.println(frameCount);
    Serial.print(F("+REC seconds = "));
    Serial.println((long)frameCount * FRAME_MS / 1000);
  }
}


// ------------------------------------------------------------
//  recordTick —— 录制用的"心跳"，在 loop() 里每轮被调用一次
//
//  【重点】这个函数是"非阻塞"的：它不 delay，只看 millis() 有没有
//  走过 FRAME_MS。所以录制期间 loop 照常转、摇杆还能实时操控。
//  和 runSequence / playRecord 的对比见 DEVLOG.md 2026-10-13。
// ------------------------------------------------------------
void recordTick() {
  if(!recording) return;                          // 没在录，直接走人

  if(millis() - recLastMs < FRAME_MS) return;     // 还没到采样点
  recLastMs = millis();

  if(frameCount >= MAX_FRAMES) {                  // 缓冲录满了，自动停
    recording = false;
    Serial.println(F("+REC FULL, auto stop"));
    return;
  }

  recFrames[frameCount][0] = basePos;
  recFrames[frameCount][1] = rArmPos;
  recFrames[frameCount][2] = fArmPos;
  recFrames[frameCount][3] = clawPos;
  frameCount++;
}


// ------------------------------------------------------------
//  playRecord —— 按键3（播放）
//
//  把录下来的帧按原顺序走一遍。两帧之间用 SUBSTEPS 次插值写舵机，
//  所以看起来是连续移动，而不是"跳一格、停 100ms、再跳一格"。
//  这和 moveAll 用的是同一个套路：把一大步拆成几个小步。
// ------------------------------------------------------------
void playRecord() {
  if(frameCount == 0) {
    Serial.println(F("+No Record. Press key2 (R) to record first."));
    return;
  }

  Serial.print(F("+PLAY. frames = "));
  Serial.println(frameCount);

  // 从"当前实际位置"开始，而不是从第 0 帧开始 —— 避免播放瞬间跳一下
  int cb = base.read();
  int cr = rArm.read();
  int cf = fArm.read();
  int cc = claw.read();

  for(int i = 0; i < frameCount; i++) {

    int tb = recFrames[i][0];
    int tr = recFrames[i][1];
    int tf = recFrames[i][2];
    int tc = recFrames[i][3];

    for(int s = 1; s <= SUBSTEPS; s++) {
      base.write(cb + (tb - cb) * s / SUBSTEPS);
      rArm.write(cr + (tr - cr) * s / SUBSTEPS);
      fArm.write(cf + (tf - cf) * s / SUBSTEPS);
      claw.write(cc + (tc - cc) * s / SUBSTEPS);
      delay(FRAME_MS / SUBSTEPS);
    }

    cb = tb; cr = tr; cf = tf; cc = tc;
  }

  // 播放结束后同步目标角度 —— 理由和 servoCmd / moveAll 一样：
  // 不回填的话，摇杆下一轮会从旧角度继续加减，把手臂猛地拽回去。
  basePos = cb;
  rArmPos = cr;
  fArmPos = cf;
  clawPos = cc;

  Serial.println(F("+PLAY DONE"));
}


// ============================================================
//  任务三：摇杆按键的手势识别
// ============================================================

// ------------------------------------------------------------
//  scanButtons —— 把"电平抖动"翻译成"单击 / 双击"
//
//  对左右两个按键各做三步：
//    ① 电平跟上一次不一样 -> 记下"从这一刻开始变"，消抖计时重新起
//    ② 稳定超过 25ms     -> 承认它真的变了。按下就 +1 次点击
//    ③ 手松开、且双击窗口（350ms）走完 -> 现在才能定性：
//         累计 1 次 = 单击      累计 2 次 = 双击
//
//  第 ③ 步的"等一等"，就是单击要延迟约 0.4 秒的原因。
// ------------------------------------------------------------
void scanButtons() {
  unsigned long now = millis();

  for(byte i = 0; i < 2; i++) {

    bool raw = digitalRead(BTN_PIN[i]);

    // ① 电平动了，重新起算消抖
    if(raw != btn[i].lastRaw) {
      btn[i].lastRaw = raw;
      btn[i].chgMs   = now;
    }

    // ② 稳定够久，才承认它真变了
    if(now - btn[i].chgMs >= BTN_DEBOUNCE_MS && raw != btn[i].stable) {
      btn[i].stable = raw;
      if(raw == LOW) {
        btn[i].clicks++;         // 按下一次
      } else {
        btn[i].relMs = now;      // 松手：双击窗口从这一刻开始倒计时
      }
    }

    // ③ 手已松开、窗口也走完了 —— 现在才能确定是单击还是双击
    if(btn[i].clicks > 0 && btn[i].stable == HIGH &&
       now - btn[i].relMs >= BTN_DOUBLE_MS) {
      byte n = btn[i].clicks;
      btn[i].clicks = 0;                       // 清掉，准备下一次
      if(n == 1) onButton(i, false);           // 一次点击 -> 单击
      else       onButton(i, true);            // 两次及以上 -> 双击
    }
  }
}


// ------------------------------------------------------------
//  onButton —— 手势识别出来之后，落到具体功能上
//
//  idx      : 0 = 左摇杆，1 = 右摇杆
//  isDouble : true = 双击，false = 单击
//
//  四个功能函数在别处已经写好了（串口 N/R/P/Z 也会调它们），
//  这里只是换了个"触发方式"，功能本身一行都不用改。
// ------------------------------------------------------------
void onButton(byte idx, bool isDouble) {

  // 先把识别到的手势报出来 —— 上机调试时靠这行判断引脚对不对
  Serial.print(F("+KEY "));
  if(idx == 0) Serial.print(F("LEFT "));
  else         Serial.print(F("RIGHT "));
  if(isDouble) Serial.println(F("DOUBLE"));
  else         Serial.println(F("SINGLE"));

  if(idx == 0 && !isDouble) {          // 左单击 = 按键1
    cyclePickPlace();
    resetButtons();
  } else if(idx == 1 && !isDouble) {   // 右单击 = 按键2（非阻塞，不用清）
    toggleRecord();
  } else if(idx == 0 && isDouble) {    // 左双击 = 按键3
    playRecord();
    resetButtons();
  } else {                             // 右双击 = 按键4
    armIniPos();
    resetButtons();
  }
}


// ------------------------------------------------------------
//  resetButtons —— 阻塞动作跑完之后，把按键状态清干净
//
//  上面三个功能（循环夹取 / 播放 / 回中）内部是 delay 死等，
//  跑的时候 scanButtons 一次都没被调用 —— 期间碰了摇杆的话，
//  那一下会积压着，动作结束后被当成一次新点击误触发。所以清一次账。
// ------------------------------------------------------------
void resetButtons() {
  for(byte i = 0; i < 2; i++) {
    btn[i].lastRaw = HIGH;
    btn[i].stable  = HIGH;
    btn[i].clicks  = 0;
    btn[i].chgMs   = millis();
    btn[i].relMs   = millis();
  }
}
