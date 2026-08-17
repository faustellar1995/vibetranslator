#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// OpenAI Chat Completions 兼容 LLM 基类（WinHTTP，运行于独立线程）。
// 子类只需提供 host / path / model / 环境变量名等差异。
class LlmWorker : public QObject {
    Q_OBJECT
public:
    explicit LlmWorker(QObject *parent = nullptr);
    ~LlmWorker() override = default;

    static QString defaultProviderId(); // "mimo"
    static QStringList providerIds();
    static QString displayNameOf(const QString &providerId);
    static QString keyEnvNameOf(const QString &providerId);
    static LlmWorker *create(const QString &providerId, QObject *parent = nullptr);

    virtual QString providerId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString keyEnvName() const = 0;

public slots:
    void translate(const QString &text, const QString &systemPrompt, const QString &apiKey);

signals:
    void success(const QString &result);
    void failure(const QString &error);

protected:
    virtual QString model() const = 0;
    virtual QString host() const = 0; // e.g. api.xiaomimimo.com
    virtual QString path() const = 0; // e.g. /v1/chat/completions
};

class MimoLlm : public LlmWorker {
    Q_OBJECT
public:
    using LlmWorker::LlmWorker;
    QString providerId() const override { return QStringLiteral("mimo"); }
    QString displayName() const override { return QStringLiteral("MiMo"); }
    QString keyEnvName() const override { return QStringLiteral("MIMO_KEY"); }

protected:
    QString model() const override { return QStringLiteral("mimo-v2.5"); }
    QString host() const override { return QStringLiteral("api.xiaomimimo.com"); }
    QString path() const override { return QStringLiteral("/v1/chat/completions"); }
};

class DeepSeekLlm : public LlmWorker {
    Q_OBJECT
public:
    using LlmWorker::LlmWorker;
    QString providerId() const override { return QStringLiteral("deepseek"); }
    QString displayName() const override { return QStringLiteral("DeepSeek"); }
    QString keyEnvName() const override { return QStringLiteral("DS_KEY"); }

protected:
    QString model() const override { return QStringLiteral("deepseek-chat"); }
    QString host() const override { return QStringLiteral("api.deepseek.com"); }
    QString path() const override { return QStringLiteral("/chat/completions"); }
};
