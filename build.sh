#!/usr/bin/env bash
# 构建 qt_translator —— Qt 5.14.2 (MinGW 7.3.0, mingw73_64 kit)
set -euo pipefail

QT_DIR="/c/Qt/Qt5.14.2/5.14.2/mingw73_64"
CMAKE="/c/Qt/Tools/CMake_64/bin/cmake.exe"
NINJA="/c/Qt/Tools/Ninja/ninja.exe"
GXX_DIR="/c/Qt/Qt5.14.2/Tools/mingw730_64/bin"

export PATH="$QT_DIR/bin:$GXX_DIR:$PATH"

BUILD_DIR="${1:-build}"

"$CMAKE" -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DCMAKE_PREFIX_PATH="$QT_DIR" \
  -DCMAKE_C_COMPILER="$GXX_DIR/gcc.exe" \
  -DCMAKE_CXX_COMPILER="$GXX_DIR/g++.exe"

"$CMAKE" --build "$BUILD_DIR"

echo "Build OK: $BUILD_DIR/qt_translator.exe"
