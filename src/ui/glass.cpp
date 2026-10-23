#include "glass.h"

#include "controls.h"
#include "theme.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace Glass {

namespace {
void boxBlur(QImage &image, int radius)
{
    const int w = image.width();
    const int h = image.height();
    if (w == 0 || h == 0 || radius < 1)
        return;
    std::vector<quint32> line(static_cast<size_t>(std::max(w, h)));
    const int window = radius * 2 + 1;
    const auto pass = [&](bool horizontal) {
        const int outer = horizontal ? h : w;
        const int inner = horizontal ? w : h;
        for (int o = 0; o < outer; ++o) {
            const auto pixel = [&](int i) -> quint32 & {
                i = std::clamp(i, 0, inner - 1);
                return horizontal
                           ? reinterpret_cast<quint32 *>(image.scanLine(o))[i]
                           : reinterpret_cast<quint32 *>(image.scanLine(i))[o];
            };
            int sum[4] = {0, 0, 0, 0};
            for (int i = -radius; i <= radius; ++i) {
                const quint32 p = pixel(i);
                sum[0] += (p >> 24) & 0xff;
                sum[1] += (p >> 16) & 0xff;
                sum[2] += (p >> 8) & 0xff;
                sum[3] += p & 0xff;
            }
            for (int i = 0; i < inner; ++i) {
                line[static_cast<size_t>(i)] =
                    (static_cast<quint32>(sum[0] / window) << 24) |
                    (static_cast<quint32>(sum[1] / window) << 16) |
                    (static_cast<quint32>(sum[2] / window) << 8) |
                    static_cast<quint32>(sum[3] / window);
                const quint32 out = pixel(i - radius);
                const quint32 in = pixel(i + radius + 1);
                sum[0] += static_cast<int>((in >> 24) & 0xff) - static_cast<int>((out >> 24) & 0xff);
                sum[1] += static_cast<int>((in >> 16) & 0xff) - static_cast<int>((out >> 16) & 0xff);
                sum[2] += static_cast<int>((in >> 8) & 0xff) - static_cast<int>((out >> 8) & 0xff);
                sum[3] += static_cast<int>(in & 0xff) - static_cast<int>(out & 0xff);
            }
            for (int i = 0; i < inner; ++i)
                pixel(i) = line[static_cast<size_t>(i)];
        }
    };
    pass(true);
    pass(false);
}
}

QImage blurred(const QImage &source, int radius)
{
    if (source.isNull())
        return source;
    constexpr int Factor = 6;
    QImage small = source
                       .scaled(std::max(1, source.width() / Factor),
                               std::max(1, source.height() / Factor),
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int r = std::max(1, radius / Factor);
    for (int i = 0; i < 3; ++i)
        boxBlur(small, r);
    QImage result = small.scaled(source.size(), Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);
    result.setDevicePixelRatio(source.devicePixelRatio());
    return result;
}

void paintPanel(QPainter &painter, const QRectF &rect, qreal radius, Tone tone)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    QColor fill;
    QColor rim;
    switch (tone) {
    case Tone::Panel:
        fill = Theme::Surface;
        rim = QColor(39, 39, 41);
        break;
    case Tone::Raised:
        fill = Theme::SurfaceHover;
        rim = QColor(46, 46, 50);
        break;
    case Tone::Bar:
        fill = Theme::Input;
        rim = QColor(37, 37, 41);
        break;
    case Tone::Sheet:
        fill = QColor(18, 18, 20);
        rim = QColor(44, 44, 48);
        break;
    case Tone::Flat:
        painter.restore();
        return;
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawPath(path);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(rim, 1));
    painter.drawPath(path);
    painter.restore();
}

}

Backdrop::Backdrop(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void Backdrop::resizeEvent(QResizeEvent *event)
{
    m_cache = QPixmap();
    QWidget::resizeEvent(event);
}

void Backdrop::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    const qreal ratio = devicePixelRatioF();
    if (m_cache.isNull() || m_cache.size() != size() * ratio) {
        m_cache = QPixmap(size() * ratio);
        m_cache.setDevicePixelRatio(ratio);
        QPainter p(&m_cache);
        p.fillRect(rect(), Theme::Base);
    }
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_cache);
}

GlassPanel::GlassPanel(Glass::Tone tone, int radius, QWidget *parent)
    : QFrame(parent), m_tone(tone), m_radius(radius)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void GlassPanel::setTone(Glass::Tone tone)
{
    m_tone = tone;
    update();
}

void GlassPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    Glass::paintPanel(painter, rect(), m_radius, m_tone);
}

namespace {
class SheetCard final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 6; ++i) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 14));
            painter.drawRoundedRect(rect().adjusted(6 - i, 10 - i, i - 6, i - 2), 12, 12);
        }
        Glass::paintPanel(painter, rect().adjusted(6, 6, -6, -6), 12, Glass::Tone::Sheet);
    }
};
}

Sheet::Sheet(QWidget *host, int width) : QWidget(host), m_host(host), m_width(width)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    m_card = new SheetCard(this);
    auto *outer = new QVBoxLayout(m_card);
    outer->setContentsMargins(6 + 24, 6 + 22, 6 + 24, 6 + 22);
    outer->setSpacing(18);

    m_header = new QWidget;
    m_header->hide();
    outer->addWidget(m_header);

    m_page = new QWidget;
    m_page->setObjectName(QStringLiteral("sheetPage"));
    m_page->setAutoFillBackground(false);
    m_body = new QVBoxLayout(m_page);
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(12);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(m_page);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea, #qt_scrollarea_viewport, #sheetPage "
                                           "{ background: transparent; }"));
    m_scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(m_scroll, 1);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (now && m_page->isAncestorOf(now))
            m_scroll->ensureWidgetVisible(now, 0, 12);
    });

    m_animation = new FrameAnimation(this);
    connect(m_animation, &FrameAnimation::valueChanged, this, &Sheet::setProgress);
    connect(m_animation, &FrameAnimation::finished, this, [this] {
        if (m_closing) {
            close();
            return;
        }
        m_cardFrozen = false;
        m_card->show();
        if (QWidget *focus = m_card->focusWidget())
            focus->setFocus();
        update();
    });
    host->installEventFilter(this);
}

void Sheet::setTitle(const QString &title, const QString &subtitle, Icon icon,
                     const QColor &iconColor)
{
    auto *layout = new QHBoxLayout(m_header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *badge = new QLabel;
    const QColor color = iconColor.isValid() ? iconColor : Theme::Accent;
    QPixmap pixmap(40 * 2, 40 * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fill = color;
        fill.setAlpha(36);
        p.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 80), 1));
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(0.5, 0.5, 39, 39), 20, 20);
        Icons::paintFilled(p, icon, QRectF(9, 9, 22, 22), color);
    }
    badge->setPixmap(pixmap);
    badge->setFixedSize(40, 40);
    layout->addWidget(badge, 0, Qt::AlignTop);
    auto *texts = new QVBoxLayout;
    texts->setSpacing(3);
    auto *titleLabel = new QLabel(title);
    titleLabel->setFont(Theme::font(17, QFont::DemiBold));
    titleLabel->setWordWrap(true);
    texts->addWidget(titleLabel);
    if (!subtitle.isEmpty()) {
        auto *sub = new QLabel(subtitle);
        sub->setWordWrap(true);
        sub->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::TextSecondary.name()));
        texts->addWidget(sub);
    }
    layout->addLayout(texts, 1);
    m_header->show();
}

void Sheet::open()
{
    if (!m_host)
        return;
    captureBackdrop();
    QWidget *focus = QApplication::focusWidget();
    for (QWidget *w = focus; w; w = w->parentWidget()) {
        if (qobject_cast<Sheet *>(w)) {
            focus = nullptr;
            break;
        }
    }
    m_returnFocus = focus;
    setGeometry(m_host->rect());
    layoutCard();
    show();
    raise();
    setFocus();
    freezeCard();
    m_animation->start(0.0, 1.0, 240, QEasingCurve::OutCubic);
}

void Sheet::captureBackdrop()
{
    const qreal ratio = m_host->devicePixelRatioF();
    QPixmap plain(m_host->size() * ratio);
    plain.setDevicePixelRatio(ratio);
    plain.fill(Qt::transparent);
    {
        QPainter painter(&plain);
        m_host->render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
        for (QObject *object : m_host->children()) {
            auto *child = qobject_cast<QWidget *>(object);
            if (!child || child == this || !child->isVisible() || child->isWindow())
                continue;
            child->render(&painter, child->pos(), QRegion(),
                          QWidget::DrawWindowBackground | QWidget::DrawChildren);
        }
    }
    m_plain = plain;
    QPixmap dimmed = plain;
    {
        QPainter dim(&dimmed);
        dim.fillRect(QRect(QPoint(), m_host->size()), QColor(0, 0, 0, 180));
    }
    m_snapshot = dimmed;
}

