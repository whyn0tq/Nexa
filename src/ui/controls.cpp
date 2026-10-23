#include "controls.h"

#include "glass.h"
#include "theme.h"

#include <QEnterEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QPainterPath>
#include <QMenu>
#include <QPushButton>

#include <cmath>

namespace {
QString withoutShortcut(const QString &tooltip)
{
    const qsizetype open = tooltip.lastIndexOf(QStringLiteral(" ("));
    if (open > 0 && tooltip.endsWith(QLatin1Char(')')))
        return tooltip.left(open);
    return tooltip;
}
}

IconButton::IconButton(Icon icon, const QString &tooltip, QWidget *parent)
    : QAbstractButton(parent), m_icon(icon)
{
    setToolTip(tooltip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
}

bool IconButton::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTipChange)
        setAccessibleName(withoutShortcut(toolTip()));
    return QAbstractButton::event(event);
}

void IconButton::focusInEvent(QFocusEvent *event)
{
    const Qt::FocusReason reason = event->reason();
    m_keyboardFocus = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason ||
                      reason == Qt::ShortcutFocusReason;
    update();
    QAbstractButton::focusInEvent(event);
}

void IconButton::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    update();
    QAbstractButton::focusOutEvent(event);
}

void IconButton::setIcon(Icon icon)
{
    m_icon = icon;
    update();
}

void IconButton::setStyle(Style style)
{
    m_style = style;
    update();
}

void IconButton::setIconSize(int size)
{
    m_iconSize = size;
    updateGeometry();
    update();
}

void IconButton::setForcedHover(bool hovered)
{
    if (m_forcedHover == hovered)
        return;
    m_forcedHover = hovered;
    update();
}

void IconButton::setForcedPressed(bool pressed)
{
    if (m_forcedPressed == pressed)
        return;
    m_forcedPressed = pressed;
    update();
}

void IconButton::setBadge(int count)
{
    m_badge = count;
    update();
}

QSize IconButton::sizeHint() const
{
    return {m_iconSize + 16, m_iconSize + 16};
}

void IconButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void IconButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool hovered = (m_hovered || m_forcedHover) && isEnabled();
    const bool pressed = (isDown() || m_forcedPressed) && isEnabled();
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = std::min<qreal>(8, r.height() / 2.6);

    QColor fill = Qt::transparent;
    QColor stroke = Qt::transparent;
    QColor glyph = isEnabled() ? Theme::TextSecondary : Theme::TextFaint;
    switch (m_style) {
    case Style::Ghost:
        if (hovered || isChecked()) {
            fill = pressed ? QColor(255, 255, 255, 8) : QColor(255, 255, 255, 14);
            glyph = Theme::Text;
        }
        break;
    case Style::Glass:
        fill = hovered ? Theme::SurfaceHover : Theme::Surface;
        stroke = hovered ? QColor(46, 46, 50) : QColor(39, 39, 41);
        if (hovered)
            glyph = Theme::Text;
        break;
    case Style::Accent:
        fill = hovered ? Theme::AccentHover : Theme::Accent;
        if (pressed)
            fill = Theme::AccentDeep;
        glyph = Qt::white;
        break;
    case Style::Danger:
        if (hovered) {
            fill = pressed ? QColor(161, 40, 41) : QColor(218, 55, 60);
            glyph = Qt::white;
        }
        break;
    }
    if (isChecked() && m_style == Style::Ghost) {
        fill = Theme::AccentSoft;
        glyph = Theme::AccentText;
    }
    painter.setPen(stroke.alpha() ? QPen(stroke, 1) : Qt::NoPen);
    painter.setBrush(fill);
    if (fill.alpha() || stroke.alpha())
        painter.drawRoundedRect(r, radius, radius);
    const qreal size = m_iconSize;
    const QRectF iconRect((width() - size) / 2.0, (height() - size) / 2.0, size, size);
    Icons::paint(painter, m_icon, iconRect, glyph);
    if (m_keyboardFocus && hasFocus()) {
        painter.setPen(QPen(Theme::Accent, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), radius, radius);
    }
    if (m_badge > 0) {
        const QString text = m_badge > 99 ? QStringLiteral("99+") : QString::number(m_badge);
        painter.setFont(Theme::font(12, QFont::Bold));
        const qreal w = std::max<qreal>(17, painter.fontMetrics().horizontalAdvance(text) + 8);
        const QRectF badge(width() - w, 0, w, 17);
        painter.setPen(QPen(Theme::Base, 1.5));
        painter.setBrush(Theme::Accent);
        painter.drawRoundedRect(badge, 8.5, 8.5);
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, text);
    }
}

