#pragma once

#include "types.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>

struct ssh_session_struct;
struct ssh_channel_struct;
struct sftp_session_struct;
typedef struct ssh_session_struct *ssh_session;
typedef struct ssh_channel_struct *ssh_channel;
typedef struct sftp_session_struct *sftp_session;

class SftpWorker final : public QObject {
    Q_OBJECT

public:
    explicit SftpWorker(QObject *parent = nullptr);
    ~SftpWorker() override;
    quint64 beginDirectoryRequest() noexcept;
    void requestStop() noexcept;

public slots:
    void connectToHost(const ConnectionProfile &profile);
    void confirmHostKey(bool accepted, bool remember);
    void disconnectFromHost();
    void listDirectory(const QString &path, quint64 generation);
    void downloadFile(const QString &remotePath, const QString &localPath,
                      bool directory);
    void uploadFile(const QString &localPath, const QString &remotePath,
                    bool directory);
    void createDirectory(const QString &path);
    void createFile(const QString &path);
    void removeEntry(const QString &path, bool directory);
    void renameEntry(const QString &oldPath, const QString &newPath);

signals:
    void hostKeyConfirmationRequired(const QString &host,
                                     const QString &fingerprint,
                                     bool changed);
    void connected(const QString &homePath);
    void disconnected();
    void directoryListed(const QString &path, const RemoteEntries &entries);
    void directoryBatchReceived(const QString &path,
                                const RemoteEntries &entries);
    void transferStarted(const QString &label, bool upload);
    void transferProgress(const QString &label, qint64 done, qint64 total);
    void transferFinished(const QString &label);
    void deletionProgress(qint64 removedCount, const QString &currentPath);
    void performanceMetric(const QString &operation, qint64 milliseconds);
    void reconnecting();
    void reconnected();
    void errorOccurred(const QString &message);

private:
    void listDirectoryWithRetry(const QString &path, quint64 generation,
                                int retries);
    void authenticate();
    void cleanup();
    QString sessionError(const QString &prefix) const;
    QString normalizedRemotePath(const QString &path) const;
    bool removeRecursively(const QString &path, bool directory,
                           quint64 &removedCount, QString &failedPath);
    bool downloadEntry(const QString &remotePath, const QString &localPath,
                       bool directory, int retries = 1);
    bool uploadEntry(const QString &localPath, const QString &remotePath,
                     bool directory, int retries = 1);
    bool reconnectSession();
    bool connectionLost() const;
    bool listDirectoryPipelined(const QString &path, quint64 generation,
                                RemoteEntries *result = nullptr,
                                int pipelineDepth = 16,
                                bool background = false);
    bool ensureListingChannel();
    void closeListingChannel();
    bool sendListingPacket(const QByteArray &payload);
    bool readListingPacket(QByteArray &payload);

    ssh_session m_session = nullptr;
    sftp_session m_sftp = nullptr;
    ssh_channel m_listingChannel = nullptr;
    quint32 m_listingRequestId = 1;
    ConnectionProfile m_profile;
    QString m_pendingFingerprint;
    QString m_runtimeTrustedFingerprint;
    bool m_waitingForHostKey = false;
    bool m_reconnecting = false;
    std::atomic_bool m_stopRequested{false};
    std::atomic_uint64_t m_directoryGeneration{0};
};
