#include "transferpanel.h"

#include "format.h"
#include "theme.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QWidget *statTile(const QString &caption, QLabel **valueTarget, QWidget *parent)
{
    auto *tile = new QWidget(parent);
    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto *title = new QLabel(caption);
    title->setObjectName(QStringLiteral("statTitle"));
    auto *value = new QLabel(QStringLiteral("0"));
    value->setObjectName(QStringLiteral("statValue"));
    layout->addWidget(title);
    layout->addWidget(value);
    *valueTarget = value;
    return tile;
}
}

TransferModel::TransferModel(QObject *parent) : QAbstractTableModel(parent) {}

int TransferModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int TransferModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

bool TransferModel::isFinished(TransferState state)
{
    return state == TransferState::Completed || state == TransferState::Failed ||
           state == TransferState::Canceled || state == TransferState::Skipped;
}

QString TransferModel::stateText(TransferState state)
{
    switch (state) {
    case TransferState::Queued:
        return tr("В очереди");
    case TransferState::Running:
        return tr("Идёт передача");
    case TransferState::Completed:
        return tr("Готово");
    case TransferState::Failed:
        return tr("Ошибка");
    case TransferState::Canceled:
        return tr("Отменено");
    case TransferState::Skipped:
        return tr("Пропущено");
    }
    return {};
}

QVariant TransferModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const TransferStatus &status = m_rows.at(index.row());

    if (role == Qt::UserRole)
        return QVariant::fromValue(status.id);
    if (role == Qt::UserRole + 1)
        return status.total > 0
                   ? static_cast<int>(status.done * 100 / status.total)
                   : (status.state == TransferState::Completed ? 100 : 0);
    if (role == Qt::UserRole + 2) {
        return status.total > 0
                   ? QStringLiteral("%1 / %2").arg(Format::size(status.done),
                                                   Format::size(status.total))
                   : Format::size(status.done);
    }
    if (role == Qt::ToolTipRole) {
        const QString detail =
            status.detail.isEmpty() ? QString()
                                    : QStringLiteral("\n%1").arg(status.detail);
        return QStringLiteral("%1\n%2%3")
            .arg(status.localPath, status.remotePath, detail);
    }
    if (role == Qt::TextAlignmentRole && index.column() == SpeedColumn)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    if (role == Qt::ForegroundRole && index.column() == StateColumn) {
        switch (status.state) {
        case TransferState::Failed:
            return Theme::palette().danger;
        case TransferState::Completed:
            return Theme::palette().success;
        case TransferState::Canceled:
        case TransferState::Skipped:
            return Theme::palette().textMuted;
        default:
            return {};
        }
    }

    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case NameColumn:
        return QStringLiteral("%1  %2")
            .arg(status.upload ? QStringLiteral("↑") : QStringLiteral("↓"),
                 status.name);
    case ProgressColumn:
        return {};
    case SpeedColumn:
        return status.state == TransferState::Running
                   ? Format::speed(status.bytesPerSecond)
                   : QStringLiteral("—");
    case StateColumn:
        return stateText(status.state);
    default:
        return {};
    }
}

QVariant TransferModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case NameColumn:
        return tr("Объект");
    case ProgressColumn:
        return tr("Прогресс");
    case SpeedColumn:
        return tr("Скорость");
    case StateColumn:
        return tr("Состояние");
    default:
        return {};
    }
}

void TransferModel::upsert(const TransferStatus &status)
{
    const auto existing = m_rowById.constFind(status.id);
    if (existing != m_rowById.cend()) {
        const int row = existing.value();
        m_rows[row] = status;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        emit summaryChanged();
        return;
    }
    const int row = static_cast<int>(m_rows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_rows.append(status);
    m_rowById.insert(status.id, row);
    endInsertRows();
    emit summaryChanged();
}

void TransferModel::removeFinished()
{
    TransferStatuses remaining;
    remaining.reserve(m_rows.size());
    for (const TransferStatus &status : m_rows) {
        if (!isFinished(status.state))
            remaining.append(status);
    }
    if (remaining.size() == m_rows.size())
        return;
    beginResetModel();
    m_rows = remaining;
    m_rowById.clear();
    for (int row = 0; row < m_rows.size(); ++row)
        m_rowById.insert(m_rows.at(row).id, row);
    endResetModel();
    emit summaryChanged();
}

const TransferStatus *TransferModel::statusAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr;
}

int TransferModel::activeCount() const
{
    int count = 0;
    for (const TransferStatus &status : m_rows) {
        if (status.state == TransferState::Running)
            ++count;
    }
    return count;
}

int TransferModel::queuedCount() const
{
    int count = 0;
    for (const TransferStatus &status : m_rows) {
        if (status.state == TransferState::Queued)
            ++count;
    }
    return count;
}

int TransferModel::failedCount() const
{
    int count = 0;
    for (const TransferStatus &status : m_rows) {
        if (status.state == TransferState::Failed)
            ++count;
    }
    return count;
}

