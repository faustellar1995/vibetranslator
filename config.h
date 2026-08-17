#pragma once
#include <QString>
#include <QVector>
#include <QByteArray>

#include "llm.h"

extern const char *kDefaultPrompt;

struct Preset {
    QString name;
    QString prompt;
};

// 按厂商解析 Key：优先对应环境变量（MIMO_KEY / DS_KEY），否则用已保存 Key
inline QString resolveApiKey(const QString &providerId, const QString &savedKey) {
    const QByteArray envName = LlmWorker::keyEnvNameOf(providerId).toUtf8();
    const QByteArray env = qgetenv(envName.constData()).trimmed();
    if (!env.isEmpty())
        return QString::fromUtf8(env);
    return savedKey.trimmed();
}

class Config {
public:
    QString currentPrompt;
    QVector<Preset> presets;
    bool hotkeyEnabled = true;
    QString provider = LlmWorker::defaultProviderId(); // "mimo" | "deepseek"
    QString mimoApiKey;
    QString deepseekApiKey;
    bool autoModeEnabled = false;
    int autoIntervalSec = 3;

    QString &apiKeyRef() {
        return provider == QLatin1String("deepseek") ? deepseekApiKey : mimoApiKey;
    }
    QString apiKey() const {
        return provider == QLatin1String("deepseek") ? deepseekApiKey : mimoApiKey;
    }
    void setApiKey(const QString &key) { apiKeyRef() = key.trimmed(); }

    static Config load();
    void save() const;
};
