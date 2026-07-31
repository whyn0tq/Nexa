#pragma once

#include "types.h"

#include <QAbstractTableModel>
#include <QIcon>

class QMimeData;

class RemoteFileModel final : public QAbstractTableModel {
    Q_OBJECT

public:
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
    static QString remoteMimeType();
    static RemoteEntries entriesFromMimeData(const QMimeData *mimeData);

private:
    static QString humanSize(qint64 bytes);

    RemoteEntries m_entries;
    QIcon m_folderIcon;
    QIcon m_fileIcon;
};
