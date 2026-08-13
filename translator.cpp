#include "translator.h"

#include <QApplication>
#include <QMetaObject>
#include <QClipboard>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Translator::Translator(TranslateBubble *bubble, QObject *parent)
    : QObject(parent), m_bubble(bubble) {
    m_hotkeyTimer.setInterval(40);
    connect(&m_hotkeyTimer, &QTimer::timeout, this, &Translator::pollKeys);
    m_hotkeyTimer.start();

    m_clipPollTimer.setInterval(100);
    connect(&m_clipPollTimer, &QTimer::timeout, this, &Translator::checkClipboardChange);

    connect(&m_autoTimer, &QTimer::timeout, this, &Translator::autoTick);

    m_worker = new DeepSeekWorker;
    m_worker->moveToThread(&m_workerThread);
    connect(m_worker, &DeepSeekWorker::success, this, &Translator::translated);
    connect(m_worker, &DeepSeekWorker::failure, this, &Translator::failed);
    connect(this, &Translator::translated, this, &Translator::onResult);
    connect(this, &Translator::failed, this, &Translator::onError);
    m_workerThread.start();
}

Translator::~Translator() {
    m_workerThread.quit();
    m_workerThread.wait(3000);
    delete m_worker;
    m_worker = nullptr;
}

void Translator::setHotkeyEnabled(bool on) {
    m_enabled = on;
    if (m_enabled)
        m_prevHotkey = false; // 重新启用时避免误触发
}

void Translator::setAutoMode(bool on) {
    m_autoMode = on;
    if (on)
        m_autoTimer.start(m_autoIntervalMs);
    else
        m_autoTimer.stop();
}

void Translator::setAutoIntervalMs(int ms) {
    m_autoIntervalMs = qMax(1000, ms);
    if (m_autoTimer.isActive())
        m_autoTimer.start(m_autoIntervalMs);
}

void Translator::autoTick() {
    if (!m_autoMode || m_copyInProgress)
        return;
    m_copyInProgress = true;
    m_autoPrevClip = QApplication::clipboard()->text();
    sendCtrlC();
    QTimer::singleShot(300, this, [this]() {
        m_copyInProgress = false;
        if (!m_autoMode)
            return;
        const QString cur = QApplication::clipboard()->text();
        if (cur.trimmed().isEmpty() || cur == m_autoPrevClip)
            return; // 无选中或剪贴板未变化
        const QString sel = cur.trimmed();
        if (sel == m_lastSource)
            return; // 与上次翻译的原文相同，跳过
        runText(sel);
    });
}

void Translator::pollKeys() {
#ifdef Q_OS_WIN
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;

    // Ctrl+F2 或 Alt+F2 触发翻译
    const bool hot = f2 && (ctrl || alt);
    if (m_enabled && hot && !m_prevHotkey)
        trigger();
    m_prevHotkey = hot;

    // 气泡显示翻译结果时，Ctrl+C 自动复制译文
    const bool cc = ctrl && ((GetAsyncKeyState('C') & 0x8000) != 0);
    if (cc && !m_prevCopy && !m_suppressCopy) {
        if (m_bubble->isVisible() && m_bubble->kind() == TranslateBubble::Result)
            copyTranslation();
    }
    m_prevCopy = cc;
#endif
}

void Translator::trigger() {
    if (m_copyInProgress)
        return; // 与自动轮询冲突时忽略，避免同时模拟 Ctrl+C
    m_copyInProgress = true;
    m_prevClip = QApplication::clipboard()->text();
    m_pendingPrevClip = m_prevClip;
    m_clipPollTries = 0;
    m_suppressCopy = true; // 屏蔽自己模拟的 Ctrl+C，避免误触发"复制译文"
    QTimer::singleShot(350, this, [this]() { m_suppressCopy = false; });
    sendCtrlC();
    // 轮询剪贴板最多 700ms：一旦发现变化（选中文字被复制）立即优先使用，
    // 避免慢应用复制超时导致误用旧剪贴板内容
    m_clipPollTimer.start();
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
    const QString key = resolveApiKey(m_apiKey);
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
