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


void setup() {

  mode = 1;
  base.attach(11);
  delay(200);

  rArm.attach(10);
  delay(200);

  fArm.attach(9);
  delay(200);

  claw.attach(5);
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

