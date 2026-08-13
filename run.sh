#!/usr/bin/env bash
# 运行 qt_translator（开发调试用，需 Qt DLL 在 PATH 中）
set -euo pipefail

QT_DIR="/c/Qt/Qt5.14.2/5.14.2/mingw73_64"
GXX_DIR="/c/Qt/Qt5.14.2/Tools/mingw730_64/bin"
BUILD_DIR="${1:-build}"

export PATH="$QT_DIR/bin:$GXX_DIR:$PATH"

EXE="$BUILD_DIR/qt_translator.exe"
if [ ! -f "$EXE" ]; then
  echo "未找到 $EXE，请先运行 ./build.sh" >&2
  exit 1
fi

"$EXE"
