#include "filelist.h"

#include "format.h"
#include "icons.h"
#include "theme.h"

#include <QDataStream>
#include <QDrag>
#include <QFileInfo>
#include <QHeaderView>
#include <QIODevice>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QUrl>

#include <algorithm>
#include <set>

namespace Mime {
QMimeData *remote(quint64 sessionId, const RemoteEntries &entries)
{
    auto *data = new QMimeData;
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream << sessionId << static_cast<qint32>(entries.size());
    for (const RemoteEntry &entry : entries) {
        stream << entry.name << entry.path << entry.size << entry.modified
               << entry.permissions << entry.directory << entry.symlink;
    }
    data->setData(Remote, encoded);
    return data;
}

RemoteEntries remoteEntries(const QMimeData *data, quint64 *sessionId)
{
    RemoteEntries entries;
    if (!data || !data->hasFormat(Remote))
        return entries;
    QByteArray encoded = data->data(Remote);
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    quint64 session = 0;
    qint32 count = 0;
    stream >> session >> count;
    if (sessionId)
        *sessionId = session;
    for (qint32 i = 0; i < count && stream.status() == QDataStream::Ok; ++i) {
        RemoteEntry entry;
        stream >> entry.name >> entry.path >> entry.size >> entry.modified >>
            entry.permissions >> entry.directory >> entry.symlink;
        entries.append(entry);
    }
    return entries;
}
}

FileListModel::FileListModel(bool remote, QObject *parent)
    : QAbstractTableModel(parent), m_remote(remote)
{
}

void FileListModel::setEntries(const RemoteEntries &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void FileListModel::clear()
{
    setEntries({});
}

const RemoteEntry *FileListModel::entry(int row) const
{
    return row >= 0 && row < m_entries.size() ? &m_entries.at(row) : nullptr;
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int FileListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    const RemoteEntry *item = entry(index.row());
    if (!item)
        return {};
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn:
            return item->name;
        case SizeColumn:
            return item->directory ? QString() : Format::size(item->size);
        case ModifiedColumn:
            return Format::timestamp(item->modified);
        default:
            return {};
        }
    case Qt::ToolTipRole:
        return item->path;
    case DirectoryRole:
        return item->directory;
    case EntryIndexRole:
        return index.row();
    case SortRole:
        switch (index.column()) {
        case NameColumn:
            return item->name.toLower();
        case SizeColumn:
            return item->directory ? -1 : item->size;
        case ModifiedColumn:
            return item->modified;
        default:
            return {};
        }
    default:
        return {};
    }
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::DisplayRole) {
        switch (section) {
        case NameColumn:
            return tr("ИМЯ");
        case SizeColumn:
            return tr("РАЗМЕР");
        case ModifiedColumn:
            return tr("ИЗМЕНЁН");
        default:
            break;
        }
    }
    if (role == Qt::TextAlignmentRole && section == SizeColumn)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

Qt::ItemFlags FileListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
           Qt::ItemIsDropEnabled;
}

QStringList FileListModel::mimeTypes() const
{
    return m_remote ? QStringList{Mime::Remote} : QStringList{QStringLiteral("text/uri-list")};
}

QMimeData *FileListModel::mimeData(const QModelIndexList &indexes) const
{
    std::set<int> rows;
    for (const QModelIndex &index : indexes)
        rows.insert(index.row());
    RemoteEntries chosen;
    for (const int row : rows) {
        if (const RemoteEntry *item = entry(row))
            chosen.append(*item);
    }
    if (m_remote)
        return Mime::remote(m_sessionId, chosen);
    auto *data = new QMimeData;
    QList<QUrl> urls;
    for (const RemoteEntry &item : chosen)
        urls.append(QUrl::fromLocalFile(item.path));
    data->setUrls(urls);
    data->setData(Mime::Local, QByteArrayLiteral("1"));
    return data;
}

void FileFilterProxy::setNameFilter(const QString &filter)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filter = filter.trimmed();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_filter = filter.trimmed();
    invalidateFilter();
#endif
}

bool FileFilterProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if (m_filter.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(row, 0, parent);
    return index.data(Qt::DisplayRole).toString().contains(m_filter, Qt::CaseInsensitive);
}

bool FileFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const bool leftDirectory = left.data(FileListModel::DirectoryRole).toBool();
    const bool rightDirectory = right.data(FileListModel::DirectoryRole).toBool();
    if (leftDirectory != rightDirectory) {
        return sortOrder() == Qt::AscendingOrder ? leftDirectory : rightDirectory;
    }
    const QVariant a = left.data(FileListModel::SortRole);
    const QVariant b = right.data(FileListModel::SortRole);
    if (left.column() == FileListModel::NameColumn)
        return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
    return QVariant::compare(a, b) == QPartialOrdering::Less;
}

QSize FileDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return {80, Theme::RowHeight};
}

void FileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const bool selected = option.state & QStyle::State_Selected;
    const QRect r = option.rect;
    const QString text = index.data(Qt::DisplayRole).toString();

    if (index.column() == FileListModel::NameColumn) {
        const bool directory = index.data(FileListModel::DirectoryRole).toBool();
        const QRectF iconRect(r.left() + 16, r.center().y() - 9.0, 18, 18);
        if (directory)
            Icons::paintFolder(*painter, iconRect);
        else
            Icons::paintFile(*painter, iconRect, QString());
        painter->setFont(Theme::font(13, directory ? QFont::Medium : QFont::Normal));
        painter->setPen(selected ? QColor(Qt::white) : Theme::Text);
        const QRect textRect(r.left() + 44, r.top(), r.width() - 52, r.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(text, Qt::ElideMiddle,
                                                            textRect.width()));
    } else {
        painter->setFont(Theme::font(12.5));
        painter->setPen(selected ? QColor(255, 255, 255, 210) : Theme::TextMuted);
        const QRect textRect = r.adjusted(14, 0, -16, 0);
        const Qt::Alignment align = index.column() == FileListModel::SizeColumn
                                        ? Qt::AlignRight | Qt::AlignVCenter
                                        : Qt::AlignLeft | Qt::AlignVCenter;
        painter->drawText(textRect, align, text);
    }
    painter->restore();
}

FileView::FileView(QWidget *parent) : QTreeView(parent)
{
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setUniformRowHeights(true);
    setSortingEnabled(true);
    setAllColumnsShowFocus(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
    setDropIndicatorShown(false);
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setItemDelegate(new FileDelegate(this));
    setContextMenuPolicy(Qt::CustomContextMenu);
    viewport()->setAttribute(Qt::WA_Hover);
    QPalette clear = palette();
    clear.setColor(QPalette::Highlight, Qt::transparent);
    clear.setColor(QPalette::HighlightedText, Theme::Text);
    setPalette(clear);
    viewport()->setAutoFillBackground(false);
    setAutoFillBackground(false);
    header()->setHighlightSections(false);
    header()->setSectionsMovable(false);
    header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header()->setMinimumSectionSize(60);
    verticalScrollBar()->setSingleStep(18);
    connect(this, &QTreeView::doubleClicked, this, &FileView::openRequested);
}

int FileView::rowAt(const QPoint &viewportPoint) const
{
    const QModelIndex index = indexAt(viewportPoint);
    return index.isValid() ? index.row() : -1;
}

void FileView::setDropRow(int row)
{
    if (row == m_dropRow)
        return;
    m_dropRow = row;
    viewport()->update();
}

void FileView::drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const
{
    const int row = index.row();
    const bool selected = selectionModel() && selectionModel()->isRowSelected(row, index.parent());
    const bool hovered = row == m_hoverRow;
    const bool dropTarget = row == m_dropRow;
    const QRectF r = QRectF(option.rect).adjusted(6, 1, -6, -1);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (dropTarget) {
        painter->setPen(QPen(Theme::Accent, 1.4));
        painter->setBrush(QColor(255, 66, 86, 46));
        painter->drawRoundedRect(r, 9, 9);
    } else if (selected) {
        QLinearGradient fill(r.topLeft(), r.topRight());
        fill.setColorAt(0, QColor(255, 66, 86, 70));
        fill.setColorAt(1, QColor(255, 66, 86, 30));
        painter->setPen(QPen(QColor(255, 90, 108, 90), 1));
        painter->setBrush(fill);
        painter->drawRoundedRect(r, 9, 9);
    } else if (hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 13));
        painter->drawRoundedRect(r, 9, 9);
    }
    painter->restore();
    QStyleOptionViewItem plain = option;
    plain.state &= ~(QStyle::State_MouseOver | QStyle::State_HasFocus);
    QTreeView::drawRow(painter, plain, index);
}

void FileView::mouseMoveEvent(QMouseEvent *event)
{
    const int row = rowAt(event->position().toPoint());
    if (row != m_hoverRow) {
        m_hoverRow = row;
        viewport()->update();
    }
    QTreeView::mouseMoveEvent(event);
}

void FileView::leaveEvent(QEvent *event)
{
    m_hoverRow = -1;
    viewport()->update();
    QTreeView::leaveEvent(event);
}

void FileView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (currentIndex().isValid())
            emit openRequested(currentIndex());
        return;
    }
    if (event->key() == Qt::Key_Backspace) {
        emit backRequested();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void FileView::startDrag(Qt::DropActions actions)
{
    Q_UNUSED(actions);
    const QModelIndexList indexes = selectionModel()->selectedRows();
    if (indexes.isEmpty())
        return;
    QMimeData *mime = model()->mimeData(indexes);
    if (!mime)
        return;
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);

    const QString label = indexes.size() == 1 ? indexes.first().data().toString()
                                              : tr("%1 объектов").arg(indexes.size());
    const QFont font = Theme::font(13, QFont::Medium);
    const QFontMetrics metrics(font);
    const int width = std::min(320, metrics.horizontalAdvance(label) + 52);
    const qreal ratio = devicePixelRatioF();
    QPixmap chip(QSize(width, 34) * ratio);
    chip.setDevicePixelRatio(ratio);
    chip.fill(Qt::transparent);
    {
        QPainter p(&chip);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(255, 255, 255, 50), 1));
        p.setBrush(QColor(22, 22, 26, 235));
        p.drawRoundedRect(QRectF(0.5, 0.5, width - 1, 33), 11, 11);
        const bool directory = indexes.first().data(FileListModel::DirectoryRole).toBool();
        if (directory || indexes.size() > 1)
            Icons::paintFolder(p, QRectF(10, 8, 18, 18));
        else
            Icons::paintFile(p, QRectF(10, 8, 18, 18), QString());
        p.setFont(font);
        p.setPen(Theme::Text);
        p.drawText(QRectF(36, 0, width - 46, 34), Qt::AlignVCenter,
                   metrics.elidedText(label, Qt::ElideMiddle, width - 46));
    }
    drag->setPixmap(chip);
    drag->setHotSpot(QPoint(18, 17));
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}
