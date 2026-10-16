#include "transferengine.h"

#include <QFileInfo>
#include <QMutexLocker>
#include <QTimer>

#include <algorithm>

namespace {
constexpr int IdleDisconnectMs = 90000;
constexpr int IdlePingMs = 20000;
constexpr int WarmIdleMs = 10 * 60 * 1000;
constexpr qint64 GiveUpAfterMs = 5LL * 60 * 1000;
}

void TransferQueue::addJob(const TransferJob &job, const TransferStatus &status)
{
    QMutexLocker locker(&m_mutex);
    auto record = std::make_shared<Job>();
    record->status = status;
    m_jobs.insert(job.id, record);
    WorkItem item;
    item.kind = WorkItem::Kind::Expand;
    item.job = job;
    m_items.push_back(item);
    m_condition.wakeAll();
}

void TransferQueue::cancel(quint64 jobId)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId)) {
        job->canceled = true;
        job->dirty = true;
    }
    skipCanceledLocked();
    m_condition.wakeAll();
}

void TransferQueue::cancelAll()
{
    QMutexLocker locker(&m_mutex);
    for (auto &job : m_jobs) {
        if (!job->finished) {
            job->canceled = true;
            job->dirty = true;
        }
    }
    skipCanceledLocked();
    m_condition.wakeAll();
}

void TransferQueue::failQueued(const QString &error)
{
    QMutexLocker locker(&m_mutex);
    for (auto &job : m_jobs) {
        if (job->finished)
            continue;
        if (job->errors.isEmpty())
            job->errors.append(error);
        job->canceled = true;
        job->filesFailed = std::max(job->filesFailed, 1);
        job->dirty = true;
    }
    skipCanceledLocked();
}

void TransferQueue::skipCanceledLocked()
{
    for (auto it = m_items.begin(); it != m_items.end();) {
        const quint64 id = it->kind == WorkItem::Kind::File ? it->file.jobId : it->job.id;
        auto job = m_jobs.value(id);
        if (job && job->canceled) {
            if (it->kind == WorkItem::Kind::Expand) {
                job->expanded = true;
            } else {
                ++job->unitsDone;
            }
            settleLocked(*job);
            it = m_items.erase(it);
        } else {
            ++it;
        }
    }
}

int TransferQueue::queuedItems() const
{
    QMutexLocker locker(&m_mutex);
    return static_cast<int>(m_items.size());
}

bool TransferQueue::hasWork() const
{
    QMutexLocker locker(&m_mutex);
    return !m_items.empty();
}

bool TransferQueue::waitForWork(int milliseconds)
{
    QMutexLocker locker(&m_mutex);
    if (!m_items.empty() || m_stopping)
        return !m_items.empty();
    m_condition.wait(&m_mutex, static_cast<unsigned long>(milliseconds));
    return !m_items.empty();
}

void TransferQueue::wakeAll()
{
    QMutexLocker locker(&m_mutex);
    m_condition.wakeAll();
}

void TransferQueue::setStopping(bool value)
{
    m_stopping = value;
    wakeAll();
}

bool TransferQueue::take(WorkItem &item, bool allowExpand)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->kind == WorkItem::Kind::Expand && !allowExpand)
            continue;
        item = *it;
        m_items.erase(it);
        return true;
    }
    return false;
}

void TransferQueue::pushFront(const QVector<FileTask> &tasks)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = tasks.crbegin(); it != tasks.crend(); ++it) {
        WorkItem item;
        item.file = *it;
        m_items.push_front(item);
    }
    m_condition.wakeAll();
}

void TransferQueue::addTasks(quint64 jobId, const QVector<FileTask> &tasks)
{
    QMutexLocker locker(&m_mutex);
    auto job = m_jobs.value(jobId);
    if (!job)
        return;
    job->units += static_cast<int>(tasks.size());
    job->filesTotal += static_cast<int>(tasks.size());
    for (const FileTask &task : tasks) {
        job->total += std::max<qint64>(task.size, 0);
        WorkItem item;
        item.file = task;
        m_items.push_back(item);
    }
    job->dirty = true;
    if (job->canceled)
        skipCanceledLocked();
    m_condition.wakeAll();
}

