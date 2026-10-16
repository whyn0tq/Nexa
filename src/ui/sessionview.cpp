#include "sessionview.h"

#include "controls.h"
#include "filelist.h"
#include "filepane.h"
#include "glass.h"
#include "hoststore.h"
#include "listingcache.h"
#include "session.h"
#include "sheets.h"
#include "theme.h"
#include "transferengine.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>

#include <atomic>

quint64 nextTransferId()
{
    static std::atomic<quint64> counter{1};
    return counter.fetch_add(1);
}

namespace {
RemoteEntries listLocal(const QString &path)
{
    RemoteEntries entries;
    if (path.isEmpty()) {
        for (const QFileInfo &drive : QDir::drives()) {
            RemoteEntry entry;
            entry.path = drive.absoluteFilePath();
            entry.name = QDir::toNativeSeparators(entry.path);
            entry.directory = true;
            entries.append(entry);
        }
        return entries;
    }
    const QFileInfoList infos =
        QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                                     QDir::System,
                                 QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        RemoteEntry entry;
        entry.name = info.fileName();
        entry.path = info.absoluteFilePath();
        entry.directory = info.isDir();
        entry.symlink = info.isSymLink();
        entry.size = info.isDir() ? 0 : info.size();
        entry.modified = info.lastModified();
        entries.append(entry);
    }
    return entries;
}

QString remoteJoin(const QString &directory, const QString &name)
{
    return directory.endsWith(QLatin1Char('/')) ? directory + name
                                                : directory + QLatin1Char('/') + name;
}

QString remoteParent(const QString &path)
{
    if (path == QStringLiteral("/"))
        return path;
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}

QString shortPath(const QString &path, int limit = 42)
{
    if (path.size() <= limit)
        return path;
    return QStringLiteral("…") + path.right(limit - 1);
}
}

DropOverlay::DropOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(80);
    connect(&m_hideTimer, &QTimer::timeout, this, &QWidget::hide);
}

void DropOverlay::showTarget(const QRect &area, Icon icon, const QString &text)
{
    m_hideTimer.stop();
    if (area == m_area && text == m_text && isVisible())
        return;
    m_area = area;
    m_icon = icon;
    m_text = text;
    setGeometry(parentWidget()->rect());
    show();
    raise();
    update();
}

void DropOverlay::hideSoon()
{
    m_hideTimer.start();
}

void DropOverlay::hideNow()
{
    m_hideTimer.stop();
    hide();
}

void DropOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(m_area).adjusted(2, 2, -2, -2);
    QPainterPath outside;
    outside.addRect(rect());
    QPainterPath inside;
    inside.addRoundedRect(area, 16, 16);
    painter.fillPath(outside.subtracted(inside), QColor(4, 4, 6, 120));
    painter.fillPath(inside, QColor(255, 66, 86, 22));
    QPen dash(Theme::Accent, 1.6, Qt::DashLine, Qt::RoundCap);
    dash.setDashPattern({5, 4});
    painter.setPen(dash);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(area, 16, 16);

    painter.setFont(Theme::font(14, QFont::DemiBold));
    const qreal textWidth = painter.fontMetrics().horizontalAdvance(m_text);
    const qreal width = std::min<qreal>(textWidth + 64, area.width() - 32);
    const QRectF pill(area.center().x() - width / 2, area.bottom() - 70, width, 46);
    Glass::paintPanel(painter, pill, 14, Glass::Tone::Sheet);
    const QRectF badge(pill.left() + 8, pill.top() + 8, 30, 30);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::Accent);
    painter.drawRoundedRect(badge, 9, 9);
    Icons::paint(painter, m_icon, badge.adjusted(7, 7, -7, -7), Qt::white, 2);
    painter.setPen(Theme::Text);
    painter.drawText(QRectF(badge.right() + 10, pill.top(), pill.right() - badge.right() - 22,
                            pill.height()),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     painter.fontMetrics().elidedText(
                         m_text, Qt::ElideMiddle,
                         static_cast<int>(pill.right() - badge.right() - 22)));
}

