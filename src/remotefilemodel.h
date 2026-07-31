#pragma once

#include "types.h"

#include <QAbstractTableModel>
#include <QFileIconProvider>
#include <QHash>
#include <QIcon>
#include <QSortFilterProxyModel>

class QMimeData;

class RemoteFileModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { NameColumn, SizeColumn, ModifiedColumn, TypeColumn,
                  ColumnCount };

    static constexpr int SortRole = Qt::UserRole + 1;
    static constexpr int IsDirectoryRole = Qt::UserRole + 2;

    explicit RemoteFileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override;

    void setEntries(const RemoteEntries &entries);
    void appendEntries(const RemoteEntries &entries);
    void clear();
    const RemoteEntry *entryAt(int row) const;
    const RemoteEntries &entries() const { return m_entries; }
    static QString remoteMimeType();
    static RemoteEntries entriesFromMimeData(const QMimeData *mimeData);

private:
    QIcon iconFor(const RemoteEntry &entry) const;

    RemoteEntries m_entries;
    QFileIconProvider m_iconProvider;
    QIcon m_folderIcon;
    QIcon m_fileIcon;
    QIcon m_linkIcon;
    mutable QHash<QString, QIcon> m_suffixIcons;
};

class RemoteSortFilterProxy final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit RemoteSortFilterProxy(QObject *parent = nullptr);

    void setNameFilter(const QString &filter);

protected:
    bool lessThan(const QModelIndex &left,
                  const QModelIndex &right) const override;
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    QString m_nameFilter;
};
