#pragma once

#include "connection.h"

#include <QByteArray>
#include <QString>

class QHostAddress;
class QSslSocket;
class QTcpSocket;

class FtpConnection final : public RemoteConnection {
public:
    FtpConnection() = default;
    ~FtpConnection() override;

    OpenResult open(const ConnectionProfile &profile,
                    bool interactiveTrust) override;
    OpenResult continueAfterTrust(bool accepted, bool remember) override;
    void close() override;
    bool isOpen() const override;
    bool ping() override;

    QString homePath() override;
    bool list(const QString &path, RemoteEntries &entries) override;
    bool makeDirectory(const QString &path) override;
    bool createFile(const QString &path) override;
    bool rename(const QString &from, const QString &to) override;
    bool remove(const RemoteEntries &entries,
                const std::function<void(qint64)> &progress) override;

    bool expand(const TransferJob &job, TransferHost &host) override;
    bool runTransfers(TransferHost &host) override;

private:
    struct Reply {
        QString text;
        int code = 0;
        bool valid = false;
    };

    OpenResult finishOpen();
    bool negotiateTls();
    bool login();
    void loadFeatures();
    QString failureText(const QString &prefix, const Reply &reply) const;

    QByteArray encodePath(const QString &path) const;
    QString decodeText(const QByteArray &data) const;
    bool waitForData(QTcpSocket *socket, int milliseconds) const;
    bool readControlLine(QByteArray &line);
    Reply readReply();
    Reply command(const QString &verb, const QString &argument = QString());

    bool enterPassiveMode(QHostAddress &address, quint16 &port);
    QSslSocket *openData(const QString &verb, const QString &argument,
                         qint64 *restartOffset = nullptr);
    void closeData(QSslSocket *socket);
    bool finishData();

    bool changeDirectory(const QString &path);
    qint64 remoteSize(const QString &path);
    qint64 remoteTime(const QString &path);
    bool remoteIsDirectory(const QString &path);
    bool readDirectory(const QString &path, RemoteEntries &entries);
    bool ensureDirectory(const QString &path, bool &created);

    FileOutcome download(FileTask &task, TransferHost &host, QString &error);
    FileOutcome upload(FileTask &task, TransferHost &host, QString &error);

    QSslSocket *m_control = nullptr;
    QByteArray m_buffer;
    QString m_pendingFingerprint;
    QString m_workingDirectory;
    QString m_lastReply;
    bool m_waitingForTrust = false;
    bool m_tlsActive = false;
    bool m_utf8 = false;
    bool m_useEpsv = true;
    bool m_hasMlsd = false;
    bool m_hasMfmt = false;
    bool m_hasSize = true;
    std::function<bool()> m_abort;
};
