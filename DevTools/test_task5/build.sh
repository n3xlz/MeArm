#!/bin/zsh
# ------------------------------------------------------------
#  任务五 PC 端测试：一键构建 + 运行
#  步骤：① 用 arduino-cli 把最新 .ino 预处理成 .cpp（补函数原型）
#        ② 用本地桩编译测试台
#        ③ 运行
#
#  依赖：arduino-cli（Arduino IDE 2.x 内置）、Arduino AVR core、
#        官方 Servo 库（~/Library/Arduino15/libraries）、g++
#  路径全部相对于本脚本，仓库 clone 到任何位置都能直接跑。
# ------------------------------------------------------------
set -e
cd "$(dirname "$0")"

ROOT="$(cd ../.. && pwd)"                 # 仓库根目录
SKETCH="$ROOT/Task1235"                   # 被测草图（Task1235.ino）
CLI="${ARDUINO_CLI:-/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli}"
OUT="${TMPDIR:-/tmp}/meArm-build"

mkdir -p gen build

echo "[1/3] 预处理最新源码 …"
"$CLI" compile --fqbn arduino:avr:uno \
  --libraries "$HOME/Library/Arduino15/libraries" \
  --build-path "$OUT" "$SKETCH" > /dev/null
cp "$OUT/sketch/Task1235.ino.cpp" gen/Task1235.ino.cpp

echo "[2/3] 编译测试台 …"
/usr/bin/g++ -std=c++17 -I inc -I gen test_task5.cpp -o build/test_task5

echo "[3/3] 运行"
echo
./build/test_task5
