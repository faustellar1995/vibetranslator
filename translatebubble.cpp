#include "translatebubble.h"

#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QPainter>
#include <QFont>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kPadH = 30;   // 左右 padding + 边框
constexpr int kPadV = 22;   // 上下 padding + 边框
constexpr int kMaxChars = 4000;
const QColor kFallbackBg(15, 20, 38, 238);
const QColor kFallbackFg(242, 245, 250);
} // namespace

TranslateBubble::TranslateBubble(QWidget *parent)
    : QWidget(parent), m_bgColor(kFallbackBg), m_fgColor(kFallbackFg) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 11));
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &TranslateBubble::tick);
    hide();
}

void TranslateBubble::showStatus(const QString &text, Kind kind) {
    m_text = text.trimmed();
    if (m_text.size() > kMaxChars)
        m_text = m_text.left(kMaxChars) + QStringLiteral("…");

    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect sg = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    const int maxW = qMax(240, qMin(680, sg.width() - 80));
    const int innerW = maxW - kPadH;
    const QRect br = fontMetrics().boundingRect(0, 0, innerW, 1000000,
                                                Qt::TextWordWrap, m_text);
    int w = qMin(maxW, br.width() + kPadH);
    int h = br.height() + kPadV;
    const int maxH = int(sg.height() * 0.75);
    if (h > maxH)
        h = maxH;
    setFixedSize(w, h);

    m_kind = kind;
    m_lastSampleMs = 0;
    applyStyle();
    update();

    const QPoint cursor = QCursor::pos();
    move(clampToScreen(QPoint(cursor.x() + 16, cursor.y() + 22)));
    show();
    raise();
    m_timer.start();
}

void TranslateBubble::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect().adjusted(1, 1, -1, -1);
    p.setPen(QColor(255, 255, 255, 40));
    p.setBrush(m_bgColor);
    p.drawRoundedRect(r, 10, 10);
    p.setPen(m_fgColor);
    p.drawText(r.adjusted(14, 10, -14, -10),
               Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_text);
}

void TranslateBubble::applyStyle() {
    if (m_kind == Result) {
        const QColor bg = adaptiveBackground();
        const QColor fg = adaptiveForeground();
        if (bg != m_bgColor || fg != m_fgColor) {
            m_bgColor = bg;
            m_fgColor = fg;
            update();
        }
    } else {
        switch (m_kind) {
        case Working:
            m_bgColor = QColor(24, 32, 58, 235);
            m_fgColor = QColor(207, 224, 255);
            break;
        case Info:
            m_bgColor = QColor(42, 40, 16, 235);
            m_fgColor = QColor(255, 217, 102);
            break;
        case Error:
            m_bgColor = QColor(72, 18, 26, 235);
            m_fgColor = QColor(255, 157, 157);
            break;
        default:
            break;
        }
        update();
    }
}

QColor TranslateBubble::adaptiveBackground() const {
    const QColor c = sampleScreenColor();
    if (!c.isValid())
        return kFallbackBg;
    return QColor(255 - c.red(), 255 - c.green(), 255 - c.blue(), 235);
}

QColor TranslateBubble::adaptiveForeground() const {
    const QColor c = sampleScreenColor();
    if (!c.isValid())
        return kFallbackFg;
    const QColor inv(255 - c.red(), 255 - c.green(), 255 - c.blue());
    const double lum = 0.299 * inv.red() + 0.587 * inv.green() + 0.114 * inv.blue();
    return lum > 140 ? QColor(17, 19, 24) : QColor(242, 245, 250);
}

QColor TranslateBubble::sampleScreenColor() const {
#ifdef Q_OS_WIN
    QScreen *scr = QGuiApplication::primaryScreen();
    if (!scr)
        return QColor();
    const double dpr = scr->devicePixelRatio();
    const QPoint pt = QCursor::pos();
    const int x = int(pt.x() * dpr);
    const int y = int(pt.y() * dpr);
    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return QColor();
    const COLORREF c = GetPixel(hdc, x, y);
    ReleaseDC(nullptr, hdc);
    if (c == CLR_INVALID)
        return QColor();
    return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
#else
    return QColor();
#endif
}

QPoint TranslateBubble::clampToScreen(const QPoint &p) const {
    QScreen *screen = QGuiApplication::primaryScreen();
    const QRect sg = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    const int x = qMax(sg.left() + 4, qMin(p.x(), sg.right() - width() - 4));
    const int y = qMax(sg.top() + 4, qMin(p.y(), sg.bottom() - height() - 4));
    return QPoint(x, y);
}

bool TranslateBubble::anyMouseButtonDown() const {
#ifdef Q_OS_WIN
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
           (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
           (GetAsyncKeyState(VK_MBUTTON) & 0x8000);
#else
    return false;
#endif
}

void TranslateBubble::tick() {
    if (anyMouseButtonDown()) {
        hide();
        m_timer.stop();
        return;
    }
    const QPoint cursor = QCursor::pos();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // 跟随过程中定期重新取色；气泡盖住鼠标位置时跳过（避免自反馈闪烁）
    if (m_kind == Result && now - m_lastSampleMs > 150 && !geometry().contains(cursor)) {
        m_lastSampleMs = now;
        applyStyle();
    }
    const QPoint target(cursor.x() + 16, cursor.y() + 22);
    const QPoint cur = pos();
    const int nx = cur.x() + int((target.x() - cur.x()) * 0.3);
    const int ny = cur.y() + int((target.y() - cur.y()) * 0.3);
    move(clampToScreen(QPoint(nx, ny)));
}