SessionView::SessionView(Session *session, ToastHost *toasts, QWidget *parent)
    : QWidget(parent), m_session(session), m_toasts(toasts)
{
    setAcceptDrops(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(buildHeader());

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(10);
    m_local = new FilePane(false);
    m_local->setCaption(tr("Этот компьютер"), Icon::Computer);
    m_remote = new FilePane(true);
    m_remote->setCaption(tr("Сервер"), Icon::Server);
    m_remote->model()->setSessionId(session->id());
    m_splitter->addWidget(m_local);
    m_splitter->addWidget(m_remote);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    layout->addWidget(m_splitter, 1);

    m_overlay = new DropOverlay(this);
    for (FilePane *pane : {m_local, m_remote}) {
        pane->view()->viewport()->installEventFilter(this);
        for (QWidget *child : pane->findChildren<QWidget *>()) {
            if (qobject_cast<QLineEdit *>(child))
                child->setAcceptDrops(false);
        }
    }

    m_watcher = new QFileSystemWatcher(this);
    m_editWatcher = new QFileSystemWatcher(this);
    m_editSettle.setSingleShot(true);
    m_editSettle.setInterval(700);
    connect(&m_editSettle, &QTimer::timeout, this, &SessionView::uploadEdits);
    connect(m_editWatcher, &QFileSystemWatcher::fileChanged, this,
            &SessionView::localFileChanged);
    m_localRefresh.setSingleShot(true);
    m_localRefresh.setInterval(350);
    connect(&m_localRefresh, &QTimer::timeout, this, &SessionView::refreshLocal);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_localRefresh.start(); });
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && isVisible())
                    m_localRefresh.start();
            });

    connect(m_local, &FilePane::navigateRequested, this,
            [this](const QString &path) { navigateLocal(path); });
    connect(m_local, &FilePane::refreshRequested, this, &SessionView::refreshLocal);
    connect(m_local, &FilePane::entryOpened, this, &SessionView::openLocalEntry);
    connect(m_local, &FilePane::contextMenuRequested, this, &SessionView::localMenu);
    connect(m_remote, &FilePane::navigateRequested, this,
            [this](const QString &path) { navigateRemote(path); });
    connect(m_remote, &FilePane::refreshRequested, this, &SessionView::refreshRemote);
    connect(m_remote, &FilePane::entryOpened, this, &SessionView::openRemoteEntry);
    connect(m_remote, &FilePane::contextMenuRequested, this, &SessionView::remoteMenu);
    connect(m_remote, &FilePane::messageActionTriggered, this, [this] {
        if (m_session->online())
            refreshRemote();
        else
            m_session->retry();
    });

    connect(session, &Session::stateChanged, this, &SessionView::onState);
    connect(session, &Session::ready, this, &SessionView::onReady);
    connect(session, &Session::listed, this,
            [this](const QString &path, const RemoteEntries &entries, quint64 generation) {
                rememberListing(path, entries);
                if (path != m_remotePath)
                    return;
                if (generation != m_listGeneration &&
                    !(generation == 0 && (m_remoteLoading || m_listGeneration == 0)))
                    return;
                m_remoteLoading = false;
                m_remote->setEntries(entries);
                m_remote->setInteractive(true);
            });
    connect(session, &Session::listFailed, this,
            [this](const QString &path, const QString &message, quint64 generation) {
                if (generation != m_listGeneration)
                    return;
                m_cache.remove(path);
                m_cacheStamp.remove(path);
                m_cacheOrder.removeAll(path);
                m_remote->showMessage(Icon::Alert, tr("Папка недоступна"), message,
                                      tr("Повторить"));
            });
    connect(session, &Session::operationDone, this,
            [this](const QString &message, const QString &refresh) {
                if (!message.isEmpty())
                    m_toasts->notify(message, ToastHost::Kind::Success);
                if (refresh == m_remotePath)
                    refreshRemote();
            });
    connect(session, &Session::operationFailed, this, [this](const QString &message) {
        m_toasts->notify(message, ToastHost::Kind::Error);
    });
    connect(session->transfers(), &TransferEngine::jobFinished, this,
            &SessionView::editJobFinished);
    connect(session, &Session::fileFetched, this, &SessionView::quickFetched);
    connect(session, &Session::fileStored, this, &SessionView::quickStored);
    m_selectionPrefetch.setSingleShot(true);
    m_selectionPrefetch.setInterval(120);
    connect(&m_selectionPrefetch, &QTimer::timeout, this, &SessionView::prefetchSelected);
    connect(m_remote->view()->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { m_selectionPrefetch.start(); });
    connect(session->transfers(), &TransferEngine::jobFinished, this,
            [this](const TransferStatus &status) {
                if (status.upload) {
                    if (remoteParent(status.remotePath) == m_remotePath ||
                        status.remotePath == m_remotePath)
                        refreshRemote();
                } else if (QFileInfo(status.localPath).absolutePath() ==
                           QFileInfo(m_localPath + QStringLiteral("/x")).absolutePath()) {
                    refreshLocal();
                }
            });

    QString localStart = QDir::homePath();
    navigateLocal(localStart, false);

    m_cacheClock.start();
    m_cacheSave.setSingleShot(true);
    m_cacheSave.setInterval(2000);
    connect(&m_cacheSave, &QTimer::timeout, this, &SessionView::saveListings);
    QString start = session->profile().initialPath;
    if (!session->hostId().isEmpty()) {
        const QString last = HostStore::instance().host(session->hostId()).lastPath;
        if (!last.isEmpty())
            start = last;
        ListingCache::Snapshot snapshot = ListingCache::load(session->hostId());
        m_cache = std::move(snapshot.listings);
        m_cacheOrder = std::move(snapshot.order);
    }
    if (!start.isEmpty())
        start = QDir::cleanPath(QDir::fromNativeSeparators(start));
    session->setStartPath(start);
    const auto preview = start.isEmpty() ? m_cache.cend() : m_cache.constFind(start);
    if (preview != m_cache.cend()) {
        m_remotePath = start;
        m_remote->setPath(start, false);
        m_remote->setEntries(preview.value());
    } else {
        m_remote->setPath(start.isEmpty() ? QStringLiteral("/") : start, false);
    }
    onState();
    QTimer::singleShot(0, m_remote->view(), [this] { m_remote->view()->setFocus(); });
}

