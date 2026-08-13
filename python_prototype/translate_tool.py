# -*- coding: utf-8 -*-
"""
PyQt5 自动翻译助手 (Python 3.14 / Windows)
============================================
- 全局快捷键 Ctrl+F2：翻译当前窗口选中的文字；若没有选中则翻译剪贴板中的文字
- 译文以半透明气泡形式跟随鼠标移动，鼠标任意点击后消失
- 使用 DeepSeek API 翻译，API Key 取自环境变量 DS_KEY（默认英译中，prompt 可自定义）
- 主界面可编辑/切换 system prompt，prompt 自动保存
- 支持最小化到系统托盘，主界面可开关全局快捷键
"""

import ctypes
from ctypes import wintypes
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

from PyQt5.QtCore import Qt, QTimer, QPoint, pyqtSignal, QObject
from PyQt5.QtGui import QColor, QFont, QIcon, QPainter, QPixmap, QCursor, QGuiApplication
from PyQt5.QtWidgets import (
    QApplication, QHBoxLayout, QInputDialog, QLabel, QListWidget, QListWidgetItem,
    QPlainTextEdit, QPushButton, QSystemTrayIcon, QVBoxLayout, QWidget, QMenu,
    QCheckBox,
)

APP_NAME = "PyQt 翻译助手"
CONFIG_PATH = Path(__file__).resolve().parent / "translator_config.json"
API_URL = "https://api.deepseek.com/chat/completions"
HOTKEY_NAME = "Ctrl+F2"

# Windows 原生消息常量（用于区分"用户点关闭"与"外部 kill"）
WM_CLOSE = 0x0010
WM_QUERYENDSESSION = 0x0011
WM_ENDSESSION = 0x0016
WM_NCLBUTTONDOWN = 0x00A1
VK_MENU = 0x12

DEFAULT_PROMPT = (
    "你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文（默认英译中）。\n"
    "要求：\n"
    "1. 只输出译文本身，不要任何解释、注释、前言或原文；\n"
    "2. 保持原文的语气与格式，专业术语翻译准确；\n"
    "3. 若原文已是中文，润色后直接返回。"
)

DEFAULT_PRESETS = [
    {"name": "英译中",
     "prompt": "你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文（默认英译中）。只输出译文本身，不要任何解释、注释、前言或原文；保持原文语气与格式，专业术语准确；若原文已是中文，润色后直接返回。"},
    {"name": "中译英",
     "prompt": "You are a professional translation engine. Translate the user's content into English. Output only the translation with no explanation, notes, preface or the original text. Keep the original tone and formatting; use accurate technical terms."},
    {"name": "日译中",
     "prompt": "你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文。只输出译文本身，不要任何解释、注释、前言或原文；保持原文语气与格式。"},
    {"name": "精简概括",
     "prompt": "请用简体中文对用户提供的内容做简明扼要的概括，不超过 100 字，直接输出概括结果，不要任何前缀或解释。"},
]


# ---------------------------------------------------------------- 配置
class Config:
    def __init__(self, current_prompt=None, presets=None, hotkey_enabled=True):
        self.current_prompt = current_prompt or DEFAULT_PROMPT
        self.presets = presets if presets is not None else [dict(p) for p in DEFAULT_PRESETS]
        self.hotkey_enabled = hotkey_enabled

    @classmethod
    def load(cls):
        try:
            if CONFIG_PATH.exists():
                d = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
                presets = d.get("presets")
                if not isinstance(presets, list) or not presets:
                    presets = [dict(p) for p in DEFAULT_PRESETS]
                cp = (d.get("current_prompt") or "").strip() or DEFAULT_PROMPT
                hk = bool(d.get("hotkey_enabled", True))
                return cls(cp, presets, hk)
        except Exception as e:
            print("读取配置失败，使用默认配置:", e)
        return cls()

    def save(self):
        try:
            CONFIG_PATH.write_text(
                json.dumps({
                    "current_prompt": self.current_prompt,
                    "presets": self.presets,
                    "hotkey_enabled": self.hotkey_enabled,
                }, ensure_ascii=False, indent=2),
                encoding="utf-8")
        except Exception as e:
            print("保存配置失败:", e)


