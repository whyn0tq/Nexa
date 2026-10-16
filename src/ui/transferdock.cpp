#include "transferdock.h"

#include "controls.h"
#include "format.h"
#include "glass.h"
#include "icons.h"
#include "theme.h"
#include "transferprefs.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr int BarHeight = 52;
constexpr int PanelHeight = 300;

QString stateText(const TransferStatus &status)
{
    switch (status.state) {
    case TransferState::Queued:
        return QCoreApplication::translate("TransferDock", "В очереди");
    case TransferState::Running:
        return status.bytesPerSecond > 0 ? Format::speed(status.bytesPerSecond)
                                         : QCoreApplication::translate("TransferDock", "Подготовка…");
    case TransferState::Completed:
        return QCoreApplication::translate("TransferDock", "Готово");
    case TransferState::Failed:
        return QCoreApplication::translate("TransferDock", "Ошибка");
    case TransferState::Canceled:
        return QCoreApplication::translate("TransferDock", "Отменено");
    case TransferState::Skipped:
        return QCoreApplication::translate("TransferDock", "Пропущено");
    }
    return {};
}

class FilterProxy final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    int filter = TransferModel::All;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &) const override
    {
        const auto *model = static_cast<TransferModel *>(sourceModel());
        const TransferStatus *status = model->at(row);
        if (!status)
            return false;
        switch (filter) {
        case TransferModel::Active:
            return !TransferModel::finished(status->state);
        case TransferModel::Done:
            return status->state == TransferState::Completed ||
                   status->state == TransferState::Skipped;
        case TransferModel::Failed:
            return status->state == TransferState::Failed ||
                   status->state == TransferState::Canceled;
        default:
            return true;
        }
    }
};

class ListView final : public QListView {
public:
    using QListView::QListView;
    int hoverRow = -1;

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QModelIndex index = indexAt(event->position().toPoint());
        if (index.row() != hoverRow) {
            hoverRow = index.row();
            viewport()->update();
        }
        QListView::mouseMoveEvent(event);
    }
    void leaveEvent(QEvent *event) override
    {
        hoverRow = -1;
        viewport()->update();
        QListView::leaveEvent(event);
    }
};
}

bool TransferModel::finished(TransferState state)
{
    return state != TransferState::Queued && state != TransferState::Running;
}

int TransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TransferModel::data(const QModelIndex &index, int role) const
{
    const TransferStatus *status = at(index.row());
    if (!status)
        return {};
    if (role == Qt::DisplayRole)
        return status->name;
    if (role == Qt::ToolTipRole)
        return status->detail.isEmpty() ? (status->upload ? status->localPath : status->remotePath)
                                        : status->detail;
    if (role == StatusRole)
        return QVariant::fromValue(*status);
    return {};
}

const TransferStatus *TransferModel::at(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

void TransferModel::upsert(const TransferStatus &status)
{
    const auto found = m_index.constFind(status.id);
    if (found == m_index.cend()) {
        beginInsertRows({}, 0, 0);
        m_rows.prepend(status);
        endInsertRows();
        m_index.clear();
        for (int i = 0; i < m_rows.size(); ++i)
            m_index.insert(m_rows.at(i).id, i);
    } else {
        const int row = found.value();
        TransferStatus merged = status;
        if (merged.sessionName.isEmpty())
            merged.sessionName = m_rows.at(row).sessionName;
        m_rows[row] = merged;
        emit dataChanged(index(row), index(row));
    }
    emit summaryChanged();
}

void TransferModel::clearFinished()
{
    beginResetModel();
    TransferStatuses kept;
    for (const TransferStatus &status : std::as_const(m_rows)) {
        if (!finished(status.state))
            kept.append(status);
    }
    m_rows = kept;
    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows.at(i).id, i);
    endResetModel();
    emit summaryChanged();
}

