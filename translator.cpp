#ifdef _WIN32
// 必须在任何 Qt 头之前包含：Qt 可能破坏 COM 的 interface 宏
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#endif

#include "translator.h"

#include <QApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QClipboard>
#include <QScreen>
#include <QCursor>

namespace {
// 获取当前真正拥有键盘焦点的窗口（跨进程可靠）
HWND focusedWindow() {
    HWND fg = GetForegroundWindow();
    if (!fg)
        return nullptr;
    DWORD tid = GetWindowThreadProcessId(fg, nullptr);
    GUITHREADINFO gti = {};
    gti.cbSize = sizeof(gti);
    if (GetGUIThreadInfo(tid, &gti) && gti.hwndFocus)
        return gti.hwndFocus;
    return fg;
}
} // namespace

Translator::Translator(TranslateBubble *bubble, QObject *parent)
    : QObject(parent), m_bubble(bubble) {
#ifdef Q_OS_WIN
    // 初始化 COM 供 UI Automation 使用（客户端需 MTA）
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE)
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                     IID_IUIAutomation, reinterpret_cast<void **>(&m_uia));
#endif
    m_hotkeyTimer.setInterval(40);
    connect(&m_hotkeyTimer, &QTimer::timeout, this, &Translator::pollKeys);
    m_hotkeyTimer.start();

    m_clipPollTimer.setInterval(100);
    connect(&m_clipPollTimer, &QTimer::timeout, this, &Translator::checkClipboardChange);

    connect(&m_autoTimer, &QTimer::timeout, this, &Translator::autoTick);

    connect(this, &Translator::translated, this, &Translator::onResult);
    connect(this, &Translator::failed, this, &Translator::onError);
    m_workerThread.start();
    recreateWorker();
}

void Translator::recreateWorker() {
    if (m_worker) {
        disconnect(m_worker, nullptr, this, nullptr);
        m_worker->deleteLater();
        m_worker = nullptr;
    }
    m_worker = LlmWorker::create(m_provider);
    m_worker->moveToThread(&m_workerThread);
    connect(m_worker, &LlmWorker::success, this, &Translator::translated);
    connect(m_worker, &LlmWorker::failure, this, &Translator::failed);
}

void Translator::setProvider(const QString &providerId) {
    const QString id = providerId.trimmed().isEmpty()
                           ? LlmWorker::defaultProviderId()
                           : providerId.trimmed().toLower();
    const QString norm =
        (id == QLatin1String("deepseek")) ? QStringLiteral("deepseek")
                                          : LlmWorker::defaultProviderId();
    if (norm == m_provider && m_worker)
        return;
    m_provider = norm;
    recreateWorker();
}

Translator::~Translator() {
    m_workerThread.quit();
    m_workerThread.wait(3000);
    delete m_worker;
    m_worker = nullptr;
#ifdef Q_OS_WIN
    if (m_uia) {
        m_uia->Release();
        m_uia = nullptr;
    }
    CoUninitialize();
#endif
}

void Translator::setHotkeyEnabled(bool on) {
    m_enabled = on;
    if (m_enabled) {
        m_prevCtrlF2 = false;
        m_prevAltF2 = false;
    }
}

void Translator::setAutoMode(bool on) {
    if (m_autoMode == on)
        return;
    m_autoMode = on;
    if (on)
        m_autoTimer.start(m_autoIntervalMs);
    else
        m_autoTimer.stop();
    emit autoModeChanged(on);
}

void Translator::setAutoIntervalMs(int ms) {
    m_autoIntervalMs = qMax(1000, ms);
    if (m_autoTimer.isActive())
        m_autoTimer.start(m_autoIntervalMs);
}

void Translator::autoTick() {
    if (!m_autoMode || m_copyInProgress)
        return;

    // 方式1：UIA 读取选中文字（非侵入式，WT/记事本/编辑器等）
    const QString sel = readSelectionUia().trimmed();
    if (!sel.isEmpty()) {
        if (sel != m_lastSource)
            runText(sel);
        return;
    }

    // 方式2：WM_COPY 回退（部分标准文本应用支持，非侵入）
    m_copyInProgress = true;
    m_autoPrevClip = QApplication::clipboard()->text();
    sendWmCopy();
    QTimer::singleShot(250, this, [this]() {
        const QString cur = QApplication::clipboard()->text();
        if (cur != m_autoPrevClip) {
            m_copyInProgress = false;
            finishAutoRead(cur);
            return;
        }
        // 方式3：Ctrl+C 回退（Chrome 等不响应 WM_COPY 的网站场景；终端除外避免干扰命令行）
        if (isTerminalFocused()) {
            m_copyInProgress = false;
            return;
        }
        sendCtrlC();
        QTimer::singleShot(300, this, [this]() {
            m_copyInProgress = false;
            finishAutoRead(QApplication::clipboard()->text());
        });
    });
}

