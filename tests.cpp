#include <cstdio>
// 控制台测试程序：验证配置/气泡/控制器/主界面/MiMo API
#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QScreen>
#include <QImage>
#include <QCheckBox>
#include <QFile>

#include "config.h"
#include "mimo.h"
#include "translatebubble.h"
#include "translator.h"
#include "mainwindow.h"

static int g_fail = 0;
static FILE *g_out = nullptr;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (cond) {                                                                    \
            std::fprintf(g_out, "  ok: %s\n", #cond);                                 \
        } else {                                                                       \
            std::fprintf(g_out, "  FAIL: %s @line %d\n", #cond, __LINE__);            \
            ++g_fail;                                                                  \
        }                                                                              \
    } while (0)
#define LOG(...) std::fprintf(g_out, __VA_ARGS__)

int main(int argc, char *argv[]) {
    g_out = fopen("test_results.txt", "w");
    if (!g_out)
        g_out = stderr;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
#endif
    LOG("TESTS START\n");
    fflush(g_out);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    // 清掉历史配置文件，保证从默认值开始测试
    QFile::remove(QCoreApplication::applicationDirPath() +
                  QStringLiteral("/translator_config.json"));

    LOG("== Config ==\n");
    Config c = Config::load();
    CHECK(c.currentPrompt == QString::fromUtf8(kDefaultPrompt));
    CHECK(c.presets.size() == 4);
    c.save();
    Config c2 = Config::load();
    CHECK(c2.presets.size() == 4);
    CHECK(c2.hotkeyEnabled == true);

    // API Key：存取 + MIMO_KEY 优先级
    c2.apiKey = QStringLiteral("sk-saved-key-123456");
    c2.save();
    Config c3 = Config::load();
    CHECK(c3.apiKey == QStringLiteral("sk-saved-key-123456"));
    const QByteArray savedEnv = qgetenv("MIMO_KEY");
    qunsetenv("MIMO_KEY");
    CHECK(resolveApiKey(QStringLiteral("sk-saved-key-123456")) ==
          QStringLiteral("sk-saved-key-123456")); // 无环境变量时用已保存 Key
    CHECK(resolveApiKey(QString()).isEmpty());
    if (!savedEnv.isEmpty())
        qputenv("MIMO_KEY", savedEnv);
    CHECK(!resolveApiKey(QString()).isEmpty()); // 有 MIMO_KEY 时优先使用（本机已设置）
    c2.apiKey.clear();
    c2.save();

    // 自动模式配置存取
    CHECK(c2.autoModeEnabled == false);
    CHECK(c2.autoIntervalSec == 3);
    c2.autoModeEnabled = true;
    c2.autoIntervalSec = 5;
    c2.save();
    Config c4 = Config::load();
    CHECK(c4.autoModeEnabled == true);
    CHECK(c4.autoIntervalSec == 5);
    c2.autoModeEnabled = false;
    c2.autoIntervalSec = 3;
    c2.save();

    LOG("== TranslateBubble ==\n");
    TranslateBubble bubble;
    bubble.showStatus(QStringLiteral("测试文本 ") + QString(20, QLatin1Char('x')),
                      TranslateBubble::Result);
    QCoreApplication::processEvents();
    CHECK(bubble.isVisible());
    CHECK(bubble.size().width() > 0 && bubble.size().height() > 0);
    const QImage img = bubble.grab().toImage();
    const QColor px = img.pixelColor(img.width() / 2, img.height() / 2);
    CHECK(px.alpha() > 0); // 背景确实被绘制（非全透明）
    LOG("  bubble bg=%s fg=%s\n", bubble.bgColor().name().toUtf8().constData(),
        bubble.fgColor().name().toUtf8().constData());

    LOG("== Translator 快捷键开关 ==\n");
    Translator tr(&bubble);
    tr.setHotkeyEnabled(false);
    CHECK(!tr.hotkeyEnabled());
    tr.setHotkeyEnabled(true);
    CHECK(tr.hotkeyEnabled());

    LOG("== 自动模式 ==\n");
    bool sigGot = false;
    bool sigVal = false;
    QObject::connect(&tr, &Translator::autoModeChanged, [&](bool on) {
        sigGot = true;
        sigVal = on;
    });
    tr.setAutoMode(true);
    CHECK(tr.autoMode());
    CHECK(sigGot && sigVal); // 开关状态变化应发出信号
    tr.setAutoIntervalMs(2500);
    CHECK(tr.autoIntervalMs() == 2500);
    sigGot = false;
    tr.setAutoMode(false);
    CHECK(!tr.autoMode());
    CHECK(sigGot && !sigVal);

    LOG("== MainWindow ==\n");
    MainWindow win(&tr, &c2);
    win.show();
    QCoreApplication::processEvents();
    win.hotkeyCheck()->setChecked(false);
    CHECK(!tr.hotkeyEnabled());
    win.hotkeyCheck()->setChecked(true);
    CHECK(tr.hotkeyEnabled());
    win.close();
    QCoreApplication::processEvents();
    CHECK(!win.isVisible()); // 关闭应隐藏而非退出

    LOG("== MiMo API (network) ==\n");
    MimoWorker worker;
    QEventLoop loop;
    bool apiDone = false;
    bool apiOk = false;
    QObject::connect(&worker, &MimoWorker::success, [&](const QString &r) {
        LOG("  API result: %s\n", r.toUtf8().constData());
        apiOk = true;
        apiDone = true;
        loop.quit();
    });
    QObject::connect(&worker, &MimoWorker::failure, [&](const QString &e) {
        LOG("  API failure: %s\n", e.toUtf8().constData());
        apiDone = true;
        loop.quit();
    });
    QTimer::singleShot(40000, &loop, &QEventLoop::quit);
    worker.translate(QStringLiteral("Hello, world! This is a quick translation test."),
                     QString::fromUtf8(kDefaultPrompt), resolveApiKey(QString()));
    loop.exec();
    CHECK(apiDone && apiOk);

    LOG("%s failures=%d\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", g_fail);
    fflush(g_out);
    fclose(g_out);
    return g_fail == 0 ? 0 : 1;
}