void TransferModel::removeSession(quint64 sessionId)
{
    beginResetModel();
    TransferStatuses kept;
    for (const TransferStatus &status : std::as_const(m_rows)) {
        if (status.sessionId != sessionId || finished(status.state))
            kept.append(status);
    }
    m_rows = kept;
    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows.at(i).id, i);
    endResetModel();
    emit summaryChanged();
}

int TransferModel::activeCount() const
{
    return static_cast<int>(std::count_if(m_rows.cbegin(), m_rows.cend(),
                                          [](const TransferStatus &s) { return !finished(s.state); }));
}

int TransferModel::failedCount() const
{
    return static_cast<int>(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const TransferStatus &s) {
        return s.state == TransferState::Failed;
    }));
}

qint64 TransferModel::speed() const
{
    qint64 total = 0;
    for (const TransferStatus &status : m_rows) {
        if (status.state == TransferState::Running)
            total += status.bytesPerSecond;
    }
    return total;
}

qint64 TransferModel::remainingBytes() const
{
    qint64 total = 0;
    for (const TransferStatus &status : m_rows) {
        if (!finished(status.state) && status.total > 0)
            total += std::max<qint64>(0, status.total - status.done);
    }
    return total;
}

double TransferModel::progress() const
{
    qint64 done = 0;
    qint64 total = 0;
    for (const TransferStatus &status : m_rows) {
        if (finished(status.state) || status.total <= 0)
            continue;
        done += std::min(status.done, status.total);
        total += status.total;
    }
    return total > 0 ? static_cast<double>(done) / static_cast<double>(total) : -1;
}

QSize TransferDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {200, 60};
}

QRect TransferDelegate::actionRect(const QRect &row)
{
    return QRect(row.right() - 44, row.center().y() - 15, 30, 30);
}

QRect TransferDelegate::secondaryRect(const QRect &row)
{
    return QRect(row.right() - 78, row.center().y() - 15, 30, 30);
}

void TransferDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const TransferStatus status = index.data(TransferModel::StatusRole).value<TransferStatus>();
    const auto *view = dynamic_cast<const ListView *>(option.widget);
    const bool hovered = view && view->hoverRow == index.row();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(option.rect).adjusted(4, 2, -4, -2);
    if (hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 12));
        painter->drawRoundedRect(r, 12, 12);
    }

    const QRectF bubble(r.left() + 10, r.center().y() - 17, 34, 34);
    const bool failed = status.state == TransferState::Failed;
    const bool done = status.state == TransferState::Completed;
    QColor tint = failed ? Theme::Danger : done ? Theme::Success
                  : status.upload ? Theme::Accent : QColor(230, 230, 236);
    QColor fill = tint;
    fill.setAlpha(34);
    painter->setPen(QPen(QColor(tint.red(), tint.green(), tint.blue(), 70), 1));
    painter->setBrush(fill);
    painter->drawRoundedRect(bubble, 11, 11);
    const Icon icon = failed ? Icon::Alert
                      : done ? Icon::Check
                      : status.upload ? Icon::Upload : Icon::Download;
    Icons::paint(*painter, icon, bubble.adjusted(8, 8, -8, -8), tint, 2);

    const qreal right = r.right() - 92;
    const qreal left = bubble.right() + 12;
    const qreal metaWidth = 250;
    const qreal nameWidth = std::max<qreal>(80, right - left - metaWidth - 16);

    painter->setFont(Theme::font(13.5, QFont::DemiBold));
    painter->setPen(Theme::Text);
    const QString name = painter->fontMetrics().elidedText(status.name, Qt::ElideMiddle,
                                                           static_cast<int>(nameWidth - 70));
    painter->drawText(QRectF(left, r.top() + 9, nameWidth, 20), Qt::AlignLeft | Qt::AlignVCenter,
                      name);
    const qreal nameEnd = left + painter->fontMetrics().horizontalAdvance(name);
    if (!status.sessionName.isEmpty()) {
        painter->setFont(Theme::font(11, QFont::DemiBold));
        const QString session = painter->fontMetrics().elidedText(status.sessionName,
                                                                  Qt::ElideRight, 120);
        const qreal w = painter->fontMetrics().horizontalAdvance(session) + 14;
        const QRectF chip(nameEnd + 10, r.top() + 10, w, 18);
        painter->setPen(QPen(QColor(255, 255, 255, 28), 1));
        painter->setBrush(QColor(255, 255, 255, 12));
        painter->drawRoundedRect(chip, 6, 6);
        painter->setPen(Theme::TextSecondary);
        painter->drawText(chip, Qt::AlignCenter, session);
    }

    painter->setFont(Theme::font(12));
    painter->setPen(failed ? Theme::Danger : Theme::TextMuted);
    QString detail = status.detail;
    if (detail.isEmpty())
        detail = status.upload ? status.remotePath : status.localPath;
    painter->drawText(QRectF(left, r.top() + 31, nameWidth, 18), Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(detail, Qt::ElideMiddle,
                                                        static_cast<int>(nameWidth)));

    const qreal metaLeft = right - metaWidth;
    const QRectF bar(metaLeft, r.top() + 15, metaWidth, 6);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 18));
    painter->drawRoundedRect(bar, 3, 3);
    double fraction = status.total > 0 ? static_cast<double>(status.done) / status.total : 0;
    if (done || status.state == TransferState::Skipped)
        fraction = 1;
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (fraction > 0) {
        QRectF filled = bar;
        filled.setWidth(std::max(6.0, bar.width() * fraction));
        QLinearGradient gradient(filled.topLeft(), filled.topRight());
        if (failed) {
            gradient.setColorAt(0, QColor(255, 92, 108, 160));
            gradient.setColorAt(1, QColor(255, 92, 108, 160));
        } else if (done) {
            gradient.setColorAt(0, QColor(61, 220, 151, 150));
            gradient.setColorAt(1, QColor(61, 220, 151, 220));
        } else {
            gradient.setColorAt(0, QColor(200, 30, 58));
            gradient.setColorAt(1, Theme::AccentHover);
        }
        painter->setBrush(gradient);
        painter->drawRoundedRect(filled, 3, 3);
    }
    painter->setFont(Theme::font(12));
    painter->setPen(Theme::TextSecondary);
    QString amount;
    if (TransferModel::finished(status.state) || status.total <= 0)
        amount = Format::size(std::max(status.done, status.total));
    else
        amount = QStringLiteral("%1 / %2").arg(Format::size(status.done), Format::size(status.total));
    if (status.filesTotal > 1) {
        amount += TransferModel::finished(status.state)
                      ? QCoreApplication::translate("TransferDock", " · файлов: %1").arg(status.filesTotal)
                      : QStringLiteral(" · %1/%2").arg(status.filesDone).arg(status.filesTotal);
    }
    painter->drawText(QRectF(metaLeft, r.top() + 28, metaWidth, 20),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(amount, Qt::ElideRight,
                                                        static_cast<int>(metaWidth - 70)));
    painter->setPen(failed ? Theme::Danger : done ? Theme::Success : Theme::Text);
    painter->setFont(Theme::font(12, QFont::DemiBold));
    QString state = stateText(status);
    if (status.state == TransferState::Running && status.bytesPerSecond > 0 && status.total > 0) {
        const qint64 seconds = (status.total - status.done) / std::max<qint64>(1, status.bytesPerSecond);
        state += QStringLiteral(" · ") + Format::duration(seconds);
    }
    painter->drawText(QRectF(metaLeft, r.top() + 28, metaWidth, 20),
                      Qt::AlignRight | Qt::AlignVCenter, state);

    const QRect action = actionRect(option.rect);
    const bool active = !TransferModel::finished(status.state);
    if (hovered || active) {
        const bool retry = failed || status.state == TransferState::Canceled;
        const Icon actionIcon = active ? Icon::Close : retry ? Icon::Retry : Icon::Folder;
        if (!active && !retry && status.upload) {
            painter->restore();
            return;
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, hovered ? 18 : 8));
        painter->drawRoundedRect(action, 9, 9);
        Icons::paint(*painter, actionIcon, QRectF(action).adjusted(8, 8, -8, -8),
                     hovered ? Theme::Text : Theme::TextMuted, 2);
    }
    painter->restore();
}

