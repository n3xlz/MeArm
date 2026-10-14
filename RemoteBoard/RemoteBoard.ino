/* ============================================================
 *  RemoteBoard —— 按键遥控板侧代码（烧到电路组自制板上）
 * ------------------------------------------------------------
 *  硬件已按 2026-10-14 收到的原理图确认：
 *
 *    主控     : ATmega328-AU（与 Arduino UNO 同一颗，5V 逻辑）
 *    4 个按键  : SW2 -> A0    SW3 -> A1    SW4 -> A2    SW5 -> A3
 *    外部上拉  : 每键 10kΩ 上拉到 3V3
 *    消抖电容  : 每键 100nF 到 GND（硬件消抖）
 *    按键类型  : KH-6X6X5H-STM 轻触开关，按下接 GND
 *               → 电平：未按下 = HIGH，按下 = LOW
 *    串口      : H2 排针的 TXD / RXD（= ATmega 的 D1/D0），9600
 *
 *  —— 串口协议（与机械臂 MeArm 代码一致）——
 *    'N'  按键1 SW2/A0   循环夹取（A -> B -> C -> A）
 *    'R'  按键2 SW3/A1   录制（按一下开始，再按一下结束并保存）
 *    'P'  按键3 SW4/A2   播放上一次录制
 *    'Z'  按键4 SW5/A3   回中（四轴回 90）
 *
 *  —— 接线（关键）——
 *    本板 TXD  ->  机械臂 UNO 的 RX(D0)
 *    本板 GND  ->  机械臂 UNO 的 GND        （必须共地）
 *    本板 RXD  ->  不接（协议是单向的：只发不收）
 *
 *  ⚠ 烧录本板前，先拔掉 H2 上的杜邦线。TXD/RXD 同时也是下载口，
 *    连着机械臂会让下载失败（和 UNO 自己的 D0/D1 一个道理）。
 *
 *  —— 怎么烧 ——
 *    板上是 USB Type-C + CH340G，按 Arduino UNO 的方式：
 *    板型选「Arduino Uno」、端口选 Type-C 对应的串口，直接上传。
 *    若板子还没烧 bootloader，先用 U5(ICSP) 接口 + USBasp 烧 bootloader。
 * ============================================================ */

// 4 个按键的引脚（已按原理图确认：SW2/3/4/5 -> A0/A1/A2/A3）
const byte BTN_PIN[4] = { A0, A1, A2, A3 };

// 按键 -> 串口字符（顺序与 BTN_PIN 一一对应，一般不用改）
const char BTN_CHAR[4] = { 'N', 'R', 'P', 'Z' };

// 消抖：电平要稳定这么久才算数。
// 板上每个按键已经并了 100nF 做硬件消抖，这里的软件消抖可以放宽。
const unsigned long DEBOUNCE_MS = 30;

bool          lastRaw[4];    // 上一次读到的原始电平
bool          stable[4];     // 消抖后确认的电平
unsigned long chgMs[4];      // 电平开始变化的时刻

void setup() {
  Serial.begin(9600);            // 波特率必须和机械臂侧一致

  for (byte i = 0; i < 4; i++) {
    pinMode(BTN_PIN[i], INPUT);  // 外部已有 10k 上拉，不用开内部上拉
    lastRaw[i] = HIGH;
    stable[i]  = HIGH;
    chgMs[i]   = 0;
  }
}

void loop() {
  unsigned long now = millis();

  for (byte i = 0; i < 4; i++) {

    bool raw = digitalRead(BTN_PIN[i]);

    // ① 电平动了，重新起算消抖计时
    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      chgMs[i]   = now;
    }

    // ② 稳定够久，才承认它真的变了；只认「按下」这一个沿
    if (now - chgMs[i] >= DEBOUNCE_MS && raw != stable[i]) {
      stable[i] = raw;
      if (raw == LOW) {               // 按下（HIGH -> LOW）
        Serial.write(BTN_CHAR[i]);    // 从 TXD 发出去
      }
    }
  }
}