void TransferQueue::expansionFinished(quint64 jobId)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId)) {
        job->expanded = true;
        settleLocked(*job);
    }
}

void TransferQueue::expansionFailed(quint64 jobId, const QString &error)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId)) {
        job->expanded = true;
        job->errors.append(error);
        job->filesFailed += 1;
        settleLocked(*job);
    }
}

void TransferQueue::requeueJob(const TransferJob &job)
{
    QMutexLocker locker(&m_mutex);
    WorkItem item;
    item.kind = WorkItem::Kind::Expand;
    item.job = job;
    m_items.push_front(item);
    m_condition.wakeAll();
}

void TransferQueue::addUnits(quint64 jobId, int count)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId))
        job->units += count;
}

void TransferQueue::progress(quint64 jobId, qint64 delta)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId)) {
        job->done += delta;
        job->dirty = true;
    }
}

void TransferQueue::finished(const FileTask &task, FileOutcome outcome,
                             bool countsAsFile, const QString &error)
{
    QMutexLocker locker(&m_mutex);
    auto job = m_jobs.value(task.jobId);
    if (!job)
        return;
    ++job->unitsDone;
    if (countsAsFile) {
        switch (outcome) {
        case FileOutcome::Completed:
            ++job->filesDone;
            break;
        case FileOutcome::Skipped:
            ++job->filesDone;
            ++job->filesSkipped;
            break;
        case FileOutcome::Failed:
            ++job->filesFailed;
            if (job->errors.size() < 20 && !error.isEmpty())
                job->errors.append(error);
            break;
        case FileOutcome::Canceled:
            break;
        }
    }
    job->dirty = true;
    settleLocked(*job);
}

void TransferQueue::settleLocked(Job &job)
{
    job.dirty = true;
    if (job.finished || !job.expanded || job.unitsDone < job.units)
        return;
    job.finished = true;
    if (job.filesFailed > 0)
        job.status.state = TransferState::Failed;
    else if (job.canceled)
        job.status.state = TransferState::Canceled;
    else if (job.filesTotal > 0 && job.filesSkipped == job.filesTotal)
        job.status.state = TransferState::Skipped;
    else
        job.status.state = TransferState::Completed;
}

void TransferQueue::current(quint64 jobId, const QString &path)
{
    QMutexLocker locker(&m_mutex);
    if (auto job = m_jobs.value(jobId))
        job->current = path;
}

bool TransferQueue::canceled(quint64 jobId) const
{
    QMutexLocker locker(&m_mutex);
    auto job = m_jobs.value(jobId);
    return !job || job->canceled;
}

QVector<TransferStatus> TransferQueue::snapshot(qint64 elapsedMs, bool &anyActive)
{
    QMutexLocker locker(&m_mutex);
    QVector<TransferStatus> result;
    anyActive = false;
    QVector<quint64> drop;
    for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it) {
        Job &job = *it.value();
        const qint64 delta = job.done - job.lastDone;
        job.lastDone = job.done;
        if (!job.finished && elapsedMs > 0) {
            const double instant = std::max<qint64>(delta, 0) * 1000.0 / elapsedMs;
            job.speed = job.speed <= 0 ? instant : job.speed * 0.7 + instant * 0.3;
        }
        if (!job.finished)
            anyActive = true;
        const bool started = job.done > 0 || job.unitsDone > 0 || job.expanded;
        if (!job.dirty && (job.finished || !started))
            continue;
        job.dirty = false;

        TransferStatus status = job.status;
        if (!job.finished)
            status.state = started ? TransferState::Running : TransferState::Queued;
        status.done = job.done;
        status.total = job.expanded || job.total > 0 ? job.total : -1;
        status.bytesPerSecond = job.finished ? 0 : static_cast<qint64>(job.speed);
        status.filesDone = job.filesDone;
        status.filesTotal = job.filesTotal;
        status.filesFailed = job.filesFailed;
        if (job.finished) {
            if (!job.errors.isEmpty())
                status.detail = job.errors.first();
            else if (job.filesSkipped > 0)
                status.detail = QCoreApplication::translate("TransferEngine",
                                                            "Пропущено: %1")
                                    .arg(job.filesSkipped);
            drop.append(it.key());
        } else {
            status.detail = job.current;
        }
        result.append(status);
    }
    for (quint64 id : drop)
        m_jobs.remove(id);
    return result;
}

