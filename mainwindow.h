#pragma once
#include <QWidget>
#include <QTimer>
#include "config.h"

class Translator;
class QPlainTextEdit;
class QListWidget;
class QListWidgetItem;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSystemTrayIcon;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(Translator *translator, Config *config);

    void setTray(QSystemTrayIcon *tray) { m_tray = tray; }
    void saveConfig();
    QCheckBox *hotkeyCheck() const { return m_hotkeyCheck; }

protected:
    void closeEvent(QCloseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    void onPromptChanged();
    void applyPreset(QListWidgetItem *item);
    void addPreset();
    void savePreset();
    void delPreset();
    void onHotkeyToggled(bool checked);
    void saveApiKey();
    void testTranslate();

public slots:
    void showHelp();

private:
    void refresh();
    void reloadPresets(const QString &selectName = QString());
    void updateKeyLabel();
    bool presetNameExists(const QString &name) const;

    Translator *m_translator;
    Config *m_config;
    QPlainTextEdit *m_promptEdit;
    QListWidget *m_presetList;
    QCheckBox *m_hotkeyCheck;
    QLabel *m_keyLabel;
    QLineEdit *m_apiKeyEdit;
    QCheckBox *m_showKeyCheck;
    QPushButton *m_saveKeyBtn;
    QSystemTrayIcon *m_tray = nullptr;
    QTimer m_debounce;
    bool m_loading = false;
    qint64 m_lastTitlebarClickMs = 0;
};