TransferDock::TransferDock(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    m_model = new TransferModel(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_bar = new QWidget;
    m_bar->setFixedHeight(BarHeight);
    m_bar->setCursor(Qt::PointingHandCursor);
    m_bar->installEventFilter(this);
    auto *bar = new QHBoxLayout(m_bar);
    bar->setContentsMargins(14, 0, 10, 0);
    bar->setSpacing(10);
    auto *icon = new QLabel;
    icon->setPixmap(Icons::pixmap(Icon::Transfers, Theme::Accent, 18));
    bar->addWidget(icon);
    m_title = makeLabel(tr("Передачи"), 13.5, Theme::Text, QFont::DemiBold);
    bar->addWidget(m_title);
    m_summary = makeLabel(QString(), 12.5, Theme::TextSecondary);
    bar->addWidget(m_summary, 1);
    QStringList policyNames;
    QStringList policyHints;
    for (int i = 0; i < 4; ++i) {
        policyNames.append(TransferPrefs::policyTitle(static_cast<OverwritePolicy>(i)));
        policyHints.append(TransferPrefs::policyHint(static_cast<OverwritePolicy>(i)));
    }
    m_policyChip = new Dropdown(policyNames);
    m_policyChip->setPrefix(tr("Если файл есть:"));
    m_policyChip->setDetails(policyHints);
    m_policyChip->setCompact(true);
    m_policyChip->setToolTip(tr("Что делать, если такой файл уже есть там, куда он передаётся"));
    bar->addWidget(m_policyChip);
    m_toggle = new IconButton(Icon::ChevronUp, tr("Показать передачи (Ctrl+J)"));
    m_toggle->setFixedSize(32, 32);
    connect(m_toggle, &IconButton::clicked, this, &TransferDock::toggle);
    bar->addWidget(m_toggle);
    layout->addWidget(m_bar);

    m_drawer = new GlassPanel(Glass::Tone::Sheet, Theme::RadiusLarge);
    m_drawer->hide();
    m_panel = new QWidget(m_drawer);
    auto *panel = new QVBoxLayout(m_panel);
    panel->setContentsMargins(12, 12, 12, 10);
    panel->setSpacing(8);
    auto *tools = new QHBoxLayout;
    tools->setSpacing(8);
    m_filter = new Segmented({tr("Все"), tr("Активные"), tr("Готово"), tr("Ошибки")});
    m_filter->setFixedWidth(360);
    m_filter->setFixedHeight(34);
    tools->addWidget(m_filter);
    tools->addStretch();
    QStringList counts;
    for (const int value : TransferPrefs::connectionChoices())
        counts.append(QString::number(value));
    m_connections = new Dropdown(counts);
    m_connections->setPrefix(tr("Потоков:"));
    m_connections->setCompact(true);
    m_connections->setToolTip(tr("Сколько параллельных соединений открывать для передач"));
    tools->addWidget(m_connections);
    tools->addSpacing(4);
    auto *clear = makeButton(tr("Очистить"));
    clear->setStyleSheet(QStringLiteral("min-height:32px;border-radius:9px;"));
    auto *cancelAll = makeButton(tr("Отменить всё"), QStringLiteral("danger"));
    cancelAll->setStyleSheet(QStringLiteral("min-height:32px;border-radius:9px;"));
    tools->addWidget(clear);
    tools->addWidget(cancelAll);
    panel->addLayout(tools);

    auto *strip = new QFrame;
    strip->setObjectName(QStringLiteral("policyStrip"));
    strip->setStyleSheet(QStringLiteral(
        "#policyStrip { background: rgba(255,255,255,0.03);"
        " border: 1px solid rgba(255,255,255,0.07); border-radius: 12px; }"));
    auto *stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(12, 5, 12, 5);
    stripLayout->setSpacing(10);
    auto *stripIcon = new QLabel;
    stripIcon->setPixmap(Icons::pixmap(Icon::File, Theme::Accent, 16));
    stripLayout->addWidget(stripIcon);
    stripLayout->addWidget(makeLabel(tr("Если файл уже есть"), 12.5, Theme::Text, QFont::DemiBold));
    m_policy = new Segmented(policyNames);
    m_policy->setFixedSize(470, 32);
    stripLayout->addWidget(m_policy);
    m_policyHint = makeLabel(QString(), 12, Theme::TextSecondary);
    m_policyHint->setMinimumWidth(0);
    m_policyHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    stripLayout->addWidget(m_policyHint, 1);
    panel->addWidget(strip);

    m_proxy = new FilterProxy(this);
    m_proxy->setSourceModel(m_model);
    auto *list = new ListView;
    m_list = list;
    m_list->setModel(m_proxy);
    m_list->setItemDelegate(new TransferDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->viewport()->installEventFilter(this);
    m_list->viewport()->setAutoFillBackground(false);
    panel->addWidget(m_list, 1);
    setFixedHeight(BarHeight);

    TransferPrefs &prefs = TransferPrefs::instance();
    showPolicy(prefs.policy());
    showConnections(prefs.connections());

    connect(m_filter, &Segmented::changed, this, &TransferDock::applyFilter);
    const auto pickPolicy = [](int index) {
        TransferPrefs::instance().setPolicy(static_cast<OverwritePolicy>(index));
    };
    connect(m_policy, &Segmented::changed, this, pickPolicy);
    connect(m_policyChip, &Dropdown::changed, this, pickPolicy);
    connect(m_connections, &Dropdown::changed, this, [](int index) {
        TransferPrefs::instance().setConnections(
            TransferPrefs::connectionChoices().value(index, 4));
    });
    connect(&prefs, &TransferPrefs::policyChanged, this, &TransferDock::showPolicy);
    connect(&prefs, &TransferPrefs::connectionsChanged, this, &TransferDock::showConnections);
    connect(clear, &QPushButton::clicked, m_model, &TransferModel::clearFinished);
    connect(cancelAll, &QPushButton::clicked, this, &TransferDock::cancelAllRequested);
    connect(m_model, &TransferModel::summaryChanged, this, &TransferDock::refreshSummary);

    m_animation = new FrameAnimation(this);
    connect(m_animation, &FrameAnimation::valueChanged, this,
            [this](qreal value) { setPanelHeight(qRound(value)); });

    m_progressAnimation = new FrameAnimation(this);
    connect(m_progressAnimation, &FrameAnimation::valueChanged, this, [this](qreal value) {
        m_shownProgress = value;
        update(QRect(0, 0, width(), 8));
    });
    refreshSummary();
}

QSize TransferDock::sizeHint() const
{
    return {600, BarHeight};
}

void TransferDock::setPanelHeight(int height)
{
    m_panelHeight = height;
    placeDrawer();
    emit heightChanged(BarHeight + height);
}

void TransferDock::toggle()
{
    setExpanded(!m_expanded);
}

void TransferDock::setExpanded(bool expanded)
{
    m_expanded = expanded;
    if (expanded)
        setVisible(true);
    m_toggle->setIcon(expanded ? Icon::ChevronDown : Icon::ChevronUp);
    m_animation->start(m_panelHeight, expanded ? PanelHeight : 0, 240, QEasingCurve::OutCubic);
}

void TransferDock::applyFilter()
{
    static_cast<FilterProxy *>(m_proxy)->filter = m_filter->current();
    m_proxy->invalidate();
}

void TransferDock::showPolicy(OverwritePolicy policy)
{
    const int index = static_cast<int>(policy);
    m_policy->setCurrent(index);
    m_policyChip->setCurrent(index);
    m_policyHint->setText(TransferPrefs::policyHint(policy));
    m_policyHint->setToolTip(m_policyHint->text());
}

void TransferDock::showConnections(int connections)
{
    m_connections->setCurrent(
        std::max<int>(0, TransferPrefs::connectionChoices().indexOf(connections)));
}

void TransferDock::refreshSummary()
{
    const int active = m_model->activeCount();
    const int failed = m_model->failedCount();
    const qint64 speed = m_model->speed();
    QStringList parts;
    if (active > 0) {
        parts.append(tr("активно: %1").arg(active));
        if (speed > 0)
            parts.append(Format::speed(speed));
        const qint64 remaining = m_model->remainingBytes();
        if (speed > 0 && remaining > 0)
            parts.append(tr("осталось %1").arg(Format::duration(remaining / speed)));
    } else if (m_model->rowCount() > 0) {
        parts.append(tr("всё завершено"));
    } else {
        parts.append(tr("перетащите файлы в окно — передача начнётся сама"));
    }
    if (failed > 0)
        parts.append(tr("ошибок: %1").arg(failed));
    m_summary->setText(parts.join(QStringLiteral("  ·  ")));
    m_progress = m_model->progress();
    if (m_progress < 0) {
        m_progressAnimation->stop();
        m_shownProgress = -1;
    } else if (std::abs(m_progress - m_shownProgress) > 0.0005) {
        m_progressAnimation->start(m_shownProgress < 0 ? 0.0 : m_shownProgress, m_progress, 450,
                                   QEasingCurve::OutCubic);
    }
    if (m_proxy)
        m_proxy->invalidate();
    update();
}

void TransferDock::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    Glass::paintPanel(painter, rect(), Theme::RadiusLarge, Glass::Tone::Bar);
    if (m_shownProgress >= 0) {
        const QRectF track(18, 2, width() - 36, 2.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 14));
        painter.drawRoundedRect(track, 1.25, 1.25);
        QRectF filled = track;
        filled.setWidth(track.width() * std::clamp(m_shownProgress, 0.0, 1.0));
        QLinearGradient gradient(filled.topLeft(), filled.topRight());
        gradient.setColorAt(0, QColor(200, 30, 58));
        gradient.setColorAt(1, Theme::AccentHover);
        painter.setBrush(gradient);
        painter.drawRoundedRect(filled, 1.25, 1.25);
    }
}

