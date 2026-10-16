#include "connection.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>

#include <libssh/libssh.h>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 5) {
        std::fprintf(stderr, "usage: host port user password-file [start-path]\n");
        return 2;
    }
    if (ssh_init() != SSH_OK)
        return 1;
    ConnectionProfile profile;
    profile.host = args.at(1);
    profile.port = static_cast<quint16>(args.at(2).toUInt());
    profile.username = args.at(3);
    QFile secret(args.at(4));
    if (!secret.open(QIODevice::ReadOnly))
        return 2;
    profile.password = QString::fromUtf8(secret.readAll()).trimmed();
    const QString start = args.size() > 5 ? args.at(5) : QString();

    for (int run = 0; run < 3; ++run) {
        auto connection = RemoteConnection::create(Protocol::Sftp);
        QElapsedTimer clock;
        clock.start();
        auto result = connection->open(profile, true);
        if (result == RemoteConnection::OpenResult::NeedsTrust) {
            const qint64 paused = clock.elapsed();
            result = connection->continueAfterTrust(true, false);
            Q_UNUSED(paused);
        }
        if (result != RemoteConnection::OpenResult::Ok) {
            std::fprintf(stderr, "connect failed: %s\n", qPrintable(connection->lastError()));
            return 1;
        }
        profile.trustedFingerprint = connection->trustedFingerprint();
        const qint64 connected = clock.elapsed();
        QString folder;
        RemoteEntries entries;
        const bool listed = connection->listStart(start, folder, entries);
        const qint64 total = clock.elapsed();
        std::printf("run %d: login %lld ms, first listing %lld ms, total %lld ms (%s, %d entries%s)\n",
                    run + 1, static_cast<long long>(connected),
                    static_cast<long long>(total - connected), static_cast<long long>(total),
                    qPrintable(folder), static_cast<int>(entries.size()),
                    listed ? "" : ", FAILED");
        connection->close();
    }
    ssh_finalize();
    return 0;
}
