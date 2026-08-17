#include "mainwindow.h"
#include "translator.h"

#include <QPlainTextEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QDateTime>
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
const char *kAppName = "Qt 翻译助手";
} // namespace

MainWindow::MainWindow(Translator *translator, Config *config)
    : QWidget(nullptr), m_translator(translator), m_config(config) {
    m_loading = true;
    setWindowTitle(QString::fromUtf8(kAppName));
    setMinimumSize(560, 500);
    resize(660, 540);

    auto *root = new QVBoxLayout(this);

    auto *tip = new QLabel(QStringLiteral(
        "系统提示词 Prompt —— 决定翻译方向/风格（默认为英译中），修改后自动保存"));
    tip->setStyleSheet(QStringLiteral("color:#888;"));
    root->addWidget(tip);

    m_promptEdit = new QPlainTextEdit;
    m_promptEdit->setPlaceholderText(QStringLiteral("在这里输入系统提示词…"));
    QFont f = m_promptEdit->font();
    f.setPointSize(11);
    m_promptEdit->setFont(f);
    root->addWidget(m_promptEdit, 3);

    root->addWidget(new QLabel(QStringLiteral("已保存的预设（点击应用）：")));

    auto *presetRow = new QHBoxLayout;
    m_presetList = new QListWidget;
    connect(m_presetList, &QListWidget::itemClicked, this, &MainWindow::applyPreset);
    presetRow->addWidget(m_presetList, 3);

    auto *btnCol = new QVBoxLayout;
    auto *btnAdd = new QPushButton(QStringLiteral("添加预设"));
    auto *btnSave = new QPushButton(QStringLiteral("保存为预设"));
    auto *btnDel = new QPushButton(QStringLiteral("删除预设"));
    auto *btnTest = new QPushButton(QStringLiteral("测试翻译"));
    for (auto *b : {btnAdd, btnSave, btnDel, btnTest})
        b->setMinimumHeight(34);
    btnCol->addWidget(btnAdd);
    btnCol->addWidget(btnSave);
    btnCol->addWidget(btnDel);
    btnCol->addWidget(btnTest);
    btnCol->addStretch(1);
    presetRow->addLayout(btnCol, 2);
    root->addLayout(presetRow, 2);

    auto *hkRow = new QHBoxLayout;
    m_hotkeyCheck = new QCheckBox(
        QStringLiteral("启用快捷键（Alt+F2 手动翻译 / Ctrl+C 复制译文）"));
    m_hotkeyCheck->setChecked(m_config->hotkeyEnabled);
    hkRow->addWidget(m_hotkeyCheck);
    hkRow->addStretch(1);
    root->addLayout(hkRow);

    // 自动模式行
    auto *autoRow = new QHBoxLayout;
    m_autoModeCheck = new QCheckBox(QStringLiteral("自动模式（Ctrl+F2 开关）"));
    m_autoModeCheck->setChecked(m_config->autoModeEnabled);
    m_autoModeCheck->setToolTip(QStringLiteral(
        "每隔设定秒数非侵入式轮询当前选中文字（不干扰命令行），与上次翻译原文不同时自动翻译；\n"
        "自动模式下手动快捷键 Alt+F2 不可用"));
    autoRow->addWidget(m_autoModeCheck);
    autoRow->addWidget(new QLabel(QStringLiteral("每")));
    m_autoSpin = new QSpinBox;
    m_autoSpin->setRange(1, 60);
    m_autoSpin->setValue(m_config->autoIntervalSec);
    m_autoSpin->setSuffix(QStringLiteral(" 秒"));
    autoRow->addWidget(m_autoSpin);
    autoRow->addWidget(new QLabel(QStringLiteral("轮询选中内容，变化时自动翻译")));
    autoRow->addStretch(1);
    root->addLayout(autoRow);

    // 厂商 + API Key
    auto *provRow = new QHBoxLayout;
    provRow->addWidget(new QLabel(QStringLiteral("模型厂商:")));
    m_providerCombo = new QComboBox;
    for (const QString &id : LlmWorker::providerIds())
        m_providerCombo->addItem(LlmWorker::displayNameOf(id), id);
    {
        const int idx = m_providerCombo->findData(m_config->provider);
        m_providerCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    provRow->addWidget(m_providerCombo);
    provRow->addStretch(1);
    root->addLayout(provRow);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(new QLabel(QStringLiteral("API Key:")));
    m_apiKeyEdit = new QLineEdit;
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    syncKeyEditFromConfig();
    keyRow->addWidget(m_apiKeyEdit, 3);
    m_showKeyCheck = new QCheckBox(QStringLiteral("显示"));
    m_showKeyCheck->setChecked(false);
    keyRow->addWidget(m_showKeyCheck);
    m_saveKeyBtn = new QPushButton(QStringLiteral("保存"));
    m_saveKeyBtn->setMinimumHeight(28);
    keyRow->addWidget(m_saveKeyBtn);
    root->addLayout(keyRow);

    auto *bottom = new QHBoxLayout;
    m_keyLabel = new QLabel;
    bottom->addWidget(m_keyLabel);
    bottom->addStretch(1);
    auto *btnHelp = new QPushButton(QStringLiteral("帮助"));
    connect(btnHelp, &QPushButton::clicked, this, &MainWindow::showHelp);
    bottom->addWidget(btnHelp);
    auto *btnHide = new QPushButton(QStringLiteral("隐藏到托盘"));
    connect(btnHide, &QPushButton::clicked, this, &QWidget::hide);
    bottom->addWidget(btnHide);
    root->addLayout(bottom);

    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addPreset);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::savePreset);
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::delPreset);
    connect(btnTest, &QPushButton::clicked, this, &MainWindow::testTranslate);
    connect(m_promptEdit, &QPlainTextEdit::textChanged, this, &MainWindow::onPromptChanged);
    connect(m_hotkeyCheck, &QCheckBox::toggled, this, &MainWindow::onHotkeyToggled);
    connect(m_autoModeCheck, &QCheckBox::toggled, this, &MainWindow::onAutoModeToggled);
    connect(m_translator, &Translator::autoModeChanged, this, [this](bool on) {
        m_autoModeCheck->setChecked(on);
    });
    connect(m_autoSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onAutoIntervalChanged);
    connect(m_showKeyCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_apiKeyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProviderChanged);
    connect(m_saveKeyBtn, &QPushButton::clicked, this, &MainWindow::saveApiKey);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(800);
    connect(&m_debounce, &QTimer::timeout, this, &MainWindow::saveConfig);

    refresh();
    m_loading = false;
    m_translator->setAutoMode(m_config->autoModeEnabled);
    m_translator->setAutoIntervalMs(m_config->autoIntervalSec * 1000);
}