qint64 TransferModel::aggregateSpeed() const
{
    qint64 total = 0;
    for (const TransferStatus &status : m_rows) {
        if (status.state == TransferState::Running)
            total += status.bytesPerSecond;
    }
    return total;
}

int TransferModel::aggregateProgress() const
{
    qint64 done = 0;
    qint64 total = 0;
    bool active = false;
    for (const TransferStatus &status : m_rows) {
        if (status.state != TransferState::Running &&
            status.state != TransferState::Queued)
            continue;
        active = true;
        if (status.total > 0) {
            done += status.done;
            total += status.total;
        }
    }
    if (!active)
        return -1;
    return total > 0 ? static_cast<int>(done * 100 / total) : 0;
}

TransferProgressDelegate::TransferProgressDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void TransferProgressDelegate::paint(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    if (index.column() != TransferModel::ProgressColumn) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem adjusted(option);
    initStyleOption(&adjusted, index);
    if (adjusted.state & QStyle::State_Selected)
        painter->fillRect(option.rect, adjusted.palette.highlight());

    const ThemePalette &palette = Theme::palette();
    const int percent = qBound(0, index.data(Qt::UserRole + 1).toInt(), 100);
    const QString caption = index.data(Qt::UserRole + 2).toString();

    QRect track = option.rect.adjusted(6, 0, -6, 0);
    track.setHeight(16);
    track.moveTop(option.rect.top() + (option.rect.height() - 16) / 2);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(palette.surfaceRaised);
    painter->drawRoundedRect(track, 4, 4);
    if (percent > 0) {
        QRect fill = track;
        fill.setWidth(track.width() * percent / 100);
        painter->setBrush(palette.accent);
        painter->drawRoundedRect(fill, 4, 4);
    }
    painter->setPen(percent > 55 ? palette.accentText : palette.textSecondary);
    QFont font = painter->font();
    font.setPointSizeF(font.pointSizeF() - 1.0);
    painter->setFont(font);
    painter->drawText(track, Qt::AlignCenter, caption);
    painter->restore();
}

QSize TransferProgressDelegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    QSize hint = QStyledItemDelegate::sizeHint(option, index);
    hint.setHeight(std::max(hint.height(), 30));
    return hint;
}

