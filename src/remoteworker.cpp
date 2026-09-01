#include "remoteworker.h"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>

#include <algorithm>

RemoteWorker::RemoteWorker(Role role, QObject *parent)
    : QObject(parent), m_role(role)
{
}

RemoteWorker::~RemoteWorker() = default;

quint64 RemoteWorker::beginDirectoryRequest() noexcept
{
    return m_directoryGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
}

void RemoteWorker::requestStop() noexcept
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    m_cancelAll.store(true, std::memory_order_relaxed);
    beginDirectoryRequest();
}

void RemoteWorker::cancelJob(quint64 id)
{
    QMutexLocker locker(&m_cancelMutex);
    m_canceledJobs.insert(id);
}

void RemoteWorker::cancelAllJobs()
{
    m_cancelAll.store(true, std::memory_order_relaxed);
}

void RemoteWorker::setOverwritePolicy(OverwritePolicy policy) noexcept
{
    m_overwritePolicy.store(static_cast<int>(policy), std::memory_order_relaxed);
}

bool RemoteWorker::isBusy() const noexcept
{
    return m_busy.load(std::memory_order_relaxed);
}

bool RemoteWorker::stopping() const noexcept
{
    return m_stopRequested.load(std::memory_order_relaxed);
}

bool RemoteWorker::jobCanceled(quint64 id) const
{
    if (m_cancelAll.load(std::memory_order_relaxed) || stopping())
        return true;
    QMutexLocker locker(&m_cancelMutex);
    return m_canceledJobs.contains(id);
}

QByteArray RemoteWorker::remoteBytes(const QString &path)
{
    return QDir::fromNativeSeparators(path).toUtf8();
}

QString RemoteWorker::remoteParentPath(const QString &value)
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(value));
    if (path == QStringLiteral("/"))
        return path;
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}

QString RemoteWorker::joinRemote(const QString &directory, const QString &name)
{
    if (directory.endsWith(QLatin1Char('/')))
        return directory + name;
    return directory + QLatin1Char('/') + name;
}

QString RemoteWorker::normalizedRemotePath(const QString &path)
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!result.startsWith(QLatin1Char('/')))
        result.prepend(QLatin1Char('/'));
    return result;
}

QString RemoteWorker::numberedVariant(const QString &path, int index)
{
    const qsizetype slash =
        std::max(path.lastIndexOf(QLatin1Char('/')),
                 path.lastIndexOf(QLatin1Char('\\')));
    const QString directory = slash < 0 ? QString() : path.left(slash + 1);
    const QString name = path.mid(slash + 1);
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot <= 0 ? name : name.left(dot);
    const QString suffix = dot <= 0 ? QString() : name.mid(dot);
    return QStringLiteral("%1%2 (%3)%4")
        .arg(directory, stem, QString::number(index), suffix);
}

void RemoteWorker::sortEntries(RemoteEntries &entries)
{
    std::sort(entries.begin(), entries.end(),
              [](const RemoteEntry &a, const RemoteEntry &b) {
                  if (a.directory != b.directory)
                      return a.directory > b.directory;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
}

TransferStatus RemoteWorker::statusFor(const TransferJob &job)
{
    TransferStatus status;
    status.id = job.id;
    status.name = job.upload ? QFileInfo(job.localPath).fileName()
                             : QFileInfo(job.remotePath).fileName();
    status.localPath = job.localPath;
    status.remotePath = job.remotePath;
    status.upload = job.upload;
    status.directory = job.directory;
    return status;
}

void RemoteWorker::publish(JobContext &context, bool force)
{
    if (!force && context.emitClock.elapsed() < 100)
        return;
    context.emitClock.restart();
    const qint64 elapsed = std::max<qint64>(context.clock.elapsed(), 1);
    context.status.bytesPerSecond = context.status.done * 1000 / elapsed;
    emit transferUpdated(context.status);
}

void RemoteWorker::enqueueTransfers(const QVector<TransferJob> &jobs)
{
    if (jobs.isEmpty())
        return;
    m_cancelAll.store(false, std::memory_order_relaxed);
    m_stopRequested.store(false, std::memory_order_relaxed);
    for (const TransferJob &job : jobs) {
        m_queue.append(job);
        TransferStatus status = statusFor(job);
        status.state = TransferState::Queued;
        emit transferQueued(status);
    }
    if (!m_draining)
        drainQueue();
}

void RemoteWorker::drainQueue()
{
    m_draining = true;
    m_busy.store(true, std::memory_order_relaxed);
    int completed = 0;
    int failed = 0;
    int skipped = 0;

    while (!m_queue.isEmpty()) {
        const TransferJob job = m_queue.takeFirst();
        TransferState outcome = TransferState::Canceled;
        if (jobCanceled(job.id) || stopping()) {
            TransferStatus status = statusFor(job);
            status.state = TransferState::Canceled;
            emit transferUpdated(status);
        } else {
            outcome = runJob(job);
        }
        switch (outcome) {
        case TransferState::Completed:
            ++completed;
            break;
        case TransferState::Failed:
            ++failed;
            break;
        default:
            ++skipped;
            break;
        }
    }

    {
        QMutexLocker locker(&m_cancelMutex);
        m_canceledJobs.clear();
    }
    m_cancelAll.store(false, std::memory_order_relaxed);
    m_busy.store(false, std::memory_order_relaxed);
    m_draining = false;
    emit queueDrained(completed, failed, skipped);
}
