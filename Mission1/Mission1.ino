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


void setup() {

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
    Serial.println("Unknown Command.");
  }
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


