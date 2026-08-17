# vibetranslator

基于 Qt5 (C++) 的全局自动翻译助手 —— 选中即译，气泡跟随鼠标，Ctrl+C 一键复制译文。

## 功能特性

- **全局快捷键**：`Alt+F2` 手动翻译（选中优先，无选中时翻译剪贴板）；`Ctrl+F2` 开关自动模式
- **自动模式**：勾选或按 `Ctrl+F2` 启用，每隔设定秒数（默认 3s，可调 1-60s）**非侵入式**轮询选中内容（不干扰命令行），与上次翻译原文不同时自动翻译；自动模式下手动快捷键不可用
- **跟随气泡**：译文以半透明气泡跟随鼠标移动，颜色随背景自动反色（屏幕取色），任意位置点击即可关闭
- **一键复制**：气泡显示译文时按 `Ctrl+C` 自动复制译文到剪贴板
- **多厂商 LLM**：默认 **MiMo**（`mimo-v2.5`），可切换 **DeepSeek**（`deepseek-chat`）；`LlmWorker` 基类 + 子类实现
- **API Key 管理**：MiMo 用 `MIMO_KEY`，DeepSeek 用 `DS_KEY`；也可在主界面分别保存
- **预设管理**：添加 / 保存 / 删除 / 点击应用，多套提示词快速切换
- **系统托盘**：关闭窗口最小化到托盘，托盘菜单可显示主界面、开关快捷键、使用帮助、退出

## 使用方法

1. 选择厂商并设置 API Key：
   - 主界面「模型厂商」下拉切换 MiMo / DeepSeek（默认 MiMo）
   - 环境变量：`MIMO_KEY` 或 `DS_KEY`（优先），或在界面输入后点「保存」
2. 选中任意窗口中的文字，按 `Alt+F2`（选中优先，无选中时自动翻译剪贴板）
3. 译文气泡跟随鼠标显示：**点击鼠标**关闭，按 **Ctrl+C** 复制译文
4. 需要持续跟译时，按 `Ctrl+F2` 或打开主界面勾选「自动模式」，并设定轮询间隔（默认 3 秒）

## 构建（Windows + Qt 5.14.2 MSVC）

依赖：VS 2022、CMake、Qt 5.14.2 `msvc2017_64`（见本机 `local-cpp` / `local-pcl-qt` skill）。

```bat
build.bat      rem 编译：build\qt_translator.exe
run.bat        rem 运行（需 Qt bin 在 PATH）
package.bat    rem 打包：dist\qt_translator\
```

单元 / 接口测试（离屏模式，含真实 MiMo API 调用）：

```bat
set QT_QPA_PLATFORM=offscreen
build\qt_translator_tests.exe
```

## 发布包

`dist/qt_translator/` 为独立发布目录（含全部 Qt DLL 与插件），拷贝到任意 Windows 机器解压即可运行；`dist/qt_translator.zip` 为压缩包。

> 早期 Python/PyQt5 原型（已不再维护）见 [`python_prototype/`](python_prototype/)。

## 技术说明

- HTTPS 请求使用 Windows 原生 **WinHTTP**，无需随包携带 OpenSSL DLL
- 选中文字读取使用 Windows SDK **UI Automation**（勿再依赖 3rdparty WIDL 头）
- 气泡使用 QPainter 手动绘制（`WA_TranslucentBackground` 下 Qt 样式表背景不渲染）
- 配置文件 `translator_config.json` 生成于程序目录（prompt、预设、API Key、快捷键开关）
