#include "browseworker.h"

#include <QTimer>

#include <algorithm>

namespace {
constexpr int KeepAliveTick = 5000;
constexpr int KeepAliveIdle = 4000;
}

BrowseWorker::BrowseWorker(QObject *parent) : QObject(parent)
{
    m_lastActivity.start();
}

BrowseWorker::~BrowseWorker()
{
    if (m_connection)
        m_connection->close();
}

void BrowseWorker::start(const ConnectionProfile &profile, const QString &startPath)
{
    if (!m_keepAlive) {
        m_keepAlive = new QTimer(this);
        m_keepAlive->setInterval(KeepAliveTick);
        connect(m_keepAlive, &QTimer::timeout, this, &BrowseWorker::keepAlive);
        m_retry = new QTimer(this);
        m_retry->setSingleShot(true);
        connect(m_retry, &QTimer::timeout, this, &BrowseWorker::attemptReconnect);
    }
    m_profile = profile;
    m_startPath = startPath;
    m_connection = RemoteConnection::create(profile.protocol);
    m_connection->setCancelCheck([this] { return m_stopping.load(); });
    m_state = State::Connecting;
    m_everOnline = false;
    m_attempt = 0;
    emit stateChanged(m_state, tr("Подключение к %1…").arg(profile.host));
    handleOpen(m_connection->open(m_profile, true));
}

void BrowseWorker::handleOpen(RemoteConnection::OpenResult result)
{
    using R = RemoteConnection::OpenResult;
    switch (result) {
    case R::NeedsTrust: {
        m_state = State::AwaitingTrust;
        const auto request = m_connection->trustRequest();
        emit stateChanged(m_state, tr("Подтвердите ключ сервера"));
        emit trustRequired(request.host, request.fingerprint, request.keyType,
                           request.changed);
        return;
    }
    case R::Ok: {
        const bool reconnected = m_everOnline;
        m_profile.trustedFingerprint = m_connection->trustedFingerprint();
        m_everOnline = true;
        m_attempt = 0;
        m_state = State::Online;
        touch();
        m_keepAlive->start();
        if (!reconnected) {
            QString folder;
            RemoteEntries entries;
            bool found = m_connection->listStart(m_startPath, folder, entries);
            if (!found && !m_connection->connectionBroken() && !m_startPath.isEmpty())
                found = m_connection->listStart(QString(), folder, entries);
            if (!found && m_connection->connectionBroken()) {
                connectionLost(m_connection->lastError());
                return;
            }
            touch();
            if (!folder.isEmpty())
                m_startPath = folder;
            if (m_startPath.isEmpty())
                m_startPath = QStringLiteral("/");
            if (found)
                emit listed(m_startPath, entries, 0);
            emit stateChanged(m_state, QString());
            emit ready(m_startPath, m_connection->serverInfo(),
                       m_connection->trustedFingerprint(), reconnected);
            if (found)
                prefetchChildren(entries, m_generation.load());
        } else {
            emit stateChanged(m_state, QString());
            emit ready(m_startPath, m_connection->serverInfo(),
                       m_connection->trustedFingerprint(), reconnected);
        }
        QTimer::singleShot(0, this, [this] {
            if (m_state == State::Online)
                measurePing();
        });
        return;
    }
    case R::AuthFailed:
    case R::Failed:
        if (m_stopping)
            return;
        if (!m_everOnline || (result == R::AuthFailed && m_attempt >= 6) ||
            m_connection->trustRequest().changed) {
            m_state = State::Offline;
            emit stateChanged(m_state, m_connection->lastError());
            return;
        }
        scheduleReconnect();
        return;
    }
}

void BrowseWorker::answerTrust(bool accepted, bool remember)
{
    if (m_state != State::AwaitingTrust || !m_connection)
        return;
    m_state = State::Connecting;
    emit stateChanged(m_state, tr("Вход…"));
    handleOpen(m_connection->continueAfterTrust(accepted, remember));
}