Segmented::Segmented(const QStringList &options, QWidget *parent)
    : QWidget(parent), m_options(options), m_enabled(options.size(), true)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFont(Theme::font(13, QFont::DemiBold));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Segmented::setCurrent(int index)
{
    if (index == m_current || index < 0 || index >= m_options.size())
        return;
    m_current = index;
    update();
    emit changed(index);
}

void Segmented::setOptionEnabled(int index, bool enabled)
{
    if (index >= 0 && index < m_enabled.size()) {
        m_enabled[index] = enabled;
        update();
    }
}

void Segmented::setCounts(const QVector<int> &counts, int alertIndex)
{
    if (counts == m_counts && alertIndex == m_alert)
        return;
    m_counts = counts;
    m_alert = alertIndex;
    updateGeometry();
    update();
}

QSize Segmented::sizeHint() const
{
    const QFontMetrics countMetrics(Theme::font(12, QFont::Bold));
    int width = 8;
    for (int i = 0; i < m_options.size(); ++i) {
        width += fontMetrics().horizontalAdvance(m_options.at(i)) + 32;
        if (m_counts.value(i) > 0)
            width += countMetrics.horizontalAdvance(QString::number(m_counts.value(i))) + 6;
    }
    return {width, 38};
}

int Segmented::indexAt(const QPoint &point) const
{
    if (m_options.isEmpty())
        return -1;
    const qreal segment = (width() - 8.0) / m_options.size();
    const int index = static_cast<int>((point.x() - 4) / segment);
    return index >= 0 && index < m_options.size() ? index : -1;
}

void Segmented::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(Theme::Stroke, 1));
    painter.setBrush(QColor(255, 255, 255, 10));
    painter.drawRoundedRect(r, 11, 11);
    const qreal segment = (width() - 8.0) / m_options.size();
    for (int i = 0; i < m_options.size(); ++i) {
        const QRectF cell(4 + i * segment, 4, segment, height() - 8);
        if (i == m_current) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::Accent);
            painter.drawRoundedRect(cell, 8, 8);
        } else if (i == m_hovered && m_enabled.at(i)) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 14));
            painter.drawRoundedRect(cell, 8, 8);
        }
        const QColor tone = i == m_current ? QColor(Qt::white)
                            : m_enabled.at(i) ? Theme::TextSecondary
                                              : Theme::TextFaint;
        const int count = m_counts.value(i);
        if (count <= 0) {
            painter.setPen(tone);
            painter.drawText(cell, Qt::AlignCenter, m_options.at(i));
            continue;
        }
        const QFont countFont = Theme::font(12, QFont::Bold);
        const QString number = QString::number(count);
        const qreal labelWidth = fontMetrics().horizontalAdvance(m_options.at(i));
        const qreal numberWidth = QFontMetricsF(countFont).horizontalAdvance(number);
        const qreal gap = 6;
        const qreal x = cell.center().x() - (labelWidth + gap + numberWidth) / 2;
        painter.setPen(tone);
        painter.drawText(QRectF(x, cell.top(), labelWidth + 1, cell.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_options.at(i));
        painter.setFont(countFont);
        painter.setPen(i == m_current ? Theme::withAlpha(Qt::white, 200)
                       : i == m_alert ? Theme::Danger
                                      : Theme::TextMuted);
        painter.drawText(QRectF(x + labelWidth + gap, cell.top(), numberWidth + 1, cell.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, number);
        painter.setFont(font());
    }
}

void Segmented::mousePressEvent(QMouseEvent *event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0 && m_enabled.at(index))
        setCurrent(index);
}

void Segmented::mouseMoveEvent(QMouseEvent *event)
{
    const int index = indexAt(event->position().toPoint());
    if (index != m_hovered) {
        m_hovered = index;
        update();
    }
}