void Translator::finishAutoRead(const QString &cur) {
    if (!m_autoMode)
        return;
    if (cur.trimmed().isEmpty() || cur == m_autoPrevClip)
        return; // 无选中或剪贴板未变化
    const QString sel = cur.trimmed();
    if (sel == m_lastSource)
        return; // 与上次翻译的原文相同，跳过
    runText(sel);
}

void Translator::pollKeys() {
#ifdef Q_OS_WIN
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;

    // Ctrl+F2：自动模式开关（始终可用）
    const bool ctrlF2 = ctrl && f2 && !alt;
    if (ctrlF2 && !m_prevCtrlF2)
        setAutoMode(!m_autoMode);
    m_prevCtrlF2 = ctrlF2;

    // Alt+F2：手动翻译（自动模式下手动快捷键不可用）
    const bool altF2 = alt && f2 && !ctrl;
    if (m_enabled && !m_autoMode && altF2 && !m_prevAltF2)
        trigger();
    m_prevAltF2 = altF2;

    // 气泡显示翻译结果时，Ctrl+C 自动复制译文
    const bool cc = ctrl && ((GetAsyncKeyState('C') & 0x8000) != 0);
    if (cc && !m_prevCopy && !m_suppressCopy) {
        if (m_bubble->isVisible() && m_bubble->kind() == TranslateBubble::Result)
            copyTranslation();
    }
    m_prevCopy = cc;
#endif
}

void Translator::sendWmCopy() {
#ifdef Q_OS_WIN
    if (HWND hwnd = focusedWindow())
        SendMessageW(hwnd, WM_COPY, 0, 0);
#endif
}

QString Translator::readSelectionUia() {
    QString text;
#ifdef Q_OS_WIN
    if (!m_uia)
        return text;
    // 依次尝试：焦点元素 → 鼠标位置元素（网页/Chrome 拖选不改变焦点，需按鼠标位置取）
    IUIAutomationElement *el = nullptr;
    if (SUCCEEDED(m_uia->GetFocusedElement(&el)) && el) {
        text = selectionFromElement(el);
        el->Release();
        if (!text.isEmpty())
            return text;
    }
    QScreen *scr = QGuiApplication::primaryScreen();
    const double dpr = scr ? scr->devicePixelRatio() : 1.0;
    const QPoint p = QCursor::pos();
    POINT pt = { static_cast<LONG>(p.x() * dpr), static_cast<LONG>(p.y() * dpr) };
    el = nullptr;
    if (SUCCEEDED(m_uia->ElementFromPoint(pt, &el)) && el) {
        text = selectionFromElement(el);
        el->Release();
    }
#endif
    return text;
}

QString Translator::selectionFromElement(IUIAutomationElement *el) {
    QString text;
#ifdef Q_OS_WIN
    IUIAutomationTextPattern *pat = nullptr;
    if (FAILED(el->GetCurrentPatternAs(UIA_TextPatternId, IID_IUIAutomationTextPattern,
                                       reinterpret_cast<void **>(&pat))) || !pat)
        return text;
    IUIAutomationTextRangeArray *ranges = nullptr;
    if (FAILED(pat->GetSelection(&ranges)) || !ranges) {
        pat->Release();
        return text;
    }
    pat->Release();
    int count = 0;
    ranges->get_Length(&count);
    if (count > 0) {
        IUIAutomationTextRange *range = nullptr;
        if (SUCCEEDED(ranges->GetElement(0, &range)) && range) {
            BSTR bstr = nullptr;
            if (SUCCEEDED(range->GetText(-1, &bstr)) && bstr) {
                text = QString::fromWCharArray(bstr, SysStringLen(bstr));
                if (text.size() > 10000)
                    text = text.left(10000);
                SysFreeString(bstr);
            }
            range->Release();
        }
    }
    ranges->Release();
    text = text.trimmed();
#endif
    return text;
}