void BrowseWorker::reconnectNow()
{
    if (m_stopping || !m_connection)
        return;
    if (m_state == State::Online && m_connection->ping())
        return;
    m_connection->close();
    m_attempt = 0;
    if (m_retry)
        m_retry->stop();
    m_state = State::Reconnecting;
    emit stateChanged(m_state, tr("Переподключение…"));
    attemptReconnect();
}

void BrowseWorker::scheduleReconnect()
{
    if (m_stopping)
        return;
    m_state = State::Reconnecting;
    static const int delays[] = {500, 1000, 2000, 3000, 5000, 8000, 10000};
    const int delay = delays[std::min<int>(m_attempt, std::size(delays) - 1)];
    ++m_attempt;
    emit stateChanged(m_state, m_attempt > 1 ? tr("Переподключение (попытка %1)…")
                                                   .arg(m_attempt)
                                             : tr("Переподключение…"));
    m_retry->start(delay);
}

void BrowseWorker::attemptReconnect()
{
    if (m_stopping || !m_connection)
        return;
    handleOpen(m_connection->open(m_profile, false));
}

void BrowseWorker::connectionLost(const QString &message)
{
    Q_UNUSED(message);
    m_keepAlive->stop();
    m_connection->close();
    if (!m_stopping)
        scheduleReconnect();
}

bool BrowseWorker::ensureOnline()
{
    if (m_state == State::Online && m_connection && m_connection->isOpen())
        return true;
    emit operationFailed(tr("Нет соединения с сервером — переподключаюсь."));
    if (m_state == State::Online)
        connectionLost(QString());
    return false;
}

void BrowseWorker::keepAlive()
{
    if (m_state != State::Online || m_lastActivity.elapsed() < KeepAliveIdle)
        return;
    measurePing();
}

void BrowseWorker::measurePing()
{
    QElapsedTimer timer;
    timer.start();
    if (m_connection->ping()) {
        touch();
        emit latency(static_cast<int>(std::min<qint64>(timer.elapsed(), 9999)));
        return;
    }
    connectionLost(m_connection->lastError());
}

void BrowseWorker::list(const QString &path, quint64 generation)
{
    if (generation != m_generation.load())
        return;
    if (!ensureOnline())
        return;
    RemoteEntries entries;
    const bool ok = m_connection->list(path, entries);
    touch();
    if (!ok) {
        if (m_connection->connectionBroken()) {
            connectionLost(m_connection->lastError());
            return;
        }
        emit listFailed(RemoteConnection::normalized(path), m_connection->lastError(),
                        generation);
        return;
    }
    emit listed(RemoteConnection::normalized(path), entries, generation);
    prefetchChildren(entries, generation);
}

void BrowseWorker::prefetchChildren(const RemoteEntries &entries, quint64 generation)
{
    constexpr int Limit = 32;
    constexpr qint64 FreshMs = 20000;
    if (!m_prefetchClock.isValid())
        m_prefetchClock.start();
    const qint64 now = m_prefetchClock.elapsed();
    QStringList directories;
    for (const RemoteEntry &entry : entries) {
        if (!entry.directory)
            continue;
        const auto seen = m_prefetchedAt.constFind(entry.path);
        if (seen != m_prefetchedAt.cend() && now - seen.value() < FreshMs)
            continue;
        directories.append(entry.path);
        if (directories.size() >= Limit)
            break;
    }
    m_prefetchQueue = directories;
    m_prefetchGeneration = generation;
    if (!m_prefetchQueue.isEmpty())
        QMetaObject::invokeMethod(this, &BrowseWorker::prefetchStep, Qt::QueuedConnection);
}

