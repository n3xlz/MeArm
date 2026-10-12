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
const int clawMax = 100;

int speedScale = 100;

int DSD = 15; //Default Servo Delay 默认电机运动延迟时间

bool mode = 1; //mode = 1: 指令模式，mode = 0: 摇杆模式

int moveStep = 3; //摇动摇杆时每次改变的角度

const int JOY_A0_MIN = 0, JOY_A0_MAX = 1023;   // 左摇杆 横  -> 底座
const int JOY_A1_MIN = 0, JOY_A1_MAX = 1023;   // 左摇杆 纵  -> 后臂
const int JOY_A2_MIN = 0, JOY_A2_MAX = 1023;   // 右摇杆 纵  -> 前臂
const int JOY_A3_MIN = 0, JOY_A3_MAX = 1023;   // 右摇杆 横  -> 爪子


// ============================================================
//  任务二：A / B / C 自动夹取与放置
// ------------------------------------------------------------
//  一个"姿态" = 四个关节的角度，顺序固定为：{ base, rArm, fArm, claw }
//  一个"动作序列" = 若干个姿态，机械臂按顺序一个一个走过去。
//
//  claw：值大 = 张开（clawMax = 100）；值小 = 夹紧（clawMin = 25）
//
//  ⚠ 下面三个序列里的数字都是【示例值】—— 能让整条动作流程跑起来，
//     但抓不到真实的物体。必须按"标定流程"实测后替换成你自己的值。
// ============================================================

