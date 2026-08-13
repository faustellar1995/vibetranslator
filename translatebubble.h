#pragma once
#include <QWidget>
#include <QTimer>
#include <QColor>

// 半透明、置顶、不拦截鼠标的气泡，跟随鼠标移动，点击后消失。
// 背景与文字用 QPainter 手动绘制（WA_TranslucentBackground 下样式表背景不渲染），
// 翻译结果根据鼠标位置的屏幕背景色自动反色，保证任何背景下都清晰。
class TranslateBubble : public QWidget {
    Q_OBJECT
public:
    enum Kind { Working, Info, Error, Result };

    explicit TranslateBubble(QWidget *parent = nullptr);

    void showStatus(const QString &text, Kind kind = Result);

    Kind kind() const { return m_kind; }
    QColor bgColor() const { return m_bgColor; }
    QColor fgColor() const { return m_fgColor; }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    void applyStyle();
    QColor adaptiveBackground() const;
    QColor adaptiveForeground() const;
    QColor sampleScreenColor() const;
    QPoint clampToScreen(const QPoint &p) const;
    bool anyMouseButtonDown() const;

    QString m_text;
    QColor m_bgColor;
    QColor m_fgColor;
    Kind m_kind = Result;
    QTimer m_timer;
    qint64 m_lastSampleMs = 0;
};
