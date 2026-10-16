#include "demo.h"

#include "connection.h"
#include "editors.h"
#include "glass.h"
#include "hoststore.h"
#include "mainwindow.h"
#include "session.h"
#include "sessionview.h"
#include "settings.h"
#include "transferdock.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QThread>
#include <QTimer>

#include <functional>
#include <memory>

namespace {

struct Item {
    QString name;
    qint64 size;
    int minutesAgo;
};

QString dayStamp(int daysAgo)
{
    return QDate::currentDate().addDays(-daysAgo).toString(QStringLiteral("yyyy-MM-dd"));
}

RemoteEntries entries(const QString &folder, const QVector<Item> &items)
{
    RemoteEntries result;
    const QDateTime now = QDateTime::currentDateTime();
    for (const Item &item : items) {
        RemoteEntry entry;
        entry.name = item.name;
        entry.path = RemoteConnection::join(folder, item.name);
        entry.directory = item.size < 0;
        entry.size = entry.directory ? 0 : item.size;
        const int jitter = item.minutesAgo < 60 ? 0 : static_cast<int>(qHash(item.name, 0) % 53);
        entry.modified = now.addSecs(-60LL * (item.minutesAgo + jitter));
        entry.permissions = entry.directory ? 0755 : 0644;
        result.append(entry);
    }
    RemoteConnection::sortEntries(result);
    return result;
}

constexpr qint64 KB = 1024;
constexpr qint64 MB = 1024 * KB;
constexpr qint64 GB = 1024 * MB;
constexpr int Hour = 60;
constexpr int Day = 24 * Hour;

QString homeOf(const QString &host)
{
    if (host.startsWith(QLatin1String("web-01")))
        return QStringLiteral("/var/www/shop");
    if (host.startsWith(QLatin1String("cs2")))
        return QStringLiteral("/home/steam/cs2");
    if (host.startsWith(QLatin1String("files")))
        return QStringLiteral("/backups");
    return QStringLiteral("/volume1");
}

int latencyOf(const QString &host)
{
    if (host.startsWith(QLatin1String("web-01")))
        return 24;
    if (host.startsWith(QLatin1String("cs2")))
        return 38;
    if (host.startsWith(QLatin1String("files")))
        return 61;
    return 3;
}

RemoteEntries listing(const QString &host, const QString &path)
{
    const QString p = RemoteConnection::normalized(path);
    if (p == QLatin1String("/var/www/shop"))
        return entries(p, {{QStringLiteral(".github"), -1, 2 * Day},
                           {QStringLiteral("app"), -1, 35},
                           {QStringLiteral("bootstrap"), -1, 3 * Day},
                           {QStringLiteral("config"), -1, 2 * Hour},
                           {QStringLiteral("database"), -1, 5 * Day},
                           {QStringLiteral("public"), -1, 35},
                           {QStringLiteral("resources"), -1, Day + 3 * Hour},
                           {QStringLiteral("routes"), -1, 2 * Day},
                           {QStringLiteral("storage"), -1, 12},
                           {QStringLiteral("vendor"), -1, 6 * Day},
                           {QStringLiteral("artisan"), 1741, 30 * Day},
                           {QStringLiteral("composer.json"), 2431, 2 * Day},
                           {QStringLiteral("composer.lock"), 318 * KB, 2 * Day},
                           {QStringLiteral("deploy.sh"), 3172, 4},
                           {QStringLiteral("package.json"), 1893, Day + 2 * Hour},
                           {QStringLiteral("phpunit.xml"), 1204, 12 * Day},
                           {QStringLiteral("README.md"), 6912, 9 * Day},
                           {QStringLiteral("vite.config.js"), 1088, Day + 2 * Hour}});
    if (p == QLatin1String("/var/www/shop/public"))
        return entries(p, {{QStringLiteral("build"), -1, 35},
                           {QStringLiteral("images"), -1, 3 * Day},
                           {QStringLiteral("favicon.ico"), 15 * KB, 60 * Day},
                           {QStringLiteral("index.php"), 1710, 30 * Day},
                           {QStringLiteral("robots.txt"), 68, 60 * Day}});
    if (p == QLatin1String("/var/www"))
        return entries(p, {{QStringLiteral("shop"), -1, 4},
                           {QStringLiteral("staging"), -1, 3 * Day},
                           {QStringLiteral("html"), -1, 90 * Day}});
    if (p == QLatin1String("/home/steam/cs2"))
        return entries(p, {{QStringLiteral("addons"), -1, 3 * Hour},
                           {QStringLiteral("cfg"), -1, 50},
                           {QStringLiteral("logs"), -1, 2},
                           {QStringLiteral("maps"), -1, 4 * Day},
                           {QStringLiteral("steamapps"), -1, 6 * Day},
                           {QStringLiteral("gamemodes_server.txt"), 4 * KB, 50},
                           {QStringLiteral("server.cfg"), 2 * KB + 310, 50},
                           {QStringLiteral("start.sh"), 612, 9 * Day},
                           {QStringLiteral("update.sh"), 488, 9 * Day}});
    if (p == QLatin1String("/backups"))
        return entries(p, {{QStringLiteral("daily"), -1, 3 * Hour},
                           {QStringLiteral("weekly"), -1, 2 * Day},
                           {QStringLiteral("monthly"), -1, 15 * Day},
                           {QStringLiteral("db-%1.sql.gz").arg(dayStamp(0)), 1434 * MB, 3 * Hour},
                           {QStringLiteral("db-%1.sql.gz").arg(dayStamp(1)), 1429 * MB, Day + 3 * Hour},
                           {QStringLiteral("media-%1.tar.zst").arg(dayStamp(2)), 18 * GB + 410 * MB,
                            2 * Day},
                           {QStringLiteral("checksums.sha256"), 642, 3 * Hour}});
    if (p == QLatin1String("/volume1"))
        return entries(p, {{QStringLiteral("Backups"), -1, 2 * Day},
                           {QStringLiteral("Documents"), -1, 6 * Hour},
                           {QStringLiteral("Media"), -1, Day},
                           {QStringLiteral("Photos"), -1, 3 * Day}});
    Q_UNUSED(host);
    return entries(p, {{QStringLiteral("assets"), -1, 3 * Day},
                       {QStringLiteral("config.json"), 860, 5 * Hour},
                       {QStringLiteral("index.js"), 12 * KB + 300, Day},
                       {QStringLiteral("notes.md"), 2400, 4 * Day},
                       {QStringLiteral("styles.css"), 38 * KB, Day}});
}

class DemoConnection final : public RemoteConnection {
public:
    OpenResult open(const ConnectionProfile &profile, bool) override
    {
        m_profile = profile;
        m_error.clear();
        m_broken = false;
        m_trustedFingerprint = QStringLiteral("SHA256:demo");
        m_serverInfo = QStringLiteral("SSH-2.0-OpenSSH_9.6");
        QThread::msleep(150);
        m_open = true;
        return OpenResult::Ok;
    }
    OpenResult continueAfterTrust(bool, bool) override { return OpenResult::Ok; }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    bool ping() override
    {
        QThread::msleep(static_cast<unsigned long>(latencyOf(m_profile.host)));
        return m_open;
    }
    QString homePath() override { return homeOf(m_profile.host); }
    bool list(const QString &path, RemoteEntries &result) override
    {
        result = listing(m_profile.host, path);
        return true;
    }
    void prefetch(const QStringList &directories,
                  const std::function<void(const QString &, const RemoteEntries &)> &onDirectory)
        override
    {
        for (const QString &directory : directories)
            onDirectory(directory, listing(m_profile.host, directory));
    }
    bool makeDirectory(const QString &) override { return true; }
    bool createFile(const QString &) override { return true; }
    bool rename(const QString &, const QString &) override { return true; }
    bool remove(const RemoteEntries &, const std::function<void(qint64)> &) override
    {
        return true;
    }
    bool expand(const TransferJob &job, TransferHost &host) override
    {
        host.expansionFailed(job.id, QStringLiteral("demo"));
        return true;
    }
    bool runTransfers(TransferHost &) override { return true; }

private:
    bool m_open = false;
};

int demoToast = 0;

QHash<QString, QString> &demoHosts()
{
    static QHash<QString, QString> ids;
    return ids;
}

}

