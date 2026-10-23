#include "titlebar.h"

#include "controls.h"
#include "glass.h"
#include "icons.h"
#include "theme.h"

#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <cmath>

namespace {
constexpr qreal TabHeight = 30;
constexpr qreal HomeWidth = 40;
constexpr qreal MaxTabWidth = 220;
constexpr qreal MinTabWidth = 96;
constexpr qreal TabGap = 4;

QColor statusColor(TabStrip::Status status)
{
    switch (status) {
    case TabStrip::Status::Online:
        return Theme::Success;
    case TabStrip::Status::Busy:
        return Theme::Warning;
    case TabStrip::Status::Offline:
        return Theme::Danger;
    case TabStrip::Status::None:
        break;
    }
    return Theme::TextFaint;
}

class Logo final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Icons::paintLogo(painter, QRectF(0, 0, width(), height()));
    }
};

class SearchPill final : public QWidget {
public:
    explicit SearchPill(QWidget *parent = nullptr) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(240, 28);
        setAttribute(Qt::WA_Hover);
        setToolTip(tr("Сервера, сессии и команды"));
    }
    std::function<void()> onClick;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool hovered = underMouse();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(hovered ? QColor(52, 52, 56) : QColor(36, 36, 38), 1));
        painter.setBrush(Theme::Base);
        painter.drawRoundedRect(r, 8, 8);
        painter.setFont(Theme::font(13));
        painter.setPen(Theme::TextMuted);
        painter.drawText(QRectF(10, 0, width() - 90, height()), Qt::AlignVCenter,
                         tr("Поиск и команды"));
        const QRectF key(width() - 82, 5, 48, 18);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::SurfaceHover);
        painter.drawRoundedRect(key, 4, 4);
        Icons::paint(painter, Icon::Search, QRectF(width() - 25, 6, 16, 16), Theme::TextSecondary);
        painter.setFont(Theme::font(12, QFont::DemiBold));
        painter.setPen(Theme::TextMuted);
        painter.drawText(key, Qt::AlignCenter, QStringLiteral("Ctrl K"));
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (rect().contains(event->position().toPoint()) && onClick)
            onClick();
    }
};
}

TabStrip::TabStrip(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(Theme::TitleBarHeight);
    m_dragTimer.setSingleShot(true);
    m_dragTimer.setInterval(450);
    connect(&m_dragTimer, &QTimer::timeout, this, [this] {
        if (m_dragHover >= 0 && m_dragHover < m_tabs.size())
            emit activated(m_tabs.at(m_dragHover).id);
    });
}

QSize TabStrip::sizeHint() const
{
    return {600, Theme::TitleBarHeight};
}

QSize TabStrip::minimumSizeHint() const
{
    return {120, Theme::TitleBarHeight};
}

void TabStrip::setTabs(const QVector<Tab> &tabs)
{
    m_tabs = tabs;
    m_hovered = -1;
    relayout();
    bool pulsing = false;
    for (const Tab &tab : m_tabs)
        pulsing = pulsing || tab.status == Status::Busy;
    if (pulsing && !m_pulseTimer)
        m_pulseTimer = startTimer(frameInterval(this), Qt::PreciseTimer);
    if (!pulsing && m_pulseTimer) {
        killTimer(m_pulseTimer);
        m_pulseTimer = 0;
    }
    update();
}

void TabStrip::updateTab(const Tab &tab)
{
    QVector<Tab> tabs = m_tabs;
    for (Tab &existing : tabs) {
        if (existing.id == tab.id)
            existing = tab;
    }
    setTabs(tabs);
}

void TabStrip::setCurrent(quint64 id)
{
    m_current = id;
    update();
}

void TabStrip::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    m_phase = animationClock() * 2.4;
    update();
}

void TabStrip::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void TabStrip::relayout()
{
    m_rects.clear();
    const QFontMetrics metrics(Theme::font(13, QFont::Medium));
    const qreal top = (height() - TabHeight) / 2.0;
    const qreal plusWidth = 32;
    QVector<qreal> widths;
    qreal total = 0;
    for (const Tab &tab : m_tabs) {
        const qreal w = tab.id == 0 ? HomeWidth
                                    : std::clamp<qreal>(metrics.horizontalAdvance(tab.title) + 66,
                                                        MinTabWidth, MaxTabWidth);
        widths.append(w);
        total += w + TabGap;
    }
    const qreal available = width() - plusWidth - 8;
    if (total > available && m_tabs.size() > 1) {
        const qreal sessions = m_tabs.size() - 1;
        const qreal each = std::max<qreal>(
            56, (available - HomeWidth - TabGap * m_tabs.size()) / sessions);
        for (int i = 0; i < widths.size(); ++i) {
            if (m_tabs.at(i).id != 0)
                widths[i] = std::min(widths.at(i), each);
        }
    }
    qreal x = 0;
    for (const qreal w : widths) {
        m_rects.append(QRectF(x, top, w, TabHeight));
        x += w + TabGap;
    }
    m_plusRect = QRectF(x + 2, top, plusWidth, TabHeight);
}

