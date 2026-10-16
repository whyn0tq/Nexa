#include "session.h"

#include "transferengine.h"

#include <QSettings>
#include <QThread>
#include <QTimer>

Session::Session(quint64 id, const QString &hostId, const QString &title,
                 const ConnectionProfile &profile, QObject *parent)
    : QObject(parent), m_id(id), m_hostId(hostId), m_title(title), m_profile(profile)
{
    m_thread = new QThread(this);
    m_worker = new BrowseWorker;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &Session::startRequested, m_worker, &BrowseWorker::start);
    connect(this, &Session::trustAnswered, m_worker, &BrowseWorker::answerTrust);
    connect(this, &Session::reconnectRequested, m_worker, &BrowseWorker::reconnectNow);
    connect(this, &Session::listRequested, m_worker, &BrowseWorker::list);
    connect(this, &Session::mkdirRequested, m_worker, &BrowseWorker::makeDirectory);
    connect(this, &Session::touchRequested, m_worker, &BrowseWorker::createFile);
    connect(this, &Session::renameRequested, m_worker, &BrowseWorker::rename);
    connect(this, &Session::removeRequested, m_worker, &BrowseWorker::remove);
    connect(this, &Session::fetchRequested, m_worker, &BrowseWorker::fetchFile);
    connect(this, &Session::storeRequested, m_worker, &BrowseWorker::storeFile);

    connect(m_worker, &BrowseWorker::stateChanged, this,
            [this](BrowseWorker::State state, const QString &message) {
                m_state = state;
                m_stateMessage = message;
                emit stateChanged();
            });
    connect(m_worker, &BrowseWorker::trustRequired, this, &Session::trustRequired);
    connect(m_worker, &BrowseWorker::ready, this,
            [this](const QString &start, const QString &info, const QString &fingerprint,
                   bool reconnected) {
                m_start = start;
                m_serverInfo = info;
                m_profile.trustedFingerprint = fingerprint;
                m_transfers->setProfile(m_profile, m_title);
                emit ready(reconnected);
                QTimer::singleShot(400, m_transfers, &TransferEngine::prewarm);
            });
    connect(m_worker, &BrowseWorker::listed, this, &Session::listed);
    connect(m_worker, &BrowseWorker::listFailed, this, &Session::listFailed);
    connect(m_worker, &BrowseWorker::operationDone, this, &Session::operationDone);
    connect(m_worker, &BrowseWorker::operationFailed, this, &Session::operationFailed);
    connect(m_worker, &BrowseWorker::removalProgress, this, &Session::removalProgress);
    connect(m_worker, &BrowseWorker::fileFetched, this, &Session::fileFetched);
    connect(m_worker, &BrowseWorker::fileStored, this, &Session::fileStored);
    connect(m_worker, &BrowseWorker::latency, this, [this](int value) {
        m_latency = value;
        emit stateChanged();
    });

    m_transfers = new TransferEngine(id, this);
    QSettings settings;
    m_transfers->setParallelism(settings.value(QStringLiteral("transfers/connections"),
                                               m_profile.protocol == Protocol::Ftp ? 3 : 4)
                                    .toInt());
    m_transfers->setPolicy(static_cast<OverwritePolicy>(
        settings.value(QStringLiteral("transfers/policy"), 0).toInt()));
    m_transfers->setProfile(m_profile, m_title);
    m_thread->start();
}

Session::~Session()
{
    shutdown();
}

void Session::setTitle(const QString &title)
{
    m_title = title;
    m_transfers->setProfile(m_profile, m_title);
    emit stateChanged();
}

void Session::setProfile(const ConnectionProfile &profile)
{
    m_profile = profile;
    m_profile.trustedFingerprint.clear();
    m_transfers->setProfile(m_profile, m_title);
    open();
}

void Session::open()
{
    m_state = State::Connecting;
    emit stateChanged();
    emit startRequested(m_profile, m_start);
}

void Session::retry()
{
    if (m_state == State::Offline || m_state == State::Idle)
        open();
    else
        emit reconnectRequested();
}

void Session::answerTrust(bool accepted, bool remember)
{
    emit trustAnswered(accepted, remember);
}

quint64 Session::list(const QString &path)
{
    const quint64 generation = m_worker->nextGeneration();
    emit listRequested(path, generation);
    return generation;
}

void Session::makeDirectory(const QString &path)
{
    emit mkdirRequested(path);
}

void Session::createFile(const QString &path)
{
    emit touchRequested(path);
}

void Session::rename(const QString &from, const QString &to)
{
    emit renameRequested(from, to);
}

void Session::remove(const RemoteEntries &entries)
{
    emit removeRequested(entries);
}

quint64 Session::fetchFile(const QString &path, qint64 sizeHint)
{
    const quint64 token = m_nextToken++;
    emit fetchRequested(token, path, sizeHint);
    return token;
}

quint64 Session::storeFile(const QString &path, const QByteArray &data)
{
    const quint64 token = m_nextToken++;
    emit storeRequested(token, path, data);
    return token;
}

void Session::shutdown()
{
    if (!m_thread)
        return;
    m_transfers->shutdown();
    m_worker->requestStop();
    QMetaObject::invokeMethod(m_worker, &BrowseWorker::stop, Qt::QueuedConnection);
    m_thread->quit();
    if (!m_thread->wait(4000)) {
        m_thread->setParent(nullptr);
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    }
    m_thread = nullptr;
}
