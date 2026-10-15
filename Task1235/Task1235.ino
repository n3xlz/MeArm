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
const int clawMax = 120;

// Default Servo Delay —— 舵机每走 1 度停多少毫秒。值越小越快。
//   H 命令减 DSD_STEP、L 命令加 DSD_STEP，范围 DSD_MIN ~ DSD_MAX。
//   取值范围刻意拉得很宽（12 倍差），演示「速度可调」时快慢对比一眼可见。
int DSD = 15;
const int DSD_MIN  = 5;    // 最快：1 度 5 ms
const int DSD_MAX  = 60;   // 最慢：1 度 60 ms
const int DSD_STEP = 5;    // H / L 每次调整的步长

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
//  ✅ SEQ_A / SEQ_B / SEQ_C 的数值全部为 2026-10-15 实测记录，各 9 位，末位都是回中。
//
// ============================================================

// —— 物体 A（三枚硬币）—— 2026-10-15 实测记录
const byte SEQ_A[][4] = {
  {  90, 110, 108,  54 },   // 1 取物位上方（爪张开）
  {  90, 110,  85,  54 },   // 2 下降到取物位
  {  90, 110,  85,  115 },   // 3 合爪夹紧
  {  90, 110, 105,  115 },   // 4 抬起（保持夹紧）
  { 147, 110, 105,  115 },   // 5 转到放置位上方
  { 147, 110,  40,  115 },   // 6 下降到放置位
  { 147, 110,  40,  25 },   // 7 张爪松开
  { 147, 110, 109,  25 },   // 8 抬起
  {  90,  90,  90,  90 }    // 9 回中
};

// —— 物体 B（笔）—— 2026-10-15 实测记录
const byte SEQ_B[][4] = {
  { 126,  80,  90,  39 },   // 1 取物位（爪张开）
  { 126, 104,  90,  39 },   // 2 调整到取物姿态
  { 126, 104,  90, 110 },   // 3 合爪夹紧
  { 165, 104,  90, 110 },   // 4 抬起并转向放置位
  { 174, 104,  90, 110 },   // 5 到达放置位上方
  { 174, 104,  46,  86 },   // 6 下降到放置位并松开
  { 174, 104,  46,  86 },   // 7 保持（原始记录与 #6 相同）
  { 141,  21, 120,  90 },   // 8 回正前的过渡位
  {  90,  90,  90,  90 }    // 9 回正中位
};

// —— 物体 A（橡皮）—— 2026-10-15 实测记录
const byte SEQ_C[][4] = {
  {  90,  90, 102,  25 },   // 1 取物位上方
  {  90,  99,  84,  25 },   // 2 下降到取物位
  {  90,  99,  84, 110 },   // 3 合爪夹紧
  {  90,  99, 120, 110 },   // 4 抬起
  {  18,  99, 112, 110 },   // 5 转到放置位上方
  {  18,  99,  55, 110 },   // 6 下降到放置位
  {  18,  99,  55,  41 },   // 7 张爪松开
  {  18,  99, 118,  41 },   // 8 抬起
  {  90,  90,  90,  90 }    // 9 回中
};


// ============================================================
//  任务三：4 个按键（机械臂侧）
// ------------------------------------------------------------
//  4 个按键全部由【两个摇杆自带的按压开关】产生：
//  把摇杆帽垂直往下按，"嗒"一下就是一个按键；左右各一个，
//  再用"单击 / 双击"组合出 4 种手势，正好对应任务要的 4 个按键。
//
//  手势 → 功能的对应关系、以及为什么用单击/双击而不是别的组合，
//  见下面「按键从哪来」那一块和 DEVLOG.md 2026-10-13。
//
//  四个手势最终都落到同一批功能函数上，功能本身与触发方式无关。
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


// ============================================================
//  任务五：轨迹绘制
// ------------------------------------------------------------
//  给一个末端坐标 (x,y,z)，算出三个关节角再写进舵机。这套
//  "坐标 -> 角度"的换算叫【逆解】，推导见 任务四-运动学推导.docx。
//
//  状态机（非阻塞，millis 看时间，跟 recordTick 一个套路）：
//    DRAW_IDLE  没在画
//    DRAW_RUN   正在画，每 DRAW_STEP_MS 推进一步
//    DRAW_PAUSE 暂停（保留 wayI / curX 等进度，发 '4' 接着画）
//    DRAW_DONE  画完一段，等下一条指令
//
//  发 '5' 任何时候都能取消：抬笔、清航点、回 IDLE。
//
//  连杆参数与任务四一致（单位 mm）：
//    D=15  H=64  L1=80  Lf=133 （Lf = L2+L3）
//  可达判据：53 ≤ ρ ≤ 213 mm
// ============================================================