# ---------------------------------------------------------------- DeepSeek API
def call_deepseek(text, system_prompt, model="deepseek-chat"):
    key = os.environ.get("DS_KEY", "").strip()
    if not key:
        raise RuntimeError("未设置环境变量 DS_KEY")
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": text},
        ],
        "temperature": 0.3,
        "stream": False,
    }
    req = urllib.request.Request(
        API_URL,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "Authorization": "Bearer " + key,
            "User-Agent": "pyqt-translator/1.0",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "ignore")
        raise RuntimeError(f"API 错误 HTTP {e.code}: {body[:300]}") from e
    except urllib.error.URLError as e:
        raise RuntimeError(f"网络错误: {e.reason}") from e
    try:
        return data["choices"][0]["message"]["content"].strip()
    except (KeyError, IndexError, TypeError):
        raise RuntimeError(f"响应解析失败: {json.dumps(data, ensure_ascii=False)[:300]}") from None


# ---------------------------------------------------------------- 半透明跟随气泡
class TranslateBubble(QWidget):
    """半透明、置顶、不拦截鼠标的气泡，跟随鼠标移动，点击后消失。
    背景与文字用 QPainter 手动绘制（WA_TranslucentBackground 下样式表背景不会渲染），
    翻译结果会根据鼠标位置的屏幕背景色自动反色，保证任何背景下都清晰可见。
    """
    STYLES = {
        "working": (QColor(24, 32, 58, 235), QColor(207, 224, 255)),
        "info":    (QColor(42, 40, 16, 235), QColor(255, 217, 102)),
        "error":   (QColor(72, 18, 26, 235), QColor(255, 157, 157)),
    }
    FALLBACK_BG = QColor(15, 20, 38, 238)   # 取色失败时的兜底
    FALLBACK_FG = QColor(242, 245, 250)
    PAD_H = 30   # 左右 padding + 边框
    PAD_V = 22   # 上下 padding + 边框
    MAX_CHARS = 4000

    def __init__(self):
        super().__init__(
            None,
            Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowTransparentForInput,
        )
        self.setAttribute(Qt.WA_TranslucentBackground)
        f = QFont("Microsoft YaHei UI", 11)
        self.setFont(f)
        self._text = ""
        self._bg_color = self.FALLBACK_BG
        self._fg_color = self.FALLBACK_FG
        self._kind = "result"
        self._last_sample = 0.0
        self._timer = QTimer(self)
        self._timer.setInterval(16)
        self._timer.timeout.connect(self._tick)
        self.hide()

    def show_status(self, text, kind="result"):
        text = (text or "").strip()
        if len(text) > self.MAX_CHARS:
            text = text[:self.MAX_CHARS] + "…"
        self._text = text

        screen = QGuiApplication.primaryScreen().availableGeometry()
        max_w = max(240, min(680, screen.width() - 80))
        inner_w = max_w - self.PAD_H
        rect = self.fontMetrics().boundingRect(0, 0, inner_w, 10 ** 6,
                                               Qt.TextWordWrap, text)
        w = min(max_w, rect.width() + self.PAD_H)
        h = rect.height() + self.PAD_V
        max_h = int(screen.height() * 0.75)
        if h > max_h:
            h = max_h
        self.setFixedSize(w, h)
        self.update()

        self._kind = kind
        self._last_sample = 0.0
        self._apply_style()

        cursor = QCursor.pos()
        self.move(self._clamp(QPoint(cursor.x() + 16, cursor.y() + 22)))
        self.show()
        self.raise_()
        self._timer.start()

    def paintEvent(self, event):
        """手动绘制：圆角半透明背景 + 文字（样式表背景在透明窗口上不渲染）。"""
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        r = self.rect().adjusted(1, 1, -1, -1)
        p.setPen(QColor(255, 255, 255, 40))
        p.setBrush(self._bg_color)
        p.drawRoundedRect(r, 10, 10)
        p.setPen(self._fg_color)
        p.drawText(r.adjusted(14, 10, -14, -10),
                   Qt.TextWordWrap | Qt.AlignLeft | Qt.AlignTop, self._text)
        p.end()

    def _apply_style(self):
        """按当前状态上色：result 用鼠标位置取色反色，保证任何背景下都清晰。"""
        if self._kind == "result":
            bg, fg = self._adaptive_colors()
        else:
            bg, fg = self.STYLES.get(self._kind, (self.FALLBACK_BG, self.FALLBACK_FG))
        if bg != self._bg_color or fg != self._fg_color:
            self._bg_color, self._fg_color = bg, fg
            self.update()

    def _adaptive_colors(self):
        """取鼠标位置屏幕像素并反色：
        - 气泡背景 = 背景色取反（半透明）
        - 文字 = 黑/白二选一，保证与反色背景的高对比（避免纯反色在灰色背景下同色失效）
        """
        color = self._sample_screen_color()
        if color is None:
            return self.FALLBACK_BG, self.FALLBACK_FG
        inv = QColor(255 - color.red(), 255 - color.green(), 255 - color.blue())
        inv.setAlpha(235)
        lum = 0.299 * inv.red() + 0.587 * inv.green() + 0.114 * inv.blue()
        fg = QColor(17, 19, 24) if lum > 140 else QColor(242, 245, 250)
        return inv, fg

    def _sample_screen_color(self):
        """用 GDI GetPixel 读取鼠标所在位置的屏幕像素（物理坐标）。"""
        try:
            scr = QGuiApplication.primaryScreen()
            dpr = scr.devicePixelRatio()
            pt = QCursor.pos()
            x = int(pt.x() * dpr)
            y = int(pt.y() * dpr)
            user32 = ctypes.windll.user32
            gdi32 = ctypes.windll.gdi32
            hdc = user32.GetDC(0)
            try:
                c = gdi32.GetPixel(hdc, x, y)
            finally:
                user32.ReleaseDC(0, hdc)
            if c == 0xFFFFFFFF:  # CLR_INVALID
                return None
            return QColor(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF)
        except Exception:
            return None

    def _tick(self):
        if self._any_mouse_button_down():
            self.hide()
            self._timer.stop()
            return
        cursor = QCursor.pos()
        # 跟随过程中定期重新取色；若气泡盖住了鼠标位置则跳过（避免自反馈闪烁）
        now = time.monotonic()
        if self._kind == "result" and now - self._last_sample > 0.15 \
                and not self.geometry().contains(cursor):
            self._last_sample = now
            self._apply_style()
        target = QPoint(cursor.x() + 16, cursor.y() + 22)
        cur = self.pos()
        nx = cur.x() + (target.x() - cur.x()) * 0.3
        ny = cur.y() + (target.y() - cur.y()) * 0.3
        self.move(self._clamp(QPoint(int(nx), int(ny))))

    def _clamp(self, p):
        screen = QGuiApplication.primaryScreen().availableGeometry()
        s = self.size()
        x = max(screen.left() + 4, min(p.x(), screen.right() - s.width() - 4))
        y = max(screen.top() + 4, min(p.y(), screen.bottom() - s.height() - 4))
        return QPoint(x, y)

    @staticmethod
    def _any_mouse_button_down():
        k = ctypes.windll.user32
        try:
            return bool(
                k.GetAsyncKeyState(0x01) & 0x8000 or  # 左键
                k.GetAsyncKeyState(0x02) & 0x8000 or  # 右键
                k.GetAsyncKeyState(0x04) & 0x8000)    # 中键
        except Exception:
            return False