void MainWindow::refresh() {
    m_presetList->clear();
    for (const Preset &p : m_config->presets) {
        auto *it = new QListWidgetItem(p.name);
        it->setData(Qt::UserRole, p.prompt);
        m_presetList->addItem(it);
    }
    m_promptEdit->setPlainText(m_config->currentPrompt);
    m_presetList->setCurrentRow(-1);
    for (int i = 0; i < m_presetList->count(); ++i) {
        if (m_presetList->item(i)->data(Qt::UserRole).toString() == m_config->currentPrompt) {
            m_presetList->setCurrentItem(m_presetList->item(i));
            break;
        }
    }
    updateKeyLabel();
}

void MainWindow::reloadPresets(const QString &selectName) {
    m_presetList->clear();
    for (const Preset &p : m_config->presets) {
        auto *it = new QListWidgetItem(p.name);
        it->setData(Qt::UserRole, p.prompt);
        m_presetList->addItem(it);
    }
    if (!selectName.isEmpty()) {
        for (int i = 0; i < m_presetList->count(); ++i) {
            if (m_presetList->item(i)->text() == selectName) {
                m_presetList->setCurrentItem(m_presetList->item(i));
                break;
            }
        }
    }
}

void MainWindow::syncKeyEditFromConfig() {
    const QString envName = LlmWorker::keyEnvNameOf(m_config->provider);
    m_apiKeyEdit->setPlaceholderText(
        QStringLiteral("sk-… 留空则使用环境变量 %1").arg(envName));
    m_apiKeyEdit->setText(m_config->apiKey());
}