// —— 连杆参数（标称值，单位 mm）——
const float ARM_D  = 15.0f;
const float ARM_H  = 64.0f;
const float ARM_L1 = 80.0f;
const float ARM_LF = 133.0f;

// —— 笔尖高度（单位 mm，待标定）——
//  penZDown：笔尖接触纸面的高度
//  penZUp  ：抬笔高度（每段画完抬一下，避免拖痕）
//  数值要等笔装好实测后替换。先放占位值让代码能编译。
float penZDown = 30.0f;
float penZUp   = 70.0f;

// —— 关节角↔舵机角度的线性映射：a = a0 + σ·θ ——
//  A0_*   是"关节角 θ=0 时对应的舵机角度"
//  SIGMA_* 是安装方向（+1 或 -1），由舵机正反装决定
//  这 6 个值要等机械臂装好后用两点标定法测出来替换，
//  方法见 任务四-运动学推导.docx 附录 A。
const float A0_BASE = 90.0f;  const float SIGMA_BASE =  1.0f;
const float A0_RARM = 90.0f;  const float SIGMA_RARM =  1.0f;
const float A0_FARM = 90.0f;  const float SIGMA_FARM =  1.0f;

// —— 轨迹步长 / 步间隔 ——
//  DRAW_STEP_MM：每步末端移动多少 mm，越小越平滑、越慢
//  DRAW_STEP_MS：每步间隔多少 ms，越小越快
const float         DRAW_STEP_MM = 2.0f;
const unsigned long DRAW_STEP_MS = 30;

// —— 状态机 ——
enum DrawState {
  DRAW_IDLE,
  DRAW_RUN,
  DRAW_PAUSE,
  DRAW_DONE
};
DrawState drawState = DRAW_IDLE;

// —— 航点表 ——
//  一条轨迹 = 一串航点 (x,y,z)。drawTick 把当前末端从"上一个航点"
//  线性插值推到"下一个航点"，到了就 wayI++。
#define MAX_WAY 16
struct WayPt { float x; float y; float z; };
WayPt way[MAX_WAY];
int   wayN = 0;        // 航点总数
int   wayI = 0;        // 当前正前往的航点索引（从 0 开始；走完一段 +1）

// 当前末端坐标（绘制过程中实时更新，暂停/继续/取消都靠它接得上）
float curX = 0, curY = 0, curZ = 0;

// —— 五点示教（任务五）——
//  先摆 5 个位置、每摆一个发一次 'T' 记下来；再发 'F' 画折线 / 'W' 画平滑曲线。
#define TEACH_N 5
byte  teachPts[TEACH_N][4];     // 记下的 5 个姿态（四轴舵机角度）
float teachXYZ[TEACH_N][3];     // 执行时把上面 5 个姿态正解成末端坐标
int   teachCount = 0;