void Segmented::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

Dropdown::Dropdown(const QStringList &items, QWidget *parent)
    : QWidget(parent), m_items(items)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Dropdown::setCurrent(int index)
{
    if (index == m_current || index < 0 || index >= m_items.size())
        return;
    m_current = index;
    updateGeometry();
    update();
    emit changed(index);
}

void Dropdown::setPrefix(const QString &prefix)
{
    m_prefix = prefix;
    updateGeometry();
    update();
}

void Dropdown::setDetails(const QStringList &details)
{
    m_details = details;
}

void Dropdown::setCompact(bool compact)
{
    m_compact = compact;
    updateGeometry();
    update();
}

QSize Dropdown::sizeHint() const
{
    const QFontMetrics value(Theme::font(13, QFont::DemiBold));
    int widest = 0;
    for (const QString &item : m_items)
        widest = std::max(widest, value.horizontalAdvance(item));
    int width = 14 + widest + 10 + 14 + 12;
    if (!m_prefix.isEmpty())
        width += QFontMetrics(Theme::font(12.5)).horizontalAdvance(m_prefix) + 6;
    return {width, m_compact ? 32 : 36};
}

void Dropdown::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = m_compact ? 9 : 10;
    QColor line = m_open ? Theme::withAlpha(Theme::Accent, 170)
                 : (m_hovered || hasFocus()) ? Theme::FieldStrokeHover
                                             : Theme::FieldStroke;
    painter.setPen(QPen(line, 1));
    painter.setBrush(m_open ? Theme::withAlpha(Theme::Accent, 22)
                            : QColor(255, 255, 255, m_hovered ? 18 : 11));
    painter.drawRoundedRect(r, radius, radius);

    qreal x = r.left() + 13;
    const qreal chevron = 14;
    const QRectF textArea(x, r.top(), r.right() - x - chevron - 12, r.height());
    if (!m_prefix.isEmpty()) {
        painter.setFont(Theme::font(12.5));
        painter.setPen(Theme::TextMuted);
        const int w = painter.fontMetrics().horizontalAdvance(m_prefix);
        painter.drawText(QRectF(x, r.top(), w, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                         m_prefix);
        x += w + 6;
    }
    painter.setFont(Theme::font(13, QFont::DemiBold));
    painter.setPen(Theme::Text);
    const QString value = m_items.value(m_current);
    const QRectF valueRect(x, r.top(), textArea.right() - x, r.height());
    painter.drawText(valueRect, Qt::AlignVCenter | Qt::AlignLeft,
                     painter.fontMetrics().elidedText(value, Qt::ElideRight,
                                                      static_cast<int>(valueRect.width())));
    Icons::paint(painter, m_open ? Icon::ChevronUp : Icon::ChevronDown,
                 QRectF(r.right() - chevron - 10, r.center().y() - chevron / 2, chevron, chevron),
                 m_open ? Theme::Accent : Theme::TextMuted);
}

void Dropdown::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        openMenu();
    event->accept();
}

void Dropdown::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void Dropdown::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void Dropdown::openMenu()
{
    QMenu *menu = makeMenu(this);
    QPixmap blank(16, 16);
    blank.fill(Qt::transparent);
    for (int i = 0; i < m_items.size(); ++i) {
        QString text = m_items.at(i);
        if (i < m_details.size() && !m_details.at(i).isEmpty())
            text += QLatin1Char('\t') + m_details.at(i);
        QAction *action = menu->addAction(
            i == m_current ? Icons::icon(Icon::Check, Theme::Accent, 16) : QIcon(blank), text);
        action->setData(i);
        if (i == m_current)
            menu->setActiveAction(action);
    }
    menu->setMinimumWidth(width());
    m_open = true;
    update();
    QAction *chosen = menu->exec(mapToGlobal(QPoint(0, height() + 6)));
    m_open = false;
    update();
    if (chosen)
        setCurrent(chosen->data().toInt());
}

StatusDot::StatusDot(QWidget *parent) : QWidget(parent)
{
    setFixedSize(14, 14);
    m_color = Theme::TextFaint;
}

