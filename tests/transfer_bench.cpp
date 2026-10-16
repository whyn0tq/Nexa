#include "sftpconnection.h"
#include "transferengine.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <libssh/libssh.h>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 9) {
        std::fprintf(stderr, "usage: host port user password-file upload|download local "
                             "remote connections\n");
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
    const bool upload = args.at(5) == QStringLiteral("upload");
    const QString local = args.at(6);
    const QString remote = args.at(7);
    const int connections = args.at(8).toInt();

    {
        SftpConnection probe;
        auto result = probe.open(profile, true);
        if (result == RemoteConnection::OpenResult::NeedsTrust)
            result = probe.continueAfterTrust(true, false);
        if (result != RemoteConnection::OpenResult::Ok) {
            std::fprintf(stderr, "connect failed: %s\n", qPrintable(probe.lastError()));
            return 1;
        }
        profile.trustedFingerprint = probe.trustedFingerprint();
    }

    TransferEngine engine(1);
    engine.setProfile(profile, QStringLiteral("bench"));
    engine.setParallelism(connections);
    engine.setPolicy(OverwritePolicy::Overwrite);

    TransferJob job;
    job.id = 1;
    job.upload = upload;
    job.localPath = local;
    job.remotePath = remote;
    job.directory = upload ? QFileInfo(local).isDir() : true;
    if (!upload && args.size() > 9 && args.at(9) == QStringLiteral("file")) {
        job.directory = false;
        job.size = -1;
    }

    QElapsedTimer clock;
    int exitCode = 0;
    QObject::connect(&engine, &TransferEngine::jobFinished, &app,
                     [&](const TransferStatus &status) {
                         const double seconds = clock.elapsed() / 1000.0;
                         std::printf("%s %s: %d files, %.1f MB in %.2f s = %.2f MB/s, "
                                     "%.0f files/s, state=%d failed=%d %s\n",
                                     upload ? "upload" : "download",
                                     qPrintable(QFileInfo(local).fileName()),
                                     status.filesTotal, status.done / 1048576.0, seconds,
                                     status.done / 1048576.0 / seconds,
                                     status.filesTotal / seconds, static_cast<int>(status.state),
                                     status.filesFailed, qPrintable(status.detail));
                         exitCode = status.state == TransferState::Completed ? 0 : 1;
                         QTimer::singleShot(0, &app, &QCoreApplication::quit);
                     });
    QObject::connect(&engine, &TransferEngine::notice, &app, [](const QString &message) {
        std::fprintf(stderr, "notice: %s\n", qPrintable(message));
    });
    clock.start();
    engine.enqueue({job});
    app.exec();
    engine.shutdown();
    ssh_finalize();
    return exitCode;
}