QRectF TabStrip::closeRect(int index) const
{
    const QRectF r = m_rects.at(index);
    return QRectF(r.right() - 26, r.top() + (r.height() - 20) / 2, 20, 20);
}

int TabStrip::tabAt(const QPoint &point) const
{
    for (int i = 0; i < m_rects.size(); ++i) {
        if (m_rects.at(i).contains(point))
            return i;
    }
    return -1;
}

bool TabStrip::isDragArea(const QPoint &point) const
{
    return tabAt(point) < 0 && !m_plusRect.contains(point);
}

bool TabStrip::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int index = tabAt(help->pos());
        if (index >= 0 && !m_tabs.at(index).tooltip.isEmpty())
            QToolTip::showText(help->globalPos(), m_tabs.at(index).tooltip, this);
        else if (m_plusRect.contains(help->pos()))
            QToolTip::showText(help->globalPos(), tr("Новое подключение (Ctrl+T)"), this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

void TabStrip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < m_tabs.size(); ++i) {
        const Tab &tab = m_tabs.at(i);
        const QRectF r = m_rects.at(i);
        const bool active = tab.id == m_current;
        const bool hovered = i == m_hovered || i == m_dragHover;
        if (active || hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, active ? 24 : 12));
            painter.drawRoundedRect(r, 8, 8);
        }

        if (tab.id == 0) {
            const QRectF glyph(r.center().x() - 9, r.center().y() - 9, 18, 18);
            if (active)
                Icons::paintFilled(painter, Icon::Server, glyph, Theme::Text);
            else
                Icons::paint(painter, Icon::Server, glyph, Theme::TextMuted);
            continue;
        }

        const QPointF dot(r.left() + 16, r.center().y());
        const QColor color = statusColor(tab.status);
        switch (tab.status) {
        case Status::Online:
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(dot, 4.0, 4.0);
            break;
        case Status::Busy: {
            painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            const QRectF ring(dot.x() - 4.5, dot.y() - 4.5, 9, 9);
            const qreal start = -std::fmod(m_phase * 86.0, 360.0);
            painter.drawArc(ring, qRound(start * 16), 270 * 16);
            break;
        }
        case Status::Offline:
            painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(dot + QPointF(-3.5, -3.5), dot + QPointF(3.5, 3.5));
            painter.drawLine(dot + QPointF(-3.5, 3.5), dot + QPointF(3.5, -3.5));
            break;
        case Status::None:
            painter.setPen(QPen(color, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(dot, 3.5, 3.5);
            break;
        }

        const bool showClose = active || hovered;
        const qreal textRight = showClose ? r.right() - 30 : r.right() - 12;
        painter.setFont(Theme::font(13, active ? QFont::DemiBold : QFont::Medium));
        painter.setPen(active ? Theme::Text : hovered ? Theme::TextSecondary : Theme::TextMuted);
        const QRectF textRect(r.left() + 28, r.top(), textRight - r.left() - 28, r.height());
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(tab.title, Qt::ElideRight,
                                                          static_cast<int>(textRect.width())));
        if (tab.label.isValid() && tab.label.alpha() > 0) {
            painter.setPen(Qt::NoPen);
            QColor label = tab.label;
            label.setAlpha(active ? 255 : 150);
            painter.setBrush(label);
            painter.drawRoundedRect(QRectF(r.left() + 6, r.top() + 9, 2.5, r.height() - 18),
                                    1.2, 1.2);
        }
        if (showClose) {
            const QRectF close = closeRect(i);
            if (i == m_hovered && m_hoverClose) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 255, 255, 26));
                painter.drawRoundedRect(close, 6, 6);
            }
            Icons::paint(painter, Icon::Close, close.adjusted(4, 4, -4, -4),
                         i == m_hovered && m_hoverClose ? Theme::Text : Theme::TextMuted);
        }
    }

    if (m_hoverPlus) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 12));
        painter.drawRoundedRect(m_plusRect, 8, 8);
    }
    Icons::paint(painter, Icon::Plus,
                 QRectF(m_plusRect.center().x() - 8, m_plusRect.center().y() - 8, 16, 16),
                 m_hoverPlus ? Theme::Text : Theme::TextSecondary);
}