bool Translator::isTerminalFocused() const {
#ifdef Q_OS_WIN
    HWND fg = GetForegroundWindow();
    if (!fg)
        return false;
    wchar_t cls[128] = {};
    GetClassNameW(fg, cls, 128);
    const QString c = QString::fromWCharArray(cls);
    return c == QLatin1String("ConsoleWindowClass")            // conhost
        || c == QLatin1String("CASCADIA_HOSTING_WINDOW_CLASS") // Windows Terminal
        || c == QLatin1String("mintty");                       // Git Bash / mintty
#else
    return false;
#endif
}

void Translator::trigger() {
    if (m_copyInProgress)
        return; // 与自动轮询冲突时忽略，避免同时复制
    m_copyInProgress = true;
    m_prevClip = QApplication::clipboard()->text();
    m_pendingPrevClip = m_prevClip;
    m_clipPollTries = 0;
    m_suppressCopy = true; // 屏蔽自己模拟的 Ctrl+C，避免误触发"复制译文"
    QTimer::singleShot(350, this, [this]() { m_suppressCopy = false; });
    sendWmCopy(); // 先尝试非侵入式复制选中内容
    // 250ms 后检查是否已复制成功；未变化则回退到 Ctrl+C（兼容不支持 WM_COPY 的应用）
    QTimer::singleShot(250, this, [this]() {
        const QString cur = QApplication::clipboard()->text();
        if (cur != m_pendingPrevClip) {
            // 选中内容已复制，优先使用
            m_clipPollTimer.stop();
            m_copyInProgress = false;
            runText(cur);
            return;
        }
        sendCtrlC();
        // 轮询剪贴板最多 700ms：一旦发现变化（选中文字被复制）立即优先使用，
        // 避免慢应用复制超时导致误用旧剪贴板内容
        m_clipPollTimer.start();
    });
}

void Translator::sendCtrlC() {
#ifdef Q_OS_WIN
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
#endif
}

void Translator::checkClipboardChange() {
    const QString cur = QApplication::clipboard()->text();
    if (cur != m_pendingPrevClip) {
        // 剪贴板已变化 => 选中文字复制成功，优先使用选中内容
        m_clipPollTimer.stop();
        m_copyInProgress = false;
        runText(cur);
        return;
    }
    if (++m_clipPollTries >= 7) { // 700ms 内无变化 => 没有选中，回退使用剪贴板内容
        m_clipPollTimer.stop();
        m_copyInProgress = false;
        runText(m_pendingPrevClip);
    }
}

void Translator::runText(const QString &text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        m_bubble->showStatus(QStringLiteral("未获取到选中文字，且剪贴板为空"),
                             TranslateBubble::Info);
        return;
    }
    m_bubble->showStatus(QStringLiteral("翻译中…"), TranslateBubble::Working);
    ++m_copyFlashSeq; // 使等待中的"已复制"恢复失效
    m_lastSource = t; // 记录本次翻译的原文（自动模式据此判断是否变化）
    const QString key = resolveApiKey(m_provider, m_apiKey);
    QMetaObject::invokeMethod(
        m_worker, [this, t, key]() { m_worker->translate(t, m_prompt, key); },
        Qt::QueuedConnection);
}

void Translator::copyTranslation() {
    if (m_lastResult.isEmpty())
        return;
    QApplication::clipboard()->setText(m_lastResult);
    m_bubble->showStatus(QStringLiteral("✓ 已复制翻译内容到剪贴板"), TranslateBubble::Info);
    const int seq = ++m_copyFlashSeq;
    QTimer::singleShot(1200, this, [this, seq]() {
        if (seq == m_copyFlashSeq && m_bubble->isVisible())
            m_bubble->showStatus(m_lastResult, TranslateBubble::Result);
    });
}

void Translator::onResult(const QString &text) {
    m_lastResult = text;
    ++m_copyFlashSeq;
    m_bubble->showStatus(text, TranslateBubble::Result);
}

void Translator::onError(const QString &error) {
    m_bubble->showStatus(QStringLiteral("翻译失败：%1").arg(error), TranslateBubble::Error);
}