void TransferDock::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
}

void TransferDock::setOverlayHost(QWidget *host)
{
    m_host = host;
    m_drawer->setParent(host);
    m_drawer->hide();
    placeDrawer();
}

void TransferDock::placeDrawer()
{
    if (!m_host)
        return;
    if (m_panelHeight <= 0 || !isVisible()) {
        m_drawer->hide();
        return;
    }
    const QPoint origin = mapTo(m_host, QPoint(0, 0));
    m_drawer->setGeometry(origin.x(), origin.y() - 10 - m_panelHeight, width(), m_panelHeight);
    if (m_panel->size() != QSize(width(), PanelHeight))
        m_panel->setGeometry(0, 0, width(), PanelHeight);
    if (!m_drawer->isVisible())
        m_drawer->show();
    m_drawer->raise();
}

void TransferDock::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeDrawer();
}

void TransferDock::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    placeDrawer();
}

void TransferDock::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_drawer->hide();
}

bool TransferDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_bar && event->type() == QEvent::MouseButtonRelease) {
        toggle();
        return true;
    }
    if (m_list && watched == m_list->viewport() &&
        event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QModelIndex index = m_list->indexAt(mouse->position().toPoint());
        if (!index.isValid())
            return false;
        const QRect row = m_list->visualRect(index);
        const TransferStatus status = index.data(TransferModel::StatusRole).value<TransferStatus>();
        if (TransferDelegate::actionRect(row).contains(mouse->position().toPoint())) {
            if (!TransferModel::finished(status.state))
                emit cancelRequested(status.sessionId, status.id);
            else if (status.state == TransferState::Failed ||
                     status.state == TransferState::Canceled)
                emit retryRequested(status);
            else if (!status.upload)
                emit revealRequested(status);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