class DemoTour {
public:
    static void run(MainWindow &window, const QString &directory);

private:
    static void openSessions(MainWindow &window);
    static void showTransfers(MainWindow &window);
    static void closeSheets(MainWindow &window);
};

void DemoTour::openSessions(MainWindow &window)
{
    for (const QString &name : {QStringLiteral("cs2"), QStringLiteral("web"),
                                QStringLiteral("storage")})
        window.openHost(demoHosts().value(name), true);
    for (const MainWindow::Tab &t : std::as_const(window.m_tabs)) {
        t.view->showLocalPlaceholder(QStringLiteral("D:/Projects/website"), {});
    }
    for (const MainWindow::Tab &t : std::as_const(window.m_tabs)) {
        if (t.session->hostId() == demoHosts().value(QStringLiteral("web")))
            window.activate(t.session->id());
    }
}

void DemoTour::showTransfers(MainWindow &window)
{
    quint64 web = 0;
    quint64 cs2 = 0;
    quint64 storage = 0;
    for (const MainWindow::Tab &t : std::as_const(window.m_tabs)) {
        if (t.session->hostId() == demoHosts().value(QStringLiteral("web")))
            web = t.session->id();
        else if (t.session->hostId() == demoHosts().value(QStringLiteral("cs2")))
            cs2 = t.session->id();
        else
            storage = t.session->id();
    }
    const auto add = [&window](quint64 session, const QString &sessionName, quint64 id,
                               const QString &name, bool upload, bool directory,
                               TransferState state, qint64 done, qint64 total, qint64 speed,
                               int filesDone, int filesTotal, const QString &detail = {}) {
        TransferStatus status;
        status.id = id;
        status.sessionId = session;
        status.sessionName = sessionName;
        status.name = name;
        status.localPath = (upload ? QStringLiteral("D:/Projects/website/")
                                   : QStringLiteral("D:/Downloads/")) +
                           name;
        status.remotePath = (sessionName == QLatin1String("Storage")
                                 ? QStringLiteral("/backups/")
                                 : sessionName == QLatin1String("CS2 Server")
                                 ? QStringLiteral("/home/steam/cs2/")
                                 : QStringLiteral("/var/www/shop/")) +
                            name;
        status.upload = upload;
        status.directory = directory;
        status.state = state;
        status.done = done;
        status.total = total;
        status.bytesPerSecond = speed;
        status.filesDone = filesDone;
        status.filesTotal = filesTotal;
        status.detail = detail;
        window.m_dock->model()->upsert(status);
    };
    add(web, QStringLiteral("web-prod"), 901, QStringLiteral("release-1.4.2.zip"), true, false,
        TransferState::Completed, 84 * MB, 84 * MB, 0, 1, 1);
    add(cs2, QStringLiteral("CS2 Server"), 902, QStringLiteral("server.cfg"), true, false,
        TransferState::Completed, 2 * KB + 310, 2 * KB + 310, 0, 1, 1);
    add(web, QStringLiteral("web-prod"), 903, QStringLiteral("storage/logs"), false, true,
        TransferState::Completed, 4900 * KB, 4900 * KB, 0, 12, 12);
    add(cs2, QStringLiteral("CS2 Server"), 904, QStringLiteral("maps"), true, true,
        TransferState::Queued, 0, 2150 * MB, 0, 0, 37);
    add(storage, QStringLiteral("Storage"), 905,
        QStringLiteral("media-%1.tar.zst").arg(dayStamp(2)), false, false,
        TransferState::Running, 6340 * MB, 18842 * MB, 92 * MB, 0, 1);
    add(web, QStringLiteral("web-prod"), 906, QStringLiteral("public/build"), true, true,
        TransferState::Running, 412 * MB, 1270 * MB, 38 * MB, 1186, 3240);
    window.m_dock->setVisible(true);
    window.m_dock->setExpanded(true);
    window.updateDock();
}

