#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QFont>
#include <QColor>
#include <QCoreApplication>
#include <QCheckBox>

#include "config.h"
#include "translatebubble.h"
#include "translator.h"
#include "mainwindow.h"

static QIcon makeTrayIcon() {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(24, 34, 66));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, 64, 64, 14, 14);
    p.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 24, QFont::Bold));
    p.setPen(QColor("#9fc3ff"));
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("译"));
    p.end();
    return QIcon(pm);
}

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    Config config = Config::load();

    TranslateBubble bubble;
    Translator translator(&bubble);
    translator.setPrompt(config.currentPrompt);
    translator.setApiKey(config.apiKey);
    translator.setHotkeyEnabled(config.hotkeyEnabled);

    MainWindow win(&translator, &config);
    win.show();

    QSystemTrayIcon tray(makeTrayIcon());
    tray.setToolTip(QStringLiteral("Qt 翻译助手 — Alt+F2 手动翻译 / Ctrl+F2 自动模式"));
    QMenu menu;
    QAction *actShow = menu.addAction(QStringLiteral("显示主界面"));
    QAction *actHelp = menu.addAction(QStringLiteral("使用帮助"));
    QAction *actToggle = menu.addAction(
        translator.hotkeyEnabled() ? QStringLiteral("停用全局快捷键")
                                   : QStringLiteral("启用全局快捷键"));
    menu.addSeparator();
    QAction *actQuit = menu.addAction(QStringLiteral("退出"));
    tray.setContextMenu(&menu);

    QObject::connect(actShow, &QAction::triggered, &win, [&win]() {
        win.showNormal();
        win.raise();
        win.activateWindow();
    });
    QObject::connect(actHelp, &QAction::triggered, &win, &MainWindow::showHelp);
    QObject::connect(actToggle, &QAction::triggered, [&win, &translator, actToggle]() {
        const bool on = !translator.hotkeyEnabled();
        translator.setHotkeyEnabled(on);
        win.hotkeyCheck()->setChecked(on);
        actToggle->setText(on ? QStringLiteral("停用全局快捷键")
                              : QStringLiteral("启用全局快捷键"));
    });
    QObject::connect(actQuit, &QAction::triggered, [&win]() {
        win.saveConfig();
        QApplication::quit();
    });
    QObject::connect(&tray, &QSystemTrayIcon::activated,
                     [&win](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::DoubleClick) {
                             win.showNormal();
                             win.raise();
                             win.activateWindow();
                         }
                     });
    win.setTray(&tray);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&win]() { win.saveConfig(); });

    if (resolveApiKey(config.apiKey).isEmpty()) {
        QTimer::singleShot(800, [&tray]() {
            tray.showMessage(QStringLiteral("Qt 翻译助手"),
                             QStringLiteral("未设置 API Key：请打开主界面设置，或配置环境变量 DS_KEY"),
                             QSystemTrayIcon::Warning, 5000);
        });
    }

    tray.show();
    return app.exec();
}
