#include "remotefilemodel.h"

#include <QApplication>
#include <QDataStream>
#include <QIODevice>
#include <QMimeData>
#include <QSet>
#include <QStyle>

RemoteFileModel::RemoteFileModel(QObject *parent)
    : QAbstractTableModel(parent),
      m_folderIcon(qApp->style()->standardIcon(QStyle::SP_DirIcon)),
      m_fileIcon(qApp->style()->standardIcon(QStyle::SP_FileIcon))
{
}

int RemoteFileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int RemoteFileModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant RemoteFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= m_entries.size())
        return {};

    const auto &entry = m_entries.at(index.row());
    if (role == Qt::DecorationRole && index.column() == 0)
        return entry.directory ? m_folderIcon : m_fileIcon;

    if (role == Qt::TextAlignmentRole && index.column() == 1)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    if (role != Qt::DisplayRole)
        return {};

    switch (index.column()) {
    case 0:
        return entry.name;
    case 1:
        return entry.directory ? QStringLiteral("—") : humanSize(entry.size);
    case 2:
        return entry.modified.isValid()
            ? entry.modified.toString(QStringLiteral("dd.MM.yyyy HH:mm"))
            : QStringLiteral("—");
    case 3:
        return entry.directory ? tr("Папка")
                               : (entry.symlink ? tr("Ссылка") : tr("Файл"));
    default:
        return {};
    }
}

QVariant RemoteFileModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case 0: return tr("Имя");
    case 1: return tr("Размер");
    case 2: return tr("Изменён");
    case 3: return tr("Тип");
    default: return {};
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
    for (const auto &index : indexes) {
        if (index.isValid())
            rows.insert(index.row());
    }

    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream << static_cast<quint32>(rows.size());
    for (const int row : rows) {
        const auto &entry = m_entries.at(row);
        stream << entry.name << entry.path << entry.size
               << entry.modified << entry.directory << entry.symlink;
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
        stream >> entry.name >> entry.path >> entry.size
               >> entry.modified >> entry.directory >> entry.symlink;
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
    for (const auto &entry : entries)
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

QString RemoteFileModel::humanSize(qint64 bytes)
{
    static const QStringList units{
        tr("Б"), tr("КБ"), tr("МБ"), tr("ГБ"), tr("ТБ")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2")
        .arg(unit == 0 ? QString::number(bytes)
                       : QString::number(value, 'f', value < 10 ? 1 : 0),
             units.at(unit));
}