QWidget *SessionView::buildHeader()
{
    m_header = new GlassPanel(Glass::Tone::Panel, Theme::RadiusLarge);
    m_header->setFixedHeight(56);
    auto *layout = new QHBoxLayout(m_header);
    layout->setContentsMargins(16, 0, 10, 0);
    layout->setSpacing(10);
    m_dot = new StatusDot;
    layout->addWidget(m_dot);
    m_title = makeLabel(QString(), 15, Theme::Text, QFont::DemiBold);
    layout->addWidget(m_title);
    m_endpoint = makeLabel(QString(), 13, Theme::TextMuted);
    layout->addWidget(m_endpoint);
    layout->addSpacing(6);
    m_status = makeLabel(QString(), 12.5, Theme::TextSecondary);
    layout->addWidget(m_status, 1);

    m_reconnect = makeButton(tr("Переподключить"), QStringLiteral("primary"), Icon::Retry, true);
    m_reconnect->setStyleSheet(QStringLiteral("min-height:32px;border-radius:9px;"));
    m_reconnect->hide();
    connect(m_reconnect, &QPushButton::clicked, this, [this] { m_session->retry(); });
    layout->addWidget(m_reconnect);

    const auto tool = [this, layout](Icon icon, const QString &tip, auto slot) {
        auto *button = new IconButton(icon, tip);
        button->setFixedSize(36, 36);
        connect(button, &IconButton::clicked, this, slot);
        layout->addWidget(button);
        return button;
    };
    tool(Icon::Upload, tr("Загрузить файлы на сервер"), [this] { uploadDialog(false); });
    tool(Icon::FolderPlus, tr("Новая папка на сервере"), [this] { createRemoteFolder(); });
    tool(Icon::Download, tr("Скачать выбранное"), [this] { downloadSelected(); });
    tool(Icon::Trash, tr("Удалить выбранное (Del)"), [this] { deleteRemote(); });
    return m_header;
}

void SessionView::updateHeader()
{
    const ConnectionProfile profile = m_session->profile();
    m_title->setText(m_session->title());
    QString protocol = profile.protocol == Protocol::Sftp ? QStringLiteral("SFTP")
                       : profile.useTls                   ? QStringLiteral("FTPS")
                                                          : QStringLiteral("FTP");
    m_endpoint->setText(QStringLiteral("%1 · %2").arg(profile.endpoint(), protocol));
}

void SessionView::onState()
{
    updateHeader();
    using State = Session::State;
    State state = m_session->state();
    if (state == State::Idle && m_session->stateMessage().isEmpty())
        state = State::Connecting;
    QString status;
    switch (state) {
    case State::Online: {
        m_dot->setColor(Theme::Success);
        status = m_session->latency() > 0
                     ? tr("Подключено · ping %1 мс").arg(m_session->latency())
                 : m_session->latency() == 0 ? tr("Подключено · ping <1 мс")
                     : tr("Подключено");
        break;
    }
    case State::Connecting:
    case State::AwaitingTrust:
        m_dot->setColor(Theme::Warning, true);
        status = m_session->stateMessage().isEmpty() ? tr("Подключение…")
                                                     : m_session->stateMessage();
        if (m_remote->model()->rowCount() == 0)
            m_remote->showMessage(Icon::Plug, status, tr("Устанавливаю защищённое соединение."));
        else
            m_remote->showList();
        break;
    case State::Reconnecting:
        m_dot->setColor(Theme::Warning, true);
        status = m_session->stateMessage().isEmpty() ? tr("Переподключение…")
                                                     : m_session->stateMessage();
        break;
    case State::Offline:
    case State::Idle:
        m_dot->setColor(Theme::Danger);
        status = tr("Нет соединения");
        m_remote->showMessage(Icon::Alert, tr("Не удалось подключиться"),
                              m_session->stateMessage().isEmpty() ? status
                                                                  : m_session->stateMessage(),
                              tr("Повторить"));
        break;
    }
    m_status->setText(status);
    m_reconnect->setVisible(state == State::Offline || state == State::Idle);
    m_remote->setInteractive(state == State::Online || m_remote->model()->rowCount() > 0);
    emit titleChanged();
}