// —— 平滑曲线模式 ——
//  drawCurve = true 时 drawTick 不走航点表，而是按参数 t∈[0,1] 实时算点。
bool  drawCurve = false;
float drawT     = 0.0f;         // 当前参数 0..1
float drawDT    = 0.0f;         // 每步增量


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
  Serial.println(F("Task5: 0=pos 1=line 2=N 6=tri 7=Z 8=V"));
  Serial.println(F("       3=pause 4=go 5=cancel"));
  Serial.println(F("       T=teach Y=clear F=polyline W=smooth"));
  Serial.println(F("Coord: x<n>,y<n>,z<n>  (x=base y=rArm z=fArm)"));
  Serial.println(F("Speed: H=faster  L=slower  (prints DSD=..)"));

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

  // ③ 任务五：轨迹绘制心跳。
  //    DRAW_RUN 时每轮推进一段；DRAW_RUN / DRAW_PAUSE 期间禁用摇杆，
  //    避免摇杆抢位置把线画歪。为什么禁摇杆见 DEVLOG.md 2026-10-13。
  if(drawState == DRAW_RUN) drawTick();
  if(drawState == DRAW_IDLE || drawState == DRAW_DONE) joyCtrl();

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
  } else if(serialCmd == 'x' || serialCmd == 'X') {
    coordCmd();
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
      case 'H': //提升机械臂整体运行速度（DSD 是"每度延迟"，越小越快）
        if(DSD > DSD_MIN) DSD -= DSD_STEP;
        if(DSD < DSD_MIN) DSD = DSD_MIN;
        Serial.print(F("Received Command:Speed Higher. DSD="));
        Serial.println(DSD);
        if(DSD == DSD_MIN) Serial.println(F("+Already at the fastest."));
        break;
      case 'L': //降低机械臂整体运行速度
        if(DSD < DSD_MAX) DSD += DSD_STEP;
        if(DSD > DSD_MAX) DSD = DSD_MAX;
        Serial.print(F("Received Command:Speed Lower. DSD="));
        Serial.println(DSD);
        if(DSD == DSD_MAX) Serial.println(F("+Already at the slowest."));
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

      // ---- 任务五：轨迹绘制 ----
      //  '0' 打印当前末端坐标（正解自检，标定时靠它对方向）
      //  '1' 画预设直线：在 y=0 平面上画 x=80 -> 170 的直线
      //  '2' 画 N    '6' 画 △    '7' 画 Z    '8' 画 V
      //  '3' 暂停    '4' 继续     '5' 取消
      //  'T' 记一个示教点（共 5 个）  'Y' 清空示教点
      //  'F' 用 5 点画折线           'W' 用 5 点画平滑曲线
      case '0':
        printForwardKinematics();
        break;
      case '1':
        Serial.println(F("Received Command:Draw Line"));
        // 起点取 x=80：再往里 ρ 会掉到最小半径 53 以下，逆解必失败。
        drawLine(80.0f, 0.0f, 170.0f, 0.0f);
        break;
      case '2': drawShape('N'); break;
      case '6': drawShape('T'); break;
      case '7': drawShape('Z'); break;
      case '8': drawShape('V'); break;
      case 'T': teachRecord();   break;
      case 'Y': teachClear();    break;
      case 'F': runTeach(false); break;
      case 'W': runTeach(true);  break;
      case '3':
        drawPause();
        break;
      case '4':
        drawResume();
        break;
      case '5':
        drawCancel();
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
  if (tb < baseMin || tb > baseMax) Serial.println(F("+Warning: base 目标角度超限，已夹到边界"));
  if (tr < rArmMin || tr > rArmMax) Serial.println(F("+Warning: rArm 目标角度超限，已夹到边界"));
  if (tf < fArmMin || tf > fArmMax) Serial.println(F("+Warning: fArm 目标角度超限，已夹到边界"));
  if (tc < clawMin || tc > clawMax) Serial.println(F("+Warning: claw 目标角度超限，已夹到边界"));

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
//  coordCmd —— 任务一（附加）：多舵机协同控制
//
//  上位机发送：  x<角度>,y<角度>,z<角度>        例：x10,y30,z20
//
//  x / y / z 与舵机的对应关系（自行定义）：
//      x -> base （底座回转）
//      y -> rArm （后臂）
//      z -> fArm （前臂）
//  夹爪不参与，保持当前位置不动。
//
//  ⚠ 三个轴是【同时】出发、同时到达的（底层走 moveAll 的比例插值），
//     不是三个 servoCmd 串起来一个个走 —— 任务书要求「单条指令同步控制三个舵机」。
//
//  解析为什么不用自己切逗号：
//     Serial.parseInt() 会【自动跳过所有非数字字符】再读一个整数。
//     所以 "x10,y30,z20" 里被读走 'x' 之后，
//     连续三次 parseInt 依次拿到 10 / 30 / 20，中间的 ',' 'y' ',' 'z' 全被自动跳掉。
// ------------------------------------------------------------
void coordCmd() {
  int vx = Serial.parseInt();
  int vy = Serial.parseInt();
  int vz = Serial.parseInt();

  // 吃掉行尾残留的 '\r' '\n'，否则会被当成下一条命令
  char clear;
  while(Serial.available() > 0) clear = Serial.read();

  Serial.print(F("+Coord  x(base)="));  Serial.print(vx);
  Serial.print(F("  y(rArm)="));       Serial.print(vy);
  Serial.print(F("  z(fArm)="));       Serial.println(vz);

  // 单条指令，同时驱动三个舵机
  moveAll(vx, vy, vz, clawPos);

  // 回读四个舵机【实际】停在的角度，方便上位机核对
  Serial.print(F("+Servo  x/base="));  Serial.print(base.read());
  Serial.print(F("  y/rArm="));        Serial.print(rArm.read());
  Serial.print(F("  z/fArm="));        Serial.print(fArm.read());
  Serial.print(F("  claw="));          Serial.println(claw.read());
  Serial.print(F("+DSD="));            Serial.println(DSD);
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
//  四个功能函数在别处已经写好了，
//  这里只是提供一个"触发方式"，功能本身一行都不用改。
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


// ============================================================
//  任务五：轨迹绘制
// ============================================================

// ------------------------------------------------------------
//  th2servo* / servo2th* —— 关节角 θ（弧度）↔ 舵机角度（度）
//
//  线性映射：a = a0 + σ·θ
//    th2servo   : 已知 θ，算舵机角度
//    servo2th   : 已知舵机角度，反算 θ
//  θ 在公式里用弧度，但映射时先转成度（舵机角度是度）。
// ------------------------------------------------------------
float th2servoBase(float th) { return A0_BASE + SIGMA_BASE * degrees(th); }
float th2servoRarm(float th) { return A0_RARM + SIGMA_RARM * degrees(th); }
float th2servoFarm(float th) { return A0_FARM + SIGMA_FARM * degrees(th); }

float servo2thBase(float a) { return radians((a - A0_BASE) / SIGMA_BASE); }
float servo2thRarm(float a) { return radians((a - A0_RARM) / SIGMA_RARM); }
float servo2thFarm(float a) { return radians((a - A0_FARM) / SIGMA_FARM); }


// ------------------------------------------------------------
//  forwardKinematics —— 正解：三个舵机角度 -> 末端 (x,y,z)
//
//  几何投影法（推导见任务四文档 3.2 节）：
//    θ1 = servo2thBase(base)    底座回转
//    θ2 = servo2thRarm(rArm)    肩
//    θ3 = servo2thFarm(fArm)    肘
//    R  = L1·cosθ2 + Lf·cos(θ2+θ3)    伸出半径
//    x  = (D + R)·cosθ1
//    y  = (D + R)·sinθ1
//    z  = H + L1·sinθ2 + Lf·sin(θ2+θ3)
//
//  注意 θ2/θ3 的正方向约定：θ2>0 抬起，θ3>0 肘往上弯。
//  标定后如果方向反了，改 SIGMA_* 的正负号即可，公式不用动。
// ------------------------------------------------------------
void forwardKinematics(float &ox, float &oy, float &oz) {
  float t1 = servo2thBase(base.read());
  float t2 = servo2thRarm(rArm.read());
  float t3 = servo2thFarm(fArm.read());

  float R = ARM_L1 * cos(t2) + ARM_LF * cos(t2 + t3);
  ox = (ARM_D + R) * cos(t1);
  oy = (ARM_D + R) * sin(t1);
  oz = ARM_H + ARM_L1 * sin(t2) + ARM_LF * sin(t2 + t3);
}


// ------------------------------------------------------------
//  printForwardKinematics —— '0' 命令：打印当前末端坐标
//  上机调试时靠它验证正解/逆解/标定有没有错。
// ------------------------------------------------------------
void printForwardKinematics() {
  float x, y, z;
  forwardKinematics(x, y, z);
  Serial.println(F("+Forward Kinematics"));
  Serial.print(F("  base=")); Serial.print(base.read());
  Serial.print(F("  rArm=")); Serial.print(rArm.read());
  Serial.print(F("  fArm=")); Serial.println(fArm.read());
  Serial.print(F("  end = ("));
  Serial.print(x); Serial.print(F(", "));
  Serial.print(y); Serial.print(F(", "));
  Serial.print(z); Serial.println(F(") mm"));
}


// ------------------------------------------------------------
//  inverseKinematics —— 逆解：末端 (x,y,z) -> 三个关节角
//
//  三步解析法（推导见任务四文档 4 节）：
//    ① θ1 = atan2(y, x)                       底座回转，解耦
//    ② r = √(x²+y²) - D,  z' = z - H,  ρ = √(r² + z'²)
//    ③ 余弦定理：θ3 = arccos((ρ² - L1² - Lf²) / (2·L1·Lf))
//       θ2 = atan2(z', r) - atan2(Lf·sinθ3, L1 + Lf·cosθ3)
//
//  返回 false = 该点不可达（ρ 出了 [|L1-Lf|, L1+Lf] = [53, 213] 范围）。
//  肘上构型选 θ3 < 0（任务四 4.4 节）。c3 夹到 [-1,1] 防数值误差。
// ------------------------------------------------------------
bool inverseKinematics(float x, float y, float z,
                       float &ot1, float &ot2, float &ot3) {
  float t1 = atan2(y, x);

  float r  = sqrt(x*x + y*y) - ARM_D;
  float zp = z - ARM_H;
  float rho = sqrt(r*r + zp*zp);

  // 可达判据：|L1 - Lf| ≤ ρ ≤ L1 + Lf
  // ⚠ 下限必须取 fabs —— Lf(133) > L1(80) 时 L1-Lf 是负数(-53)，
  //    不取绝对值的话下限判据恒不成立、等于失效。
  if(rho < fabs(ARM_L1 - ARM_LF) || rho > ARM_L1 + ARM_LF) return false;

  float c3 = (rho*rho - ARM_L1*ARM_L1 - ARM_LF*ARM_LF) /
             (2.0f * ARM_L1 * ARM_LF);
  if(c3 >  1.0f) c3 =  1.0f;
  if(c3 < -1.0f) c3 = -1.0f;
  float t3 = -acos(c3);    // 取负 = 肘上构型

  float t2 = atan2(zp, r) - atan2(ARM_LF * sin(t3),
                                  ARM_L1 + ARM_LF * cos(t3));

  ot1 = t1; ot2 = t2; ot3 = t3;
  return true;
}


// ------------------------------------------------------------
//  moveToSync —— 把末端走到 (x,y,z)，并等舵机走完
//
//  调逆解算出三个关节角，映射到舵机角度，用 moveAll 让三轴
//  同时插值走完（阻塞）。drawLine 的"起笔抬笔"那两步用它：
//  这两步是瞬间动作，不进状态机、不需要被暂停。
// ------------------------------------------------------------
bool moveToSync(float x, float y, float z) {
  float t1, t2, t3;
  if(!inverseKinematics(x, y, z, t1, t2, t3)) {
    Serial.print(F("+IK FAIL at ("));
    Serial.print(x); Serial.print(F(","));
    Serial.print(y); Serial.print(F(","));
    Serial.print(z); Serial.println(F(")"));
    return false;
  }
  int ab = (int)th2servoBase(t1);
  int ar = (int)th2servoRarm(t2);
  int af = (int)th2servoFarm(t3);
  moveAll(ab, ar, af, clawPos);    // 复用任务二的 moveAll：四轴同时插值
  curX = x; curY = y; curZ = z;
  return true;
}


// ------------------------------------------------------------
//  moveTo —— 只设 pos，不等舵机走完
//
//  和 moveToSync 的区别：这里不调 moveAll、不 delay，只把目标角度
//  写进 basePos/rArmPos/fArmPos。然后由 loop 里的 updateServos 慢慢
//  把舵机推过去。drawTick 的每一步用它 —— 这才是真正的非阻塞画线。
//  为什么分两个版本、moveAll 为什么不能直接用见 DEVLOG.md 2026-10-13。
// ------------------------------------------------------------
bool moveTo(float x, float y, float z) {
  float t1, t2, t3;
  if(!inverseKinematics(x, y, z, t1, t2, t3)) {
    Serial.print(F("+IK FAIL at ("));
    Serial.print(x); Serial.print(F(","));
    Serial.print(y); Serial.print(F(","));
    Serial.print(z); Serial.println(F(")"));
    return false;
  }
  int ab = (int)th2servoBase(t1);
  int ar = (int)th2servoRarm(t2);
  int af = (int)th2servoFarm(t3);

  // —— 必须钳位 + 报警 ——
  // 目标点【几何上可达】（ρ 落在 53~213 内）不等于【舵机摆得到】：
  // 算出来的关节角一旦超出舵机行程，绝大多数情况是那 6 个标定常量还没填。
  // 不钳位的话，负角度会被 Servo::write() 当成 0°，舵机直接顶死机械限位、堵转发烫。
  // 报警做 2 秒限流 —— 画线时每 30 ms 就会调一次这里，不限流会刷屏。
  if(ab < baseMin || ab > baseMax ||
     ar < rArmMin || ar > rArmMax ||
     af < fArmMin || af > fArmMax) {
    static unsigned long lastWarn = 0;
    if(millis() - lastWarn > 2000) {
      lastWarn = millis();
      Serial.print(F("+Angle out of range! base="));  Serial.print(ab);
      Serial.print(F(" rArm="));                      Serial.print(ar);
      Serial.print(F(" fArm="));                      Serial.println(af);
      Serial.println(F("+ 多半是任务五标定还没做 -> 任务五操作手册.md 第 1 节"));
    }
    ab = constrain(ab, baseMin, baseMax);
    ar = constrain(ar, rArmMin, rArmMax);
    af = constrain(af, fArmMin, fArmMax);
  }

  basePos = ab; rArmPos = ar; fArmPos = af;
  curX = x; curY = y; curZ = z;
  return true;
}


// ------------------------------------------------------------
//  drawLine —— '1' 命令：在纸面 z=penZDown 上画一条直线
//
//  动作流程：
//    1) moveToSync 抬笔到起点上方 (x1, y1, penZUp)    —— 阻塞，瞬间
//    2) moveToSync 下笔到起点     (x1, y1, penZDown)  —— 阻塞，瞬间
//    3) 装载画线段：终点 (x2, y2, penZDown)，状态转 DRAW_RUN
//    4) drawTick 在 loop 里非阻塞地一步步把线画过去
//    5) 画到终点后 drawTick 自己抬笔、状态转 DRAW_DONE
// ------------------------------------------------------------
void drawLine(float x1, float y1, float x2, float y2) {
  // 1. 抬笔到起点上方
  if(!moveToSync(x1, y1, penZUp)) return;
  // 2. 下笔
  if(!moveToSync(x1, y1, penZDown)) return;

  // 3. 装载画线段
  way[0].x = x2;  way[0].y = y2;  way[0].z = penZDown;
  wayN = 1;  wayI = 0;
  curX = x1; curY = y1; curZ = penZDown;
  drawCurve = false;               // 直线走航点模式
  drawState = DRAW_RUN;
  Serial.println(F("+DRAW RUN"));
}


// ------------------------------------------------------------
//  drawTick —— 任务五心跳，loop 里每轮调一次（DRAW_RUN 时才推进）
//
//  做两件事：
//    ① 到点了（millis 走过 DRAW_STEP_MS）就推进一段
//    ② 推进用"线性插值"：把"当前点 cur -> way[wayI]"按 DRAW_STEP_MM
//       拆成若干小步，每步走一小段。走完一段 wayI++。
//       全部航点走完 -> 抬笔、状态转 DRAW_DONE。
//
//  为什么不在每步里 delay：那样会卡住 loop，scanButtons / 串口指令
//  全都收不到。非阻塞才能被 '3' 暂停、'5' 取消。
// ------------------------------------------------------------
void drawTick() {
  static unsigned long lastMs = 0;
  unsigned long now = millis();
  if(now - lastMs < DRAW_STEP_MS) return;
  lastMs = now;

  // —— 平滑曲线模式：按参数 t 实时算点（不走航点表）——
  if(drawCurve) {
    drawT += drawDT;
    if(drawT >= 1.0f) {
      moveToSync(curX, curY, penZUp);
      drawCurve = false;
      drawState = DRAW_DONE;
      Serial.println(F("+DRAW DONE"));
      return;
    }
    float nx, ny, nz;
    catmullRom(drawT, nx, ny, nz);
    if(!moveTo(nx, ny, nz)) { drawCancel(); return; }
    return;
  }

  // 全部航点走完了 -> 抬笔、状态转 DONE
  if(wayI >= wayN) {
    moveToSync(curX, curY, penZUp);
    drawState = DRAW_DONE;
    Serial.println(F("+DRAW DONE"));
    return;
  }

  // 当前段：cur -> way[wayI]
  float tx = way[wayI].x, ty = way[wayI].y, tz = way[wayI].z;
  float dx = tx - curX, dy = ty - curY, dz = tz - curZ;
  float dist = sqrt(dx*dx + dy*dy + dz*dz);

  if(dist < DRAW_STEP_MM) {
    // 这段太短，直接到位，下轮走下一段
    if(!moveTo(tx, ty, tz)) { drawCancel(); return; }
    wayI++;
    return;
  }

  // 推进一步：方向 × 步长
  float nx = curX + dx / dist * DRAW_STEP_MM;
  float ny = curY + dy / dist * DRAW_STEP_MM;
  float nz = curZ + dz / dist * DRAW_STEP_MM;
  if(!moveTo(nx, ny, nz)) { drawCancel(); return; }
}


// ------------------------------------------------------------
//  drawPause —— '3'：暂停（保留 wayI / curX 等，状态转 PAUSE）
// ------------------------------------------------------------
void drawPause() {
  if(drawState != DRAW_RUN) {
    Serial.println(F("+Not drawing, cannot pause"));
    return;
  }
  drawState = DRAW_PAUSE;
  Serial.println(F("+DRAW PAUSE"));
}


// ------------------------------------------------------------
//  drawResume —— '4'：继续（状态从 PAUSE 转回 RUN，进度原样保留）
// ------------------------------------------------------------
void drawResume() {
  if(drawState != DRAW_PAUSE) {
    Serial.println(F("+Not paused, cannot resume"));
    return;
  }
  drawState = DRAW_RUN;
  Serial.println(F("+DRAW RESUME"));
}


// ------------------------------------------------------------
//  drawCancel —— '5'：取消（抬笔、清航点、回 IDLE）
// ------------------------------------------------------------
void drawCancel() {
  wayN = 0;  wayI = 0;
  drawCurve = false;
  drawState = DRAW_IDLE;
  moveToSync(curX, curY, penZUp);    // 抬笔（在 penZDown 的话抬起来）
  Serial.println(F("+DRAW CANCEL"));
}


// ============================================================
//  任务五：基础图形（N / △ / Z / V）
// ============================================================

// ------------------------------------------------------------
//  drawPolyline —— 通用折线：给定一串顶点，一笔画出来
//
//  和 drawLine 的区别：drawLine 只画一段（1 个航点），
//  drawPolyline 能画任意多段（n 个顶点 -> n-1 段），中途不抬笔。
//  基础图形 N/△/Z/V 就是用它画的。
// ------------------------------------------------------------
void drawPolyline(const WayPt *pts, int n) {
  if(n < 2 || n > MAX_WAY) {
    Serial.println(F("! bad polyline point count"));
    return;
  }

  // 抬笔到起点上方，再下笔
  if(!moveToSync(pts[0].x, pts[0].y, penZUp))   return;
  if(!moveToSync(pts[0].x, pts[0].y, penZDown)) return;

  // 装载航点，交给 drawTick 非阻塞地走
  for(int i = 0; i < n; i++) way[i] = pts[i];
  wayN = n;  wayI = 0;
  curX = pts[0].x;  curY = pts[0].y;  curZ = penZDown;
  drawCurve = false;
  drawState = DRAW_RUN;
  Serial.println(F("+DRAW RUN"));
}


// ------------------------------------------------------------
//  drawShape —— '2' / '6' / '7' / '8'：画基础图形
//
//  图形都画在中心 (100, 0) 附近，x∈[70,130]、y∈[-30,30]。
//  标称连杆参数下这些点的 ρ 约 70~123，稳稳落在可达区间 [53,213]。
//  ⚠ 若笔尖高度 penZDown 标定后与占位值 30 差很多，可达性要重核一遍。
// ------------------------------------------------------------
void drawShape(char which) {
  WayPt p[4];
  int n = 0;

  switch(which) {
    case 'N':                       // 两竖 + 一斜
      p[0].x =  70; p[0].y = -30; p[0].z = penZDown;
      p[1].x =  70; p[1].y =  30; p[1].z = penZDown;
      p[2].x = 130; p[2].y = -30; p[2].z = penZDown;
      p[3].x = 130; p[3].y =  30; p[3].z = penZDown;
      n = 4;
      break;
    case 'T':                       // △ 三角形（回到起点闭合）
      p[0].x = 100; p[0].y =  35; p[0].z = penZDown;
      p[1].x =  70; p[1].y = -30; p[1].z = penZDown;
      p[2].x = 130; p[2].y = -30; p[2].z = penZDown;
      p[3].x = 100; p[3].y =  35; p[3].z = penZDown;
      n = 4;
      break;
    case 'Z':                       // 上横 + 斜 + 下横
      p[0].x =  70; p[0].y =  30; p[0].z = penZDown;
      p[1].x = 130; p[1].y =  30; p[1].z = penZDown;
      p[2].x =  70; p[2].y = -30; p[2].z = penZDown;
      p[3].x = 130; p[3].y = -30; p[3].z = penZDown;
      n = 4;
      break;
    case 'V':                       // 左上一撇 + 右上一捺
      p[0].x =  70; p[0].y =  30; p[0].z = penZDown;
      p[1].x = 100; p[1].y = -30; p[1].z = penZDown;
      p[2].x = 130; p[2].y =  30; p[2].z = penZDown;
      n = 3;
      break;
    default:
      Serial.println(F("! unknown shape"));
      return;
  }

  Serial.print(F("+Draw shape "));
  Serial.println(which);
  drawPolyline(p, n);
}


// ============================================================
//  任务五：五点示教（折线 / 平滑曲线）
// ============================================================

// 'T'：记一个示教点（把当前四轴角度存下来）
void teachRecord() {
  if(teachCount >= TEACH_N) {
    Serial.println(F("! teach full (5). send Y to clear"));
    return;
  }
  teachPts[teachCount][0] = basePos;
  teachPts[teachCount][1] = rArmPos;
  teachPts[teachCount][2] = fArmPos;
  teachPts[teachCount][3] = clawPos;
  teachCount++;

  Serial.print(F("+Teach #"));
  Serial.print(teachCount);
  Serial.print(F(" { "));
  Serial.print(basePos); Serial.print(F(", "));
  Serial.print(rArmPos); Serial.print(F(", "));
  Serial.print(fArmPos); Serial.print(F(", "));
  Serial.print(clawPos);
  Serial.println(F(" }"));
}

// 'Y'：清空示教点
void teachClear() {
  teachCount = 0;
  Serial.println(F("+Teach cleared"));
}


// ------------------------------------------------------------
//  runTeach —— 'F' 画折线 / 'W' 画平滑曲线
//
//  先把 5 个示教姿态【正解】成末端坐标（存进 teachXYZ），然后：
//    折线 -> 5 个坐标直接当航点，交给 drawTick 一段段走（复用现成机制）
//    曲线 -> 记下总步数，drawTick 每步按参数 t 调 catmullRom 实时算点
// ------------------------------------------------------------
void runTeach(bool smooth) {
  if(teachCount < TEACH_N) {
    Serial.print(F("! need 5 teach points, now "));
    Serial.println(teachCount);
    return;
  }

  // 5 个姿态 -> 末端坐标（几何正解），顺便累加总弧长用来定步数
  float totalLen = 0;
  for(byte i = 0; i < TEACH_N; i++) {
    float t1 = servo2thBase(teachPts[i][0]);
    float t2 = servo2thRarm(teachPts[i][1]);
    float t3 = servo2thFarm(teachPts[i][2]);
    float R  = ARM_L1 * cos(t2) + ARM_LF * cos(t2 + t3);

    teachXYZ[i][0] = (ARM_D + R) * cos(t1);
    teachXYZ[i][1] = (ARM_D + R) * sin(t1);
    teachXYZ[i][2] = ARM_H + ARM_L1 * sin(t2) + ARM_LF * sin(t2 + t3);

    if(i > 0) {
      float dx = teachXYZ[i][0] - teachXYZ[i-1][0];
      float dy = teachXYZ[i][1] - teachXYZ[i-1][1];
      float dz = teachXYZ[i][2] - teachXYZ[i-1][2];
      totalLen += sqrt(dx*dx + dy*dy + dz*dz);
    }
  }

  if(smooth) {
    // 平滑曲线：参数化，drawTick 里每步现算
    int steps = (int)(totalLen / DRAW_STEP_MM);
    if(steps < 20) steps = 20;
    drawDT    = 1.0f / (float)steps;
    drawT     = 0.0f;
    drawCurve = true;
    curX = teachXYZ[0][0];  curY = teachXYZ[0][1];  curZ = teachXYZ[0][2];
    drawState = DRAW_RUN;
    Serial.println(F("+DRAW CURVE RUN (5 pts, smooth)"));
  } else {
    // 折线：5 个坐标就是 5 个航点
    for(byte i = 0; i < TEACH_N; i++) {
      way[i].x = teachXYZ[i][0];
      way[i].y = teachXYZ[i][1];
      way[i].z = teachXYZ[i][2];
    }
    wayN = TEACH_N;  wayI = 0;
    curX = teachXYZ[0][0];  curY = teachXYZ[0][1];  curZ = teachXYZ[0][2];
    drawCurve = false;
    drawState = DRAW_RUN;
    Serial.println(F("+DRAW POLYLINE RUN (5 pts)"));
  }
}


// ------------------------------------------------------------
//  catmullRom —— 参数 t∈[0,1] 取平滑曲线上一点
//
//  Catmull-Rom 样条：曲线【过】所有示教点，且在点处切线连续。
//  5 个点定出 4 段，第 k 段由 P[k-1..k+2] 四点插值。
//  两端没有邻居就拿端点自己顶上（P[-1]=P[0]、P[5]=P[4]）。
// ------------------------------------------------------------
void catmullRom(float t, float &ox, float &oy, float &oz) {
  float fseg = t * (TEACH_N - 1);            // 0 .. 4
  int   seg  = (int)fseg;
  if(seg > TEACH_N - 2) seg = TEACH_N - 2;   // 最右夹到第 3 段
  float u = fseg - seg;                      // 段内 0..1

  int i1 = seg, i2 = seg + 1;
  int i0 = (i1 > 0)           ? i1 - 1 : 0;
  int i3 = (i2 < TEACH_N - 1) ? i2 + 1 : TEACH_N - 1;

  float u2 = u * u, u3 = u2 * u;

  float p0[3], p1[3], p2[3], p3[3];
  for(byte k = 0; k < 3; k++) {
    p0[k] = teachXYZ[i0][k];
    p1[k] = teachXYZ[i1][k];
    p2[k] = teachXYZ[i2][k];
    p3[k] = teachXYZ[i3][k];
  }

  float v[3];
  for(byte k = 0; k < 3; k++) {
    v[k] = 0.5f * ( 2.0f * p1[k]
                  + (-p0[k] + p2[k]) * u
                  + (2.0f*p0[k] - 5.0f*p1[k] + 4.0f*p2[k] - p3[k]) * u2
                  + (-p0[k] + 3.0f*p1[k] - 3.0f*p2[k] + p3[k]) * u3 );
  }

  ox = v[0];  oy = v[1];  oz = v[2];
}
