// Servo.h —— PC 端测试桩：只记录"上次写进去的角度"，read() 把它读回来。
// 这与 Arduino Servo 库的可观察行为一致（read() 返回最近一次 write() 的值）。
#pragma once
#include "Arduino.h"

class Servo {
  int _pin = -1;
  int _val = 90;
public:
  void attach(int p) { _pin = p; }
  void write(int v)  { _val = v; }
  int  read()        { return _val; }
  int  pin() const   { return _pin; }
};
