#include "launcher.h"

#include "controls.h"
#include "glass.h"
#include "theme.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int VisibleRows = 6;

QString protocolBadge(const ConnectionProfile &profile)
{
    if (profile.protocol == Protocol::Sftp)
        return QStringLiteral("SFTP");
    return profile.useTls ? QStringLiteral("FTPS") : QStringLiteral("FTP");
}

class QuickBar final : public QWidget {
public:
    using QWidget::QWidget;

    void setError(bool error)
    {
        m_error = error;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        Glass::paintPanel(painter, rect(), 8, Glass::Tone::Bar);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_error ? Theme::Danger : Theme::FieldStroke, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        Icons::paint(painter, Icon::Bolt, QRectF(14, (height() - 18) / 2.0, 18, 18),
                     Theme::TextSecondary);
    }

private:
    bool m_error = false;
};
}

HostRow::HostRow(const SavedHost &host, bool open, QWidget *parent)
    : QWidget(parent), m_host(host), m_open(open)
{
    setFixedHeight(RowHeight);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setAccessibleName(host.title());
    setAccessibleDescription(Icons::hostKindName(host.kind) + QStringLiteral(", ") +
                             host.profile.endpoint());
}

void HostRow::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void HostRow::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void HostRow::focusInEvent(QFocusEvent *event)
{
    for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
        if (auto *scroll = qobject_cast<QScrollArea *>(w)) {
            scroll->ensureWidgetVisible(this, 0, 0);
            break;
        }
    }
    update();
    QWidget::focusInEvent(event);
}

void HostRow::focusOutEvent(QFocusEvent *event)
{
    update();
    QWidget::focusOutEvent(event);
}

void HostRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit connectRequested(m_host.id);
}

void HostRow::contextMenuEvent(QContextMenuEvent *event)
{
    const QPoint at = event->reason() == QContextMenuEvent::Mouse
                          ? event->globalPos()
                          : mapToGlobal(QPoint(54, height() - 4));
    emit menuRequested(m_host.id, at);
}

void HostRow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        emit connectRequested(m_host.id);
        return;
    case Qt::Key_Up:
    case Qt::Key_Down: {
        const auto rows = parentWidget()->findChildren<HostRow *>(Qt::FindDirectChildrenOnly);
        const qsizetype next = rows.indexOf(this) + (event->key() == Qt::Key_Down ? 1 : -1);
        if (next >= 0 && next < rows.size())
            rows.at(next)->setFocus(Qt::TabFocusReason);
        return;
    }
    default:
        QWidget::keyPressEvent(event);
    }
}

void HostRow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 1.5, -0.5, -1.5);
    if (m_hovered || hasFocus()) {
        painter.setPen(hasFocus() ? QPen(Theme::Accent, 2) : Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, m_hovered ? 14 : 8));
        painter.drawRoundedRect(hasFocus() ? r.adjusted(0.5, 0.5, -0.5, -0.5) : r, 8, 8);
    }

    const QColor label = Theme::labelColor(m_host.color);
    const QRectF avatar(10, (height() - 32) / 2.0, 32, 32);
    painter.setPen(QPen(Theme::withAlpha(label, 130), 1));
    painter.setBrush(Theme::withAlpha(label, 56));
    painter.drawRoundedRect(avatar, 8, 8);
    Icons::paint(painter, Icons::hostIcon(m_host.kind), avatar.adjusted(7, 7, -7, -7),
                 Theme::labelGlyph(m_host.color));

    qreal right = width() - 14;
    painter.setFont(Theme::font(12, QFont::Bold));
    const QString protocol = protocolBadge(m_host.profile);
    const qreal protocolWidth = painter.fontMetrics().horizontalAdvance(protocol);
    painter.setPen(Theme::TextMuted);
    painter.drawText(QRectF(right - protocolWidth, 0, protocolWidth, height()),
                     Qt::AlignVCenter | Qt::AlignRight, protocol);
    right -= protocolWidth + 12;
    if (m_open) {
        const QString open = tr("открыт");
        painter.setFont(Theme::font(12, QFont::DemiBold));
        const qreal w = painter.fontMetrics().horizontalAdvance(open) + 14;
        const QRectF pill(right - w, (height() - 20) / 2.0, w, 20);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::AccentSoft);
        painter.drawRoundedRect(pill, 10, 10);
        painter.setPen(Theme::AccentText);
        painter.drawText(pill, Qt::AlignCenter, open);
        right = pill.left() - 12;
    }

    const qreal left = avatar.right() + 12;
    const qreal textWidth = std::max<qreal>(0, right - left);
    painter.setFont(Theme::font(13.5, QFont::DemiBold));
    painter.setPen(Theme::Text);
    painter.drawText(QRectF(left, 8, textWidth, 19), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_host.title(), Qt::ElideRight,
                                                      static_cast<int>(textWidth)));
    painter.setFont(Theme::font(12));
    painter.setPen(Theme::TextMuted);
    painter.drawText(QRectF(left, 27, textWidth, 17), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_host.profile.endpoint(), Qt::ElideRight,
                                                      static_cast<int>(textWidth)));
}

