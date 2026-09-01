#pragma once

#include "remoteworker.h"
#include "types.h"

#include <QByteArray>
#include <QString>
#include <QVector>

struct ssh_session_struct;
struct ssh_channel_struct;
struct sftp_session_struct;
struct sftp_file_struct;
typedef struct ssh_session_struct *ssh_session;
typedef struct ssh_channel_struct *ssh_channel;
typedef struct sftp_session_struct *sftp_session;
typedef struct sftp_file_struct *sftp_file;

class SftpWorker final : public RemoteWorker {
    Q_OBJECT

public:
    explicit SftpWorker(Role role, QObject *parent = nullptr);
    ~SftpWorker() override;

public slots:
    void connectToHost(const ConnectionProfile &profile) override;
    void confirmHostKey(bool accepted, bool remember) override;
    void disconnectFromHost() override;
    void listDirectory(const QString &path, quint64 generation) override;
    void createDirectory(const QString &path) override;
    void createFile(const QString &path) override;
    void removeEntry(const QString &path, bool directory) override;
    void renameEntry(const QString &oldPath, const QString &newPath) override;

private:
    bool openSession();
    void finishConnect();
    bool authenticate();
    bool authenticateWithKey(unsigned int methods);
    bool authenticateInteractive();
    bool startSftp();
    void cleanup();
    bool reconnectSession();
    bool connectionLost() const;
    QString sessionError(const QString &prefix) const;

    void listDirectoryWithRetry(const QString &path, quint64 generation,
                                int retries);
    bool listDirectoryPipelined(const QString &path, quint64 generation,
                                RemoteEntries *result, int pipelineDepth,
                                bool background);
    bool listDirectoryFallback(const QString &path, quint64 generation,
                               int retries);
    bool readRemoteDirectory(const QString &path, RemoteEntries &entries);
    bool ensureListingChannel();
    void closeListingChannel();
    bool sendListingPacket(const QByteArray &payload);
    bool readListingPacket(QByteArray &payload);

    TransferState runJob(const TransferJob &job) override;
    bool expandJob(const TransferJob &job, QVector<FileTask> &tasks,
                   QString &failure);
    bool expandDownload(const TransferJob &job, QVector<FileTask> &tasks,
                        QString &failure);
    bool expandUpload(const TransferJob &job, QVector<FileTask> &tasks,
                      QString &failure);
    FileOutcome downloadFileTask(const FileTask &task, JobContext &context,
                                 int retries);
    FileOutcome uploadFileTask(const FileTask &task, JobContext &context,
                               int retries);

    void refreshTransferLimits();
    bool removeRecursively(const QString &path, bool directory,
                           quint64 &removedCount, QString &failedPath);
    bool ensureRemoteDirectory(const QString &path);
    void applyRemoteMetadata(const QString &remotePath, const FileTask &task);
    void applyLocalMetadata(const QString &localPath, const FileTask &task);

    ssh_session m_session = nullptr;
    sftp_session m_sftp = nullptr;
    ssh_channel m_listingChannel = nullptr;
    quint32 m_listingRequestId = 1;
    size_t m_readChunk = 64 * 1024;
    size_t m_writeChunk = 64 * 1024;
    QString m_pendingFingerprint;
    QString m_pendingKeyType;
    QString m_runtimeTrustedFingerprint;
    QString m_serverBanner;
    bool m_waitingForHostKey = false;
    bool m_reconnecting = false;
};