void SessionView::onReady(bool reconnected)
{
    if (reconnected && m_navigatedOnce) {
        refreshRemote();
        m_toasts->notify(tr("Соединение с %1 восстановлено").arg(m_session->title()),
                         ToastHost::Kind::Success);
        return;
    }
    const QString start = m_navigatedWhileConnecting && !m_remotePath.isEmpty()
                              ? m_remotePath
                              : m_session->startPath();
    m_navigatedOnce = true;
    navigateRemote(start.isEmpty() ? QStringLiteral("/") : start, false);
}

void SessionView::rememberListing(const QString &path, const RemoteEntries &entries)
{
    m_cache.insert(path, entries);
    m_cacheStamp.insert(path, m_cacheClock.elapsed());
    if (!m_session->hostId().isEmpty())
        m_cacheSave.start();
}

void SessionView::saveListings()
{
    m_cacheSave.stop();
    if (m_session->hostId().isEmpty() || m_cache.isEmpty())
        return;
    ListingCache::Snapshot snapshot;
    QStringList order;
    const QString current = m_remotePath;
    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it) {
        if (!m_cacheOrder.contains(it.key()) &&
            remoteParent(it.key()) == current)
            order.append(it.key());
    }
    for (const QString &path : std::as_const(m_cacheOrder)) {
        if (m_cache.contains(path) && path != current)
            order.append(path);
    }
    if (m_cache.contains(current))
        order.append(current);
    snapshot.order = order;
    for (const QString &path : std::as_const(order))
        snapshot.listings.insert(path, m_cache.value(path));
    ListingCache::save(m_session->hostId(), snapshot);
}

SessionView::~SessionView()
{
    saveListings();
}

void SessionView::navigateRemote(const QString &path, bool record)
{
    QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!clean.startsWith(QLatin1Char('/')))
        clean = remoteJoin(m_remotePath.isEmpty() ? QStringLiteral("/") : m_remotePath, clean);
    m_remotePath = clean;
    m_remote->setPath(clean, record);
    m_cacheOrder.removeAll(clean);
    m_cacheOrder.append(clean);
    const auto cached = m_cache.constFind(clean);
    m_remoteLoading = cached == m_cache.cend();
    if (!m_remoteLoading) {
        m_remote->setEntries(cached.value());
    } else {
        m_remote->model()->clear();
        m_remote->showLoading(tr("Загрузка…"));
    }
    if (!m_session->hostId().isEmpty()) {
        HostStore::instance().setLastPath(m_session->hostId(), clean);
        m_cacheSave.start();
    }
    if (!m_session->online()) {
        m_navigatedWhileConnecting = true;
        return;
    }
    constexpr qint64 FreshMs = 3000;
    const auto stamp = m_cacheStamp.constFind(clean);
    if (!m_remoteLoading && stamp != m_cacheStamp.cend() &&
        m_cacheClock.elapsed() - stamp.value() < FreshMs)
        return;
    m_listGeneration = m_session->list(clean);
}

void SessionView::refreshRemote()
{
    if (m_remotePath.isEmpty())
        return;
    m_listGeneration = m_session->list(m_remotePath);
}

void SessionView::navigateLocal(const QString &path, bool record)
{
    QString target = path;
    if (!target.isEmpty()) {
        const QFileInfo info(target);
        if (!info.exists() || !info.isDir()) {
            m_toasts->notify(tr("Папка «%1» недоступна").arg(path), ToastHost::Kind::Warning);
            return;
        }
        target = QDir::fromNativeSeparators(info.absoluteFilePath());
    }
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_localPath = target;
    m_local->setPath(target, record);
    m_local->setEntries(listLocal(target));
    if (!target.isEmpty())
        m_watcher->addPath(target);
}

void SessionView::showLocalPlaceholder(const QString &path, const RemoteEntries &entries)
{
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_localPlaceholder = true;
    m_localPath = path;
    m_local->setPath(path, false);
    m_local->setEntries(entries);
}

void SessionView::refreshLocal()
{
    if (m_localPlaceholder)
        return;
    if (!m_localPath.isEmpty() && !QFileInfo::exists(m_localPath)) {
        navigateLocal(QDir::homePath());
        return;
    }
    m_local->setEntries(listLocal(m_localPath));
}

void SessionView::navigateBack()
{
    if (m_remote->view()->hasFocus() || !m_local->view()->hasFocus())
        m_remote->goBack();
    else
        m_local->goBack();
}

void SessionView::navigateForward()
{
    if (m_remote->view()->hasFocus() || !m_local->view()->hasFocus())
        m_remote->goForward();
    else
        m_local->goForward();
}

void SessionView::openRemoteEntry(const RemoteEntry &entry)
{
    if (entry.directory) {
        navigateRemote(entry.path);
        return;
    }
    download({entry}, m_localPath.isEmpty() ? QDir::homePath() : m_localPath);
}

