#pragma once

#include "types.h"

#include <QCoreApplication>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

struct SegmentGroup {
    std::atomic_int remaining{0};
    std::atomic_bool failed{false};
    QString partialLocal;
    QString finalLocal;
    QString remoteDestination;
    qint64 size = 0;
    qint64 modified = 0;
    quint32 permissions = 0;
    QMutex errorMutex;
    QString error;
};

struct FileTask {
    quint64 jobId = 0;
    QString localPath;
    QString remotePath;
    qint64 size = -1;
    qint64 modified = 0;
    quint32 permissions = 0;
    bool upload = false;
    bool overwrite = false;

    bool remoteAbsent = false;
    bool restarted = false;
    int attempt = 0;
    qint64 reported = 0;

    std::shared_ptr<SegmentGroup> group;
    qint64 segmentStart = 0;
    qint64 segmentEnd = -1;

    bool isSegment() const { return group != nullptr; }
};

enum class FileOutcome { Completed, Skipped, Failed, Canceled };

struct WorkItem {
    enum class Kind { File, Expand };
    Kind kind = Kind::File;
    FileTask file;
    TransferJob job;
};

class TransferHost {
public:
    virtual ~TransferHost() = default;

    virtual bool take(WorkItem &item, bool allowExpand, bool allowSegments) = 0;
    virtual void pushFront(const QVector<FileTask> &tasks) = 0;
    virtual void addTasks(quint64 jobId, const QVector<FileTask> &tasks) = 0;
    virtual void expansionFinished(quint64 jobId) = 0;
    virtual void expansionFailed(quint64 jobId, const QString &error) = 0;
    virtual void requeueJob(const TransferJob &job) = 0;
    virtual void addUnits(quint64 jobId, int count) = 0;
    virtual void progress(quint64 jobId, qint64 delta) = 0;
    virtual void finished(const FileTask &task, FileOutcome outcome,
                          bool countsAsFile, const QString &error) = 0;
    virtual void current(quint64 jobId, const QString &path) = 0;
    virtual bool canceled(quint64 jobId) const = 0;
    virtual bool stopping() const = 0;
    virtual OverwritePolicy policy() const = 0;
    virtual int parallelism() const = 0;
};

class RemoteConnection {
    Q_DECLARE_TR_FUNCTIONS(RemoteConnection)

public:
    enum class OpenResult { Ok, NeedsTrust, Failed, AuthFailed };

    struct TrustRequest {
        QString host;
        QString fingerprint;
        QString keyType;
        bool changed = false;
    };

    virtual ~RemoteConnection() = default;

    static std::unique_ptr<RemoteConnection> create(Protocol protocol);
    using Factory = std::function<std::unique_ptr<RemoteConnection>(Protocol)>;
    static void setFactory(Factory factory);

    virtual OpenResult open(const ConnectionProfile &profile,
                            bool interactiveTrust) = 0;
    virtual OpenResult continueAfterTrust(bool accepted, bool remember) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool ping() = 0;

    virtual QString homePath() = 0;
    virtual bool list(const QString &path, RemoteEntries &entries) = 0;
    virtual void prefetch(const QStringList &directories,
                          const std::function<void(const QString &, const RemoteEntries &)> &onDirectory)
    {
        Q_UNUSED(directories);
        Q_UNUSED(onDirectory);
    }
    virtual bool supportsQuickFiles() const { return false; }
    virtual bool readFile(const QString &path, qint64 sizeHint, QByteArray &data)
    {
        Q_UNUSED(path);
        Q_UNUSED(sizeHint);
        data.clear();
        return false;
    }
    virtual bool writeFile(const QString &path, const QByteArray &data)
    {
        Q_UNUSED(path);
        Q_UNUSED(data);
        return false;
    }
    virtual bool listStart(const QString &path, QString &absolute, RemoteEntries &entries)
    {
        absolute = path.isEmpty() ? homePath() : normalized(path);
        return list(absolute, entries);
    }
    virtual bool makeDirectory(const QString &path) = 0;
    virtual bool createFile(const QString &path) = 0;
    virtual bool rename(const QString &from, const QString &to) = 0;
    virtual bool remove(const RemoteEntries &entries,
                        const std::function<void(qint64)> &progress) = 0;

    virtual bool expand(const TransferJob &job, TransferHost &host) = 0;
    virtual bool runTransfers(TransferHost &host) = 0;

    TrustRequest trustRequest() const { return m_trust; }
    QString trustedFingerprint() const { return m_trustedFingerprint; }
    QString serverInfo() const { return m_serverInfo; }
    QString lastError() const { return m_error; }
    bool connectionBroken() const { return m_broken; }
    void setCancelCheck(std::function<bool()> check) { m_cancel = std::move(check); }

    static QString parentPath(const QString &path);
    static QString join(const QString &directory, const QString &name);
    static QString normalized(const QString &path);
    static QString numberedVariant(const QString &path, int index);
    static void sortEntries(RemoteEntries &entries);
    static QString partialSuffix() { return QStringLiteral(".nexapart"); }
    static QString segmentSuffix() { return QStringLiteral(".nexaseg"); }

protected:
    bool canceledByUser() const { return m_cancel && m_cancel(); }
    void fail(const QString &message, bool broken = false)
    {
        m_error = message;
        m_broken = m_broken || broken;
    }

    ConnectionProfile m_profile;
    TrustRequest m_trust;
    QString m_trustedFingerprint;
    QString m_serverInfo;
    QString m_error;
    bool m_broken = false;
    std::function<bool()> m_cancel;
};
