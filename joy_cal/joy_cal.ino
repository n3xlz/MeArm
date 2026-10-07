/*
 * joy_cal.ino —— meArm 摇杆标定工具
 *
 * 这个 sketch 是"仪器"，不是功能代码。它干四件事：
 *   1. 上电后你松手 3 秒，它记录四个轴的"居中值"
 *   2. 之后你把四个方向逐个推到底，它追踪每个轴到过的极值
 *   3. 每 0.3 秒在串口打印一次，让你亲眼看到数字在动
 *   4. 每次打印都当场自检一遍你的数据：接线断了 / 某个方向没推过 / 两侧不对称，
 *      它会直接写在那一行末尾。不用等我人工判断
 *
 * 它和主工程 Mission1.ino 完全独立、互不干扰，可以直接烧录。
 * 不需要舵机，也不需要 Servo 库。
 *
 * 接线（meARM_WDT 扩展板已焊好，插上即用）—— 以下为 2026-10-07 实测结果：
 *   A0 = 左摇杆 左右
 *   A1 = 左摇杆 前后
 *   A2 = 右摇杆 前后     <-- 注意：左右两个摇杆的轴顺序是相反的
 *   A3 = 右摇杆 左右
 */

const int JOY_COUNT = 4;

// 四个轴接在哪些模拟口
const int joyPin[JOY_COUNT] = { A0, A1, A2, A3 };

// 打印时的名字（2026-10-07 实测后更正：A2/A3 的轴顺序与 A0/A1 相反）
const char* joyName[JOY_COUNT] = {
  "A0 左-左右",
  "A1 左-前后",
  "A2 右-前后",
  "A3 右-左右"
};

int joyCenter[JOY_COUNT];   // 居中值（松手时的读数）
int joyMin[JOY_COUNT];      // 到过的最小值
int joyMax[JOY_COUNT];      // 到过的最大值

// ---------------------------------------------------------------
// 重置极值追踪。
// 注意初值：找最小值要从"最大可能的数"开始，找最大值要从 0 开始。
// 否则没有任何读数能比初值更极端，追踪就永远不会启动。
// ---------------------------------------------------------------
void resetTracking() {
  for (int i = 0; i < JOY_COUNT; i++) {
    joyMin[i] = 1023;
    joyMax[i] = 0;
  }
}

void setup() {
  Serial.begin(9600);   // 串口监视器右下角也要选 9600

  Serial.println();
  Serial.println("=========== 摇杆标定工具 ===========");
  Serial.println("步骤 1: 现在把手完全离开摇杆！");
  delay(1000);
  Serial.println("         3 秒后记录居中值...");
  delay(1000);
  Serial.println("         2...");
  delay(1000);
  Serial.println("         1...");

  // 每个轴采 20 次取平均，避开最后那一瞬间的抖动
  for (int i = 0; i < JOY_COUNT; i++) {
    long sum = 0;
    for (int k = 0; k < 20; k++) {
      sum += analogRead(joyPin[i]);
      delay(5);
    }
    joyCenter[i] = sum / 20;
  }

  resetTracking();

  Serial.println(">> 居中值已记录！");
  Serial.println("步骤 2: 把四个方向逐个推到底，每个方向停 1 秒");
  Serial.println("步骤 3: 推完把屏幕上那 4 行数字发我");
  Serial.println("         (推错了就在串口里发 r 回车，重置极值重推)");
  Serial.println();
}

void loop() {
  // 串口发 r 可以重置极值，重新推一轮
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      resetTracking();
      Serial.println(">> 极值已重置，请重新推四个方向");
    }
  }

  // 每一轮 loop 都追踪极值（这里不加延迟，才不会漏掉快速一推）
  for (int i = 0; i < JOY_COUNT; i++) {
    int v = analogRead(joyPin[i]);
    if (v < joyMin[i]) joyMin[i] = v;
    if (v > joyMax[i]) joyMax[i] = v;
  }

  // 每 300ms 打印一次。太快会刷屏看不清，太慢又看不到极值变化
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 300) {
    lastPrint = millis();

    for (int i = 0; i < JOY_COUNT; i++) {
      int v = analogRead(joyPin[i]);

      Serial.print(joyName[i]);
      Serial.print("   现在=");
      Serial.print(v);
      Serial.print("  居中=");
      Serial.print(joyCenter[i]);
      Serial.print("  最小=");
      Serial.print(joyMin[i]);
      Serial.print("  最大=");
      Serial.print(joyMax[i]);

      // 自检：把你手上的数据当场判一次合格
      int down = joyCenter[i] - joyMin[i];   // 往"小值"方向一共走了多远
      int up   = joyMax[i] - joyCenter[i];   // 往"大值"方向一共走了多远

      if (joyMax[i] - joyMin[i] < 20) {
        // 从头到尾几乎没变过 -> 这根线大概率没通
        Serial.print("   <-- 完全不动? 检查接线");
      } else if (down < 15) {
        // 有最大值却没最小值 -> 那个方向你根本没推
        Serial.print("   <-- 只推了一个方向! 还差「小值」那侧");
      } else if (up < 15) {
        Serial.print("   <-- 只推了一个方向! 还差「大值」那侧");
      } else if (min(down, up) < max(down, up) / 2) {
        // 两侧行程差一倍以上 -> 有一侧没推到底
        Serial.print("   <-- 两侧不对称, 有一侧没推到底?");
      } else {
        Serial.print("   <-- 行程合格");
      }
      Serial.println();
    }
    Serial.println("-----------------------------------------");
  }
}
