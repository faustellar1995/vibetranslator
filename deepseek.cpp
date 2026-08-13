#include "deepseek.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QByteArray>

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

DeepSeekWorker::DeepSeekWorker(QObject *parent) : QObject(parent) {}

void DeepSeekWorker::translate(const QString &text, const QString &systemPrompt,
                               const QString &apiKey) {
    const QString key = apiKey.trimmed();
    if (key.isEmpty()) {
        emit failure(QStringLiteral("未设置 API Key（请在主界面设置，或配置环境变量 DS_KEY）"));
        return;
    }

    QJsonObject sys;
    sys[QStringLiteral("role")] = QStringLiteral("system");
    sys[QStringLiteral("content")] = systemPrompt;
    QJsonObject usr;
    usr[QStringLiteral("role")] = QStringLiteral("user");
    usr[QStringLiteral("content")] = text;
    QJsonArray msgs;
    msgs.append(sys);
    msgs.append(usr);

    QJsonObject body;
    body[QStringLiteral("model")] = QStringLiteral("deepseek-chat");
    body[QStringLiteral("messages")] = msgs;
    body[QStringLiteral("temperature")] = 0.3;
    body[QStringLiteral("stream")] = false;
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    HINTERNET hSession = WinHttpOpen(L"qt-translator/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        emit failure(QStringLiteral("WinHttpOpen 失败 (错误码 %1)").arg((int)GetLastError()));
        return;
    }
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com",
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        emit failure(QStringLiteral("WinHttpConnect 失败 (错误码 %1)").arg((int)GetLastError()));
        WinHttpCloseHandle(hSession);
        return;
    }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/chat/completions", nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        emit failure(QStringLiteral("WinHttpOpenRequest 失败 (错误码 %1)")
                         .arg((int)GetLastError()));
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }
    WinHttpSetTimeouts(hRequest, 30000, 30000, 30000, 30000);

    const std::wstring authW =
        QStringLiteral("Authorization: Bearer %1").arg(key).toStdWString();
    const std::wstring ctW = L"Content-Type: application/json";
    WinHttpAddRequestHeaders(hRequest, authW.c_str(), (DWORD)authW.size(),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    WinHttpAddRequestHeaders(hRequest, ctW.c_str(), (DWORD)ctW.size(),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    auto fail = [&](const QString &msg) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        emit failure(msg);
    };

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            const_cast<char *>(payload.constData()), (DWORD)payload.size(),
                            (DWORD)payload.size(), 0)) {
        fail(QStringLiteral("WinHttpSendRequest 失败 (错误码 %1)").arg((int)GetLastError()));
        return;
    }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        fail(QStringLiteral("WinHttpReceiveResponse 失败 (错误码 %1)").arg((int)GetLastError()));
        return;
    }

    DWORD status = 0;
    DWORD statusLen = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen,
                        WINHTTP_NO_HEADER_INDEX);

    std::string respBody;
    DWORD avail = 0;
    do {
        avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail))
            break;
        if (avail == 0)
            break;
        std::vector<char> buf(avail);
        DWORD got = 0;
        if (!WinHttpReadData(hRequest, buf.data(), avail, &got))
            break;
        respBody.append(buf.data(), got);
    } while (avail > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    const QByteArray bodyBytes(respBody.data(), (int)respBody.size());
    if (status != 200) {
        emit failure(QStringLiteral("API 错误 HTTP %1: %2")
                         .arg(status).arg(QString::fromUtf8(bodyBytes.left(300))));
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes, &err);
    if (err.error != QJsonParseError::NoError) {
        emit failure(QStringLiteral("响应解析失败: %1")
                         .arg(QString::fromUtf8(bodyBytes.left(300))));
        return;
    }
    const QJsonArray choices =
        doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit failure(QStringLiteral("响应解析失败: %1")
                         .arg(QString::fromUtf8(bodyBytes.left(300))));
        return;
    }
    emit success(choices.first()
                     .toObject()
                     .value(QStringLiteral("message"))
                     .toObject()
                     .value(QStringLiteral("content"))
                     .toString()
                     .trimmed());
}
