// ------------------------------------------------------------
//  Arduino.h —— PC 端测试桩（只为把 Task1235.ino 原样编译到电脑上跑）
//  仅实现 .ino 实际用到的 API，行为尽量贴近 Arduino 真实语义。
// ------------------------------------------------------------
#pragma once
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>

typedef uint8_t  byte;
typedef bool     boolean;

#define HIGH 0x1
#define LOW  0x0
#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

#define A0 14
#define A1 15
#define A2 16
#define A3 17

#define F(x) (x)

// —— 假时间：可被测试代码直接推进，用于验证非阻塞状态机 ——
extern unsigned long g_millis;
inline unsigned long millis() { return g_millis; }
inline void delay(unsigned long ms) { g_millis += ms; }

inline double degrees(double r) { return r * 180.0 / M_PI; }
inline double radians(double d) { return d * M_PI / 180.0; }

inline int  analogRead(int)  { return 512; }   // 恒定中位偏上，摇杆不动
extern int  g_pinLevel[32];                    // 可控引脚电平：测试里用它模拟按键按下
inline int  digitalRead(int p) { return (p >= 0 && p < 32) ? g_pinLevel[p] : HIGH; }
inline void pinMode(int, int) {}

template <class T> inline T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
template <class T> inline T min(T a, T b) { return a < b ? a : b; }
template <class T> inline T max(T a, T b) { return a > b ? a : b; }

// —— 串口桩：输出同时进内存缓冲（供断言）和 stderr（由 echo 控制）——
struct SerialStub {
  bool echo = false;
  std::string buf;

  void begin(long) {}
  int  available() { return 0; }
  int  read() { return -1; }
  long parseInt() { return 0; }

  void put(const std::string& s) { buf += s; if (echo) fputs(s.c_str(), stderr); }

  void print(const char* s) { put(s); }
  void print(const std::string& s) { put(s); }
  void print(int v)   { put(std::to_string(v)); }
  void print(long v)  { put(std::to_string(v)); }
  void print(unsigned int v)  { put(std::to_string(v)); }
  void print(unsigned long v) { put(std::to_string(v)); }
  void print(double v){ char t[64]; snprintf(t,sizeof t,"%g",v); put(t); }
  void print(float v) { print((double)v); }

  void println() { put("\n"); }
  template <class T> void println(T v) { print(v); println(); }

  void clear() { buf.clear(); }
  bool has(const char* needle) const { return buf.find(needle) != std::string::npos; }
};
extern SerialStub Serial;
