#pragma once

#include "types.h"

#include <QHash>
#include <QElapsedTimer>
#include <QMainWindow>

class QFileSystemModel;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableView;
class QThread;
class QTreeView;
class SftpWorker;
class RemoteFileModel;
class LocalFileView;
class RemoteFileView;

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
    void downloadRequested(const QString &remotePath, const QString &localPath,
                           bool directory);
    void uploadRequested(const QString &localPath, const QString &remotePath,
                         bool directory);
    void remoteMkdirRequested(const QString &path);
    void remoteFileCreateRequested(const QString &path);
    void remoteRemoveRequested(const QString &path, bool directory);
    void remoteRenameRequested(const QString &oldPath, const QString &newPath);

private slots:
    void showConnectDialog();
    void onConnected(const QString &homePath);
    void onDisconnected();
    void onHostKeyRequired(const QString &host, const QString &fingerprint,
                           bool changed);
    void onRemoteListed(const QString &path, const RemoteEntries &entries);
    void onRemoteBatchReceived(const QString &path,
                               const RemoteEntries &entries);
    void onError(const QString &message);
    void openLocal(const QModelIndex &index);
    void openRemote(const QModelIndex &index);
    void goToRemotePath();
    void goRemoteUp();
    void uploadSelected();
    void downloadSelected();
    void createRemoteFolder();
    void createRemoteFile();
    void removeRemoteSelected();
    void renameRemoteSelected();
    void refreshRemote();

private:
    void buildUi();
    void applyTheme();
    void setConnectionState(bool connected);
    void requestRemoteDirectory(const QString &path, bool useCache = true);
    QString selectedRemotePath() const;
    bool selectedRemoteIsDirectory() const;
    static QString humanSize(qint64 bytes);

    QThread *m_workerThread = nullptr;
    SftpWorker *m_worker = nullptr;
    QFileSystemModel *m_localModel = nullptr;
    LocalFileView *m_localView = nullptr;
    RemoteFileView *m_remoteView = nullptr;
    RemoteFileModel *m_remoteModel = nullptr;
    QLineEdit *m_localPath = nullptr;
    QLineEdit *m_remotePath = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QProgressBar *m_progress = nullptr;
    RemoteEntries m_remoteEntries;
    QHash<QString, RemoteEntries> m_directoryCache;
    QString m_currentRemotePath = QStringLiteral("/");
    bool m_connected = false;
    bool m_showingCachedDirectory = false;
    QElapsedTimer m_transferTimer;
};
