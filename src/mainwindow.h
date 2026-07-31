#pragma once

#include "theme.h"
#include "types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>
#include <QStringList>

class QFileSystemModel;
class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;

class LocalFileView;
class PathBar;
class RemoteFileModel;
class RemoteFileView;
class RemoteSortFilterProxy;
class SftpWorker;
class TransferPanel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void connectRequested(const ConnectionProfile &profile);
    void hostKeyConfirmed(bool accepted, bool remember);
    void disconnectRequested();
    void remoteListRequested(const QString &path, quint64 generation);
    void transfersRequested(const QVector<TransferJob> &jobs);
    void transferConnectRequested(const ConnectionProfile &profile);
    void remoteMkdirRequested(const QString &path);
    void remoteFileCreateRequested(const QString &path);
    void remoteRemoveRequested(const QString &path, bool directory);
    void remoteRenameRequested(const QString &oldPath, const QString &newPath);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showConnectDialog();
    void onConnected(const QString &homePath, const QString &banner,
                     const QString &fingerprint);
    void onDisconnected();
    void onHostKeyRequired(const QString &host, const QString &fingerprint,
                           const QString &keyType, bool changed);
    void onRemoteListed(const QString &path, const RemoteEntries &entries);
    void onRemoteBatchReceived(const QString &path,
                               const RemoteEntries &entries);
    void onError(const QString &message);
    void openLocal(const QModelIndex &index);
    void openRemote(const QModelIndex &index);
    void navigateLocal(const QString &path);
    void navigateRemote(const QString &path);
    void uploadSelected();
    void downloadSelected();
    void createRemoteFolder();
    void createRemoteFile();
    void removeRemoteSelected();
    void renameRemoteSelected();
    void refreshRemote();
    void toggleTheme();

private:
    enum class Notice { Info, Success, Warning, Error };

    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildFilesPage();
    QWidget *buildLocalPanel();
    QWidget *buildRemotePanel();
    void wireWorkers();
    void applyTheme();
    void setConnectionState(bool connected);
    void setActivePage(int index);
    void requestRemoteDirectory(const QString &path, bool useCache = true);
    void enqueueJobs(const QVector<TransferJob> &jobs);
    void showNotice(const QString &text, Notice severity,
                    const QString &detail = QString());
    void refreshNavigationButtons();
    void refreshPanelMeta();
    void updateTransferSummary();
    TransferJob downloadJobFor(const RemoteEntry &entry,
                               const QString &destinationDirectory);
    RemoteEntries selectedRemoteEntries() const;
    const RemoteEntry *remoteEntryAt(const QModelIndex &proxyIndex) const;
    QString remoteDestinationFor(const QModelIndex &proxyIndex) const;

    QThread *m_browseThread = nullptr;
    QThread *m_transferThread = nullptr;
    SftpWorker *m_browseWorker = nullptr;
    SftpWorker *m_transferWorker = nullptr;

    QFileSystemModel *m_localModel = nullptr;
    LocalFileView *m_localView = nullptr;
    RemoteFileView *m_remoteView = nullptr;
    RemoteFileModel *m_remoteModel = nullptr;
    RemoteSortFilterProxy *m_remoteProxy = nullptr;
    TransferPanel *m_transferPanel = nullptr;

    PathBar *m_localPathBar = nullptr;
    PathBar *m_remotePathBar = nullptr;
    QLineEdit *m_localFilter = nullptr;
    QLineEdit *m_remoteFilter = nullptr;
    QLabel *m_localMeta = nullptr;
    QLabel *m_remoteMeta = nullptr;
    QFrame *m_remoteCard = nullptr;
    QWidget *m_remoteEmptyState = nullptr;

    QPushButton *m_localBack = nullptr;
    QPushButton *m_localForward = nullptr;
    QPushButton *m_localUp = nullptr;
    QPushButton *m_remoteBack = nullptr;
    QPushButton *m_remoteForward = nullptr;
    QPushButton *m_remoteUp = nullptr;

    QStackedWidget *m_pages = nullptr;
    QVector<QPushButton *> m_navigationButtons;
    QLabel *m_navigationBadge = nullptr;

    QLabel *m_connectionLabel = nullptr;
    QLabel *m_connectionDetail = nullptr;
    QLabel *m_themeHint = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QPushButton *m_themeButton = nullptr;

    QFrame *m_notification = nullptr;
    QLabel *m_notificationText = nullptr;
    QLabel *m_notificationDetail = nullptr;
    QTimer *m_notificationTimer = nullptr;

    QProgressBar *m_progress = nullptr;
    QLabel *m_statusLeft = nullptr;

    QHash<QString, RemoteEntries> m_directoryCache;
    QStringList m_errorLog;
    ConnectionProfile m_activeProfile;
    QString m_currentRemotePath = QStringLiteral("/");
    quint64 m_nextJobId = 1;
    bool m_connected = false;
    bool m_transferSessionReady = false;
    bool m_showingCachedDirectory = false;
    bool m_closing = false;
};
