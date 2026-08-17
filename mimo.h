#pragma once
#include <QObject>

// Xiaomi MiMo API 请求工作对象：运行在独立线程中，避免阻塞 UI。
// OpenAI Chat Completions 兼容协议；使用 WinHTTP 发起 HTTPS（无需 OpenSSL DLL）。
// 文档：https://mimo.mi.com/docs/en-US/api/guidance/rate-limit
class MimoWorker : public QObject {
    Q_OBJECT
public:
    explicit MimoWorker(QObject *parent = nullptr);
    ~MimoWorker() override = default;

public slots:
    void translate(const QString &text, const QString &systemPrompt, const QString &apiKey);

signals:
    void success(const QString &result);
    void failure(const QString &error);
};