void Sheet::refreshBackdrop()
{
    if (m_backdropPending)
        return;
    m_backdropPending = true;
    QTimer::singleShot(0, this, [this] {
        m_backdropPending = false;
        if (!m_host || m_closing)
            return;
        captureBackdrop();
        update();
    });
}

void Sheet::dismiss()
{
    if (m_closing)
        return;
    m_closing = true;
    emit dismissed();
    freezeCard();
    m_animation->start(m_progress, 0.0, 200, QEasingCurve::OutCubic);
}

void Sheet::freezeCard()
{
    const qreal ratio = devicePixelRatioF();
    QPixmap image(m_card->size() * ratio);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::transparent);
    m_card->render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
    m_cardImage = image;
    m_cardFrozen = true;
    if (m_card->isAncestorOf(QApplication::focusWidget()))
        setFocus(Qt::OtherFocusReason);
    m_card->hide();
    update();
}

void Sheet::setProgress(qreal value)
{
    m_progress = value;
    update();
}

void Sheet::relayout()
{
    layoutCard();
}

void Sheet::layoutCard()
{
    const int width = std::min(m_width + 12, this->width() - 32);
    m_card->setFixedWidth(width);
    QLayout *layout = m_card->layout();
    layout->activate();
    const QMargins margins = layout->contentsMargins();
    const int inner = width - margins.left() - margins.right();
    const auto heightOf = [inner](QWidget *w) {
        return w->hasHeightForWidth() ? w->heightForWidth(inner) : w->sizeHint().height();
    };
    int wanted = margins.top() + margins.bottom() + heightOf(m_page);
    if (!m_header->isHidden())
        wanted += heightOf(m_header) + layout->spacing();
    const int height = std::min(wanted, this->height() - 32);
    m_card->resize(width, height);
    positionCard();
}

void Sheet::positionCard()
{
    m_card->move((width() - m_card->width()) / 2, (height() - m_card->height()) / 2);
}

void Sheet::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(rect(), m_plain);
    painter.setOpacity(m_progress);
    painter.drawPixmap(rect(), m_snapshot);
    if (!m_cardFrozen)
        return;
    const QRectF target = m_card->geometry();
    const qreal scale = 0.96 + 0.04 * m_progress;
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(target.center() + QPointF(0, (1.0 - m_progress) * 14));
    painter.scale(scale, scale);
    painter.drawPixmap(QRectF(-target.width() / 2, -target.height() / 2, target.width(),
                              target.height()),
                       m_cardImage, QRectF(m_cardImage.rect()));
}

void Sheet::mousePressEvent(QMouseEvent *event)
{
    if (!m_card->geometry().contains(event->position().toPoint()))
        rejected();
}

void Sheet::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        rejected();
        return;
    }
    QWidget::keyPressEvent(event);
}

void Sheet::hideEvent(QHideEvent *event)
{
    if (m_returnFocus && m_returnFocus->isVisible())
        m_returnFocus->setFocus(Qt::OtherFocusReason);
    QWidget::hideEvent(event);
}

bool Sheet::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && event->type() == QEvent::Resize) {
        setGeometry(m_host->rect());
        layoutCard();
        refreshBackdrop();
    }
    return QWidget::eventFilter(watched, event);
}

