#pragma once

#include "connection.h"
#include "types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QStringList>

#include <atomic>
#include <memory>

class QTimer;

class BrowseWorker final : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Connecting, AwaitingTrust, Online, Reconnecting, Offline };
    Q_ENUM(State)

    explicit BrowseWorker(QObject *parent = nullptr);
    ~BrowseWorker() override;

    quint64 nextGeneration() { return ++m_generation; }
    void requestStop() { m_stopping = true; }

public slots:
    void start(const ConnectionProfile &profile, const QString &startPath);
    void answerTrust(bool accepted, bool remember);
    void reconnectNow();
    void list(const QString &path, quint64 generation);
    void makeDirectory(const QString &path);
    void createFile(const QString &path);
    void rename(const QString &from, const QString &to);
    void remove(const RemoteEntries &entries);
    void fetchFile(quint64 token, const QString &path, qint64 sizeHint);
    void storeFile(quint64 token, const QString &path, const QByteArray &data);
    void stop();

signals:
    void stateChanged(BrowseWorker::State state, const QString &message);
    void trustRequired(const QString &host, const QString &fingerprint,
                       const QString &keyType, bool changed);
    void ready(const QString &startFolder, const QString &serverInfo,
               const QString &fingerprint, bool reconnected);
    void listed(const QString &path, const RemoteEntries &entries,
                quint64 generation);
    void listFailed(const QString &path, const QString &message,
                    quint64 generation);
    void operationDone(const QString &message, const QString &refreshPath);
    void operationFailed(const QString &message);
    void removalProgress(qint64 count);
    void fileFetched(quint64 token, const QString &path, const QByteArray &data,
                     const QString &error);
    void fileStored(quint64 token, const QString &path, const QString &error);
    void latency(int milliseconds);

private:
    void handleOpen(RemoteConnection::OpenResult result);
    bool ensureOnline();
    void connectionLost(const QString &message);
    void scheduleReconnect();
    void attemptReconnect();
    void keepAlive();
    void measurePing();
    void prefetchChildren(const RemoteEntries &entries, quint64 generation);
    void prefetchStep();
    void touch() { m_lastActivity.restart(); }

    std::unique_ptr<RemoteConnection> m_connection;
    ConnectionProfile m_profile;
    QString m_startPath;
    QTimer *m_keepAlive = nullptr;
    QTimer *m_retry = nullptr;
    QElapsedTimer m_lastActivity;
    QElapsedTimer m_prefetchClock;
    QHash<QString, qint64> m_prefetchedAt;
    QStringList m_prefetchQueue;
    quint64 m_prefetchGeneration = 0;
    State m_state = State::Idle;
    int m_attempt = 0;
    bool m_everOnline = false;
    std::atomic<quint64> m_generation{0};
    std::atomic_bool m_stopping{false};
};