void SessionView::openLocalEntry(const RemoteEntry &entry)
{
    if (entry.directory) {
        navigateLocal(entry.path);
        return;
    }
    uploadPaths({entry.path});
}

void SessionView::enqueue(const QVector<TransferJob> &jobs)
{
    if (jobs.isEmpty())
        return;
    m_session->transfers()->enqueue(jobs);
    const QString what = jobs.size() == 1
                             ? QFileInfo(jobs.first().upload ? jobs.first().localPath
                                                             : jobs.first().remotePath)
                                   .fileName()
                             : tr("%1 объектов").arg(jobs.size());
    m_toasts->notify(jobs.first().upload ? tr("Загрузка: %1").arg(what)
                                         : tr("Скачивание: %1").arg(what),
                     ToastHost::Kind::Info,
                     jobs.first().upload
                         ? tr("на %1 → %2").arg(m_session->title(),
                                                shortPath(remoteParent(jobs.first().remotePath)))
                         : tr("в %1").arg(shortPath(QDir::toNativeSeparators(
                               QFileInfo(jobs.first().localPath).absolutePath()))));
}

void SessionView::uploadPaths(const QStringList &paths, const QString &remoteDirectory)
{
    const QString target = remoteDirectory.isEmpty() ? m_remotePath : remoteDirectory;
    if (target.isEmpty()) {
        m_toasts->notify(tr("Дождитесь подключения к серверу"), ToastHost::Kind::Warning);
        return;
    }
    QVector<TransferJob> jobs;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists())
            continue;
        TransferJob job;
        job.id = nextTransferId();
        job.upload = true;
        job.localPath = info.absoluteFilePath();
        job.remotePath = remoteJoin(target, info.fileName());
        job.directory = info.isDir() && !info.isSymLink();
        job.size = job.directory ? -1 : info.size();
        jobs.append(job);
    }
    enqueue(jobs);
}

void SessionView::download(const RemoteEntries &entries, const QString &localDirectory)
{
    QVector<TransferJob> jobs;
    for (const RemoteEntry &entry : entries) {
        TransferJob job;
        job.id = nextTransferId();
        job.remotePath = entry.path;
        job.localPath = QDir(localDirectory).filePath(entry.name);
        job.directory = entry.directory;
        job.permissions = entry.permissions;
        if (!job.directory) {
            job.size = entry.size;
            job.modified = entry.modified.isValid() ? entry.modified.toSecsSinceEpoch() : 0;
        }
        jobs.append(job);
    }
    enqueue(jobs);
}

void SessionView::downloadSelected()
{
    const RemoteEntries entries = m_remote->selectedEntries();
    if (entries.isEmpty()) {
        m_toasts->notify(tr("Выберите файлы на сервере"), ToastHost::Kind::Info);
        return;
    }
    if (m_localPath.isEmpty()) {
        m_toasts->notify(tr("Откройте папку на компьютере, куда скачивать"),
                         ToastHost::Kind::Warning);
        return;
    }
    download(entries, m_localPath);
}

void SessionView::uploadSelected()
{
    QStringList paths;
    for (const RemoteEntry &entry : m_local->selectedEntries())
        paths.append(entry.path);
    if (paths.isEmpty()) {
        m_toasts->notify(tr("Выберите файлы на компьютере"), ToastHost::Kind::Info);
        return;
    }
    uploadPaths(paths);
}

