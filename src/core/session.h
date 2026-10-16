#pragma once

#include "browseworker.h"
#include "types.h"

#include <QObject>

class QThread;
class TransferEngine;

class Session final : public QObject {
    Q_OBJECT

public:
    using State = BrowseWorker::State;

    Session(quint64 id, const QString &hostId, const QString &title,
            const ConnectionProfile &profile, QObject *parent = nullptr);
    ~Session() override;

    quint64 id() const { return m_id; }
    QString hostId() const { return m_hostId; }
    QString title() const { return m_title; }
    void setTitle(const QString &title);
    void setProfile(const ConnectionProfile &profile);
    ConnectionProfile profile() const { return m_profile; }
    State state() const { return m_state; }
    QString stateMessage() const { return m_stateMessage; }
    QString startPath() const { return m_start; }
    void setStartPath(const QString &path) { m_start = path; }
    QString serverInfo() const { return m_serverInfo; }
    int latency() const { return m_latency; }
    bool online() const { return m_state == State::Online; }
    TransferEngine *transfers() const { return m_transfers; }

    void open();
    void retry();
    void answerTrust(bool accepted, bool remember);
    quint64 list(const QString &path);
    void makeDirectory(const QString &path);
    void createFile(const QString &path);
    void rename(const QString &from, const QString &to);
    void remove(const RemoteEntries &entries);
    bool supportsQuickFiles() const { return m_profile.protocol == Protocol::Sftp; }
    quint64 fetchFile(const QString &path, qint64 sizeHint);
    quint64 storeFile(const QString &path, const QByteArray &data);
    void shutdown();

signals:
    void stateChanged();
    void trustRequired(const QString &host, const QString &fingerprint,
                       const QString &keyType, bool changed);
    void ready(bool reconnected);
    void listed(const QString &path, const RemoteEntries &entries, quint64 generation);
    void listFailed(const QString &path, const QString &message, quint64 generation);
    void operationDone(const QString &message, const QString &refreshPath);
    void operationFailed(const QString &message);
    void removalProgress(qint64 count);
    void fileFetched(quint64 token, const QString &path, const QByteArray &data,
                     const QString &error);
    void fileStored(quint64 token, const QString &path, const QString &error);

    void startRequested(const ConnectionProfile &profile, const QString &startPath);
    void trustAnswered(bool accepted, bool remember);
    void reconnectRequested();
    void listRequested(const QString &path, quint64 generation);
    void mkdirRequested(const QString &path);
    void touchRequested(const QString &path);
    void renameRequested(const QString &from, const QString &to);
    void removeRequested(const RemoteEntries &entries);
    void fetchRequested(quint64 token, const QString &path, qint64 sizeHint);
    void storeRequested(quint64 token, const QString &path, const QByteArray &data);

private:
    quint64 m_id;
    QString m_hostId;
    QString m_title;
    ConnectionProfile m_profile;
    QThread *m_thread = nullptr;
    BrowseWorker *m_worker = nullptr;
    TransferEngine *m_transfers = nullptr;
    State m_state = State::Idle;
    QString m_stateMessage;
    QString m_start;
    QString m_serverInfo;
    int m_latency = -1;
    quint64 m_nextToken = 1;
};
