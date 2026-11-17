#include "connection.h"
#include "transferengine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <cstdio>
#include <future>
#include <thread>

namespace {
const QByteArray Target = "/csgo/addons/metamod/utils.vdf";
const QByteArray Next = "/csgo/addons/metamod/z.cfg";
const QByteArray Hidden = "/csgo/addons/metamod/.in.utils.vdf.";
const QByteArray Subdir = "/csgo/addons/metamod/sub";
const QByteArray Nested = Subdir + "/nested.cfg";
const QByteArray Payload = QByteArray("complete file content\n").repeated(240000);
const QByteArray Partial = "old";

enum class Scenario {
    Direct, Conflict, Restart, RenameDenied, PermissionDenied, Overwrite,
    Interrupted, Disconnected, FolderInterrupted, ConnectionLimit
};

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

bool send(QTcpSocket &socket, const QByteArray &line)
{
    const QByteArray reply = line + "\r\n";
    return socket.write(reply) == reply.size() && socket.waitForBytesWritten(3000);
}

struct Server {
    Scenario scenario;
    QHash<QByteArray, QByteArray> files;
    QList<QByteArray> stores;
    QList<QByteArray> restarts;
    QList<QByteArray> deletes;
    QList<QByteArray> directories;
    QByteArray renamed;
    int sessions = 0;
    int sizes = 0;
    bool retry = false;