void BrowseWorker::prefetchStep()
{
    constexpr int Portion = 6;
    if (m_prefetchQueue.isEmpty() || m_state != State::Online)
        return;
    if (m_prefetchGeneration != m_generation.load()) {
        m_prefetchQueue.clear();
        return;
    }
    const QStringList portion = m_prefetchQueue.mid(0, Portion);
    m_prefetchQueue = m_prefetchQueue.mid(Portion);
    const qint64 now = m_prefetchClock.elapsed();
    m_connection->prefetch(portion, [this, now](const QString &path, const RemoteEntries &found) {
        m_prefetchedAt.insert(path, now);
        emit listed(path, found, 0);
    });
    touch();
    if (m_connection->connectionBroken()) {
        m_prefetchQueue.clear();
        connectionLost(m_connection->lastError());
        return;
    }
    if (!m_prefetchQueue.isEmpty())
        QMetaObject::invokeMethod(this, &BrowseWorker::prefetchStep, Qt::QueuedConnection);
}

void BrowseWorker::fetchFile(quint64 token, const QString &path, qint64 sizeHint)
{
    if (m_state != State::Online || !m_connection || !m_connection->isOpen()) {
        emit fileFetched(token, path, {}, tr("Нет соединения с сервером."));
        return;
    }
    QByteArray data;
    const bool ok = m_connection->readFile(path, sizeHint, data);
    touch();
    emit fileFetched(token, path, ok ? data : QByteArray(), ok ? QString() : m_connection->lastError());
    if (!ok && m_connection->connectionBroken())
        connectionLost(m_connection->lastError());
}

void BrowseWorker::storeFile(quint64 token, const QString &path, const QByteArray &data)
{
    if (m_state != State::Online || !m_connection || !m_connection->isOpen()) {
        emit fileStored(token, path, tr("Нет соединения с сервером."));
        return;
    }
    const bool ok = m_connection->writeFile(path, data);
    touch();
    emit fileStored(token, path, ok ? QString() : m_connection->lastError());
    if (!ok && m_connection->connectionBroken())
        connectionLost(m_connection->lastError());
}

void BrowseWorker::makeDirectory(const QString &path)
{
    if (!ensureOnline())
        return;
    const bool ok = m_connection->makeDirectory(path);
    touch();
    if (!ok) {
        emit operationFailed(m_connection->lastError());
        if (m_connection->connectionBroken())
            connectionLost(m_connection->lastError());
        return;
    }
    emit operationDone(tr("Папка создана"), RemoteConnection::parentPath(path));
}

void BrowseWorker::createFile(const QString &path)
{
    if (!ensureOnline())
        return;
    const bool ok = m_connection->createFile(path);
    touch();
    if (!ok) {
        emit operationFailed(m_connection->lastError());
        if (m_connection->connectionBroken())
            connectionLost(m_connection->lastError());
        return;
    }
    emit operationDone(tr("Файл создан"), RemoteConnection::parentPath(path));
}

void BrowseWorker::rename(const QString &from, const QString &to)
{
    if (!ensureOnline())
        return;
    const bool ok = m_connection->rename(from, to);
    touch();
    if (!ok) {
        emit operationFailed(m_connection->lastError());
        if (m_connection->connectionBroken())
            connectionLost(m_connection->lastError());
        return;
    }
    emit operationDone(tr("Переименовано"), RemoteConnection::parentPath(to));
}

void BrowseWorker::remove(const RemoteEntries &entries)
{
    if (entries.isEmpty() || !ensureOnline())
        return;
    const bool ok = m_connection->remove(entries, [this](qint64 count) {
        emit removalProgress(count);
    });
    touch();
    if (!ok) {
        emit operationFailed(m_connection->lastError());
        if (m_connection->connectionBroken())
            connectionLost(m_connection->lastError());
        emit operationDone(QString(), RemoteConnection::parentPath(entries.first().path));
        return;
    }
    emit operationDone(entries.size() == 1 ? tr("Удалено: %1").arg(entries.first().name)
                                           : tr("Удалено объектов: %1").arg(entries.size()),
                       RemoteConnection::parentPath(entries.first().path));
}

void BrowseWorker::stop()
{
    m_stopping = true;
    if (m_keepAlive)
        m_keepAlive->stop();
    if (m_retry)
        m_retry->stop();
    if (m_connection)
        m_connection->close();
    m_state = State::Idle;
}