void SessionView::uploadDialog(bool folder)
{
    const QString start = m_localPath.isEmpty() ? QDir::homePath() : m_localPath;
    if (folder) {
        const QString path =
            QFileDialog::getExistingDirectory(this, tr("Папка для загрузки"), start);
        if (!path.isEmpty())
            uploadPaths({path});
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Файлы для загрузки"), start);
    if (!files.isEmpty())
        uploadPaths(files);
}

void SessionView::createRemoteFolder()
{
    if (!m_session->online())
        return;
    Sheets::prompt(window(), tr("Новая папка"), tr("Имя папки"), QString(), tr("Создать"),
                   Icon::FolderPlus, [this](const QString &name) {
                       m_session->makeDirectory(remoteJoin(m_remotePath, name));
                   });
}

void SessionView::createRemoteFile()
{
    if (!m_session->online())
        return;
    Sheets::prompt(window(), tr("Новый файл"), tr("Имя файла"), QString(), tr("Создать"),
                   Icon::FilePlus, [this](const QString &name) {
                       m_session->createFile(remoteJoin(m_remotePath, name));
                   });
}

void SessionView::renameRemote()
{
    const RemoteEntries entries = m_remote->selectedEntries();
    if (entries.size() != 1 || !m_session->online())
        return;
    const RemoteEntry entry = entries.first();
    Sheets::prompt(window(), tr("Переименовать"), tr("Новое имя"), entry.name,
                   tr("Переименовать"), Icon::Edit, [this, entry](const QString &name) {
                       if (name != entry.name)
                           m_session->rename(entry.path,
                                             remoteJoin(remoteParent(entry.path), name));
                   });
}

void SessionView::deleteRemote()
{
    const RemoteEntries entries = m_remote->selectedEntries();
    if (entries.isEmpty() || !m_session->online())
        return;
    const QString what = entries.size() == 1 ? tr("«%1»").arg(entries.first().name)
                                             : tr("%1 объектов").arg(entries.size());
    Sheets::confirm(window(), tr("Удалить %1?").arg(what),
                    tr("Папки удаляются вместе с содержимым. Отменить это нельзя."),
                    tr("Удалить"), true, [this, entries] {
                        QStringList paths;
                        for (const RemoteEntry &entry : entries)
                            paths.append(entry.path);
                        m_remote->removeEntries(paths);
                        m_cache.remove(m_remotePath);
                        m_session->remove(entries);
                    });
}

bool SessionView::localFocused() const
{
    const QWidget *focus = QApplication::focusWidget();
    return focus && (focus == m_local || m_local->isAncestorOf(focus));
}

void SessionView::deleteFocused()
{
    if (localFocused())
        deleteLocal();
    else
        deleteRemote();
}

void SessionView::renameFocused()
{
    if (localFocused())
        renameLocal();
    else
        renameRemote();
}

void SessionView::deleteLocal()
{
    const RemoteEntries entries = m_local->selectedEntries();
    if (entries.isEmpty() || m_localPath.isEmpty())
        return;
    const QString what = entries.size() == 1 ? tr("«%1»").arg(entries.first().name)
                                             : tr("%1 объектов").arg(entries.size());
    Sheets::confirm(window(), tr("Удалить %1 с компьютера?").arg(what),
                    tr("Файлы будут перемещены в корзину Windows — их можно восстановить."),
                    tr("В корзину"), true, [this, entries] {
                        QStringList removed;
                        QStringList failed;
                        for (const RemoteEntry &entry : entries) {
                            if (QFile::moveToTrash(entry.path))
                                removed.append(entry.path);
                            else
                                failed.append(entry.name);
                        }
                        m_local->removeEntries(removed);
                        if (!failed.isEmpty()) {
                            m_toasts->notify(tr("Не удалось удалить: %1")
                                                 .arg(failed.join(QStringLiteral(", "))),
                                             ToastHost::Kind::Error);
                        } else {
                            m_toasts->notify(removed.size() == 1
                                                 ? tr("В корзине: %1").arg(entries.first().name)
                                                 : tr("В корзину перемещено: %1").arg(removed.size()),
                                             ToastHost::Kind::Success);
                        }
                        m_localRefresh.start();
                    });
}

void SessionView::renameLocal()
{
    const RemoteEntries entries = m_local->selectedEntries();
    if (entries.size() != 1 || m_localPath.isEmpty())
        return;
    const RemoteEntry entry = entries.first();
    Sheets::prompt(window(), tr("Переименовать"), tr("Новое имя"), entry.name,
                   tr("Переименовать"), Icon::Rename, [this, entry](const QString &name) {
                       if (name == entry.name)
                           return;
                       const QString target = QDir(m_localPath).filePath(name);
                       if (QFileInfo::exists(target) || !QFile::rename(entry.path, target)) {
                           m_toasts->notify(tr("Не удалось переименовать «%1»").arg(entry.name),
                                            ToastHost::Kind::Error);
                           return;
                       }
                       refreshLocal();
                   });
}

void SessionView::createLocalFolder()
{
    if (m_localPath.isEmpty())
        return;
    Sheets::prompt(window(), tr("Новая папка на компьютере"), tr("Имя папки"), QString(),
                   tr("Создать"), Icon::FolderPlus, [this](const QString &name) {
                       if (!QDir(m_localPath).mkdir(name)) {
                           m_toasts->notify(tr("Не удалось создать папку «%1»").arg(name),
                                            ToastHost::Kind::Error);
                           return;
                       }
                       refreshLocal();
                   });
}

void SessionView::focusRemoteFilter()
{
    m_remote->focusFilter();
}

void SessionView::editRemotePath()
{
    m_remote->editPath();
}

void SessionView::remoteMenu(const QPoint &position, bool onItem)
{
    const RemoteEntries selected = m_remote->selectedEntries();
    const bool online = m_session->online();
    QMenu *menu = makeMenu(this);
    if (onItem && selected.size() == 1 && selected.first().directory) {
        connect(addMenuAction(menu, Icon::Folder, tr("Открыть")), &QAction::triggered, this,
                [this, entry = selected.first()] { navigateRemote(entry.path); });
    }
    if (onItem) {
        QAction *download = addMenuAction(menu, Icon::Download, tr("Скачать"));
        download->setEnabled(online);
        connect(download, &QAction::triggered, this, &SessionView::downloadSelected);
        if (selected.size() == 1 && !selected.first().directory) {
            QAction *edit =
                addMenuAction(menu, Icon::Edit, tr("Просмотр / правка"), QStringLiteral("F4"));
            edit->setEnabled(online);
            connect(edit, &QAction::triggered, this, &SessionView::openSelected);
        }
        menu->addSeparator();
    }
    QAction *uploadFiles = addMenuAction(menu, Icon::Upload, tr("Загрузить файлы…"));
    QAction *uploadFolder = addMenuAction(menu, Icon::Upload, tr("Загрузить папку…"));
    menu->addSeparator();
    QAction *newFolder = addMenuAction(menu, Icon::FolderPlus, tr("Новая папка"));
    QAction *newFile = addMenuAction(menu, Icon::FilePlus, tr("Новый файл"));
    for (QAction *action : {uploadFiles, uploadFolder, newFolder, newFile})
        action->setEnabled(online);
    connect(uploadFiles, &QAction::triggered, this, [this] { uploadDialog(false); });
    connect(uploadFolder, &QAction::triggered, this, [this] { uploadDialog(true); });
    connect(newFolder, &QAction::triggered, this, &SessionView::createRemoteFolder);
    connect(newFile, &QAction::triggered, this, &SessionView::createRemoteFile);
    if (onItem) {
        menu->addSeparator();
        QAction *rename = addMenuAction(menu, Icon::Rename, tr("Переименовать"), QStringLiteral("F2"));
        rename->setEnabled(online && selected.size() == 1);
        connect(rename, &QAction::triggered, this, &SessionView::renameRemote);
        connect(addMenuAction(menu, Icon::Copy, tr("Копировать путь")), &QAction::triggered,
                this, [selected] {
                    QStringList paths;
                    for (const RemoteEntry &entry : selected)
                        paths.append(entry.path);
                    QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
                });
        menu->addSeparator();
        QAction *remove = addMenuAction(menu, Icon::Trash, tr("Удалить"), QStringLiteral("Del"));
        remove->setEnabled(online);
        connect(remove, &QAction::triggered, this, &SessionView::deleteRemote);
    } else {
        menu->addSeparator();
        connect(addMenuAction(menu, Icon::Copy, tr("Копировать путь папки")),
                &QAction::triggered, this,
                [this] { QApplication::clipboard()->setText(m_remotePath); });
        connect(addMenuAction(menu, Icon::Refresh, tr("Обновить"), QStringLiteral("F5")),
                &QAction::triggered, this, &SessionView::refreshRemote);
    }
    menu->popup(position);
}

void SessionView::localMenu(const QPoint &position, bool onItem)
{
    const RemoteEntries selected = m_local->selectedEntries();
    QMenu *menu = makeMenu(this);
    if (onItem) {
        QAction *upload = addMenuAction(menu, Icon::Upload, tr("Загрузить на сервер"));
        upload->setEnabled(m_session->online());
        connect(upload, &QAction::triggered, this, &SessionView::uploadSelected);
        if (selected.size() == 1) {
            connect(addMenuAction(menu, Icon::ExternalLink, tr("Открыть")), &QAction::triggered,
                    this, [entry = selected.first()] {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(entry.path));
                    });
        }
        if (!m_localPath.isEmpty()) {
            menu->addSeparator();
            QAction *rename =
                addMenuAction(menu, Icon::Rename, tr("Переименовать"), QStringLiteral("F2"));
            rename->setEnabled(selected.size() == 1);
            connect(rename, &QAction::triggered, this, &SessionView::renameLocal);
            connect(addMenuAction(menu, Icon::Trash, tr("Удалить в корзину"), QStringLiteral("Del")),
                    &QAction::triggered, this, &SessionView::deleteLocal);
        }
        menu->addSeparator();
    }
    if (!m_localPath.isEmpty()) {
        connect(addMenuAction(menu, Icon::FolderPlus, tr("Новая папка")), &QAction::triggered,
                this, &SessionView::createLocalFolder);
        connect(addMenuAction(menu, Icon::Folder, tr("Показать в Проводнике")),
                &QAction::triggered, this,
                [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_localPath)); });
    }
    connect(addMenuAction(menu, Icon::Home, tr("Домашняя папка")), &QAction::triggered, this,
            [this] { navigateLocal(QDir::homePath()); });
    connect(addMenuAction(menu, Icon::Refresh, tr("Обновить")), &QAction::triggered, this,
            &SessionView::refreshLocal);
    menu->popup(position);
}