void TabStrip::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    const int index = tabAt(point);
    const bool overClose = index >= 0 && m_tabs.at(index).id != 0 &&
                           closeRect(index).contains(point);
    const bool overPlus = m_plusRect.contains(point);
    if (index != m_hovered || overClose != m_hoverClose || overPlus != m_hoverPlus) {
        m_hovered = index;
        m_hoverClose = overClose;
        m_hoverPlus = overPlus;
        setCursor(index >= 0 || overPlus ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void TabStrip::mousePressEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    m_pressed = tabAt(point);
    if (m_pressed < 0)
        return;
    const Tab &tab = m_tabs.at(m_pressed);
    if (event->button() == Qt::LeftButton &&
        !(tab.id != 0 && closeRect(m_pressed).contains(point))) {
        emit activated(tab.id);
    }
    if (event->button() == Qt::RightButton && tab.id != 0)
        emit contextMenuRequested(tab.id, event->globalPosition().toPoint());
}

void TabStrip::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint point = event->position().toPoint();
    const int index = tabAt(point);
    if (m_plusRect.contains(point) && event->button() == Qt::LeftButton) {
        emit newRequested();
    } else if (index >= 0 && index == m_pressed && m_tabs.at(index).id != 0) {
        const bool close = (event->button() == Qt::LeftButton &&
                            closeRect(index).contains(point)) ||
                           event->button() == Qt::MiddleButton;
        if (close)
            emit closeRequested(m_tabs.at(index).id);
    }
    m_pressed = -1;
}

void TabStrip::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    m_hoverClose = false;
    m_hoverPlus = false;
    update();
    QWidget::leaveEvent(event);
}

void TabStrip::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void TabStrip::dragMoveEvent(QDragMoveEvent *event)
{
    const int index = tabAt(event->position().toPoint());
    if (index != m_dragHover) {
        m_dragHover = index;
        m_dragTimer.start();
        update();
    }
    event->ignore();
    if (index >= 0)
        event->acceptProposedAction();
}

void TabStrip::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    m_dragHover = -1;
    m_dragTimer.stop();
    update();
}

void TabStrip::dropEvent(QDropEvent *event)
{
    m_dragHover = -1;
    m_dragTimer.stop();
    update();
    event->ignore();
}

TitleBar::TitleBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(Theme::TitleBarHeight);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(6);

    m_logo = new Logo(this);
    m_logo->setFixedSize(26, 26);
    m_logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_logo);
    layout->addSpacing(8);

    m_tabs = new TabStrip(this);
    layout->addWidget(m_tabs, 1);

    auto *search = new SearchPill(this);
    search->onClick = [this] { emit paletteRequested(); };
    m_search = search;
    layout->addWidget(m_search);
    layout->addSpacing(4);

    m_transfers = new IconButton(Icon::Transfers, tr("Передачи (Ctrl+J)"), this);
    m_transfers->setFixedSize(34, 32);
    connect(m_transfers, &IconButton::clicked, this, &TitleBar::transfersRequested);
    layout->addWidget(m_transfers);
    m_settings = new IconButton(Icon::Gear, tr("Настройки (Ctrl+,)"), this);
    m_settings->setFixedSize(34, 32);
    connect(m_settings, &IconButton::clicked, this, &TitleBar::settingsRequested);
    layout->addWidget(m_settings);
    layout->addSpacing(6);

    m_minimize = new IconButton(Icon::Minimize, tr("Свернуть"), this);
    m_maximize = new IconButton(Icon::Maximize, tr("Развернуть"), this);
    m_close = new IconButton(Icon::Close, tr("Закрыть"), this);
    m_close->setStyle(IconButton::Style::Danger);
    for (IconButton *button : {m_minimize, m_maximize, m_close}) {
        button->setFixedSize(40, 32);
        button->setIconSize(16);
        layout->addWidget(button);
    }
    connect(m_minimize, &IconButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &IconButton::clicked, this, &TitleBar::maximizeRequested);
    connect(m_close, &IconButton::clicked, this, &TitleBar::closeRequested);
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximize->setIcon(maximized ? Icon::Restore : Icon::Maximize);
    m_maximize->setToolTip(maximized ? tr("Восстановить") : tr("Развернуть"));
}

void TitleBar::setTransferBadge(int active)
{
    m_transferBadge = active;
    m_transfers->setBadge(active);
}

bool TitleBar::isCaption(const QPoint &point) const
{
    QWidget *child = childAt(point);
    if (!child || child == m_logo)
        return true;
    if (child == m_tabs)
        return m_tabs->isDragArea(m_tabs->mapFrom(this, point));
    return false;
}

void TitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(QRect(0, height() - 1, width(), 1), Theme::Divider);
}
