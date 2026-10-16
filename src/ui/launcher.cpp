#include "launcher.h"

#include "controls.h"
#include "format.h"
#include "glass.h"
#include "theme.h"

#include <QEnterEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString protocolBadge(const ConnectionProfile &profile)
{
    if (profile.protocol == Protocol::Sftp)
        return QStringLiteral("SFTP");
    return profile.useTls ? QStringLiteral("FTPS") : QStringLiteral("FTP");
}

class Hero final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Icons::paintLogo(painter, QRectF(0, 0, width(), height()));
    }
};

class QuickBar final : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Glass::paintPanel(painter, rect(), 16, Glass::Tone::Raised);
        painter.setRenderHint(QPainter::Antialiasing);
        Icons::paint(painter, Icon::Bolt, QRectF(18, (height() - 20) / 2.0, 20, 20),
                     Theme::Accent, 1.8);
    }
};
}

HostCard::HostCard(const SavedHost &host, bool open, QWidget *parent)
    : QWidget(parent), m_host(host), m_open(open)
{
    setFixedHeight(124);
    setMinimumWidth(230);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setToolTip(tr("Нажмите, чтобы подключиться"));
}

QRectF HostCard::moreRect() const
{
    return QRectF(width() - 42, 12, 30, 30);
}

void HostCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void HostCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    m_hoverMore = false;
    update();
    QWidget::leaveEvent(event);
}

void HostCard::mouseMoveEvent(QMouseEvent *event)
{
    const bool over = moreRect().contains(event->position());
    if (over != m_hoverMore) {
        m_hoverMore = over;
        update();
    }
}

void HostCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (!rect().contains(event->position().toPoint()))
        return;
    if (event->button() == Qt::RightButton ||
        (event->button() == Qt::LeftButton && moreRect().contains(event->position()))) {
        emit menuRequested(m_host.id, event->globalPosition().toPoint());
        return;
    }
    if (event->button() == Qt::LeftButton)
        emit connectRequested(m_host.id);
}

void HostCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    Glass::paintPanel(painter, r, 16, m_hovered ? Glass::Tone::Raised : Glass::Tone::Panel);
    const QColor label = Theme::labelColor(m_host.color);
    if (m_hovered) {
        QRadialGradient glow(QPointF(r.left() + 40, r.top()), r.width() * 0.8);
        QColor c = label;
        c.setAlpha(34);
        glow.setColorAt(0, c);
        c.setAlpha(0);
        glow.setColorAt(1, c);
        QPainterPath clip;
        clip.addRoundedRect(r, 16, 16);
        painter.fillPath(clip, glow);
        painter.setPen(QPen(QColor(label.red(), label.green(), label.blue(), 90), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
    }

    const QRectF avatar(r.left() + 16, r.top() + 16, 40, 40);
    QLinearGradient fill(avatar.topLeft(), avatar.bottomRight());
    QColor top = label;
    top.setAlpha(70);
    QColor bottom = label;
    bottom.setAlpha(28);
    fill.setColorAt(0, top);
    fill.setColorAt(1, bottom);
    painter.setPen(QPen(QColor(label.red(), label.green(), label.blue(), 110), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(avatar, 12, 12);
    painter.setFont(Theme::font(17, QFont::Bold));
    painter.setPen(Qt::white);
    const QString initial = m_host.title().left(1).toUpper();
    painter.drawText(avatar, Qt::AlignCenter, initial);

    const qreal textLeft = avatar.right() + 14;
    const qreal textWidth = r.right() - textLeft - 44;
    painter.setFont(Theme::font(14.5, QFont::DemiBold));
    painter.setPen(Theme::Text);
    painter.drawText(QRectF(textLeft, avatar.top() - 1, textWidth, 22),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_host.title(), Qt::ElideRight,
                                                      static_cast<int>(textWidth)));
    painter.setFont(Theme::font(12.5));
    painter.setPen(Theme::TextMuted);
    const QString endpoint = m_host.profile.username.isEmpty()
                                 ? m_host.profile.host
                                 : m_host.profile.username + QLatin1Char('@') + m_host.profile.host;
    painter.drawText(QRectF(textLeft, avatar.top() + 21, textWidth, 20),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(endpoint, Qt::ElideMiddle,
                                                      static_cast<int>(textWidth)));

    const qreal rowY = r.bottom() - 34;
    const QString badge = protocolBadge(m_host.profile);
    painter.setFont(Theme::font(11, QFont::Bold));
    const qreal badgeWidth = painter.fontMetrics().horizontalAdvance(badge) + 16;
    const QRectF badgeRect(r.left() + 16, rowY, badgeWidth, 20);
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.setBrush(QColor(255, 255, 255, 14));
    painter.drawRoundedRect(badgeRect, 7, 7);
    painter.setPen(Theme::TextSecondary);
    painter.drawText(badgeRect, Qt::AlignCenter, badge);

    qreal x = badgeRect.right() + 8;
    if (m_open) {
        const QString text = tr("открыт");
        const qreal w = painter.fontMetrics().horizontalAdvance(text) + 26;
        const QRectF openRect(x, rowY, w, 20);
        painter.setPen(QPen(QColor(61, 220, 151, 70), 1));
        painter.setBrush(QColor(61, 220, 151, 26));
        painter.drawRoundedRect(openRect, 7, 7);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::Success);
        painter.drawEllipse(QPointF(openRect.left() + 10, openRect.center().y()), 3, 3);
        painter.setPen(Theme::Success);
        painter.drawText(openRect.adjusted(16, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        x = openRect.right() + 8;
    }
    painter.setFont(Theme::font(12));
    painter.setPen(Theme::TextFaint);
    const QString when = m_host.lastUsed.isValid() ? Format::timestamp(m_host.lastUsed)
                                                   : tr("ещё не подключались");
    painter.drawText(QRectF(x, rowY, r.right() - x - 14, 20), Qt::AlignVCenter | Qt::AlignRight,
                     when);

    if (m_hovered) {
        const QRectF more = moreRect();
        if (m_hoverMore) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 24));
            painter.drawRoundedRect(more, 9, 9);
        }
        Icons::paint(painter, Icon::More, more.adjusted(6, 6, -6, -6),
                     m_hoverMore ? Theme::Text : Theme::TextSecondary);
    }
}