void DemoTour::closeSheets(MainWindow &window)
{
    for (Sheet *sheet : window.findChildren<Sheet *>())
        sheet->close();
}

void DemoTour::run(MainWindow &window, const QString &directory)
{
    QDir().mkpath(directory);
    window.resize(1440, 900);

    struct Step {
        int delay;
        std::function<void()> action;
    };
    const auto shot = [&window, directory](const QString &name) {
        return [&window, directory, name] {
            window.grab().save(QDir(directory).filePath(name));
        };
    };
    auto steps = std::make_shared<QVector<Step>>(QVector<Step>{
        {1200, shot(QStringLiteral("nexa-home.png"))},
        {100, [&window] { openSessions(window); }},
        {2200,
         [&window] {
             demoToast = window.m_toasts->notify(QObject::tr("Сохранено на сервере · мгновенно"),
                                     ToastHost::Kind::Success,
                                     QStringLiteral("/var/www/shop/deploy.sh"));
         }},
        {600, shot(QStringLiteral("nexa-session.png"))},
        {100,
         [&window] {
             window.m_toasts->dismiss(demoToast);
             showTransfers(window);
         }},
        {1200, shot(QStringLiteral("nexa-transfers.png"))},
        {100,
         [&window] {
             window.m_dock->setExpanded(false);
             window.showPalette();
         }},
        {900, shot(QStringLiteral("nexa-palette.png"))},
        {100,
         [&window] {
             closeSheets(window);
             (new SettingsSheet(&window))->open();
         }},
        {900, shot(QStringLiteral("nexa-settings.png"))},
        {100,
         [&window] {
             closeSheets(window);
             window.addHost();
         }},
        {900, shot(QStringLiteral("nexa-new-server.png"))},
        {100, [] { QCoreApplication::exit(0); }},
    });
    auto next = std::make_shared<std::function<void(int)>>();
    *next = [steps, next](int index) {
        if (index >= steps->size())
            return;
        const Step step = steps->at(index);
        QTimer::singleShot(step.delay, qApp, [steps, next, index, step] {
            step.action();
            (*next)(index + 1);
        });
    };
    (*next)(0);
}

