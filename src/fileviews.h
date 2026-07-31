#pragma once

#include <QTableView>
#include <QTreeView>
#include <QUrl>
#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
class ExplorerDropTarget;
#endif

class LocalFileView final : public QTreeView {
    Q_OBJECT

public:
    explicit LocalFileView(QWidget *parent = nullptr);

signals:
    void remoteDataDropped(const QByteArray &data,
                           const QModelIndex &target);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

class RemoteFileView final : public QTableView
#ifdef Q_OS_WIN
                           , public QAbstractNativeEventFilter
#endif
{
    Q_OBJECT

public:
    explicit RemoteFileView(QWidget *parent = nullptr);
    ~RemoteFileView() override;

signals:
    void localUrlsDropped(const QList<QUrl> &urls,
                          const QModelIndex &target);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

private:
    friend class ExplorerDropTarget;
    void acceptExplorerDrop(const QList<QUrl> &urls,
                            const QPoint &globalPosition);
    ExplorerDropTarget *m_explorerDropTarget = nullptr;
    bool m_oleInitialized = false;
#endif
};
