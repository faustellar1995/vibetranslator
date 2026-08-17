#pragma once
#include <QObject>
#include <QTimer>
#include <QThread>
#include "translatebubble.h"
#include "llm.h"
#include "config.h"

// 翻译控制器：
// - Ctrl+F2 开关自动模式（定时轮询选中内容，变化时自动翻译，非侵入式）
// - Alt+F2 手动翻译（选中优先，无选中回退剪贴板；自动模式下不可用）
// - 气泡显示译文时按 Ctrl+C 自动复制译文

struct IUIAutomation;
struct IUIAutomationElement;

class Translator : public QObject {
    Q_OBJECT
public:
    explicit Translator(TranslateBubble *bubble, QObject *parent = nullptr);
    ~Translator() override;

    void setPrompt(const QString &p) { m_prompt = p; }
    void setApiKey(const QString &k) { m_apiKey = k.trimmed(); }
    void setProvider(const QString &providerId);
    QString provider() const { return m_provider; }
    void setHotkeyEnabled(bool on);
    bool hotkeyEnabled() const { return m_enabled; }

    void setAutoMode(bool on);
    void setAutoIntervalMs(int ms);
    bool autoMode() const { return m_autoMode; }
    int autoIntervalMs() const { return m_autoIntervalMs; }

    void runText(const QString &text);

signals:
    void autoModeChanged(bool on);
    void translated(const QString &text);
    void failed(const QString &error);

private slots:
    void pollKeys();
    void checkClipboardChange();
    void autoTick();
    void onResult(const QString &text);
    void onError(const QString &error);

private:
    void recreateWorker();
    void trigger();
    void sendCtrlC();
    void sendWmCopy();
    void copyTranslation();
    void finishAutoRead(const QString &cur);
    QString readSelectionUia();
    QString selectionFromElement(IUIAutomationElement *el);
    bool isTerminalFocused() const;

    TranslateBubble *m_bubble;
    QString m_prompt;
    QString m_apiKey;
    QString m_provider = LlmWorker::defaultProviderId();
    bool m_enabled = true;
    bool m_prevCtrlF2 = false;
    bool m_prevAltF2 = false;
    bool m_prevCopy = false;
    bool m_suppressCopy = false;
    bool m_copyInProgress = false;
    QString m_prevClip;
    QString m_pendingPrevClip;
    int m_clipPollTries = 0;
    QString m_lastResult;
    QString m_lastSource;
    int m_copyFlashSeq = 0;
    bool m_autoMode = false;
    int m_autoIntervalMs = 3000;
    QString m_autoPrevClip;
    QTimer m_hotkeyTimer;
    QTimer m_clipPollTimer;
    QTimer m_autoTimer;
    IUIAutomation *m_uia = nullptr;
    QThread m_workerThread;
    LlmWorker *m_worker = nullptr;
};
