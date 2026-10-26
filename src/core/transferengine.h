#pragma once

#include "connection.h"
#include "types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

#include <atomic>
#include <deque>
#include <memory>

class QTimer;
class TransferEngine;

class TransferQueue final : public TransferHost {
public:
    struct Job {
        TransferStatus status;
        qint64 done = 0;
        qint64 total = 0;
        int units = 0;
        int unitsDone = 0;
        int filesTotal = 0;
        int filesDone = 0;
        int filesFailed = 0;
        int filesSkipped = 0;
        bool expanded = false;
        bool canceled = false;
        bool finished = false;
        bool dirty = true;
        QStringList errors;
        QString current;
        qint64 lastDone = 0;
        double speed = 0;
    };

    TransferQueue() = default;

    void addJob(const TransferJob &job, const TransferStatus &status);
    void cancel(quint64 jobId);
    void cancelAll();
    void failQueued(const QString &error);
    int queuedItems() const;
    bool hasWork() const;
    bool waitForWork(int milliseconds);
    void wakeAll();
    void setStopping(bool value);
    void setPolicy(OverwritePolicy policy) { m_policy = static_cast<int>(policy); }
    void setParallelism(int value) { m_parallel = value; }
    void setCapacity(int value) { m_capacity = value; }
    int capacity() const { return m_capacity; }

    QVector<TransferStatus> snapshot(qint64 elapsedMs, bool &anyActive);

    bool take(WorkItem &item, bool allowExpand, bool allowSegments) override;
    void pushFront(const QVector<FileTask> &tasks) override;
    void addTasks(quint64 jobId, const QVector<FileTask> &tasks) override;
    void expansionFinished(quint64 jobId) override;
    void expansionFailed(quint64 jobId, const QString &error) override;
    void requeueJob(const TransferJob &job) override;
    void addUnits(quint64 jobId, int count) override;
    void progress(quint64 jobId, qint64 delta) override;
    void finished(const FileTask &task, FileOutcome outcome, bool countsAsFile,
                  const QString &error) override;
    void current(quint64 jobId, const QString &path) override;
    bool canceled(quint64 jobId) const override;
    bool stopping() const override { return m_stopping.load(); }
    OverwritePolicy policy() const override
    {
        return static_cast<OverwritePolicy>(m_policy.load());
    }
    int parallelism() const override
    {
        return std::max(1, std::min(m_parallel.load(), m_capacity.load()));
    }

private:
    void settleLocked(Job &job);
    void skipCanceledLocked();

    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    std::deque<WorkItem> m_items;
    QHash<quint64, std::shared_ptr<Job>> m_jobs;
    std::atomic_bool m_stopping{false};
    std::atomic_int m_policy{static_cast<int>(OverwritePolicy::Resume)};
    std::atomic_int m_parallel{2};
    std::atomic_int m_capacity{64};
};

class TransferWorker final : public QThread {
    Q_OBJECT

public:
    TransferWorker(std::shared_ptr<TransferQueue> queue,
                   const ConnectionProfile &profile, bool warm = false);

    bool connected() const { return m_connected.load(); }
    void requestStop() { m_stop = true; }

signals:
    void connectionFailed(const QString &error, bool authentication);
    void connectionReady();

protected:
    void run() override;

private:
    std::shared_ptr<TransferQueue> m_queue;
    ConnectionProfile m_profile;
    std::atomic_bool m_stop{false};
    bool m_warm = false;
    std::atomic_bool m_connected{false};
};

class TransferEngine final : public QObject {
    Q_OBJECT

public:
    explicit TransferEngine(quint64 sessionId, QObject *parent = nullptr);
    ~TransferEngine() override;

    void setProfile(const ConnectionProfile &profile, const QString &sessionName);
    void setPolicy(OverwritePolicy policy);
    void setParallelism(int connections);
    void enqueue(const QVector<TransferJob> &jobs);
    void cancel(quint64 jobId);
    void cancelAll();
    void shutdown();
    bool busy() const;
    void prewarm();

signals:
    void statusChanged(const TransferStatus &status);
    void jobFinished(const TransferStatus &status);
    void notice(const QString &message);

private:
    friend class TransferWorker;
    void tick();
    void spawnWorkers();
    void workerFailed(TransferWorker *worker, const QString &error, bool auth);

    quint64 m_sessionId;
    QString m_sessionName;
    ConnectionProfile m_profile;
    std::shared_ptr<TransferQueue> m_queue;
    QVector<TransferWorker *> m_workers;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    qint64 m_lastTick = 0;
    QElapsedTimer m_failingSince;
    int m_parallel = 2;
    bool m_shutdown = false;
};