LauncherPage::LauncherPage(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));
    m_scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(m_scroll);

    auto *content = new QWidget;
    m_scroll->setWidget(content);
    content->setAutoFillBackground(false);
    content->setAttribute(Qt::WA_TranslucentBackground);
    auto *center = new QHBoxLayout(content);
    center->setContentsMargins(24, 0, 24, 24);
    auto *column = new QVBoxLayout;
    column->setSpacing(0);
    center->addStretch(1);
    center->addLayout(column, 100);
    center->addStretch(1);

    auto *columnHost = new QWidget;
    columnHost->setMaximumWidth(1080);
    columnHost->setLayout(new QVBoxLayout);
    column->addWidget(columnHost);
    auto *body = static_cast<QVBoxLayout *>(columnHost->layout());
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    body->addSpacing(44);
    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(18);
    auto *hero = new Hero;
    hero->setFixedSize(60, 60);
    heroRow->addWidget(hero);
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(2);
    heroText->addStretch();
    auto *title = makeLabel(tr("Куда подключимся?"), 28, Theme::Text, QFont::Bold);
    heroText->addWidget(title);
    heroText->addStretch();
    heroRow->addLayout(heroText, 1);
    body->addLayout(heroRow);
    body->addSpacing(26);

    auto *quick = new QuickBar;
    quick->setFixedHeight(62);
    auto *quickLayout = new QHBoxLayout(quick);
    quickLayout->setContentsMargins(50, 10, 10, 10);
    quickLayout->setSpacing(10);
    m_quick = new QLineEdit;
    m_quick->setPlaceholderText(tr("Быстрое подключение: user@host:22, sftp://… или ftp://…"));
    m_quick->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;font-size:15px;min-height:40px;padding:0;}"
        "QLineEdit:focus{background:transparent;}"));
    quickLayout->addWidget(m_quick, 1);
    auto *go = makeButton(tr("Подключиться"), QStringLiteral("primary"), Icon::Plug, true);
    go->setMinimumHeight(42);
    go->setStyleSheet(QStringLiteral("border-radius:12px;padding:0 18px;"));
    quickLayout->addWidget(go);
    body->addWidget(quick);
    connect(go, &QPushButton::clicked, this, [this] { emit quickConnect(m_quick->text()); });
    connect(m_quick, &QLineEdit::returnPressed, this,
            [this] { emit quickConnect(m_quick->text()); });

    body->addSpacing(36);
    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    header->addWidget(makeLabel(tr("Сервера"), 18, Theme::Text, QFont::DemiBold));
    m_count = makeLabel(QString(), 13, Theme::TextMuted);
    header->addWidget(m_count);
    header->addStretch();
    m_search = new QLineEdit;
    m_search->setProperty("compact", true);
    m_search->setPlaceholderText(tr("Поиск"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(220);
    m_search->addAction(Icons::icon(Icon::Search, Theme::TextMuted, 14),
                        QLineEdit::LeadingPosition);
    header->addWidget(m_search);
    auto *add = makeButton(tr("Новый сервер"), QString(), Icon::Plus, true);
    add->setMinimumHeight(32);
    add->setStyleSheet(QStringLiteral("min-height:32px;border-radius:9px;"));
    header->addWidget(add);
    body->addLayout(header);
    body->addSpacing(14);
    connect(add, &QPushButton::clicked, this, &LauncherPage::addHost);
    connect(m_search, &QLineEdit::textChanged, this, &LauncherPage::rebuild);

    m_grid = new QWidget;
    m_gridLayout = new QGridLayout(m_grid);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(14);
    body->addWidget(m_grid);

    m_empty = new GlassPanel(Glass::Tone::Panel, 18);
    auto *emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setContentsMargins(30, 40, 30, 40);
    emptyLayout->setSpacing(10);
    auto *emptyTitle = makeLabel(tr("Пока ни одного сервера"), 17, Theme::Text, QFont::DemiBold);
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto *emptyText = makeLabel(tr("Добавьте сервер — данные для входа сохранятся в "
                                   "зашифрованном виде, а подключаться можно в один клик."),
                                13, Theme::TextSecondary);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setWordWrap(true);
    auto *emptyAdd = makeButton(tr("Добавить сервер"), QStringLiteral("primary"), Icon::Plus, true);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addSpacing(8);
    emptyLayout->addWidget(emptyAdd, 0, Qt::AlignHCenter);
    connect(emptyAdd, &QPushButton::clicked, this, &LauncherPage::addHost);
    body->addWidget(m_empty);
    body->addStretch(1);

    connect(&HostStore::instance(), &HostStore::changed, this, &LauncherPage::rebuild);
    rebuild();
}

void LauncherPage::setOpenHosts(const QStringList &ids)
{
    m_open = ids;
    rebuild();
}

void LauncherPage::focusQuickConnect()
{
    m_quick->setFocus();
    m_quick->selectAll();
}

void LauncherPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int columns = std::clamp((std::min(width(), 1080) - 48 + 14) / (260 + 14), 1, 4);
    if (columns != m_columns)
        rebuild();
}