# ---------------------------------------------------------------- 翻译控制器（全局快捷键 + 剪贴板 + API）
class Translator(QObject):
    translated = pyqtSignal(str)
    failed = pyqtSignal(str)

    VK_CTRL = 0x11
    VK_C = 0x43
    VK_F2 = 0x71
    KEYEVENTF_KEYUP = 0x0002

    def __init__(self, bubble):
        super().__init__()
        self.bubble = bubble
        self.prompt = DEFAULT_PROMPT
        self._prev_clip = ""
        self._prev_hotkey = False
        self._enabled = True

        self._hotkey_timer = QTimer(self)
        self._hotkey_timer.setInterval(40)
        self._hotkey_timer.timeout.connect(self._poll_hotkey)
        self._hotkey_timer.start()

        self.translated.connect(self._on_result)
        self.failed.connect(self._on_error)

    # ---- 快捷键开关（主界面可控制）----
    def set_hotkey_enabled(self, enabled):
        self._enabled = bool(enabled)
        if self._enabled:
            self._prev_hotkey = False  # 重新启用时避免误触发
            if not self._hotkey_timer.isActive():
                self._hotkey_timer.start()
        else:
            self._hotkey_timer.stop()

    def hotkey_enabled(self):
        return self._enabled

    def _poll_hotkey(self):
        if not self._enabled:
            return
        k = ctypes.windll.user32
        try:
            ctrl = bool(k.GetAsyncKeyState(self.VK_CTRL) & 0x8000)
            f2 = bool(k.GetAsyncKeyState(self.VK_F2) & 0x8000)
        except Exception:
            return
        pressed = ctrl and f2
        if pressed and not self._prev_hotkey:
            self.trigger()
        self._prev_hotkey = pressed

    # ---- 触发流程 ----
    def trigger(self):
        try:
            self._prev_clip = QApplication.clipboard().text() or ""
        except Exception:
            self._prev_clip = ""
        self._send_ctrl_c()
        QTimer.singleShot(180, self._translate_from_clipboard)

    def _send_ctrl_c(self):
        k = ctypes.windll.user32
        try:
            k.keybd_event(self.VK_CTRL, 0, 0, 0)
            k.keybd_event(self.VK_C, 0, 0, 0)
            k.keybd_event(self.VK_C, 0, self.KEYEVENTF_KEYUP, 0)
            k.keybd_event(self.VK_CTRL, 0, self.KEYEVENTF_KEYUP, 0)
        except Exception:
            pass

    def _translate_from_clipboard(self):
        try:
            cur = QApplication.clipboard().text() or ""
        except Exception:
            cur = ""
        # 剪贴板变化 => 有选中文字被复制；否则用原有剪贴板内容
        if cur.strip() and cur != self._prev_clip:
            text = cur
        else:
            text = self._prev_clip
        self.run_text(text)

    def run_text(self, text):
        text = (text or "").strip()
        if not text:
            self.bubble.show_status("未获取到选中文字，且剪贴板为空", "info")
            return
        self.bubble.show_status("翻译中…")
        threading.Thread(target=self._worker, args=(text,), daemon=True).start()

    def _worker(self, text):
        try:
            result = call_deepseek(text, self.prompt)
            self.translated.emit(result)
        except Exception as e:
            self.failed.emit(str(e))

    def _on_result(self, text):
        self.bubble.show_status(text, "result")

    def _on_error(self, msg):
        self.bubble.show_status(f"翻译失败：{msg}", "error")