void StatusDot::setColor(const QColor &color, bool pulsing)
{
    m_color = color;
    if (pulsing != m_pulsing) {
        m_pulsing = pulsing;
        if (m_timer) {
            killTimer(m_timer);
            m_timer = 0;
        }
        if (pulsing)
            m_timer = startTimer(frameInterval(this), Qt::PreciseTimer);
    }
    update();
}

void StatusDot::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    m_phase = animationClock() * 2.4;
    update();
}

void StatusDot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0);
    QColor halo = m_color;
    const qreal wave = m_pulsing ? (std::sin(m_phase) + 1) / 2 : 0.5;
    halo.setAlphaF(0.18 + 0.22 * wave);
    painter.setPen(Qt::NoPen);
    painter.setBrush(halo);
    painter.drawEllipse(center, 6.0, 6.0);
    painter.setBrush(m_color);
    painter.drawEllipse(center, 3.5, 3.5);
}

QPushButton *makeButton(const QString &text, const QString &variant, Icon icon,
                        bool withIcon)
{
    auto *button = new QPushButton(text);
    if (!variant.isEmpty())
        button->setProperty("variant", variant);
    button->setCursor(Qt::PointingHandCursor);
    if (withIcon) {
        const QColor color = variant == QStringLiteral("primary") ? QColor(Qt::white)
                                                                 : Theme::Text;
        button->setIcon(Icons::icon(icon, color, 16));
        button->setIconSize(QSize(16, 16));
    }
    return button;
}

QLabel *makeLabel(const QString &text, int pixelSize, const QColor &color,
                  QFont::Weight weight)
{
    auto *label = new QLabel(text);
    label->setFont(Theme::font(pixelSize, weight));
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, color);
    label->setPalette(palette);
    label->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::rgba(color)));
    return label;
}

QString badgeHtml(const QString &text, const QColor &color)
{
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(color.name(), text.toHtmlEscaped());
}

namespace {
class GlassMenu final : public QMenu {
public:
    using QMenu::QMenu;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            painter.setPen(QPen(QColor(44, 44, 48), 1));
            painter.setBrush(Theme::Surface);
            painter.drawRoundedRect(r, 8, 8);
        }
        QMenu::paintEvent(event);
    }
};
}

QMenu *makeMenu(QWidget *parent)
{
    auto *menu = new GlassMenu(parent);
    menu->setProperty("glass", true);
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint |
                         Qt::NoDropShadowWindowHint);
    return menu;
}

QAction *addMenuAction(QMenu *menu, Icon icon, const QString &text, const QString &shortcut)
{
    QAction *action = menu->addAction(Icons::icon(icon, Theme::TextSecondary, 16), text);
    if (!shortcut.isEmpty())
        action->setShortcut(QKeySequence(shortcut));
    action->setShortcutVisibleInContextMenu(!shortcut.isEmpty());
    return action;
}

int frameInterval(const QWidget *widget)
{
    const QScreen *screen = widget && widget->screen() ? widget->screen()
                                                       : QGuiApplication::primaryScreen();
    const qreal hz = screen ? std::max<qreal>(60.0, screen->refreshRate()) : 60.0;
    return std::max(4, static_cast<int>(1000.0 / hz));
}

qreal animationClock()
{
    static QElapsedTimer clock;
    if (!clock.isValid())
        clock.start();
    return clock.elapsed() / 1000.0;
}

FrameAnimation::FrameAnimation(QWidget *owner) : QObject(owner), m_owner(owner)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &FrameAnimation::tick);
}

void FrameAnimation::start(qreal from, qreal to, int milliseconds, QEasingCurve::Type curve)
{
    m_from = from;
    m_to = to;
    m_duration = std::max(1, milliseconds);
    m_curve = QEasingCurve(curve);
    m_clock.start();
    m_timer.start(frameInterval(m_owner));
    tick();
}

void FrameAnimation::stop()
{
    m_timer.stop();
}

void FrameAnimation::tick()
{
    const qreal t = std::min<qreal>(1.0, m_clock.nsecsElapsed() / (m_duration * 1e6));
    m_value = m_from + (m_to - m_from) * m_curve.valueForProgress(t);
    emit valueChanged(m_value);
    if (t >= 1.0) {
        m_timer.stop();
        emit finished();
    }
}