LauncherPage::LauncherPage(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(24, 12, 24, 12);
    auto *column = new QWidget;
    column->setMaximumWidth(680);
    row->addStretch(1);
    row->addWidget(column, 100);
    row->addStretch(1);

    auto *body = new QVBoxLayout(column);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addStretch(3);
    auto *title = makeLabel(tr("Куда подключимся?"), 24, Theme::Text, QFont::Bold);
    title->setAlignment(Qt::AlignHCenter);
    body->addWidget(title);
    body->addSpacing(16);

    auto *quick = new QuickBar;
    m_bar = quick;
    quick->setFixedHeight(48);
    auto *quickLayout = new QHBoxLayout(quick);
    quickLayout->setContentsMargins(40, 7, 7, 7);
    quickLayout->setSpacing(8);
    m_quick = new QLineEdit;
    m_quick->setPlaceholderText(tr("пользователь@хост:порт"));
    m_quick->setAccessibleName(tr("Быстрое подключение"));
    m_quick->setStyleSheet(QStringLiteral(
        "QLineEdit{background:transparent;border:none;font-size:14px;min-height:32px;padding:0;}"
        "QLineEdit:focus{background:transparent;}"));
    quickLayout->addWidget(m_quick, 1);
    auto *go = makeButton(tr("Подключиться"), QStringLiteral("primary"), Icon::Plug, true);
    quickLayout->addWidget(go);
    body->addWidget(quick);

    m_error = makeLabel(QString(), 12.5, Theme::Danger);
    m_error->setContentsMargins(4, 6, 4, 0);
    m_error->setWordWrap(true);
    m_error->hide();
    body->addWidget(m_error);
    body->addSpacing(28);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(4, 0, 0, 0);
    header->setSpacing(8);
    header->addWidget(makeLabel(tr("Сервера").toUpper(), 12, Theme::TextMuted, QFont::DemiBold));
    m_count = makeLabel(QString(), 12, Theme::TextMuted);
    header->addWidget(m_count);
    header->addStretch();
    auto *add = makeButton(tr("Новый сервер"), QStringLiteral("ghost"), Icon::Plus, true);
    header->addWidget(add);
    body->addLayout(header);
    body->addSpacing(6);

    auto *list = new QWidget;
    m_rows = new QVBoxLayout(list);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(0);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(list);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));
    m_scroll->viewport()->setAutoFillBackground(false);
    list->setAutoFillBackground(false);
    body->addWidget(m_scroll);

    m_empty = makeLabel(tr("Пока ни одного сервера"), 13, Theme::TextSecondary);
    m_empty->setContentsMargins(4, 8, 4, 8);
    body->addWidget(m_empty);
    body->addStretch(4);
    setFocusProxy(m_quick);

    connect(go, &QPushButton::clicked, this, &LauncherPage::submit);
    connect(m_quick, &QLineEdit::returnPressed, this, &LauncherPage::submit);
    connect(m_quick, &QLineEdit::textEdited, this, [this] { showError(QString()); });
    connect(add, &QPushButton::clicked, this, &LauncherPage::addHost);
    connect(&HostStore::instance(), &HostStore::changed, this, &LauncherPage::rebuild);
    rebuild();
}

void LauncherPage::submit()
{
    const QString text = m_quick->text().trimmed();
    if (text.isEmpty()) {
        showError(tr("Введите адрес сервера, например deploy@example.com:22"));
        m_quick->setFocus();
        return;
    }
    bool ok = false;
    HostStore::parseAddress(text, &ok);
    if (!ok) {
        showError(tr("Не получается разобрать адрес. Формат: пользователь@хост:порт"));
        m_quick->setFocus();
        return;
    }
    showError(QString());
    emit quickConnect(text);
}

void LauncherPage::showError(const QString &text)
{
    m_error->setText(text);
    m_error->setVisible(!text.isEmpty());
    m_quick->setAccessibleDescription(text);
    static_cast<QuickBar *>(m_bar)->setError(!text.isEmpty());
}

void LauncherPage::setOpenHosts(const QStringList &ids)
{
    if (ids == m_open)
        return;
    m_open = ids;
    rebuild();
}

void LauncherPage::rebuild()
{
    QWidget *list = m_scroll->widget();
    const bool hadFocus = list->isAncestorOf(QApplication::focusWidget());
    while (QLayoutItem *item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    QVector<SavedHost> hosts = HostStore::instance().hosts();
    std::stable_sort(hosts.begin(), hosts.end(), [](const SavedHost &a, const SavedHost &b) {
        if (a.lastUsed.isValid() != b.lastUsed.isValid())
            return a.lastUsed.isValid();
        if (a.lastUsed != b.lastUsed)
            return a.lastUsed > b.lastUsed;
        return a.title().localeAwareCompare(b.title()) < 0;
    });
    for (const SavedHost &host : std::as_const(hosts)) {
        auto *row = new HostRow(host, m_open.contains(host.id), list);
        connect(row, &HostRow::connectRequested, this, &LauncherPage::connectHost);
        connect(row, &HostRow::menuRequested, this, &LauncherPage::hostMenu);
        m_rows->addWidget(row);
    }

    const int count = static_cast<int>(hosts.size());
    m_count->setText(count > 0 ? QString::number(count) : QString());
    m_scroll->setVisible(count > 0);
    m_empty->setVisible(count == 0);
    m_scroll->setFixedHeight(std::min(count, VisibleRows) * HostRow::RowHeight);
    if (hadFocus && count > 0)
        static_cast<QWidget *>(m_rows->itemAt(0)->widget())->setFocus(Qt::OtherFocusReason);
}

void LauncherPage::focusQuickConnect()
{
    m_quick->setFocus();
    m_quick->selectAll();
}
