#pragma once
#include <QObject>
#include <QTimer>
#include <QThread>
#include "translatebubble.h"
#include "deepseek.h"
#include "config.h"

// 翻译控制器：全局快捷键 Ctrl+F2 / Alt+F2 + 剪贴板 + DeepSeek 请求（工作线程）
// 气泡显示翻译结果时，按 Ctrl+C 自动复制译文
class Translator : public QObject {
    Q_OBJECT
public:
    explicit Translator(TranslateBubble *bubble, QObject *parent = nullptr);
    ~Translator() override;

    void setPrompt(const QString &p) { m_prompt = p; }
    void setApiKey(const QString &k) { m_apiKey = k.trimmed(); }
    void setHotkeyEnabled(bool on);
    bool hotkeyEnabled() const { return m_enabled; }

    void setAutoMode(bool on);
    void setAutoIntervalMs(int ms);
    bool autoMode() const { return m_autoMode; }
    int autoIntervalMs() const { return m_autoIntervalMs; }

    void runText(const QString &text);

signals:
    void translated(const QString &text);
    void failed(const QString &error);

private slots:
    void pollKeys();
    void checkClipboardChange();
    void autoTick();
    void onResult(const QString &text);
    void onError(const QString &error);

private:
    void trigger();
    void sendCtrlC();
    void copyTranslation();

    TranslateBubble *m_bubble;
    QString m_prompt;
    QString m_apiKey;
    bool m_enabled = true;
    bool m_prevHotkey = false;
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
    QThread m_workerThread;
    DeepSeekWorker *m_worker = nullptr;
};