    bool serve(QTcpSocket &control);
};

bool Server::serve(QTcpSocket &control)
{
    QTcpServer passive;
    qint64 offset = 0;
    QByteArray renameFrom;
    if (!send(control, "220 Test FTP server"))
        return false;
    ++sessions;
    retry = false;

    for (;;) {
        while (!control.canReadLine()) {
            if (!control.waitForReadyRead(3000))
                return retry && control.state() == QAbstractSocket::UnconnectedState;
        }
        const QByteArray line = control.readLine().trimmed();
        const QByteArray verb = line.split(' ').first();
        const QByteArray path = line.mid(verb.size() + 1);
        QByteArray reply = "500 Unexpected command";
        if (verb == "USER") {
            if (scenario == Scenario::ConnectionLimit && sessions == 1) {
                retry = true;
                reply = "530 Sorry, the maximum number of allowed clients (1) are already connected.";
            } else {
                reply = "230 Logged in";
            }
        } else if (verb == "TYPE") {
            reply = "200 Binary mode";
        } else if (verb == "FEAT") {
            reply = "211 No extensions";
        } else if (verb == "MKD") {
            directories.append(path);
            if (path == Subdir && scenario == Scenario::FolderInterrupted && sessions == 1) {
                retry = true;
                reply = "450 Directory temporarily unavailable";
            } else {
                reply = "550 Directory exists";
            }
        } else if (verb == "CWD") {
            reply = "250 Directory changed";
        } else if (verb == "SIZE") {
            ++sizes;
            reply = files.contains(path) ? "213 " + QByteArray::number(files.value(path).size())
                                         : "550 File not found";
        } else if (verb == "REST") {
            offset = path.toLongLong();
            restarts.append(path);
            reply = "350 Restart accepted";
        } else if (verb == "EPSV") {
            if (!passive.listen(QHostAddress::LocalHost))
                return false;
            reply = "229 Entering Extended Passive Mode (|||" +
                    QByteArray::number(passive.serverPort()) + "|)";
        } else if (verb == "STOR") {
            stores.append(path);
            if (!passive.hasPendingConnections() && !passive.waitForNewConnection(3000))
                return false;
            std::unique_ptr<QTcpSocket> data(passive.nextPendingConnection());
            passive.close();
            if (path == Target && scenario == Scenario::PermissionDenied) {
                reply = "550 Permission denied";
            } else if (scenario == Scenario::Restart && offset > 0) {
                reply = "501 REST not compatible with server configuration";
            } else if (path == Target && files.contains(Hidden)) {
                reply = "550 " + path + ": Temporary hidden file " + Hidden + " already exists";
            } else {
                if (!send(control, "150 Opening data connection"))
                    return false;
                const bool interrupted = path == Target && sessions == 1 &&
                    (scenario == Scenario::Interrupted || scenario == Scenario::Disconnected);
                if (interrupted && scenario == Scenario::Disconnected) {
                    while (data->bytesAvailable() < 3) {
                        if (!data->waitForReadyRead(3000))
                            return false;
                    }
                    files.insert(path, data->read(3));
                    retry = true;
                    return true;
                }
                QByteArray content = files.value(path).left(offset);
                while (data->state() != QAbstractSocket::UnconnectedState) {
                    if (data->bytesAvailable() == 0)
                        data->waitForReadyRead(3000);
                    content += data->readAll();
                    if (data->error() == QAbstractSocket::SocketTimeoutError)
                        return false;
                }
                if (interrupted) {
                    files.insert(path, content.left(3));
                    retry = true;
                } else {
                    files.insert(path, content);
                }
                offset = 0;
                reply = interrupted ? "426 Data connection closed" : "226 Transfer complete";
            }
        } else if (verb == "RNFR") {
            renameFrom = path;
            reply = files.contains(path) ? "350 Ready to rename" : "550 File not found";
        } else if (verb == "RNTO") {
            if (scenario == Scenario::RenameDenied || !files.contains(renameFrom)) {
                reply = "550 Rename denied";
            } else {
                files.insert(path, files.take(renameFrom));
                renamed = path;
                reply = "250 Renamed";
            }
        } else if (verb == "DELE") {
            deletes.append(path);
            reply = files.remove(path) ? "250 Deleted" : "550 File not found";
        } else if (verb == "QUIT") {
            return true;
        }
        if (!send(control, reply))
            return false;
    }
}

TransferStatus upload(quint16 port, const QString &directory, Scenario scenario)
{
    ConnectionProfile profile;
    profile.protocol = Protocol::Ftp;
    profile.host = QStringLiteral("127.0.0.1");
    profile.port = port;
    TransferJob job;
    job.id = 1;
    job.upload = true;
    job.directory = true;
    job.overwrite = scenario == Scenario::Overwrite;
    job.localPath = directory;
    job.remotePath = QStringLiteral("/csgo/addons/metamod");
    TransferStatus status;
    status.id = job.id;
    auto queue = std::make_shared<TransferQueue>();
    queue->addJob(job, status);
    TransferWorker worker(queue, profile);
    bool authFailed = false;
    QObject::connect(&worker, &TransferWorker::connectionFailed, &worker,
                     [&authFailed](const QString &, bool auth) { authFailed |= auth; },
                     Qt::DirectConnection);
    worker.start();
    const QDeadlineTimer deadline(10000);
    bool active = true;
    while (active && !deadline.hasExpired()) {
        const auto updates = queue->snapshot(0, active);
        if (!updates.isEmpty())
            status = updates.first();
        if (active)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    worker.requestStop();
    queue->wakeAll();
    worker.wait();
    if (authFailed)
        status.state = TransferState::Failed;
    return status;
}

void test(Scenario scenario, const QString &directory)
{
    QTcpServer listener;
    if (!listener.listen(QHostAddress::LocalHost)) {
        check(false, "can listen on localhost");
        return;
    }
    auto result = std::async(std::launch::async, upload, listener.serverPort(), directory, scenario);
    Server server{};
    server.scenario = scenario;
    const bool staged = scenario == Scenario::Conflict || scenario == Scenario::Restart ||
                        scenario == Scenario::RenameDenied;
    if (staged)
        server.files.insert(Hidden, "another transfer");
    if (scenario == Scenario::Restart)
        server.files.insert(Target, Partial);
    if (scenario == Scenario::Overwrite)
        server.files.insert(Target, QByteArray(Payload.size(), 'x'));
    const bool interrupted = scenario == Scenario::Interrupted || scenario == Scenario::Disconnected;
    const bool nested = scenario == Scenario::FolderInterrupted;
    const int sessions = interrupted || nested || scenario == Scenario::ConnectionLimit ? 2 : 1;
    for (int index = 0; index < sessions; ++index) {
        if (!listener.waitForNewConnection(3000)) {
            listener.close();
            check(false, "FTP client connects");
            return;
        }
        std::unique_ptr<QTcpSocket> control(listener.nextPendingConnection());
        check(server.serve(*control), "FTP session completes");
        control->abort();
    }
    const TransferStatus status = result.get();

    const bool failed = scenario == Scenario::RenameDenied ||
                        scenario == Scenario::PermissionDenied;
    if (status.state != (failed ? TransferState::Failed : TransferState::Completed))
        std::fprintf(stderr, "FTP upload status: %s\n", qPrintable(status.detail));
    check(status.state == (failed ? TransferState::Failed : TransferState::Completed),
          "folder reports expected result");
    check(status.filesDone == (failed ? 1 : nested ? 3 : 2), "folder reports completed files");
    check(server.files.value(Next) == Payload, "next file still uploads");
    check(server.files.value(Target) == (failed ? QByteArray() : Payload),
          "destination contains the expected bytes");
    if (!failed)
        check(status.done == (nested ? 3 : 2) * Payload.size(), "folder progress counts each byte once");

    if (staged) {
        const QByteArray temporary = server.stores.value(server.stores.size() - 2);
        check(temporary.startsWith("/csgo/addons/metamod/nexa-") &&
                  temporary.endsWith(".nexapart"),
              "retry uses a staging file in the destination directory");
        check(!server.files.contains(temporary), "staging file is removed");
        check(server.files.value(Hidden) == "another transfer", "server hidden file is preserved");
        if (scenario == Scenario::RenameDenied) {
            check(status.detail.contains(QStringLiteral("Rename denied")), "rename error is reported");
            check(server.deletes == QList<QByteArray>{temporary}, "only staging file is deleted");
        } else {
            check(server.renamed == Target && server.deletes.isEmpty(),
                  "staging file is renamed to destination");
        }
    } else if (interrupted) {
        check(server.stores.count(Target) == 2 && server.stores.count(Next) == 1,
              "interrupted upload is retried without duplicating other files");
        check(server.restarts == QList<QByteArray>{"3"}, "retry restarts from saved bytes");
    } else if (nested) {
        check(server.files.value(Nested) == Payload, "remaining directory is uploaded after reconnect");
        check(server.stores.count(Target) == 1 && server.stores.count(Next) == 1,
              "directory retry does not enqueue the parent files again");
        check(server.directories.count(Subdir) == 2, "temporary directory failure is retried");
    } else {
        check(server.stores == QList<QByteArray>{Target, Next} && server.deletes.isEmpty(),
              "ordinary writes and generic permission errors use the original path");
    }
    if (scenario == Scenario::Restart)
        check(server.restarts == QList<QByteArray>{"3", "0"}, "restart marker is reset");
    if (scenario == Scenario::Overwrite)
        check(server.sizes == 0, "explicit overwrite skips the SIZE round trips");
    if (scenario == Scenario::Direct)
        check(server.sizes == 2, "resume policy checks the size of both destination files");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QTemporaryDir directory(QDir::current().filePath(QStringLiteral("ftp-upload-XXXXXX")));
    if (!directory.isValid()) {
        std::fprintf(stderr, "Cannot create test directory: %s\n", qPrintable(directory.errorString()));
        return 1;
    }
    for (const QString &name : {QStringLiteral("utils.vdf"), QStringLiteral("z.cfg")}) {
        QFile file(directory.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(Payload) != Payload.size()) {
            std::fprintf(stderr, "Cannot write test file: %s\n", qPrintable(file.errorString()));
            return 1;
        }
    }
    for (const Scenario scenario : {Scenario::Direct, Scenario::Conflict, Scenario::Restart,
                                    Scenario::RenameDenied, Scenario::PermissionDenied,
                                    Scenario::Overwrite, Scenario::Interrupted,
                                    Scenario::Disconnected, Scenario::ConnectionLimit})
        test(scenario, directory.path());
    if (!QDir(directory.path()).mkdir(QStringLiteral("sub")))
        return 1;
    QFile nested(directory.filePath(QStringLiteral("sub/nested.cfg")));
    if (!nested.open(QIODevice::WriteOnly) || nested.write(Payload) != Payload.size())
        return 1;
    nested.close();
    test(Scenario::FolderInterrupted, directory.path());
    return failures == 0 ? 0 : 1;
}