# ---------------------------------------------------------------- 主界面
class MainWindow(QWidget):
    def __init__(self, translator, config):
        super().__init__()
        self.translator = translator
        self.config = config
        self._loading = True
        self._tray = None
        self._last_titlebar_click = 0.0  # 记录最近一次标题栏点击时间

        self.setWindowTitle(APP_NAME)
        self.setMinimumSize(560, 500)
        self.resize(660, 540)

        root = QVBoxLayout(self)

        tip = QLabel("系统提示词 Prompt —— 决定翻译方向/风格（默认为英译中），修改后自动保存")
        tip.setStyleSheet("color:#888;")
        root.addWidget(tip)

        self.prompt_edit = QPlainTextEdit()
        self.prompt_edit.setPlaceholderText("在这里输入系统提示词…")
        f = self.prompt_edit.font()
        f.setPointSize(11)
        self.prompt_edit.setFont(f)
        root.addWidget(self.prompt_edit, 3)

        root.addWidget(QLabel("已保存的预设（点击应用）："))

        preset_row = QHBoxLayout()
        self.preset_list = QListWidget()
        self.preset_list.itemClicked.connect(self._apply_preset)
        preset_row.addWidget(self.preset_list, 3)

        btn_col = QVBoxLayout()
        self.btn_save_preset = QPushButton("保存为预设")
        self.btn_del_preset = QPushButton("删除预设")
        self.btn_test = QPushButton("测试翻译")
        for b in (self.btn_save_preset, self.btn_del_preset, self.btn_test):
            b.setMinimumHeight(34)
            btn_col.addWidget(b)
        btn_col.addStretch(1)
        preset_row.addLayout(btn_col, 2)
        root.addLayout(preset_row, 2)

        # 快捷键开关
        hk_row = QHBoxLayout()
        self.hotkey_check = QCheckBox(f"启用全局快捷键 {HOTKEY_NAME}（翻译选中/剪贴板文字）")
        self.hotkey_check.setChecked(self.config.hotkey_enabled)
        self.hotkey_check.toggled.connect(self._on_hotkey_toggled)
        hk_row.addWidget(self.hotkey_check)
        hk_row.addStretch(1)
        root.addLayout(hk_row)

        bottom = QHBoxLayout()
        self.key_label = QLabel()
        bottom.addWidget(self.key_label)
        bottom.addStretch(1)
        self.btn_hide = QPushButton("隐藏到托盘")
        self.btn_hide.clicked.connect(self.hide)
        bottom.addWidget(self.btn_hide)
        root.addLayout(bottom)

        self.btn_save_preset.clicked.connect(self._save_preset)
        self.btn_del_preset.clicked.connect(self._del_preset)
        self.btn_test.clicked.connect(self._test)
        self.prompt_edit.textChanged.connect(self._on_prompt_changed)

        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.setInterval(800)
        self._debounce.timeout.connect(self._save_config)

        self._refresh()
        self._loading = False

    # ---- 界面刷新 / 保存 ----
    def _refresh(self):
        self.preset_list.clear()
        for p in self.config.presets:
            it = QListWidgetItem(p.get("name", "未命名"))
            it.setData(Qt.UserRole, p.get("prompt", ""))
            self.preset_list.addItem(it)
        self.prompt_edit.setPlainText(self.config.current_prompt)
        self.preset_list.setCurrentRow(-1)
        for i in range(self.preset_list.count()):
            it = self.preset_list.item(i)
            if it.data(Qt.UserRole) == self.config.current_prompt:
                self.preset_list.setCurrentItem(it)
                break
        self._update_key_label()

    def _reload_presets(self, select_name=None):
        self.preset_list.clear()
        for p in self.config.presets:
            it = QListWidgetItem(p.get("name", "未命名"))
            it.setData(Qt.UserRole, p.get("prompt", ""))
            self.preset_list.addItem(it)
        if select_name:
            for i in range(self.preset_list.count()):
                if self.preset_list.item(i).text() == select_name:
                    self.preset_list.setCurrentItem(self.preset_list.item(i))
                    break

    def _update_key_label(self):
        key = os.environ.get("DS_KEY", "").strip()
        if key:
            self.key_label.setText("DS_KEY: 已设置 ✓")
            self.key_label.setStyleSheet("color:#3c9e4a;")
        else:
            self.key_label.setText("⚠ DS_KEY: 未设置（翻译将无法工作）")
            self.key_label.setStyleSheet("color:#c0392b;")

    def _save_config(self):
        prompt = self.prompt_edit.toPlainText().strip() or DEFAULT_PROMPT
        self.config.current_prompt = prompt
        self.config.hotkey_enabled = self.translator.hotkey_enabled()
        self.config.save()

    # ---- 事件 ----
    def _on_prompt_changed(self):
        if self._loading:
            return
        prompt = self.prompt_edit.toPlainText().strip() or DEFAULT_PROMPT
        self.translator.prompt = prompt
        self._debounce.start()

    def _on_hotkey_toggled(self, checked):
        self.translator.set_hotkey_enabled(checked)
        self._save_config()
        if not checked:
            self.translator.bubble.hide()  # 关闭快捷键时同时隐藏气泡

    def _apply_preset(self, item):
        prompt = item.data(Qt.UserRole) or DEFAULT_PROMPT
        self._loading = True
        self.prompt_edit.setPlainText(prompt)
        self._loading = False
        self.translator.prompt = prompt
        self._save_config()

    def _save_preset(self):
        prompt = self.prompt_edit.toPlainText().strip() or DEFAULT_PROMPT
        item = self.preset_list.currentItem()
        if item:
            name = item.text()
        else:
            name, ok = QInputDialog.getText(self, "保存预设", "预设名称：", text="自定义")
            if not ok or not name.strip():
                return
            name = name.strip()
        for p in self.config.presets:
            if p.get("name") == name:
                p["prompt"] = prompt
                break
        else:
            self.config.presets.append({"name": name, "prompt": prompt})
        self._reload_presets(select_name=name)
        self._save_config()

    def _del_preset(self):
        item = self.preset_list.currentItem()
        if not item:
            return
        name = item.text()
        self.config.presets = [p for p in self.config.presets if p.get("name") != name]
        self._reload_presets()
        self._save_config()

    def _test(self):
        prompt = self.prompt_edit.toPlainText().strip() or DEFAULT_PROMPT
        self.translator.prompt = prompt
        self.translator.run_text("Hello, world! This is a quick translation test. "
                                 "The quick brown fox jumps over the lazy dog.")

    # ---- 关闭 -> 最小化到托盘 ----
    def closeEvent(self, event):
        # 只有"用户主动关闭"（点 X / Alt+F4）走到这里：隐藏到托盘
        event.ignore()
        self.hide()
        if self._tray is not None:
            self._tray.showMessage(APP_NAME,
                                   f"已最小化到托盘，按 {HOTKEY_NAME} 随时翻译",
                                   QSystemTrayIcon.Information, 2500)

    def nativeEvent(self, eventType, message):
        """拦截 WM_CLOSE：
        - 用户点标题栏 X（先有 WM_NCLBUTTONDOWN）或 Alt+F4 => 交给 closeEvent 最小化到托盘
        - 外部关闭请求（taskkill / 任务管理器结束任务 / 关机）=> 真正退出，避免"kill 后卡住"
        """
        if eventType == b"windows_generic_MSG":
            try:
                msg = ctypes.cast(int(message), ctypes.POINTER(wintypes.MSG)).contents
                if msg.message == WM_NCLBUTTONDOWN:
                    self._last_titlebar_click = time.monotonic()
                elif msg.message == WM_CLOSE:
                    user_click = (time.monotonic() - self._last_titlebar_click < 1.0)
                    alt_down = bool(ctypes.windll.user32.GetAsyncKeyState(VK_MENU) & 0x8000)
                    if user_click or alt_down:
                        return False  # 用户主动关闭 -> closeEvent 里隐藏到托盘
                    QApplication.quit()  # 外部关闭请求 -> 真正退出
                    return True, 0
                elif msg.message in (WM_QUERYENDSESSION, WM_ENDSESSION):
                    # 系统关机/注销：允许结束并退出
                    QTimer.singleShot(0, QApplication.quit)
                    return True, 1
            except Exception:
                pass
        return super().nativeEvent(eventType, message)


