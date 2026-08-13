#pragma once
#include <QObject>

// DeepSeek API 请求工作对象：运行在独立线程中，避免阻塞 UI。
// 使用 Windows 原生 WinHTTP 发起 HTTPS 请求（无需 OpenSSL DLL，便于打包分发）。
class DeepSeekWorker : public QObject {
    Q_OBJECT
public:
    explicit DeepSeekWorker(QObject *parent = nullptr);
    ~DeepSeekWorker() override = default;

public slots:
    void translate(const QString &text, const QString &systemPrompt, const QString &apiKey);

signals:
    void success(const QString &result);
    void failure(const QString &error);
};
