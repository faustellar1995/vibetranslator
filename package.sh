#!/usr/bin/env bash
# 打包发布：windeployqt 拷贝 Qt DLL + 手动拷贝 MinGW 运行时，输出 dist/qt_translator/
set -euo pipefail

QT_DIR="/c/Qt/Qt5.14.2/5.14.2/mingw73_64"
GXX_DIR="/c/Qt/Qt5.14.2/Tools/mingw730_64/bin"
BUILD_DIR="${1:-build}"
DIST="dist/qt_translator"

# MinGW 构建必须把 Qt bin / MinGW bin 加入 PATH，windeployqt 才能解析 Qt 安装与编译器运行时
export PATH="$QT_DIR/bin:$GXX_DIR:$PATH"

rm -rf "$DIST"
mkdir -p "$DIST"

cp "$BUILD_DIR/qt_translator.exe" "$DIST/"
"$QT_DIR/bin/windeployqt.exe" --no-translations \
  --no-system-d3d-compiler --no-opengl-sw --verbose 1 "$DIST/qt_translator.exe" >/dev/null

# MinGW 运行时 DLL
cp "$GXX_DIR/libgcc_s_seh-1.dll" "$GXX_DIR/libstdc++-6.dll" "$GXX_DIR/libwinpthread-1.dll" "$DIST/"

powershell -NoProfile -Command "Compress-Archive -Path '$DIST/*' -DestinationPath 'dist/qt_translator.zip' -Force" >/dev/null

echo "打包完成:"
ls -la "$DIST"
echo "压缩包: dist/qt_translator.zip"