TransferWorker::TransferWorker(std::shared_ptr<TransferQueue> queue,
                               const ConnectionProfile &profile, bool warm)
    : m_queue(std::move(queue)), m_profile(profile), m_warm(warm)
{
}

void TransferWorker::run()
{
    auto connection = RemoteConnection::create(m_profile.protocol);
    connection->setCancelCheck([this] { return m_stop.load() || m_queue->stopping(); });
    QElapsedTimer idle;
    idle.start();
    QElapsedTimer lastPing;
    lastPing.start();
    int failures = 0;

    if (m_warm && !m_queue->hasWork()) {
        if (connection->open(m_profile, false) == RemoteConnection::OpenResult::Ok) {
            m_connected = true;
            emit connectionReady();
        } else {
            connection->close();
        }
        idle.restart();
        lastPing.restart();
    }
    while (!m_stop && !m_queue->stopping()) {
        if (!m_queue->hasWork()) {
            m_queue->waitForWork(2000);
            if (m_stop || m_queue->stopping())
                break;
            if (m_queue->hasWork())
                continue;
            if (idle.elapsed() > (m_warm ? WarmIdleMs : IdleDisconnectMs))
                break;
            if (connection->isOpen() && lastPing.elapsed() > IdlePingMs) {
                lastPing.restart();
                if (!connection->ping()) {
                    connection->close();
                    m_connected = false;
                }
            }
            continue;
        }

        if (!connection->isOpen()) {
            const auto result = connection->open(m_profile, false);
            if (result != RemoteConnection::OpenResult::Ok) {
                m_connected = false;
                ++failures;
                emit connectionFailed(connection->lastError(),
                                      result == RemoteConnection::OpenResult::AuthFailed);
                const int delay = std::min(30000, 1000 << std::min(failures - 1, 5));
                QElapsedTimer wait;
                wait.start();
                while (!m_stop && !m_queue->stopping() && wait.elapsed() < delay)
                    QThread::msleep(100);
                continue;
            }
            failures = 0;
            m_connected = true;
            emit connectionReady();
        }

        m_warm = false;
        const bool healthy = connection->runTransfers(*m_queue);
        idle.restart();
        lastPing.restart();
        if (!healthy) {
            connection->close();
            m_connected = false;
            QThread::msleep(300);
        }
    }
    connection->close();
    m_connected = false;
}

TransferEngine::TransferEngine(quint64 sessionId, QObject *parent)
    : QObject(parent), m_sessionId(sessionId),
      m_queue(std::make_shared<TransferQueue>())
{
    m_clock.start();
    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &TransferEngine::tick);
}

TransferEngine::~TransferEngine()
{
    shutdown();
}

void TransferEngine::setProfile(const ConnectionProfile &profile,
                                const QString &sessionName)
{
    m_profile = profile;
    m_sessionName = sessionName;
}

void TransferEngine::setPolicy(OverwritePolicy policy)
{
    m_queue->setPolicy(policy);
}

void TransferEngine::setParallelism(int connections)
{
    m_parallel = std::clamp(connections, 1, 16);
    m_queue->setParallelism(m_parallel);
}

bool TransferEngine::busy() const
{
    return m_queue->hasWork() || m_timer->isActive();
}

void TransferEngine::enqueue(const QVector<TransferJob> &jobs)
{
    if (m_shutdown)
        return;
    for (const TransferJob &job : jobs) {
        TransferStatus status;
        status.id = job.id;
        status.sessionId = m_sessionId;
        status.sessionName = m_sessionName;
        status.name = job.upload ? QFileInfo(job.localPath).fileName()
                                 : QFileInfo(job.remotePath).fileName();
        status.localPath = job.localPath;
        status.remotePath = job.remotePath;
        status.upload = job.upload;
        status.directory = job.directory;
        status.state = TransferState::Queued;
        status.total = job.directory ? -1 : job.size;
        m_queue->addJob(job, status);
        emit statusChanged(status);
    }
    if (!m_timer->isActive()) {
        m_lastTick = m_clock.elapsed();
        m_timer->start();
    }
    spawnWorkers();
}