void LauncherPage::rebuild()
{
    while (QLayoutItem *item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QVector<SavedHost> hosts = HostStore::instance().hosts();
    std::sort(hosts.begin(), hosts.end(), [](const SavedHost &a, const SavedHost &b) {
        if (a.lastUsed != b.lastUsed)
            return a.lastUsed > b.lastUsed;
        return a.title().compare(b.title(), Qt::CaseInsensitive) < 0;
    });
    const QString filter = m_search->text().trimmed();
    m_count->setText(hosts.isEmpty() ? QString() : QString::number(hosts.size()));
    m_columns = std::clamp((std::min(width(), 1080) - 48 + 14) / (260 + 14), 1, 4);
    int index = 0;
    for (const SavedHost &host : std::as_const(hosts)) {
        if (!filter.isEmpty() && !host.title().contains(filter, Qt::CaseInsensitive) &&
            !host.profile.host.contains(filter, Qt::CaseInsensitive) &&
            !host.profile.username.contains(filter, Qt::CaseInsensitive))
            continue;
        auto *card = new HostCard(host, m_open.contains(host.id));
        connect(card, &HostCard::connectRequested, this, &LauncherPage::connectHost);
        connect(card, &HostCard::editRequested, this, &LauncherPage::editHost);
        connect(card, &HostCard::menuRequested, this, &LauncherPage::hostMenu);
        m_gridLayout->addWidget(card, index / m_columns, index % m_columns);
        ++index;
    }
    for (int column = 0; column < 4; ++column)
        m_gridLayout->setColumnStretch(column, column < m_columns ? 1 : 0);
    m_empty->setVisible(hosts.isEmpty());
    m_grid->setVisible(!hosts.isEmpty());
}