TransferPanel::TransferPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::SpaceLg);

    auto *summary = new QFrame;
    summary->setObjectName(QStringLiteral("transferCard"));
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(Theme::SpaceXl, Theme::SpaceLg,
                                      Theme::SpaceXl, Theme::SpaceLg);
    summaryLayout->setSpacing(Theme::SpaceXl * 2);
    summaryLayout->addWidget(statTile(tr("АКТИВНЫХ"), &m_activeValue, summary));
    summaryLayout->addWidget(statTile(tr("В ОЧЕРЕДИ"), &m_queuedValue, summary));
    summaryLayout->addWidget(statTile(tr("СКОРОСТЬ"), &m_speedValue, summary));
    summaryLayout->addWidget(statTile(tr("ОШИБОК"), &m_failedValue, summary));
    summaryLayout->addStretch();

    auto *policyColumn = new QVBoxLayout;
    policyColumn->setSpacing(2);
    auto *policyCaption = new QLabel(tr("ЕСЛИ ФАЙЛ УЖЕ ЕСТЬ"));
    policyCaption->setObjectName(QStringLiteral("statTitle"));
    m_policy = new QComboBox;
    m_policy->addItem(tr("Продолжить"), static_cast<int>(OverwritePolicy::Resume));
    m_policy->addItem(tr("Перезаписать"),
                      static_cast<int>(OverwritePolicy::Overwrite));
    m_policy->addItem(tr("Пропустить"), static_cast<int>(OverwritePolicy::Skip));
    m_policy->addItem(tr("Переименовать"),
                      static_cast<int>(OverwritePolicy::Rename));
    m_policy->setToolTip(
        tr("«Продолжить» дозагружает прерванную передачу и пропускает файлы, "
           "которые уже совпадают по размеру."));
    m_policy->setCurrentIndex(m_policy->findData(
        QSettings()
            .value(QStringLiteral("transfers/overwritePolicy"),
                   static_cast<int>(OverwritePolicy::Resume))
            .toInt()));
    policyColumn->addWidget(policyCaption);
    policyColumn->addWidget(m_policy);
    summaryLayout->addLayout(policyColumn);
    summaryLayout->addSpacing(Theme::SpaceLg);

    connect(m_policy, &QComboBox::currentIndexChanged, this, [this] {
        const OverwritePolicy policy = overwritePolicy();
        QSettings().setValue(QStringLiteral("transfers/overwritePolicy"),
                             static_cast<int>(policy));
        emit overwritePolicyChanged(policy);
    });

    m_cancelAll = new QPushButton(tr("Отменить всё"));
    m_cancelAll->setObjectName(QStringLiteral("dangerButton"));
    m_clearFinished = new QPushButton(tr("Очистить завершённые"));
    m_clearFinished->setObjectName(QStringLiteral("secondaryButton"));
    summaryLayout->addWidget(m_clearFinished);
    summaryLayout->addWidget(m_cancelAll);
    layout->addWidget(summary);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("fileCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("panelHeader"));

    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::SpaceLg, Theme::SpaceLg,
                                     Theme::SpaceLg, Theme::SpaceMd);
    headerLayout->setSpacing(2);
    auto *title = new QLabel(tr("Очередь передач"));
    title->setObjectName(QStringLiteral("panelTitle"));
    auto *meta = new QLabel(
        tr("Передачи идут по отдельному SSH-соединению, поэтому панели "
           "остаются отзывчивыми"));
    meta->setObjectName(QStringLiteral("panelMeta"));
    meta->setWordWrap(true);
    headerLayout->addWidget(title);
    headerLayout->addWidget(meta);
    cardLayout->addWidget(header);

    m_model = new TransferModel(this);
    m_view = new QTableView;
    m_view->setModel(m_model);
    m_view->setItemDelegate(new TransferProgressDelegate(this));
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setAlternatingRowColors(true);
    m_view->setShowGrid(false);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->verticalHeader()->hide();
    m_view->verticalHeader()->setDefaultSectionSize(Theme::RowHeight + 6);
    m_view->horizontalHeader()->setSectionResizeMode(TransferModel::NameColumn,
                                                     QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(
        TransferModel::ProgressColumn, QHeaderView::Fixed);
    m_view->horizontalHeader()->resizeSection(TransferModel::ProgressColumn, 220);
    m_view->horizontalHeader()->setSectionResizeMode(
        TransferModel::SpeedColumn, QHeaderView::ResizeToContents);
    m_view->horizontalHeader()->setSectionResizeMode(
        TransferModel::StateColumn, QHeaderView::ResizeToContents);
    cardLayout->addWidget(m_view, 1);

    auto *emptyHost = new QWidget;
    auto *emptyLayout = new QVBoxLayout(emptyHost);
    emptyLayout->setContentsMargins(Theme::SpaceXl, 0, Theme::SpaceXl, 0);
    emptyLayout->addStretch();
    auto *emptyIcon = new QLabel(QStringLiteral("⇅"));
    emptyIcon->setObjectName(QStringLiteral("emptyStateIcon"));
    emptyIcon->setAlignment(Qt::AlignCenter);
    m_emptyHint = new QLabel(tr("Очередь пуста. Перетащите файлы между "
                                "панелями, чтобы начать передачу."));
    m_emptyHint->setObjectName(QStringLiteral("emptyStateText"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addSpacing(Theme::SpaceSm);
    emptyLayout->addWidget(m_emptyHint);
    emptyLayout->addStretch();
    m_emptyHost = emptyHost;
    cardLayout->addWidget(emptyHost, 1);
    layout->addWidget(card, 1);

    connect(m_cancelAll, &QPushButton::clicked,
            this, &TransferPanel::cancelAllRequested);
    connect(m_clearFinished, &QPushButton::clicked,
            this, &TransferPanel::clearFinishedRequested);
    connect(m_model, &TransferModel::summaryChanged,
            this, &TransferPanel::refreshSummary);
    connect(m_view, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
                const QModelIndex index = m_view->indexAt(position);
                const TransferStatus *status = m_model->statusAt(index.row());
                if (!status)
                    return;
                QMenu menu(this);
                auto *cancel = menu.addAction(tr("Отменить"));
                cancel->setEnabled(!TransferModel::isFinished(status->state));
                auto *reveal = menu.addAction(tr("Показать папку назначения"));
                const quint64 id = status->id;
                const TransferStatus copy = *status;
                connect(cancel, &QAction::triggered, this,
                        [this, id] { emit cancelRequested(id); });
                connect(reveal, &QAction::triggered, this,
                        [this, copy] { emit revealRequested(copy); });
                menu.exec(m_view->viewport()->mapToGlobal(position));
            });

    refreshSummary();
}

void TransferPanel::applyStatus(const TransferStatus &status)
{
    m_model->upsert(status);
}

OverwritePolicy TransferPanel::overwritePolicy() const
{
    return static_cast<OverwritePolicy>(m_policy->currentData().toInt());
}

void TransferPanel::refreshSummary()
{
    const int active = m_model->activeCount();
    const int queued = m_model->queuedCount();
    const int failed = m_model->failedCount();
    m_activeValue->setText(QString::number(active));
    m_queuedValue->setText(QString::number(queued));
    m_speedValue->setText(Format::speed(m_model->aggregateSpeed()));
    m_failedValue->setText(QString::number(failed));
    const bool empty = m_model->rowCount() == 0;
    m_emptyHost->setVisible(empty);
    m_view->setVisible(!empty);
    m_cancelAll->setEnabled(active + queued > 0);
    m_clearFinished->setEnabled(m_model->rowCount() > active + queued);
}
