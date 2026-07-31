#pragma once

#include "types.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <atomic>

struct ssh_session_struct;
struct ssh_channel_struct;
struct sftp_session_struct;
struct sftp_file_struct;
typedef struct ssh_session_struct *ssh_session;
typedef struct ssh_channel_struct *ssh_channel;
typedef struct sftp_session_struct *sftp_session;
typedef struct sftp_file_struct *sftp_file;

class SftpWorker final : public QObject {
    Q_OBJECT

public:
    enum class Role { Browsing, Transfers };

    explicit SftpWorker(Role role, QObject *parent = nullptr);
    ~SftpWorker() override;

    quint64 beginDirectoryRequest() noexcept;
    void requestStop() noexcept;
    void cancelJob(quint64 id);
    void cancelAllJobs();
    void setOverwritePolicy(OverwritePolicy policy) noexcept;
    bool isBusy() const noexcept;

public slots:
    void connectToHost(const ConnectionProfile &profile);
    void confirmHostKey(bool accepted, bool remember);
    void disconnectFromHost();
    void listDirectory(const QString &path, quint64 generation);
    void enqueueTransfers(const QVector<TransferJob> &jobs);
    void createDirectory(const QString &path);
    void createFile(const QString &path);
    void removeEntry(const QString &path, bool directory);
    void renameEntry(const QString &oldPath, const QString &newPath);

signals:
    void hostKeyConfirmationRequired(const QString &host,
                                     const QString &fingerprint,
                                     const QString &keyType,
                                     bool changed);
    void connected(const QString &homePath, const QString &banner,
                   const QString &fingerprint);
    void disconnected();
    void directoryListed(const QString &path, const RemoteEntries &entries);
    void directoryBatchReceived(const QString &path,
                                const RemoteEntries &entries);
    void transferQueued(const TransferStatus &status);
    void transferUpdated(const TransferStatus &status);
    void queueDrained(int completed, int failed, int skipped);
    void deletionProgress(qint64 removedCount, const QString &currentPath);
    void performanceMetric(const QString &operation, qint64 milliseconds);
    void reconnecting();
    void reconnected();
    void errorOccurred(const QString &message);

private:

    struct FileTask {
        QString localPath;
        QString remotePath;
        qint64 size = -1;
        quint32 permissions = 0;
        qint64 modified = 0;
    };

    struct JobContext {
        TransferStatus status;
        qint64 baseDone = 0;
        QElapsedTimer clock;
        QElapsedTimer emitClock;
        bool canceled = false;
    };

    enum class FileOutcome { Completed, Skipped, Canceled, Failed };

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

    void drainQueue();
    TransferState runJob(const TransferJob &job);
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
    void publish(JobContext &context, bool force = false);

    void refreshTransferLimits();
    QString normalizedRemotePath(const QString &path) const;
    bool removeRecursively(const QString &path, bool directory,
                           quint64 &removedCount, QString &failedPath);
    bool ensureRemoteDirectory(const QString &path);
    void applyRemoteMetadata(const QString &remotePath, const FileTask &task);
    void applyLocalMetadata(const QString &localPath, const FileTask &task);
    bool jobCanceled(quint64 id) const;
    bool stopping() const noexcept;

    const Role m_role;
    ssh_session m_session = nullptr;
    sftp_session m_sftp = nullptr;
    ssh_channel m_listingChannel = nullptr;
    quint32 m_listingRequestId = 1;
    size_t m_readChunk = 64 * 1024;
    size_t m_writeChunk = 64 * 1024;
    ConnectionProfile m_profile;
    QString m_pendingFingerprint;
    QString m_pendingKeyType;
    QString m_runtimeTrustedFingerprint;
    QString m_serverBanner;
    bool m_waitingForHostKey = false;
    bool m_reconnecting = false;
    bool m_draining = false;
    QVector<TransferJob> m_queue;

    mutable QMutex m_cancelMutex;
    QSet<quint64> m_canceledJobs;

    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_cancelAll{false};
    std::atomic_bool m_busy{false};
    std::atomic_int m_overwritePolicy{
        static_cast<int>(OverwritePolicy::Resume)};
    std::atomic_uint64_t m_directoryGeneration{0};
};