void TransferEngine::cancel(quint64 jobId)
{
    m_queue->cancel(jobId);
}

void TransferEngine::cancelAll()
{
    m_queue->cancelAll();
}

void TransferEngine::spawnWorkers()
{
    m_workers.erase(std::remove_if(m_workers.begin(), m_workers.end(),
                                   [](TransferWorker *worker) {
                                       if (worker->isFinished()) {
                                           worker->deleteLater();
                                           return true;
                                       }
                                       return false;
                                   }),
                    m_workers.end());
    if (m_workers.isEmpty())
        m_queue->setCapacity(64);
    const int queued = m_queue->queuedItems();
    if (queued == 0)
        return;
    const int limit = std::min(m_parallel, m_queue->capacity());
    const int wanted = std::min(limit, std::max<int>(1, queued));
    while (m_workers.size() < wanted) {
        auto *worker = new TransferWorker(m_queue, m_profile);
        connect(worker, &TransferWorker::connectionFailed, this,
                [this, worker](const QString &error, bool auth) {
                    workerFailed(worker, error, auth);
                });
        connect(worker, &TransferWorker::connectionReady, this,
                [this] { m_failingSince.invalidate(); });
        m_workers.append(worker);
        worker->start();
    }
}

void TransferEngine::prewarm()
{
    if (m_shutdown || m_profile.protocol != Protocol::Sftp)
        return;
    for (TransferWorker *worker : std::as_const(m_workers)) {
        if (!worker->isFinished())
            return;
    }
    auto *worker = new TransferWorker(m_queue, m_profile, true);
    connect(worker, &TransferWorker::connectionFailed, this,
            [this, worker](const QString &error, bool auth) { workerFailed(worker, error, auth); });
    connect(worker, &TransferWorker::connectionReady, this,
            [this] { m_failingSince.invalidate(); });
    m_workers.append(worker);
    worker->start();
}

void TransferEngine::workerFailed(TransferWorker *worker, const QString &error,
                                  bool auth)
{
    int connected = 0;
    for (TransferWorker *other : std::as_const(m_workers)) {
        if (other != worker && other->connected())
            ++connected;
    }
    if (connected > 0) {
        m_queue->setCapacity(connected);
        worker->requestStop();
        return;
    }
    if (auth) {
        m_queue->failQueued(error);
        emit notice(error);
        return;
    }
    if (!m_failingSince.isValid()) {
        m_failingSince.start();
        emit notice(tr("Передачи ждут соединения: %1").arg(error));
    } else if (m_failingSince.elapsed() > GiveUpAfterMs) {
        m_queue->failQueued(error);
        m_failingSince.invalidate();
    }
}

void TransferEngine::tick()
{
    const qint64 now = m_clock.elapsed();
    bool anyActive = false;
    const QVector<TransferStatus> changes = m_queue->snapshot(now - m_lastTick, anyActive);
    m_lastTick = now;
    for (const TransferStatus &status : changes) {
        emit statusChanged(status);
        if (status.state != TransferState::Running &&
            status.state != TransferState::Queued)
            emit jobFinished(status);
    }
    if (anyActive)
        spawnWorkers();
    else
        m_timer->stop();
}

void TransferEngine::shutdown()
{
    if (m_shutdown)
        return;
    m_shutdown = true;
    m_timer->stop();
    m_queue->setStopping(true);
    for (TransferWorker *worker : std::as_const(m_workers))
        worker->requestStop();
    for (TransferWorker *worker : std::as_const(m_workers)) {
        if (worker->wait(3000)) {
            delete worker;
        } else {
            worker->disconnect();
            connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        }
    }
    m_workers.clear();
}
