#pragma once

#include "remoteworker.h"
#include "types.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class QHostAddress;
class QSslSocket;
class QTcpSocket;

class FtpWorker final : public RemoteWorker {
    Q_OBJECT

public:
    explicit FtpWorker(Role role, QObject *parent = nullptr);
    ~FtpWorker() override;

public slots:
    void connectToHost(const ConnectionProfile &profile) override;
    void confirmHostKey(bool accepted, bool remember) override;
    void disconnectFromHost() override;
    void listDirectory(const QString &path, quint64 generation) override;
    void createDirectory(const QString &path) override;
    void createFile(const QString &path) override;
    void removeEntry(const QString &path, bool directory) override;
    void renameEntry(const QString &oldPath, const QString &newPath) override;

private:
    struct Reply {
        QString text;
        int code = 0;
        bool valid = false;
    };

    bool openControl();
    bool negotiateTls();
    bool acceptCertificate();
    void finishConnect();
    bool login();
    void loadFeatures();
    void cleanup();
    bool reconnectSession();
    bool controlReady() const;
    QString failureText(const QString &prefix, const Reply &reply) const;

    QByteArray encodePath(const QString &path) const;
    QString decodeText(const QByteArray &data) const;
    bool waitForData(QTcpSocket *socket, int milliseconds) const;
    bool readControlLine(QByteArray &line);
    Reply readReply();
    Reply command(const QString &verb, const QString &argument = QString());

    bool enterPassiveMode(QHostAddress &address, quint16 &port);
    QSslSocket *openDataConnection(const QString &verb, const QString &argument,
                                   qint64 *restartOffset = nullptr);
    void closeDataConnection(QSslSocket *socket);
    bool finishDataTransfer();

    bool changeDirectory(const QString &path);
    QString currentDirectory();
    qint64 remoteFileSize(const QString &path);
    qint64 remoteModifiedTime(const QString &path);
    bool remoteIsDirectory(const QString &path);
    bool readRemoteDirectory(const QString &path, RemoteEntries &entries,
                             QString &failure);

    TransferState runJob(const TransferJob &job) override;
    bool expandJob(const TransferJob &job, QVector<FileTask> &tasks,
                   QString &failure);
    bool expandDownload(const TransferJob &job, QVector<FileTask> &tasks,
                        QString &failure);
    bool expandUpload(const TransferJob &job, QVector<FileTask> &tasks,
                      QString &failure);
    FileOutcome downloadFileTask(const FileTask &task, JobContext &context,
                                 int retries);
    FileOutcome uploadFileTask(const FileTask &task, JobContext &context,
                               int retries);

    bool ensureRemoteDirectory(const QString &path);
    void applyRemoteMetadata(const QString &remotePath, const FileTask &task);
    void applyLocalMetadata(const QString &localPath, const FileTask &task);
    bool removeRecursively(const QString &path, bool directory,
                           quint64 &removedCount, QString &failedPath);

    QSslSocket *m_control = nullptr;
    QByteArray m_controlBuffer;
    QString m_pendingFingerprint;
    QString m_runtimeTrustedFingerprint;
    QString m_serverBanner;
    QString m_workingDirectory;
    QString m_lastReply;
    bool m_waitingForHostKey = false;
    bool m_reconnecting = false;
    bool m_tlsActive = false;
    bool m_utf8 = false;
    bool m_useEpsv = true;
    bool m_hasMlsd = false;
    bool m_hasMfmt = false;
    bool m_hasSize = true;
};