// —— 物体 A（三枚硬币）——
const int SEQ_A[][4] = {
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
const int SEQ_B[][4] = {
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
const int SEQ_C[][4] = {
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


void setup() {

  mode = 1;
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
  
  Serial.begin(9600);  //串口波特率
  Serial.println("Start!");
  if(mode) {
    Serial.println("Now is Command Mode.");
  }

}

void loop() {
  if(Serial.available() > 0) {
    char serialCmd = Serial.read();
    armDataCmd(serialCmd);
  }

  if(!mode) {
    joyCtrl();
    base.write(basePos);
    delay(10);
    rArm.write(rArmPos);
    delay(10);
    fArm.write(fArmPos);
    delay(10);
    claw.write(clawPos);
    delay(10);
  }

}

void armDataCmd(char serialCmd) {
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
        Serial.println("Received Command:Claw Open");
        clawPos = clawMax;
        servoCmd('c',clawPos,DSD);
        break;
      case 'S': //机械臂爪子关闭
        Serial.println("Received Command:Claw Close");
        clawPos = clawMin;
        servoCmd('c',clawPos,DSD);
        break;
      case 'H': //提升机械臂整体运行速度
        Serial.println("Received Command:Speed Higher");
        DSD -= 3;
        if(DSD <= 9) {
          DSD = 9;
          Serial.println("Servo Speed is Too High.");
        } else if(DSD >= 45) {
          DSD = 45;
          Serial.println("Servo Speed is Too Low.");
        }
        break;
      case 'L': //降低机械臂整体运行速度
        Serial.println("Received Command:Speed Lower");
        DSD += 3;
        if(DSD <= 9) {
          DSD = 9;
          Serial.println("Servo Speed is Too High.");
        } else if(DSD >= 45) {
          DSD = 45;
          Serial.println("Servo Speed is Too Low.");
        }
        break;

      case 'M': //指令模式 / 摇杆模式 切换
        mode = !mode;
        if(mode) {
          Serial.println("Now is Command Mode.");
        } else {
          Serial.println("Now is Joystick Mode.");
        }
        break;
      case '?':
        reportStatus();
        break;
      case 'I':
        armIniPos();
        break;

      case 'A':   // 任务二：夹取物体 A 并放置到另一个位置
        Serial.println("Received Command:Pick & Place Object A");
        runSequence(SEQ_A, sizeof(SEQ_A) / sizeof(SEQ_A[0]));
        break;
      case 'B':   // 任务二：夹取物体 B 并放置到另一个位置
        Serial.println("Received Command:Pick & Place Object B");
        runSequence(SEQ_B, sizeof(SEQ_B) / sizeof(SEQ_B[0]));
        break;
      case 'C':   // 任务二：夹取物体 C 并放置到另一个位置
        Serial.println("Received Command:Pick & Place Object C");
        runSequence(SEQ_C, sizeof(SEQ_C) / sizeof(SEQ_C[0]));
        break;

      default:
        Serial.println("Unknown Command.");

    }
  }
}

void reportStatus() {

  Serial.println();
  Serial.println();
  Serial.println("+ Robot-Arm Status Report +");
  Serial.print("Claw Position:"); Serial.println(claw.read());
  Serial.print("Rear Arm Position:"); Serial.println(rArm.read());
  Serial.print("Front Arm Position:"); Serial.println(fArm.read());
  Serial.print("Base Position:"); Serial.println(base.read());
  Serial.println("++++++++++++++++++");
  Serial.println(" ");

}

// ------------------------------------------------------------
//  joyCtrl —— 读四个摇杆轴，映射成四个关节的目标角度
// ------------------------------------------------------------
void joyCtrl() {
  basePos = map(analogRead(A0), JOY_A0_MIN, JOY_A0_MAX, baseMin, baseMax);
  basePos = constrain(basePos, baseMin, baseMax);

  rArmPos = map(analogRead(A1), JOY_A1_MIN, JOY_A1_MAX, rArmMin, rArmMax);
  rArmPos = constrain(rArmPos, rArmMin, rArmMax);

  fArmPos = map(analogRead(A2), JOY_A2_MIN, JOY_A2_MAX, fArmMin, fArmMax);
  fArmPos = constrain(fArmPos, fArmMin, fArmMax);

  clawPos = map(analogRead(A3), JOY_A3_MIN, JOY_A3_MAX, clawMin, clawMax);
  clawPos = constrain(clawPos, clawMin, clawMax);
}


void servoCmd(char servoName,int toPos,int servoDelay)
{

  Servo servo2go;

  Serial.println(" ");
  Serial.print("+Command: Servo ");
  Serial.print(servoName);
  Serial.print(" to ");
  Serial.print(toPos);
  Serial.print(" at servoDelay value ");
  Serial.print(servoDelay);
  Serial.println(".");
  Serial.println(" ");

  int fromPos;

  switch(servoName) {
    case 'b':
      if(toPos >= baseMin && toPos <= baseMax) {
        servo2go = base;
        fromPos = base.read();
        break;
      } else {
        Serial.println("+Warning: Base Servo Value Out Of Limit!");
        return;
      }
    case 'r':
      if(toPos >= rArmMin && toPos <= rArmMax) {
        servo2go = rArm;
        fromPos = rArm.read();
        break;
      } else {
        Serial.println("+Warning: Rear Servo Value Out Of Limit!");
        return;
      }
    case 'f':
      if(toPos >= fArmMin && toPos <= fArmMax) {
        servo2go = fArm;
        fromPos = fArm.read();
        break;
      } else {
        Serial.println("+Warning: Arm Servo Value Out Of Limit!");
        return;
      }
    case 'c':
      if(toPos >= clawMin && toPos <= clawMax) {
        servo2go = claw;
        fromPos = claw.read();
        break;
      } else {
        Serial.println("+Warning: Claw Servo Value Out Of Limit!");
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

}

void armIniPos() {
  Serial.println("+Command:Restore Initial Position.");
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
//  和 servoCmd 的区别：
//    servoCmd 是一个轴走完再走下一个（顺序执行，动作一顿一顿）
//    moveAll  是四个轴一起动、同时到达（动作一气呵成）
//
//  原理：先算出四个轴里最长的那段行程 = 总共要走多少步；
//        第 s 步时，每个轴都走到"自己行程的 s / 总量"处。
//        行程长的轴每步走得多，行程短的轴每步走得少，最后一起到位。
// ------------------------------------------------------------
void moveAll(int tb, int tr, int tf, int tc) {

  // 限位保护：填错数字超过范围时，先提醒你，再夹回边界
  if (tb < baseMin || tb > baseMax) Serial.println("+Warning: 序列里 base 角度超限，已夹到边界");
  if (tr < rArmMin || tr > rArmMax) Serial.println("+Warning: 序列里 rArm 角度超限，已夹到边界");
  if (tf < fArmMin || tf > fArmMax) Serial.println("+Warning: 序列里 fArm 角度超限，已夹到边界");
  if (tc < clawMin || tc > clawMax) Serial.println("+Warning: 序列里 claw 角度超限，已夹到边界");

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
void runSequence(const int seq[][4], int steps) {
  for (int i = 0; i < steps; i++) {
    moveAll(seq[i][0], seq[i][1], seq[i][2], seq[i][3]);
  }
}
