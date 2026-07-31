#pragma once

#include "types.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QStyledItemDelegate>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableView;

class TransferModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { NameColumn, ProgressColumn, SpeedColumn, StateColumn,
                  ColumnCount };

    explicit TransferModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void upsert(const TransferStatus &status);
    void removeFinished();
    const TransferStatus *statusAt(int row) const;

    int activeCount() const;
    int queuedCount() const;
    int failedCount() const;
    qint64 aggregateSpeed() const;

    int aggregateProgress() const;

    static bool isFinished(TransferState state);
    static QString stateText(TransferState state);

signals:
    void summaryChanged();

private:
    TransferStatuses m_rows;
    QHash<quint64, int> m_rowById;
};

class TransferProgressDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit TransferProgressDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

class TransferPanel final : public QWidget {
    Q_OBJECT

public:
    explicit TransferPanel(QWidget *parent = nullptr);

    TransferModel *model() const { return m_model; }
    void applyStatus(const TransferStatus &status);

    OverwritePolicy overwritePolicy() const;

signals:
    void cancelRequested(quint64 id);
    void cancelAllRequested();
    void clearFinishedRequested();
    void revealRequested(const TransferStatus &status);
    void overwritePolicyChanged(OverwritePolicy policy);

private:
    void refreshSummary();

    TransferModel *m_model = nullptr;
    QTableView *m_view = nullptr;
    QLabel *m_activeValue = nullptr;
    QLabel *m_queuedValue = nullptr;
    QLabel *m_speedValue = nullptr;
    QLabel *m_failedValue = nullptr;
    QLabel *m_emptyHint = nullptr;
    QWidget *m_emptyHost = nullptr;
    QPushButton *m_cancelAll = nullptr;
    QPushButton *m_clearFinished = nullptr;
    QComboBox *m_policy = nullptr;
};
