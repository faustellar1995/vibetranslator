#include "config.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

const char *kDefaultPrompt =
    "你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文（默认英译中）。\n"
    "要求：\n"
    "1. 只输出译文本身，不要任何解释、注释、前言或原文；\n"
    "2. 保持原文的语气与格式，专业术语翻译准确；\n"
    "3. 若原文已是中文，润色后直接返回。";

static const Preset kDefaultPresets[] = {
    {QStringLiteral("英译中"),
     QStringLiteral("你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文（默认英译中）。只输出译文本身，不要任何解释、注释、前言或原文；保持原文语气与格式，专业术语准确；若原文已是中文，润色后直接返回。")},
    {QStringLiteral("中译英"),
     QStringLiteral("You are a professional translation engine. Translate the user's content into English. Output only the translation with no explanation, notes, preface or the original text. Keep the original tone and formatting; use accurate technical terms.")},
    {QStringLiteral("日译中"),
     QStringLiteral("你是一位专业的翻译引擎。请把用户提供的内容翻译成简体中文。只输出译文本身，不要任何解释、注释、前言或原文；保持原文语气与格式。")},
    {QStringLiteral("精简概括"),
     QStringLiteral("请用简体中文对用户提供的内容做简明扼要的概括，不超过 100 字，直接输出概括结果，不要任何前缀或解释。")},
};

static QString configPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/translator_config.json");
}

Config Config::load() {
    Config c;
    c.currentPrompt = QString::fromUtf8(kDefaultPrompt);
    for (const Preset &p : kDefaultPresets)
        c.presets.append(p);

    QFile f(configPath());
    if (f.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        f.close();
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            const QString cp = o.value(QStringLiteral("current_prompt")).toString().trimmed();
            if (!cp.isEmpty())
                c.currentPrompt = cp;
            if (o.value(QStringLiteral("presets")).isArray()) {
                QVector<Preset> ps;
                for (const QJsonValue &v : o.value(QStringLiteral("presets")).toArray()) {
                    const QJsonObject po = v.toObject();
                    Preset p;
                    p.name = po.value(QStringLiteral("name")).toString();
                    p.prompt = po.value(QStringLiteral("prompt")).toString();
                    if (!p.name.isEmpty())
                        ps.append(p);
                }
                if (!ps.isEmpty())
                    c.presets = ps;
            }
            c.hotkeyEnabled = o.value(QStringLiteral("hotkey_enabled")).toBool(true);
            c.apiKey = o.value(QStringLiteral("api_key")).toString();
        }
    }
    return c;
}

void Config::save() const {
    QJsonObject o;
    o[QStringLiteral("current_prompt")] = currentPrompt;
    QJsonArray arr;
    for (const Preset &p : presets) {
        QJsonObject po;
        po[QStringLiteral("name")] = p.name;
        po[QStringLiteral("prompt")] = p.prompt;
        arr.append(po);
    }
    o[QStringLiteral("presets")] = arr;
    o[QStringLiteral("hotkey_enabled")] = hotkeyEnabled;
    o[QStringLiteral("api_key")] = apiKey;

    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        f.close();
    } else {
        qWarning() << "无法写入配置文件:" << configPath();
    }
}