void MainWindow::updateKeyLabel() {
    const QString envName = LlmWorker::keyEnvNameOf(m_config->provider);
    const QByteArray env = qgetenv(envName.toUtf8().constData()).trimmed();
    const QString saved = m_config->apiKey().trimmed();
    const QString vendor = LlmWorker::displayNameOf(m_config->provider);
    if (!env.isEmpty()) {
        m_keyLabel->setText(
            QStringLiteral("%1 · %2 环境变量已设置 ✓（优先使用）").arg(vendor, envName));
        m_keyLabel->setStyleSheet(QStringLiteral("color:#3c9e4a;"));
    } else if (!saved.isEmpty()) {
        m_keyLabel->setText(QStringLiteral("%1 · 使用已保存的 API Key（…%2）")
                                .arg(vendor, saved.right(6)));
        m_keyLabel->setStyleSheet(QStringLiteral("color:#3c9e4a;"));
    } else {
        m_keyLabel->setText(
            QStringLiteral("⚠ %1：请输入并保存 Key，或配置环境变量 %2")
                .arg(vendor, envName));
        m_keyLabel->setStyleSheet(QStringLiteral("color:#c0392b;"));
    }
}

void MainWindow::saveConfig() {
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    m_config->currentPrompt = prompt;
    m_config->hotkeyEnabled = m_translator->hotkeyEnabled();
    m_config->autoModeEnabled = m_translator->autoMode();
    m_config->autoIntervalSec = qMax(1, m_translator->autoIntervalMs() / 1000);
    m_config->provider = m_translator->provider();
    m_config->save();
}

void MainWindow::onPromptChanged() {
    if (m_loading)
        return;
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    m_translator->setPrompt(prompt);
    m_debounce.start();
}

void MainWindow::onHotkeyToggled(bool checked) {
    m_translator->setHotkeyEnabled(checked);
    saveConfig();
}

void MainWindow::onAutoModeToggled(bool checked) {
    m_translator->setAutoMode(checked);
    saveConfig();
}

void MainWindow::onAutoIntervalChanged(int sec) {
    m_translator->setAutoIntervalMs(sec * 1000);
    saveConfig();
}

void MainWindow::onProviderChanged(int index) {
    if (m_loading || index < 0)
        return;
    // 切换前把当前输入框内容写回对应厂商 Key（未点保存也能保留）
    m_config->setApiKey(m_apiKeyEdit->text().trimmed());
    const QString id = m_providerCombo->itemData(index).toString();
    m_config->provider = id;
    m_translator->setProvider(id);
    syncKeyEditFromConfig();
    m_translator->setApiKey(m_config->apiKey());
    saveConfig();
    updateKeyLabel();
}

void MainWindow::saveApiKey() {
    m_config->setApiKey(m_apiKeyEdit->text().trimmed());
    m_translator->setApiKey(m_config->apiKey());
    saveConfig();
    updateKeyLabel();
    m_saveKeyBtn->setText(QStringLiteral("已保存 ✓"));
    QTimer::singleShot(1200, this, [this]() { m_saveKeyBtn->setText(QStringLiteral("保存")); });
}

void MainWindow::applyPreset(QListWidgetItem *item) {
    QString prompt = item->data(Qt::UserRole).toString();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    m_loading = true;
    m_promptEdit->setPlainText(prompt);
    m_loading = false;
    m_translator->setPrompt(prompt);
    saveConfig();
}