namespace {
class Toast final : public QWidget {
public:
    Toast(const QString &text, const QString &detail, ToastHost::Kind kind,
          QWidget *parent)
        : QWidget(parent), m_kind(kind)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 12, 10, 12);
        layout->setSpacing(12);
        auto *icon = new QLabel;
        icon->setPixmap(Icons::pixmap(iconFor(kind), colorFor(kind), 18, 0, true));
        icon->setFixedSize(18, 18);
        layout->addWidget(icon, 0, Qt::AlignTop);
        auto *texts = new QVBoxLayout;
        texts->setSpacing(2);
        auto *label = new QLabel(text);
        label->setWordWrap(true);
        label->setFont(Theme::font(13, QFont::Medium));
        texts->addWidget(label);
        if (!detail.isEmpty()) {
            auto *sub = new QLabel(detail);
            sub->setWordWrap(true);
            sub->setStyleSheet(QStringLiteral("color:%1;font-size:12px;")
                                   .arg(Theme::TextSecondary.name()));
            texts->addWidget(sub);
        }
        layout->addLayout(texts, 1);
        auto *close = new QPushButton;
        close->setProperty("variant", QStringLiteral("ghost"));
        close->setIcon(Icons::icon(Icon::Close, Theme::TextMuted, 14));
        close->setFixedSize(26, 26);
        close->setStyleSheet(QStringLiteral("min-height:0;padding:0;border-radius:8px;"));
        close->setCursor(Qt::PointingHandCursor);
        connect(close, &QPushButton::clicked, this, &QWidget::deleteLater);
        layout->addWidget(close, 0, Qt::AlignTop);
        setFixedWidth(360);
    }

    static QColor colorFor(ToastHost::Kind kind)
    {
        switch (kind) {
        case ToastHost::Kind::Success:
            return Theme::Success;
        case ToastHost::Kind::Warning:
            return Theme::Warning;
        case ToastHost::Kind::Error:
            return Theme::Danger;
        case ToastHost::Kind::Info:
            break;
        }
        return Theme::TextSecondary;
    }

    static Icon iconFor(ToastHost::Kind kind)
    {
        switch (kind) {
        case ToastHost::Kind::Success:
            return Icon::Check;
        case ToastHost::Kind::Warning:
            return Icon::Alert;
        case ToastHost::Kind::Error:
            return Icon::Alert;
        case ToastHost::Kind::Info:
            break;
        }
        return Icon::Info;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Glass::paintPanel(painter, rect(), 8, Glass::Tone::Sheet);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor accent = colorFor(m_kind);
        accent.setAlpha(m_kind == ToastHost::Kind::Info ? 0 : 200);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(QRectF(5, 14, 3, height() - 28), 1.5, 1.5);
    }

private:
    ToastHost::Kind m_kind;
};
}

ToastHost::ToastHost(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    parent->installEventFilter(this);
    resize(0, 0);
}

void ToastHost::setBottomMargin(int margin)
{
    m_bottomMargin = margin;
    relayout();
}

int ToastHost::notify(const QString &text, Kind kind, const QString &detail)
{
    const int id = m_nextId++;
    while (m_toasts.size() >= 3) {
        QWidget *oldest = m_toasts.takeFirst();
        oldest->deleteLater();
    }
    auto *toast = new Toast(text, detail, kind, parentWidget());
    toast->setProperty("toastId", id);
    m_toasts.append(toast);
    connect(toast, &QObject::destroyed, this, [this, toast] {
        m_toasts.removeOne(toast);
        relayout();
    });
    const auto fade = [toast](qreal from, qreal to, int ms, bool remove) {
        auto *effect = new QGraphicsOpacityEffect(toast);
        effect->setOpacity(from);
        toast->setGraphicsEffect(effect);
        auto *animation = new FrameAnimation(toast);
        QObject::connect(animation, &FrameAnimation::valueChanged, effect,
                         &QGraphicsOpacityEffect::setOpacity);
        QObject::connect(animation, &FrameAnimation::finished, toast, [toast, animation, remove] {
            animation->deleteLater();
            if (remove)
                toast->deleteLater();
            else
                toast->setGraphicsEffect(nullptr);
        });
        animation->start(from, to, ms, QEasingCurve::OutCubic);
    };
    toast->adjustSize();
    fade(0.0, 1.0, 200, false);
    toast->show();
    toast->raise();
    relayout();

    const int lifetime = kind == Kind::Error ? 9000 : 4500;
    QTimer::singleShot(lifetime, toast, [fade] { fade(1.0, 0.0, 240, true); });
    return id;
}

void ToastHost::dismiss(int id)
{
    for (QWidget *toast : std::as_const(m_toasts)) {
        if (toast->property("toastId").toInt() == id)
            toast->deleteLater();
    }
}

void ToastHost::relayout()
{
    QWidget *parent = parentWidget();
    if (!parent)
        return;
    int y = parent->height() - m_bottomMargin;
    for (auto it = m_toasts.crbegin(); it != m_toasts.crend(); ++it) {
        QWidget *toast = *it;
        toast->adjustSize();
        y -= toast->height();
        toast->move(parent->width() - toast->width() - 16, y);
        toast->raise();
        y -= 10;
    }
}

bool ToastHost::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        relayout();
    return QWidget::eventFilter(watched, event);
}
