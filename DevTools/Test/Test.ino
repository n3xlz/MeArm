#include <Servo.h>
Servo base, fArm, rArm, claw;
/*
base 底座;fore arm 前臂;
r arm后臂;claw 爪子;
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

void setup() {

  base.attach(11);
  delay(200);
  rArm.attach(10);
  delay(200);
  fArm.attach(9);
  delay(200);
  claw.attach(6);
  delay(200);
  Serial.begin(9600);  //串口波特率
  Serial.println("Start!");
}

void loop() {
  if(Serial.available() > 0) {
    char serialCmd = Serial.read();
    armDataCmd(serialCmd);
  }

  base.write(basePos);
  delay(10);
  rArm.write(rArmPos);
  delay(10);
  fArm.write(fArmPos);
  delay(10);
  claw.write(clawPos);
  delay(10);
}

void armDataCmd(char serialCmd) {
  int servoData;
  if(serialCmd == 'b' || serialCmd == 'r' || serialCmd == 'f' || serialCmd == 'c') {
    servoData = Serial.parseInt();
    servoCmd(serialCmd,servoData,DSD);
  } else {
    switch(serialCmd) {
      case 'o':
        reportStatus();
        break;
      case 'i':
        armIniPos();
        break;
      default:
        Serial.println("Unknown Command.");
    }
  }

  Serial.print("serialCmd =");
  Serial.print(serialCmd);

  int fromPos;
  int toPos;
  
  switch(serialCmd) {
    case 'b':
      fromPos = base.read();
      toPos = servoData;
      if(fromPos <= toPos) {
        for(int i = fromPos;i <= toPos;i++) {
          base.write(i);
          delay(DSD);
        }
      } else {
          for(int i = fromPos;i >= toPos;i--) {
            base.write(i);
            delay(DSD);
        }
      }
      basePos = servoData;
      Serial.print("Set base servo value: ");
      Serial.print(servoData);
      break;
    case 'r':
      fromPos = rArm.read();
      toPos = servoData;
      if(fromPos <= toPos) {
        for(int i = fromPos;i <= toPos;i++) {
          rArm.write(i);
          delay(DSD);
        }
      } else {
          for(int i = fromPos;i >= toPos;i--) {
            rArm.write(i);
            delay(DSD);
        }
      }
      rArmPos = servoData;
      Serial.print("Set rArm servo value: ");
      Serial.print(servoData);
      break;
    case 'f':
      fromPos = fArm.read();
      toPos = servoData;
      if(fromPos <= toPos) {
        for(int i = fromPos;i <= toPos;i++) {
          fArm.write(i);
          delay(DSD);
        }
      } else {
          for(int i = fromPos;i >= toPos;i--) {
            fArm.write(i);
            delay(DSD);
        }
      }
      fArmPos = servoData;
      Serial.print("Set fArm servo value: ");
      Serial.print(servoData);
      break;
    case 'c':
      fromPos = claw.read();
      toPos = servoData;
      if(fromPos <= toPos) {
        for(int i = fromPos;i <= toPos;i++) {
          claw.write(i);
          delay(DSD);
        }
      } else {
          for(int i = fromPos;i >= toPos;i--) {
            claw.write(i);
            delay(DSD);
        }
      }
      clawPos = servoData;
      Serial.print("Set claw servo value: ");
      Serial.print(servoData);
      break;
  }
}

void reportStatus() {
  Serial.println("");
  Serial.println("");
  Serial.println("+ Robot-Arm Status Report +");
  Serial.print("Claw Position:"); Serial.println(claw.read());
  Serial.print("Rear Arm Position:"); Serial.println(rArm.read());
  Serial.print("Front Arm Position:"); Serial.println(fArm.read());
  Serial.print("Base Position:"); Serial.println(base.read());
  Serial.println("++++++++++++++++++");
  Serial.println(" ");
}

void servoCmd(char servoName,int toPos,int servoDelay) {

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