void MainWindow::savePreset() {
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    QString name;
    if (QListWidgetItem *item = m_presetList->currentItem()) {
        name = item->text();
    } else {
        bool ok = false;
        name = QInputDialog::getText(this, QStringLiteral("保存预设"),
                                     QStringLiteral("预设名称："), QLineEdit::Normal,
                                     QStringLiteral("自定义"), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        name = name.trimmed();
    }
    bool found = false;
    for (Preset &p : m_config->presets) {
        if (p.name == name) {
            p.prompt = prompt;
            found = true;
            break;
        }
    }
    if (!found)
        m_config->presets.append({name, prompt});
    reloadPresets(name);
    saveConfig();
}

void MainWindow::addPreset() {
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("添加预设"),
                                         QStringLiteral("预设名称："), QLineEdit::Normal,
                                         QStringLiteral("自定义"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    name = name.trimmed();
    // 重名自动加序号，保证预设唯一
    const QString base = name;
    int n = 2;
    while (presetNameExists(name))
        name = QStringLiteral("%1 (%2)").arg(base).arg(n++);
    m_config->presets.append({name, prompt});
    reloadPresets(name);
    saveConfig();
}

bool MainWindow::presetNameExists(const QString &name) const {
    for (const Preset &p : m_config->presets)
        if (p.name == name)
            return true;
    return false;
}

void MainWindow::delPreset() {
    QListWidgetItem *item = m_presetList->currentItem();
    if (!item)
        return;
    const QString name = item->text();
    QVector<Preset> kept;
    for (const Preset &p : m_config->presets)
        if (p.name != name)
            kept.append(p);
    m_config->presets = kept;
    reloadPresets();
    saveConfig();
}

void MainWindow::testTranslate() {
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty())
        prompt = QString::fromUtf8(kDefaultPrompt);
    m_translator->setPrompt(prompt);
    m_translator->runText(QStringLiteral(
        "Hello, world! This is a quick translation test. "
        "The quick brown fox jumps over the lazy dog."));
}

void MainWindow::showHelp() {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("使用帮助"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<h3>Qt 翻译助手 — 使用说明</h3>"
        "<p><b>快速翻译</b></p>"
        "<ul>"
        "<li>选中任意窗口中的文字，按 <b>Alt+F2</b> 立即翻译（选中优先，无选中时自动翻译剪贴板内容）。</li>"
        "<li>译文以半透明气泡<b>跟随鼠标</b>移动，气泡颜色会随背景自动反色，"
        "任意<b>点击鼠标</b>即可关闭气泡。</li>"
        "<li>气泡显示译文时按 <b>Ctrl+C</b>，自动把译文复制到剪贴板，可直接粘贴。</li>"
        "<li><b>Ctrl+F2</b>：开关<b>自动模式</b>。启用后每隔设定秒数（默认 3s）非侵入式轮询当前选中文字，"
        "与上次翻译的原文不同时自动翻译，无需按键；自动模式下手动快捷键 Alt+F2 不可用。</li>"
        "</ul>"
        "<p><b>模型厂商</b></p>"
        "<ul>"
        "<li>支持 <b>MiMo</b>（默认，模型 mimo-v2.5）与 <b>DeepSeek</b>（模型 deepseek-chat），可在主界面下拉切换。</li>"
        "<li>MiMo 优先环境变量 <b>MIMO_KEY</b>；DeepSeek 优先 <b>DS_KEY</b>；也可分别保存 Key。</li>"
        "</ul>"
        "<p><b>Prompt 与预设</b></p>"
        "<ul>"
        "<li>系统提示词决定翻译方向（默认为英译中），修改后<b>自动保存</b>。</li>"
        "<li>「添加预设」：以新名称保存当前提示词；「保存为预设」：更新选中的预设；「删除预设」：移除选中预设。</li>"
        "<li>点击预设列表项即可应用对应提示词。</li>"
        "</ul>"
        "<p><b>托盘</b></p>"
        "<ul>"
        "<li>关闭窗口即最小化到托盘；托盘菜单可显示主界面、开关快捷键、使用帮助、退出。</li>"
        "</ul>"));
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // 只有"用户主动关闭"（点 X / Alt+F4）走到这里：隐藏到托盘
    event->ignore();
    hide();
    if (m_tray) {
        m_tray->showMessage(QString::fromUtf8(kAppName),
                            QStringLiteral("已最小化到托盘，Alt+F2 手动翻译 / Ctrl+F2 自动模式"),
                            QSystemTrayIcon::Information, 2500);
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCLBUTTONDOWN) {
            m_lastTitlebarClickMs = QDateTime::currentMSecsSinceEpoch();
        } else if (msg->message == WM_CLOSE) {
            // 用户点 X（先有标题栏点击）或 Alt+F4 => 交给 closeEvent 隐藏到托盘
            // 外部关闭请求（taskkill / 任务管理器 / 结束会话）=> 真正退出
            const bool userClick =
                (QDateTime::currentMSecsSinceEpoch() - m_lastTitlebarClickMs < 1000);
            const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (userClick || altDown)
                return false;
            QApplication::quit();
            *result = 0;
            return true;
        } else if (msg->message == WM_QUERYENDSESSION || msg->message == WM_ENDSESSION) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            *result = 1;
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