QStringList SessionView::localPaths(const QMimeData *data)
{
    QStringList paths;
    if (!data)
        return paths;
    for (const QUrl &url : data->urls()) {
        if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile()))
            paths.append(url.toLocalFile());
    }
    if (paths.isEmpty() && data->hasFormat(Mime::Virtual)) {
        const QString list = QString::fromUtf8(data->data(Mime::Virtual));
        for (const QString &path : list.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (QFileInfo::exists(path))
                paths.append(path);
        }
    }
    return paths;
}

SessionView::DropPlan SessionView::planDrop(QWidget *target, const QPoint &position,
                                            const QMimeData *mime) const
{
    DropPlan plan;
    if (!mime)
        return plan;
    quint64 sourceSession = 0;
    if (mime->hasFormat(Mime::Remote)) {
        Mime::remoteEntries(mime, &sourceSession);
        if (sourceSession != m_session->id() || target != m_local->view()->viewport())
            return plan;
        plan.kind = DropPlan::Kind::Download;
        plan.pane = m_local;
        plan.directory = m_localPath;
        if (const RemoteEntry *entry = m_local->entryAtViewport(position)) {
            if (entry->directory) {
                plan.directory = entry->path;
                plan.row = m_local->view()->rowAt(position);
            }
        }
        if (plan.directory.isEmpty())
            plan.kind = DropPlan::Kind::None;
        return plan;
    }

    const bool files = mime->hasUrls() || mime->hasFormat(Mime::Virtual);
    if (!files)
        return plan;
    const bool fromLocalPane = mime->hasFormat(Mime::Local);
    if (fromLocalPane && target == m_local->view()->viewport())
        return plan;
    plan.kind = DropPlan::Kind::Upload;
    plan.pane = m_remote;
    plan.directory = m_remotePath;
    if (target == m_remote->view()->viewport()) {
        if (const RemoteEntry *entry = m_remote->entryAtViewport(position)) {
            if (entry->directory) {
                plan.directory = entry->path;
                plan.row = m_remote->view()->rowAt(position);
            }
        }
    }
    if (!m_session->online() && m_remotePath.isEmpty())
        plan.kind = DropPlan::Kind::None;
    return plan;
}

