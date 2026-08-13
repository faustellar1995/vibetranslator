# Python / PyQt5 原型版本（早期版本，已不再维护）

> ⚠️ **注意：本目录为早期 Python 原型实现，仅作历史参考，功能与稳定性不及 C++ 正式版。**
> 正式版本（当前推荐）为仓库根目录的 **Qt5 (C++) 版 `vibetranslator`**，
> 发布包见 GitHub Releases：`qt_translator.zip`（Windows 独立运行包）。

## 说明

`translate_tool.py` 是本项目最初的 Python + PyQt5 原型（Python 3.14 / Windows），
实现了核心思路的完整验证：

- 全局快捷键 `Ctrl+F2` 翻译选中 / 剪贴板文字
- 半透明气泡跟随鼠标，点击关闭
- 屏幕取色反色保证任意背景下可读
- DeepSeek API 翻译（环境变量 `DS_KEY`）
- 主界面编辑 / 切换 prompt，自动保存
- 最小化到托盘、快捷键开关

该原型中确认的若干关键技术点（如 `WA_TranslucentBackground` 下样式表背景不渲染、
必须用 QPainter 手动绘制气泡、`taskkill` 的 WM_CLOSE 处理等）均已移植到 C++ 正式版。

## 运行（需要 Python 3.14 + PyQt5）

```bash
set DS_KEY=sk-xxxx
C:\Python314\pythonw.exe translate_tool.py
```

## 与 C++ 正式版的差异

| 项目 | Python 原型 | C++ 正式版 |
|---|---|---|
| 语言 / 框架 | Python 3.14 + PyQt5 | C++17 + Qt 5.14.2 |
| 快捷键 | 仅 Ctrl+F2 | Ctrl+F2 / Alt+F2 |
| 复制译文 | 无 | 气泡显示时 Ctrl+C 一键复制 |
| API Key | 仅环境变量 | 界面管理 + DS_KEY 优先 |
| 选中内容优先 | 单次读取剪贴板（慢应用会误判） | 轮询 700ms 检测，优先选中内容 |
| HTTPS | urllib + OpenSSL（依赖系统 TLS） | 原生 WinHTTP（零外部依赖） |
| 打包 | 无 | windeployqt 独立发布目录 + zip |
