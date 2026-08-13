#pragma once
#include <QString>
#include <QVector>
#include <QByteArray>

extern const char *kDefaultPrompt;

struct Preset {
    QString name;
    QString prompt;
};

// 解析实际使用的 API Key：优先环境变量 DS_KEY，否则用已保存的 Key
inline QString resolveApiKey(const QString &savedKey) {
    const QByteArray env = qgetenv("DS_KEY").trimmed();
    if (!env.isEmpty())
        return QString::fromUtf8(env);
    return savedKey.trimmed();
}

class Config {
public:
    QString currentPrompt;
    QVector<Preset> presets;
    bool hotkeyEnabled = true;
    QString apiKey;   // 手动设置的 API Key（留空则使用环境变量 DS_KEY）

    static Config load();
    void save() const;
};
