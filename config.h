#pragma once
#include <QString>
#include <QVector>
#include <QByteArray>

extern const char *kDefaultPrompt;

struct Preset {
    QString name;
    QString prompt;
};

// 解析实际使用的 API Key：优先环境变量 MIMO_KEY，否则用已保存的 Key
inline QString resolveApiKey(const QString &savedKey) {
    const QByteArray env = qgetenv("MIMO_KEY").trimmed();
    if (!env.isEmpty())
        return QString::fromUtf8(env);
    return savedKey.trimmed();
}

class Config {
public:
    QString currentPrompt;
    QVector<Preset> presets;
    bool hotkeyEnabled = true;
    QString apiKey;   // 手动设置的 API Key（留空则使用环境变量 MIMO_KEY）
    bool autoModeEnabled = false;
    int autoIntervalSec = 3;

    static Config load();
    void save() const;
};
