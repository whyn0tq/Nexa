#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

struct RemoteEntry {
    QString name;
    QString path;
    qint64 size = 0;
    QDateTime modified;
    quint32 permissions = 0;
    bool directory = false;
    bool symlink = false;
};

using RemoteEntries = QVector<RemoteEntry>;

enum class Protocol {
    Sftp = 0,
    Ftp
};

constexpr quint16 defaultPort(Protocol protocol) noexcept
{
    return protocol == Protocol::Ftp ? quint16(21) : quint16(22);
}

struct ConnectionProfile {
    QString host;
    quint16 port = 22;
    QString username;
    QString password;
    QString privateKeyPath;
    QString keyPassphrase;

    QString trustedFingerprint;
    Protocol protocol = Protocol::Sftp;
    bool usePrivateKey = false;
    bool useAgent = true;
    bool useTls = false;
};

enum class OverwritePolicy {
    Resume = 0,
    Overwrite,
    Skip,
    Rename
};

enum class TransferState {
    Queued = 0,
    Running,
    Completed,
    Failed,
    Canceled,
    Skipped
};

struct TransferJob {
    quint64 id = 0;
    QString localPath;
    QString remotePath;

    qint64 size = -1;
    qint64 modified = 0;
    quint32 permissions = 0;
    bool upload = false;
    bool directory = false;
};

struct TransferStatus {
    quint64 id = 0;
    QString name;
    QString localPath;
    QString remotePath;
    TransferState state = TransferState::Queued;
    bool upload = false;
    bool directory = false;
    qint64 done = 0;
    qint64 total = -1;
    qint64 bytesPerSecond = 0;
    QString detail;
};

using TransferStatuses = QVector<TransferStatus>;

Q_DECLARE_METATYPE(RemoteEntry)
Q_DECLARE_METATYPE(RemoteEntries)
Q_DECLARE_METATYPE(ConnectionProfile)
Q_DECLARE_METATYPE(TransferJob)
Q_DECLARE_METATYPE(TransferStatus)
Q_DECLARE_METATYPE(QVector<TransferJob>)
Q_DECLARE_METATYPE(OverwritePolicy)
Q_DECLARE_METATYPE(Protocol)
