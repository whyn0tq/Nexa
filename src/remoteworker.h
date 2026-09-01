#pragma once

#include "types.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <atomic>

class RemoteWorker : public QObject {
    Q_OBJECT

public:
    enum class Role { Browsing, Transfers };

    explicit RemoteWorker(Role role, QObject *parent = nullptr);
    ~RemoteWorker() override;

    quint64 beginDirectoryRequest() noexcept;
    void requestStop() noexcept;
    void cancelJob(quint64 id);
    void cancelAllJobs();
    void setOverwritePolicy(OverwritePolicy policy) noexcept;
    bool isBusy() const noexcept;

public slots:
    virtual void connectToHost(const ConnectionProfile &profile) = 0;
    virtual void confirmHostKey(bool accepted, bool remember) = 0;
    virtual void disconnectFromHost() = 0;
    virtual void listDirectory(const QString &path, quint64 generation) = 0;
    virtual void createDirectory(const QString &path) = 0;
    virtual void createFile(const QString &path) = 0;
    virtual void removeEntry(const QString &path, bool directory) = 0;
    virtual void renameEntry(const QString &oldPath, const QString &newPath) = 0;
    void enqueueTransfers(const QVector<TransferJob> &jobs);

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

protected:
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

    virtual TransferState runJob(const TransferJob &job) = 0;

    static QByteArray remoteBytes(const QString &path);
    static QString remoteParentPath(const QString &value);
    static QString joinRemote(const QString &directory, const QString &name);
    static QString normalizedRemotePath(const QString &path);
    static QString numberedVariant(const QString &path, int index);
    static void sortEntries(RemoteEntries &entries);
    static TransferStatus statusFor(const TransferJob &job);

    void publish(JobContext &context, bool force = false);
    bool jobCanceled(quint64 id) const;
    bool stopping() const noexcept;

    const Role m_role;
    ConnectionProfile m_profile;
    QVector<TransferJob> m_queue;
    bool m_draining = false;

    mutable QMutex m_cancelMutex;
    QSet<quint64> m_canceledJobs;

    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_cancelAll{false};
    std::atomic_bool m_busy{false};
    std::atomic_int m_overwritePolicy{
        static_cast<int>(OverwritePolicy::Resume)};
    std::atomic_uint64_t m_directoryGeneration{0};

private:
    void drainQueue();
};
