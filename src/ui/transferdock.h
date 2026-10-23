#pragma once

#include "types.h"

#include <QAbstractListModel>
#include <QHash>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QWidget>

class IconButton;
class QLabel;
class QPropertyAnimation;
class FrameAnimation;
class Segmented;
class Dropdown;

class TransferModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Filter { All, Active, Done, Failed };
    static constexpr int StatusRole = Qt::UserRole + 1;

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void upsert(const TransferStatus &status);
    void clearFinished();
    void removeSession(quint64 sessionId);
    const TransferStatus *at(int row) const;
    int activeCount() const;
    int failedCount() const;
    int countFor(Filter filter) const;
    qint64 speed() const;
    qint64 remainingBytes() const;
    double progress() const;

    static bool finished(TransferState state);
    static bool matches(Filter filter, const TransferStatus &status);

signals:
    void summaryChanged();

private:
    TransferStatuses m_rows;
    QHash<quint64, int> m_index;
};

class TransferDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    static QRect actionRect(const QRect &row);
    static QRect secondaryRect(const QRect &row);
};

class TransferDock final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int panelHeight READ panelHeight WRITE setPanelHeight)

public:
    explicit TransferDock(QWidget *parent = nullptr);

    TransferModel *model() const { return m_model; }
    void toggle();
    void setExpanded(bool expanded);
    bool expanded() const { return m_expanded; }
    int panelHeight() const { return m_panelHeight; }
    void setPanelHeight(int height);
    QSize sizeHint() const override;
    void setOverlayHost(QWidget *host);

signals:
    void cancelRequested(quint64 sessionId, quint64 id);
    void cancelAllRequested();
    void retryRequested(const TransferStatus &status);
    void revealRequested(const TransferStatus &status);
    void heightChanged(int height);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refreshSummary();
    void showPolicy(OverwritePolicy policy);
    void showConnections(int connections);
    void applyFilter();
    void placeDrawer();

    TransferModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QListView *m_list = nullptr;
    QWidget *m_panel = nullptr;
    QWidget *m_drawer = nullptr;
    QWidget *m_host = nullptr;
    FrameAnimation *m_progressAnimation = nullptr;
    double m_shownProgress = -1;
    QWidget *m_bar = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;
    IconButton *m_toggle = nullptr;
    Segmented *m_filter = nullptr;
    Dropdown *m_policyChip = nullptr;
    Dropdown *m_connections = nullptr;
    FrameAnimation *m_animation = nullptr;
    int m_panelHeight = 0;
    bool m_expanded = false;
    double m_progress = -1;
};