bool SessionView::handleDrag(QWidget *target, QEvent *event)
{
    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto *drag = static_cast<QDragMoveEvent *>(event);
        const QPoint position = drag->position().toPoint();
        const DropPlan plan = planDrop(target, position, drag->mimeData());
        m_local->view()->setDropRow(plan.pane == m_local ? plan.row : -1);
        m_remote->view()->setDropRow(plan.pane == m_remote ? plan.row : -1);
        if (plan.kind == DropPlan::Kind::None) {
            m_overlay->hideSoon();
            drag->ignore();
            return true;
        }
        const QRect area(plan.pane->mapTo(this, QPoint(0, 0)), plan.pane->size());
        if (plan.kind == DropPlan::Kind::Upload) {
            m_overlay->showTarget(area, Icon::Upload,
                                  tr("Загрузить в %1").arg(shortPath(plan.directory)));
        } else {
            m_overlay->showTarget(area, Icon::Download,
                                  tr("Скачать в %1")
                                      .arg(shortPath(QDir::toNativeSeparators(plan.directory))));
        }
        drag->setDropAction(Qt::CopyAction);
        drag->accept();
        return true;
    }
    case QEvent::DragLeave:
        m_overlay->hideSoon();
        m_local->view()->setDropRow(-1);
        m_remote->view()->setDropRow(-1);
        return true;
    case QEvent::Drop: {
        auto *drop = static_cast<QDropEvent *>(event);
        const DropPlan plan = planDrop(target, drop->position().toPoint(), drop->mimeData());
        m_overlay->hideNow();
        m_local->view()->setDropRow(-1);
        m_remote->view()->setDropRow(-1);
        if (plan.kind == DropPlan::Kind::Upload) {
            const QStringList paths = localPaths(drop->mimeData());
            if (paths.isEmpty())
                m_toasts->notify(tr("Не удалось прочитать перетаскиваемые файлы"),
                                 ToastHost::Kind::Warning);
            else
                uploadPaths(paths, plan.directory);
        } else if (plan.kind == DropPlan::Kind::Download) {
            download(Mime::remoteEntries(drop->mimeData()), plan.directory);
        }
        drop->setDropAction(Qt::CopyAction);
        drop->accept();
        return true;
    }
    default:
        return false;
    }
}

bool SessionView::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (widget && (widget == m_local->view()->viewport() ||
                   widget == m_remote->view()->viewport())) {
        if (handleDrag(widget, event))
            return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SessionView::dragEnterEvent(QDragEnterEvent *event)
{
    handleDrag(this, event);
}

void SessionView::dragMoveEvent(QDragMoveEvent *event)
{
    handleDrag(this, event);
}

void SessionView::dragLeaveEvent(QDragLeaveEvent *event)
{
    handleDrag(this, event);
}

void SessionView::dropEvent(QDropEvent *event)
{
    handleDrag(this, event);
}

void SessionView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_overlay->isVisible())
        m_overlay->setGeometry(rect());
}
