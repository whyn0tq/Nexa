#pragma once

#include "types.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTreeView>

class QMimeData;

namespace Mime {
inline const QString Remote = QStringLiteral("application/x-nexa-remote");
inline const QString Local = QStringLiteral("application/x-nexa-local");
inline const QString Virtual = QStringLiteral("application/x-nexa-virtual-files");

QMimeData *remote(quint64 sessionId, const RemoteEntries &entries);
RemoteEntries remoteEntries(const QMimeData *data, quint64 *sessionId = nullptr);
}

class FileListModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { NameColumn, SizeColumn, ModifiedColumn, ColumnCount };
    enum Role { SortRole = Qt::UserRole + 1, DirectoryRole, EntryIndexRole };

    explicit FileListModel(bool remote, QObject *parent = nullptr);

    void setSessionId(quint64 id) { m_sessionId = id; }
    void setEntries(const RemoteEntries &entries);
    void clear();
    const RemoteEntries &entries() const { return m_entries; }
    const RemoteEntry *entry(int row) const;
    bool isRemote() const { return m_remote; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override { return Qt::CopyAction; }

private:
    RemoteEntries m_entries;
    quint64 m_sessionId = 0;
    bool m_remote;
};

class FileFilterProxy final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setNameFilter(const QString &filter);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    QString m_filter;
};

class FileDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

class FileView final : public QTreeView {
    Q_OBJECT

public:
    explicit FileView(QWidget *parent = nullptr);

    void setDropRow(int row);
    int rowAt(const QPoint &viewportPoint) const;

signals:
    void openRequested(const QModelIndex &index);
    void backRequested();

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void startDrag(Qt::DropActions actions) override;

private:
    int m_hoverRow = -1;
    int m_dropRow = -1;
};
