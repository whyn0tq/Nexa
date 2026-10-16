#pragma once

#include "connection.h"
#include "sftpchannel.h"

#include <QStringList>

class SftpConnection final : public RemoteConnection {
public:
    SftpConnection() = default;
    ~SftpConnection() override;

    OpenResult open(const ConnectionProfile &profile,
                    bool interactiveTrust) override;
    OpenResult continueAfterTrust(bool accepted, bool remember) override;
    void close() override;
    bool isOpen() const override;
    bool ping() override;

    QString homePath() override;
    bool list(const QString &path, RemoteEntries &entries) override;
    bool listStart(const QString &path, QString &absolute, RemoteEntries &entries) override;
    void prefetch(const QStringList &directories,
                  const std::function<void(const QString &, const RemoteEntries &)> &onDirectory) override;
    bool supportsQuickFiles() const override { return true; }
    bool readFile(const QString &path, qint64 sizeHint, QByteArray &data) override;
    bool writeFile(const QString &path, const QByteArray &data) override;
    bool makeDirectory(const QString &path) override;
    bool createFile(const QString &path) override;
    bool rename(const QString &from, const QString &to) override;
    bool remove(const RemoteEntries &entries,
                const std::function<void(qint64)> &progress) override;

    bool expand(const TransferJob &job, TransferHost &host) override;
    bool runTransfers(TransferHost &host) override;

private:
    struct Active;
    struct Request;
    class Pipeline;
    friend class Pipeline;

    OpenResult finishOpen();
    bool authenticate();
    bool authenticateWithKey(unsigned int methods);
    bool authenticateInteractive();
    void tuneSocket();
    QString sshError(const QString &prefix) const;
    bool channelFailed(const QString &prefix);

    bool listMany(const QStringList &directories,
                  const std::function<void(const QString &, RemoteEntries &)> &onDirectory,
                  QString &failedDirectory, QString &failure);
    void resolveLinks(RemoteEntries &entries);
    bool expandUpload(const TransferJob &job, TransferHost &host);
    bool expandDownload(const TransferJob &job, TransferHost &host);

    ssh_session m_session = nullptr;
    sftp::Channel m_channel;
    QString m_pendingFingerprint;
    bool m_waitingForTrust = false;
};
