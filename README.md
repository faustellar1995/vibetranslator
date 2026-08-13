# vibetranslator

基于 Qt5 (C++) 的全局自动翻译助手 —— 选中即译，气泡跟随鼠标，Ctrl+C 一键复制译文。

## 功能特性

- **全局快捷键**：`Alt+F2` 手动翻译（选中优先，无选中时翻译剪贴板）；`Ctrl+F2` 开关自动模式
- **自动模式**：勾选或按 `Ctrl+F2` 启用，每隔设定秒数（默认 3s，可调 1-60s）**非侵入式**轮询选中内容（不干扰命令行），与上次翻译原文不同时自动翻译；自动模式下手动快捷键不可用
- **跟随气泡**：译文以半透明气泡跟随鼠标移动，颜色随背景自动反色（屏幕取色），任意位置点击即可关闭
- **一键复制**：气泡显示译文时按 `Ctrl+C` 自动复制译文到剪贴板
- **DeepSeek 翻译**：默认英译中，system prompt 可自定义（主界面编辑，修改自动保存）
- **API Key 管理**：优先使用环境变量 `DS_KEY`，也可在主界面手动设置并自动存取
- **预设管理**：添加 / 保存 / 删除 / 点击应用，多套提示词快速切换
- **系统托盘**：关闭窗口最小化到托盘，托盘菜单可显示主界面、开关快捷键、使用帮助、退出

## 使用方法

1. 设置 API Key（二选一）：
   - 设置环境变量 `DS_KEY`（优先使用），或
   - 打开主界面 → 输入 Key → 点「保存」
2. 选中任意窗口中的文字，按 `Alt+F2`（选中优先，无选中时自动翻译剪贴板）
3. 译文气泡跟随鼠标显示：**点击鼠标**关闭，按 **Ctrl+C** 复制译文
4. 需要持续跟译时，按 `Ctrl+F2` 或打开主界面勾选「自动模式」，并设定轮询间隔（默认 3 秒）

## 构建（Windows + Qt 5.14.2 MinGW）

依赖：CMake、Ninja、Qt 5.14.2（`mingw73_64` kit）、MinGW 7.3.0

```bash
./build.sh    # 编译：build/qt_translator.exe + build/qt_translator_tests.exe
./run.sh      # 运行（开发调试）
./package.sh  # 打包发布：dist/qt_translator/ + dist/qt_translator.zip
```

单元 / 接口测试（离屏模式，含真实 DeepSeek API 调用）：

```bash
QT_QPA_PLATFORM=offscreen ./build/qt_translator_tests.exe
```

## 发布包

`dist/qt_translator/` 为独立发布目录（含全部 Qt DLL 与插件），拷贝到任意 Windows 机器解压即可运行；`dist/qt_translator.zip` 为压缩包。

> 早期 Python/PyQt5 原型（已不再维护）见 [`python_prototype/`](python_prototype/)。

## 技术说明

- HTTPS 请求使用 Windows 原生 **WinHTTP**，无需随包携带 OpenSSL DLL
- 气泡使用 QPainter 手动绘制（`WA_TranslucentBackground` 下 Qt 样式表背景不渲染）
- 配置文件 `translator_config.json` 生成于程序目录（prompt、预设、API Key、快捷键开关）