namespace Demo {

void install()
{
    Editors::useFixedList(
        {{Editors::Kind::Notepad, Editors::nameOf(Editors::Kind::Notepad),
          QStringLiteral(R"(C:\Windows\System32\notepad.exe)"), true},
         {Editors::Kind::VSCode, Editors::nameOf(Editors::Kind::VSCode),
          QStringLiteral(R"(C:\Program Files\Microsoft VS Code\Code.exe)"), true},
         {Editors::Kind::NotepadPlusPlus, Editors::nameOf(Editors::Kind::NotepadPlusPlus),
          QStringLiteral(R"(C:\Program Files\Notepad++\notepad++.exe)"), true},
         {Editors::Kind::Custom, Editors::nameOf(Editors::Kind::Custom), QString(), false}});
    RemoteConnection::setFactory(
        [](Protocol) { return std::make_unique<DemoConnection>(); });

    const QDateTime now = QDateTime::currentDateTime();
    const auto add = [&now](const QString &key, const QString &name, const QString &user,
                            const QString &host, Protocol protocol, bool tls, int color,
                            qint64 minutesAgo) {
        SavedHost saved;
        saved.name = name;
        saved.profile.host = host;
        saved.profile.username = user;
        saved.profile.protocol = protocol;
        saved.profile.useTls = tls;
        saved.profile.port = protocol == Protocol::Ftp ? 21 : 22;
        saved.color = color;
        if (minutesAgo >= 0)
            saved.lastUsed = now.addSecs(-60 * minutesAgo);
        saved.lastPath = homeOf(host);
        demoHosts().insert(key, HostStore::instance().save(saved));
    };
    add(QStringLiteral("cs2"), QStringLiteral("CS2 Server"), QStringLiteral("steam"),
        QStringLiteral("cs2.example.net"), Protocol::Sftp, false, 0, 42);
    add(QStringLiteral("web"), QStringLiteral("web-prod"), QStringLiteral("deploy"),
        QStringLiteral("web-01.example.com"), Protocol::Sftp, false, 1, 17 * Hour);
    add(QStringLiteral("storage"), QStringLiteral("Storage"), QStringLiteral("backup"),
        QStringLiteral("files.example.org"), Protocol::Ftp, true, 2, 5 * Day + 3 * Hour);
    add(QStringLiteral("nas"), QStringLiteral("Home NAS"), QStringLiteral("admin"),
        QStringLiteral("10.0.0.15"), Protocol::Sftp, false, 3, -1);
}

void tour(MainWindow &window, const QString &directory)
{
    DemoTour::run(window, directory);
}

}