# ---------------------------------------------------------------- 托盘图标
def make_tray_icon():
    pm = QPixmap(64, 64)
    pm.fill(Qt.transparent)
    p = QPainter(pm)
    p.setRenderHint(QPainter.Antialiasing)
    p.setBrush(QColor(24, 34, 66))
    p.setPen(Qt.NoPen)
    p.drawRoundedRect(0, 0, 64, 64, 14, 14)
    f = QFont("Microsoft YaHei UI", 24, QFont.Bold)
    p.setFont(f)
    p.setPen(QColor("#9fc3ff"))
    p.drawText(pm.rect(), Qt.AlignCenter, "译")
    p.end()
    return QIcon(pm)


# ---------------------------------------------------------------- 入口
def main():
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)

    config = Config.load()
    bubble = TranslateBubble()
    translator = Translator(bubble)
    translator.prompt = config.current_prompt
    translator.set_hotkey_enabled(config.hotkey_enabled)

    win = MainWindow(translator, config)
    win.show()

    tray = QSystemTrayIcon(make_tray_icon())
    tray.setToolTip(f"{APP_NAME} — {HOTKEY_NAME} 翻译选中/剪贴板文字")
    menu = QMenu()
    act_show = menu.addAction("显示主界面")
    act_toggle_hk = menu.addAction("启用全局快捷键" if translator.hotkey_enabled()
                                   else "停用全局快捷键")
    menu.addSeparator()
    act_quit = menu.addAction("退出")
    tray.setContextMenu(menu)

    def show_win():
        win.showNormal()
        win.raise_()
        win.activateWindow()

    def toggle_hk():
        new_state = not translator.hotkey_enabled()
        translator.set_hotkey_enabled(new_state)
        win.hotkey_check.setChecked(new_state)
        act_toggle_hk.setText("停用全局快捷键" if new_state else "启用全局快捷键")

    def quit_app():
        win._save_config()
        bubble.hide()
        app.quit()

    act_show.triggered.connect(show_win)
    act_toggle_hk.triggered.connect(toggle_hk)
    act_quit.triggered.connect(quit_app)
    tray.activated.connect(
        lambda reason: show_win() if reason == QSystemTrayIcon.DoubleClick else None)
    win._tray = tray

    if not os.environ.get("DS_KEY", "").strip():
        QTimer.singleShot(800, lambda: tray.showMessage(
            APP_NAME, "未检测到环境变量 DS_KEY，翻译将无法工作",
            QSystemTrayIcon.Warning, 5000))

    # 退出前兜底保存配置
    app.aboutToQuit.connect(lambda: (win._save_config(), bubble.hide()))

    tray.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
