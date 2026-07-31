#include "remotefilemodel.h"

#include "format.h"

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QMimeData>
#include <QSet>
#include <QStyle>
#include <QTemporaryFile>

RemoteFileModel::RemoteFileModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_folderIcon(m_iconProvider.icon(QFileIconProvider::Folder)),
      m_fileIcon(m_iconProvider.icon(QFileIconProvider::File)),
      m_linkIcon(qApp->style()->standardIcon(QStyle::SP_FileLinkIcon))
{
}

int RemoteFileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int RemoteFileModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QIcon RemoteFileModel::iconFor(const RemoteEntry &entry) const
{
    if (entry.directory)
        return m_folderIcon;
    if (entry.symlink)
        return m_linkIcon;
    const QString suffix = QFileInfo(entry.name).suffix().toLower();
    if (suffix.isEmpty())
        return m_fileIcon;
    const auto cached = m_suffixIcons.constFind(suffix);
    if (cached != m_suffixIcons.cend())
        return cached.value();

    QIcon icon = m_fileIcon;
    QTemporaryFile probe(QDir::tempPath() +
                         QStringLiteral("/nexa-icon-XXXXXX.") + suffix);
    if (probe.open()) {
        const QIcon resolved = m_iconProvider.icon(QFileInfo(probe.fileName()));
        if (!resolved.isNull())
            icon = resolved;
    }
    m_suffixIcons.insert(suffix, icon);
    return icon;
}

QVariant RemoteFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const RemoteEntry &entry = m_entries.at(index.row());

    switch (role) {
    case Qt::DecorationRole:
        return index.column() == NameColumn ? QVariant(iconFor(entry))
                                            : QVariant();
    case Qt::TextAlignmentRole:
        return index.column() == SizeColumn
                   ? QVariant(static_cast<int>(Qt::AlignRight | Qt::AlignVCenter))
                   : QVariant();
    case Qt::ToolTipRole:
        return entry.path;
    case IsDirectoryRole:
        return entry.directory;
    case SortRole:
        switch (index.column()) {
        case NameColumn:
            return entry.name.toLower();
        case SizeColumn:
            return entry.directory ? qint64(-1) : entry.size;
        case ModifiedColumn:
            return entry.modified.isValid() ? entry.modified.toSecsSinceEpoch()
                                            : qint64(0);
        case TypeColumn:
            return QFileInfo(entry.name).suffix().toLower();
        default:
            return {};
        }
    case Qt::DisplayRole:
        break;
    default:
        return {};
    }

    switch (index.column()) {
    case NameColumn:
        return entry.name;
    case SizeColumn:
        return entry.directory ? QStringLiteral("—") : Format::size(entry.size);
    case ModifiedColumn:
        return Format::timestamp(entry.modified);
    case TypeColumn:
        if (entry.directory)
            return tr("Папка");
        if (entry.symlink)
            return tr("Ссылка");
        {
            const QString suffix = QFileInfo(entry.name).suffix().toUpper();
            return suffix.isEmpty() ? tr("Файл") : suffix;
        }
    default:
        return {};
    }
}

QVariant RemoteFileModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case NameColumn:
        return tr("Имя");
    case SizeColumn:
        return tr("Размер");
    case ModifiedColumn:
        return tr("Изменён");
    case TypeColumn:
        return tr("Тип");
    default:
        return {};
    }
}

Qt::ItemFlags RemoteFileModel::flags(const QModelIndex &index) const
{
    return QAbstractTableModel::flags(index) |
           (index.isValid() ? Qt::ItemIsDragEnabled : Qt::NoItemFlags);
}

QStringList RemoteFileModel::mimeTypes() const
{
    return {remoteMimeType()};
}

QMimeData *RemoteFileModel::mimeData(const QModelIndexList &indexes) const
{
    QSet<int> rows;
    for (const QModelIndex &index : indexes) {
        if (index.isValid())
            rows.insert(index.row());
    }

    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream << static_cast<quint32>(rows.size());
    for (const int row : rows) {
        if (row < 0 || row >= m_entries.size())
            continue;
        const RemoteEntry &entry = m_entries.at(row);
        stream << entry.name << entry.path << entry.size << entry.modified
               << entry.directory << entry.symlink;
    }
    auto *result = new QMimeData;
    result->setData(remoteMimeType(), encoded);
    return result;
}

Qt::DropActions RemoteFileModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QString RemoteFileModel::remoteMimeType()
{
    return QStringLiteral("application/x-nexa-remote-entries");
}

RemoteEntries RemoteFileModel::entriesFromMimeData(const QMimeData *mimeData)
{
    RemoteEntries result;
    if (!mimeData || !mimeData->hasFormat(remoteMimeType()))
        return result;
    QByteArray encoded = mimeData->data(remoteMimeType());
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    quint32 count = 0;
    stream >> count;
    if (count > 100000)
        return {};
    result.reserve(static_cast<qsizetype>(count));
    for (quint32 i = 0; i < count && stream.status() == QDataStream::Ok; ++i) {
        RemoteEntry entry;
        stream >> entry.name >> entry.path >> entry.size >> entry.modified >>
            entry.directory >> entry.symlink;
        result.append(entry);
    }
    return stream.status() == QDataStream::Ok ? result : RemoteEntries{};
}

void RemoteFileModel::setEntries(const RemoteEntries &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void RemoteFileModel::appendEntries(const RemoteEntries &entries)
{
    if (entries.isEmpty())
        return;
    const int first = static_cast<int>(m_entries.size());
    const int last = first + static_cast<int>(entries.size()) - 1;
    beginInsertRows(QModelIndex(), first, last);
    m_entries.reserve(m_entries.size() + entries.size());
    for (const RemoteEntry &entry : entries)
        m_entries.append(entry);
    endInsertRows();
}

void RemoteFileModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

const RemoteEntry *RemoteFileModel::entryAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? &m_entries.at(row) : nullptr;
}

RemoteSortFilterProxy::RemoteSortFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(RemoteFileModel::SortRole);
    setDynamicSortFilter(true);
}

void RemoteSortFilterProxy::setNameFilter(const QString &filter)
{
    const QString trimmed = filter.trimmed();
    if (trimmed == m_nameFilter)
        return;
    beginFilterChange();
    m_nameFilter = trimmed;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool RemoteSortFilterProxy::lessThan(const QModelIndex &left,
                                     const QModelIndex &right) const
{
    const bool leftDirectory =
        left.data(RemoteFileModel::IsDirectoryRole).toBool();
    const bool rightDirectory =
        right.data(RemoteFileModel::IsDirectoryRole).toBool();
    if (leftDirectory != rightDirectory) {
        return sortOrder() == Qt::AscendingOrder ? leftDirectory : rightDirectory;
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

bool RemoteSortFilterProxy::filterAcceptsRow(int row,
                                             const QModelIndex &parent) const
{
    if (m_nameFilter.isEmpty())
        return true;
    const QModelIndex index =
        sourceModel()->index(row, RemoteFileModel::NameColumn, parent);
    return index.data(Qt::DisplayRole)
        .toString()
        .contains(m_nameFilter, Qt::CaseInsensitive);
}
