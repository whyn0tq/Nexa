#include "mainwindow.h"

#include "controls.h"
#include "filelist.h"
#include "glass.h"
#include "launcher.h"
#include "nativewindow.h"
#include "palette.h"
#include "session.h"
#include "sessionview.h"
#include "settings.h"
#include "sheets.h"
#include "theme.h"
#include "titlebar.h"
#include "transferdock.h"
#include "transferengine.h"
#include "transferprefs.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

quint64 nextTransferId();

namespace {
const QSize CompactSize(980, 620);
const QSize WideSize(1440, 900);

QRect placed(const QPoint &center, QSize size, const QRect &area)
{
    size = size.boundedTo(area.size());
    QRect rect(QPoint(0, 0), size);
    rect.moveCenter(center);
    rect.moveLeft(std::clamp(rect.left(), area.left(), area.right() - size.width() + 1));
    rect.moveTop(std::clamp(rect.top(), area.top(), area.bottom() - size.height() + 1));
    return rect;
}

QRect lerp(const QRect &from, const QRect &to, qreal t)
{
    const auto mix = [t](int a, int b) { return a + qRound((b - a) * t); };
    return QRect(QPoint(mix(from.left(), to.left()), mix(from.top(), to.top())),
                 QSize(mix(from.width(), to.width()), mix(from.height(), to.height())));
}

TabStrip::Status statusFor(Session::State state)
{
    switch (state) {
    case Session::State::Online:
        return TabStrip::Status::Online;
    case Session::State::Connecting:
    case Session::State::AwaitingTrust:
    case Session::State::Reconnecting:
        return TabStrip::Status::Busy;
    case Session::State::Offline:
    case Session::State::Idle:
        return TabStrip::Status::Offline;
    }
    return TabStrip::Status::None;
}
}

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);
    setWindowTitle(QStringLiteral("Nexa"));
    setMinimumSize(CompactSize);
    setAcceptDrops(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_root = new Backdrop;
    outer->addWidget(m_root);

    auto *layout = new QVBoxLayout(m_root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_titleBar = new TitleBar;
    layout->addWidget(m_titleBar);

    auto *content = new QVBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    m_pages = new QStackedWidget;
    m_launcher = new LauncherPage;
    m_pages->addWidget(m_launcher);
    content->addWidget(m_pages, 1);
    auto *dockRow = new QVBoxLayout;
    dockRow->setContentsMargins(12, 0, 12, 12);
    m_dock = new TransferDock;
    dockRow->addWidget(m_dock);
    content->addLayout(dockRow);
    m_dock->setOverlayHost(m_root);
    layout->addLayout(content, 1);

    m_toasts = new ToastHost(this);
    m_toasts->setBottomMargin(80);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRequested, this,
            [this] { isMaximized() ? showNormal() : showMaximized(); });
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::paletteRequested, this, &MainWindow::showPalette);
    connect(m_titleBar, &TitleBar::settingsRequested, this, [this] { (new SettingsSheet(this))->open(); });
    connect(m_titleBar, &TitleBar::transfersRequested, this, [this] {
        m_dock->setVisible(true);
        m_dock->toggle();
    });
    TabStrip *tabs = m_titleBar->tabs();
    connect(tabs, &TabStrip::activated, this, &MainWindow::activate);
    connect(tabs, &TabStrip::closeRequested, this,
            [this](quint64 id) { closeSession(id); });
    connect(tabs, &TabStrip::newRequested, this, [this] {
        activate(0);
        addHost();
    });
    connect(tabs, &TabStrip::contextMenuRequested, this, &MainWindow::tabMenu);

    connect(m_launcher, &LauncherPage::quickConnect, this, &MainWindow::quickConnect);
    connect(m_launcher, &LauncherPage::connectHost, this, [this](const QString &id) { openHost(id); });
    connect(m_launcher, &LauncherPage::hostMenu, this, &MainWindow::hostMenu);
    connect(m_launcher, &LauncherPage::addHost, this, [this] { addHost(); });

    TransferModel *model = m_dock->model();
    connect(model, &TransferModel::summaryChanged, this, &MainWindow::updateDock);
    connect(m_dock, &TransferDock::cancelRequested, this, [this](quint64 sessionId, quint64 id) {
        if (Tab *t = tab(sessionId))
            t->session->transfers()->cancel(id);
    });
    connect(m_dock, &TransferDock::cancelAllRequested, this, [this] {
        const auto cancelAll = [this] {
            for (const Tab &t : std::as_const(m_tabs))
                t.session->transfers()->cancelAll();
        };
        const int active = activeTransfers();
        if (active == 0) {
            cancelAll();
            return;
        }
        Sheets::confirm(this, tr("Остановить все передачи?"),
                        tr("Остановятся передачи в работе и в очереди: %1.").arg(active),
                        tr("Остановить всё"), true, cancelAll);
    });
    connect(m_dock, &TransferDock::retryRequested, this, [this](const TransferStatus &status) {
        Tab *t = tab(status.sessionId);
        if (!t) {
            m_toasts->notify(tr("Сессия уже закрыта"), ToastHost::Kind::Warning);
            return;
        }
        TransferJob job;
        job.id = nextTransferId();
        job.upload = status.upload;
        job.localPath = status.localPath;
        job.remotePath = status.remotePath;
        job.directory = status.directory;
        if (!job.directory && !job.upload)
            job.size = -1;
        t->session->transfers()->enqueue({job});
    });
    connect(m_dock, &TransferDock::revealRequested, this, [](const TransferStatus &status) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(status.localPath).absolutePath()));
    });
    connect(m_dock, &TransferDock::heightChanged, this, [this] { updateDock(); });
    connect(&TransferPrefs::instance(), &TransferPrefs::policyChanged, this,
            [this](OverwritePolicy policy) {
                for (const Tab &t : std::as_const(m_tabs))
                    t.session->transfers()->setPolicy(policy);
            });
    connect(&TransferPrefs::instance(), &TransferPrefs::connectionsChanged, this,
            [this](int connections) {
                for (const Tab &t : std::as_const(m_tabs))
                    t.session->transfers()->setParallelism(connections);
                m_toasts->notify(tr("Параллельных соединений: %1").arg(connections),
                                 ToastHost::Kind::Info);
            });

    buildShortcuts();
    refreshTabs();
    updateDock();

    m_resize = new FrameAnimation(this);
    connect(m_resize, &FrameAnimation::valueChanged, this,
            [this](qreal t) {
                setGeometry(lerp(m_resizeFrom, m_resizeTo, t));
                repaint();
                Win::waitForFrame();
            });
    connect(m_resize, &FrameAnimation::finished, this, [this] {
        if (const auto done = std::exchange(m_resizeDone, {}))
            done();
    });

    QSettings settings;
    m_wideSize = settings.value(QStringLiteral("window/size"), WideSize).toSize()
                     .expandedTo(CompactSize);
    m_wideMaximized = settings.value(QStringLiteral("window/maximized")).toBool();
    bool restoring = false;
    for (const QString &id : HostStore::instance().openSessions())
        restoring = restoring || HostStore::instance().contains(id);
    const QScreen *display = QGuiApplication::screenAt(QCursor::pos());
    const QRect area = (display ? display : QGuiApplication::primaryScreen())->availableGeometry();
    m_compact = !restoring;
    setGeometry(placed(area.center(), m_compact ? CompactSize : m_wideSize, area));
    if (!m_compact && m_wideMaximized)
        setWindowState(Qt::WindowMaximized);
}

void MainWindow::expandWindow(bool animated)
{
    if (!m_compact)
        return;
    m_compact = false;
    if (isMaximized() || isFullScreen() || isMinimized())
        return;
    const QRect area = screen()->availableGeometry();
    const QRect from = geometry();
    const QRect wide = placed(from.center(), m_wideSize.expandedTo(from.size()), area);
    if (!animated) {
        setGeometry(wide);
        if (m_wideMaximized)
            showMaximized();
        return;
    }
    if (!m_wideMaximized) {
        animateTo(wide);
        return;
    }
    animateTo(area, [this, wide] {
        setGeometry(wide);
        showMaximized();
    });
}

void MainWindow::collapseWindow()
{
    if (m_compact)
        return;
    m_compact = true;
    if (isMinimized() || isFullScreen())
        return;
    if (!m_resize->isRunning()) {
        m_wideMaximized = isMaximized();
        m_wideSize = (m_wideMaximized ? normalGeometry().size() : size()).expandedTo(CompactSize);
    }
    const QRect from = geometry();
    if (isMaximized()) {
        showNormal();
        setGeometry(from);
    }
    animateTo(placed(from.center(), CompactSize, screen()->availableGeometry()));
}

void MainWindow::animateTo(const QRect &target, std::function<void()> done)
{
    m_resize->stop();
    m_resizeFrom = geometry();
    m_resizeTo = target;
    m_resizeDone = std::move(done);
    m_resize->start(0.0, 1.0, 650, QEasingCurve::OutCubic);
}

MainWindow::~MainWindow()
{
    for (const Tab &t : std::as_const(m_tabs))
        t.session->shutdown();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_framePrepared) {
        m_framePrepared = true;
        Win::prepareFrameless(this);
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (Win::handleNativeEvent(this, m_titleBar, message, result))
        return true;
    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
        m_titleBar->setMaximized(isMaximized());
    QWidget::changeEvent(event);
}

void MainWindow::buildShortcuts()
{
    const auto bind = [this](const QKeySequence &sequence, auto slot) {
        auto *shortcut = new QShortcut(sequence, this);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    bind(QKeySequence(Qt::CTRL | Qt::Key_K), [this] { showPalette(); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_Comma), [this] { (new SettingsSheet(this))->open(); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_P), [this] { showPalette(); });
    bind(QKeySequence(Qt::CTRL | Qt::Key_T), [this] {
        activate(0);
        m_launcher->focusQuickConnect();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_W), [this] {
        if (m_current != 0)
            closeSession(m_current);
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_Tab), [this] {
        QVector<quint64> ids{0};
        for (const Tab &t : std::as_const(m_tabs))
            ids.append(t.session->id());
        activate(ids.at((ids.indexOf(m_current) + 1) % ids.size()));
    });
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), [this] {
        QVector<quint64> ids{0};
        for (const Tab &t : std::as_const(m_tabs))
            ids.append(t.session->id());
        activate(ids.at((ids.indexOf(m_current) - 1 + ids.size()) % ids.size()));
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_J), [this] {
        m_dock->setVisible(true);
        m_dock->toggle();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_N), [this] { addHost(); });
    bind(QKeySequence(Qt::Key_F5), [this] {
        if (SessionView *view = currentView()) {
            view->refreshRemote();
            view->refreshLocal();
        }
    });
    for (const auto key : {Qt::Key_F3, Qt::Key_F4}) {
        bind(QKeySequence(key), [this] {
            if (SessionView *view = currentView())
                view->openSelected();
        });
    }
    bind(QKeySequence(Qt::Key_F2), [this] {
        if (SessionView *view = currentView())
            view->renameFocused();
    });
    bind(QKeySequence(Qt::Key_Delete), [this] {
        if (SessionView *view = currentView())
            view->deleteFocused();
    });
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), [this] {
        if (SessionView *view = currentView())
            view->createRemoteFolder();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_U), [this] {
        if (SessionView *view = currentView())
            view->uploadDialog(false);
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_D), [this] {
        if (SessionView *view = currentView())
            view->downloadSelected();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_L), [this] {
        if (SessionView *view = currentView())
            view->editRemotePath();
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_F), [this] {
        if (SessionView *view = currentView())
            view->focusRemoteFilter();
    });
    bind(QKeySequence(Qt::ALT | Qt::Key_Left), [this] {
        if (SessionView *view = currentView())
            view->navigateBack();
    });
    bind(QKeySequence(Qt::ALT | Qt::Key_Right), [this] {
        if (SessionView *view = currentView())
            view->navigateForward();
    });
}

MainWindow::Tab *MainWindow::tab(quint64 id)
{
    for (Tab &t : m_tabs) {
        if (t.session->id() == id)
            return &t;
    }
    return nullptr;
}

SessionView *MainWindow::currentView() const
{
    for (const Tab &t : m_tabs) {
        if (t.session->id() == m_current)
            return t.view;
    }
    return nullptr;
}

void MainWindow::openHost(const QString &id, bool forceNew)
{
    if (!forceNew) {
        for (const Tab &t : std::as_const(m_tabs)) {
            if (t.session->hostId() == id) {
                activate(t.session->id());
                if (t.session->state() == Session::State::Offline ||
                    t.session->state() == Session::State::Idle)
                    t.session->retry();
                return;
            }
        }
    }
    const SavedHost host = HostStore::instance().host(id);
    if (host.id.isEmpty())
        return;
    HostStore::instance().touch(id);
    openProfile(host.profile, host.title(), host.id, host.color);
}

void MainWindow::openProfile(const ConnectionProfile &profile, const QString &title,
                             const QString &hostId, int color)
{
    const quint64 id = m_nextSessionId++;
    auto *session = new Session(id, hostId, title, profile, this);
    session->setProperty("labelColor", color);
    auto *view = new SessionView(session, m_toasts);
    m_pages->addWidget(view);
    m_tabs.append({session, view});

    connect(session, &Session::stateChanged, this, &MainWindow::refreshTabs);
    connect(session, &Session::trustRequired, this,
            [this, session](const QString &server, const QString &fingerprint,
                            const QString &keyType, bool changed) {
                activate(session->id());
                Sheets::trust(this, server, fingerprint, keyType, changed,
                              [session](bool accepted, bool remember) {
                                  session->answerTrust(accepted, remember);
                              });
            });
    TransferEngine *engine = session->transfers();
    connect(engine, &TransferEngine::statusChanged, m_dock->model(), &TransferModel::upsert);
    connect(engine, &TransferEngine::jobFinished, this, [this](const TransferStatus &status) {
        if (status.state == TransferState::Failed) {
            m_toasts->notify(tr("Не удалось: %1").arg(status.name), ToastHost::Kind::Error,
                             status.detail);
        } else if (status.state == TransferState::Completed && activeTransfers() == 0) {
            m_toasts->notify(tr("Передачи завершены"), ToastHost::Kind::Success, status.name);
        }
    });
    connect(engine, &TransferEngine::notice, this,
            [this](const QString &message) { m_toasts->notify(message, ToastHost::Kind::Warning); });

    activate(id);
    session->open();
    saveOpenSessions();
}

void MainWindow::closeSession(quint64 id, bool confirmTransfers)
{
    Tab *t = tab(id);
    if (!t)
        return;
    if (confirmTransfers && t->session->transfers()->busy()) {
        Sheets::confirm(this, tr("Закрыть «%1»?").arg(t->session->title()),
                        tr("В этой сессии ещё идут передачи. Они будут прерваны; "
                           "незавершённые файлы можно докачать позже."),
                        tr("Закрыть"), true, [this, id] { closeSession(id, false); });
        return;
    }
    Session *session = t->session;
    SessionView *view = t->view;
    const int index = static_cast<int>(t - m_tabs.data());
    m_tabs.removeAt(index);
    m_dock->model()->removeSession(id);
    m_pages->removeWidget(view);
    view->deleteLater();
    session->shutdown();
    session->deleteLater();
    if (m_current == id) {
        const quint64 next = m_tabs.isEmpty() ? 0
                                              : m_tabs.at(std::min<int>(index, m_tabs.size() - 1))
                                                    .session->id();
        activate(next);
    }
    if (m_tabs.isEmpty())
        collapseWindow();
    refreshTabs();
    saveOpenSessions();
}

void MainWindow::activate(quint64 id)
{
    if (id == 0) {
        m_current = 0;
        m_pages->setCurrentWidget(m_launcher);
    } else if (Tab *t = tab(id)) {
        m_current = id;
        m_pages->setCurrentWidget(t->view);
        expandWindow(isVisible());
    }
    m_titleBar->tabs()->setCurrent(m_current);
    if (!m_closing)
        QSettings().setValue(QStringLiteral("session/active"), m_current);
}

void MainWindow::refreshTabs()
{
    QVector<TabStrip::Tab> tabs;
    TabStrip::Tab home;
    home.id = 0;
    home.title = tr("Сервера");
    home.tooltip = tr("Сервера и быстрое подключение");
    tabs.append(home);
    QStringList open;
    for (const Tab &t : std::as_const(m_tabs)) {
        if (!t.session->hostId().isEmpty())
            open.append(t.session->hostId());
        TabStrip::Tab item;
        item.id = t.session->id();
        item.title = t.session->title();
        item.status = statusFor(t.session->state());
        item.label = Theme::labelColor(t.session->property("labelColor").toInt());
        item.tooltip = QStringLiteral("%1\n%2").arg(t.session->profile().endpoint(),
                                                    t.session->stateMessage().isEmpty()
                                                        ? (t.session->online() ? tr("Подключено")
                                                                               : QString())
                                                        : t.session->stateMessage());
        tabs.append(item);
    }
    m_titleBar->tabs()->setTabs(tabs);
    m_titleBar->tabs()->setCurrent(m_current);
    m_launcher->setOpenHosts(open);
}

void MainWindow::saveOpenSessions()
{
    if (m_closing)
        return;
    QStringList ids;
    for (const Tab &t : std::as_const(m_tabs)) {
        if (!t.session->hostId().isEmpty())
            ids.append(t.session->hostId());
    }
    HostStore::instance().setOpenSessions(ids);
}

void MainWindow::restoreSessions()
{
    const QStringList ids = HostStore::instance().openSessions();
    for (const QString &id : ids) {
        if (HostStore::instance().contains(id))
            openHost(id, true);
    }
    activate(m_tabs.isEmpty() ? 0 : m_tabs.first().session->id());
}

void MainWindow::editHost(const QString &id)
{
    const SavedHost host = HostStore::instance().host(id);
    if (host.id.isEmpty())
        return;
    auto *editor = new HostEditor(this, host, false);
    editor->onSave = [this](const SavedHost &value, bool connect) {
        HostStore::instance().save(value);
        for (Tab &t : m_tabs) {
            if (t.session->hostId() == value.id) {
                const ConnectionProfile before = t.session->profile();
                t.session->setTitle(value.title());
                t.session->setProperty("labelColor", value.color);
                const ConnectionProfile &after = value.profile;
                if (before.host != after.host || before.port != after.port ||
                    before.username != after.username || before.password != after.password ||
                    before.auth != after.auth || before.protocol != after.protocol ||
                    before.useTls != after.useTls ||
                    before.privateKeyPath != after.privateKeyPath ||
                    before.keyPassphrase != after.keyPassphrase)
                    t.session->setProfile(after);
            }
        }
        refreshTabs();
        if (connect)
            openHost(value.id);
    };
    editor->open();
}

void MainWindow::addHost(const ConnectionProfile &prefill)
{
    SavedHost host;
    host.profile = prefill;
    if (host.profile.port == 0)
        host.profile.port = defaultPort(host.profile.protocol);
    host.color = static_cast<int>(HostStore::instance().hosts().size()) % Theme::labelColorCount();
    auto *editor = new HostEditor(this, host, true);
    editor->onSave = [this](const SavedHost &value, bool connect) {
        const QString id = HostStore::instance().save(value);
        m_toasts->notify(tr("Сервер «%1» сохранён").arg(value.title()), ToastHost::Kind::Success);
        if (connect)
            openHost(id);
    };
    editor->open();
}

void MainWindow::quickConnect(const QString &address)
{
    bool ok = false;
    const ConnectionProfile profile = HostStore::parseAddress(address, &ok);
    if (!ok) {
        addHost();
        return;
    }
    for (const SavedHost &host : HostStore::instance().hosts()) {
        if (host.profile.host == profile.host && host.profile.port == profile.port &&
            (profile.username.isEmpty() || host.profile.username == profile.username)) {
            openHost(host.id);
            return;
        }
    }
    addHost(profile);
}

void MainWindow::hostMenu(const QString &id, const QPoint &position)
{
    QMenu *menu = makeMenu(this);
    connect(addMenuAction(menu, Icon::Plug, tr("Подключиться")), &QAction::triggered, this,
            [this, id] { openHost(id); });
    connect(addMenuAction(menu, Icon::Plus, tr("Ещё одна сессия")), &QAction::triggered, this,
            [this, id] { openHost(id, true); });
    menu->addSeparator();
    connect(addMenuAction(menu, Icon::Edit, tr("Изменить")), &QAction::triggered, this,
            [this, id] { editHost(id); });
    connect(addMenuAction(menu, Icon::Copy, tr("Дублировать")), &QAction::triggered, this,
            [id] {
                SavedHost copy = HostStore::instance().host(id);
                copy.id.clear();
                copy.name = QObject::tr("%1 (копия)").arg(copy.title());
                copy.lastUsed = QDateTime();
                HostStore::instance().save(copy);
            });
    menu->addSeparator();
    connect(addMenuAction(menu, Icon::Trash, tr("Удалить")), &QAction::triggered, this,
            [this, id] {
                const SavedHost host = HostStore::instance().host(id);
                Sheets::confirm(this, tr("Удалить «%1»?").arg(host.title()),
                                tr("Сервер и сохранённые данные входа будут удалены из Nexa."),
                                tr("Удалить"), true, [id] { HostStore::instance().remove(id); });
            });
    menu->popup(position);
}

void MainWindow::tabMenu(quint64 id, const QPoint &position)
{
    Tab *t = tab(id);
    if (!t)
        return;
    Session *session = t->session;
    QMenu *menu = makeMenu(this);
    connect(addMenuAction(menu, Icon::Retry, tr("Переподключить")), &QAction::triggered,
            session, [session] { session->retry(); });
    if (!session->hostId().isEmpty()) {
        const QString hostId = session->hostId();
        connect(addMenuAction(menu, Icon::Plus, tr("Ещё одна сессия")), &QAction::triggered,
                this, [this, hostId] { openHost(hostId, true); });
        connect(addMenuAction(menu, Icon::Edit, tr("Изменить сервер")), &QAction::triggered,
                this, [this, hostId] { editHost(hostId); });
    }
    menu->addSeparator();
    connect(addMenuAction(menu, Icon::Close, tr("Закрыть"), QStringLiteral("Ctrl+W")),
            &QAction::triggered, this, [this, id] { closeSession(id); });
    connect(addMenuAction(menu, Icon::Close, tr("Закрыть остальные")), &QAction::triggered,
            this, [this, id] {
                QVector<quint64> others;
                for (const Tab &other : std::as_const(m_tabs)) {
                    if (other.session->id() != id)
                        others.append(other.session->id());
                }
                for (const quint64 other : others)
                    closeSession(other);
            });
    menu->popup(position);
}

void MainWindow::showPalette()
{
    QVector<CommandPalette::Command> commands;
    QSet<QString> openHosts;
    const HostStore &store = HostStore::instance();
    for (const Tab &t : std::as_const(m_tabs)) {
        const quint64 id = t.session->id();
        const QString hostId = t.session->hostId();
        openHosts.insert(hostId);
        const Icon icon = Icons::hostIcon(store.contains(hostId) ? store.host(hostId).kind
                                                                 : HostKind::Server);
        commands.append({icon, tr("Перейти: %1").arg(t.session->title()),
                         t.session->profile().endpoint(), tr("ВКЛАДКА"),
                         [this, id] { activate(id); }});
    }
    for (const SavedHost &host : store.hosts()) {
        if (openHosts.contains(host.id))
            continue;
        const QString id = host.id;
        commands.append({Icons::hostIcon(host.kind), host.title(), host.profile.endpoint(), tr("СЕРВЕР"),
                         [this, id] { openHost(id); }});
    }
    commands.append({Icon::Plus, tr("Новый сервер"), QStringLiteral("Ctrl+N"), tr("ДЕЙСТВИЕ"),
                     [this] { addHost(); }});
    commands.append({Icon::Bolt, tr("Быстрое подключение"), QStringLiteral("Ctrl+T"),
                     tr("ДЕЙСТВИЕ"), [this] {
                         activate(0);
                         m_launcher->focusQuickConnect();
                     }});
    commands.append({Icon::Gear, tr("Настройки"), QStringLiteral("Ctrl+,"), tr("ДЕЙСТВИЕ"),
                     [this] { (new SettingsSheet(this))->open(); }});
    commands.append({Icon::Transfers, tr("Передачи"), QStringLiteral("Ctrl+J"), tr("ДЕЙСТВИЕ"),
                     [this] {
                         m_dock->setVisible(true);
                         m_dock->setExpanded(true);
                     }});
    if (SessionView *view = currentView()) {
        commands.append({Icon::Upload, tr("Загрузить файлы на сервер"), QStringLiteral("Ctrl+U"),
                         tr("ДЕЙСТВИЕ"), [view] { view->uploadDialog(false); }});
        commands.append({Icon::Upload, tr("Загрузить папку на сервер"), QString(),
                         tr("ДЕЙСТВИЕ"), [view] { view->uploadDialog(true); }});
        commands.append({Icon::Edit, tr("Просмотр / правка файла"), QStringLiteral("F4"),
                         tr("ДЕЙСТВИЕ"), [view] { view->openSelected(); }});
        commands.append({Icon::FolderPlus, tr("Новая папка на сервере"),
                         QStringLiteral("Ctrl+Shift+N"), tr("ДЕЙСТВИЕ"),
                         [view] { view->createRemoteFolder(); }});
        commands.append({Icon::Refresh, tr("Обновить"), QStringLiteral("F5"), tr("ДЕЙСТВИЕ"),
                         [view] { view->refreshRemote(); }});
        const quint64 id = m_current;
        commands.append({Icon::Retry, tr("Переподключить"), QString(), tr("ДЕЙСТВИЕ"),
                         [this, id] {
                             if (Tab *t = tab(id))
                                 t->session->retry();
                         }});
        commands.append({Icon::Close, tr("Закрыть вкладку"), QStringLiteral("Ctrl+W"),
                         tr("ДЕЙСТВИЕ"), [this, id] { closeSession(id); }});
    }
    auto *palette = new CommandPalette(this, commands);
    palette->open();
}

int MainWindow::activeTransfers() const
{
    return m_dock->model()->activeCount();
}

void MainWindow::updateDock()
{
    const int active = activeTransfers();
    m_titleBar->setTransferBadge(active);
    const bool any = m_dock->model()->rowCount() > 0;
    if (any && !m_dock->isVisible())
        m_dock->setVisible(true);
    if (!any && !m_dock->expanded())
        m_dock->setVisible(false);
    m_toasts->setBottomMargin(m_dock->isVisible() ? m_dock->height() + m_dock->panelHeight() + 34
                                                  : 16);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_closing && activeTransfers() > 0) {
        event->ignore();
        Sheets::confirm(this, tr("Выйти из Nexa?"),
                        tr("Идут передачи (%1). Незавершённые файлы можно будет докачать "
                           "после запуска.")
                            .arg(activeTransfers()),
                        tr("Выйти"), true, [this] {
                            m_closing = true;
                            close();
                        });
        return;
    }
    m_closing = true;
    if (!m_compact && !m_resize->isRunning() && !isMinimized()) {
        m_wideMaximized = isMaximized();
        m_wideSize = m_wideMaximized ? normalGeometry().size() : size();
    }
    QSettings settings;
    settings.remove(QStringLiteral("window/geometry"));
    settings.setValue(QStringLiteral("window/size"), m_wideSize);
    settings.setValue(QStringLiteral("window/maximized"), m_wideMaximized);
    hide();
    for (const Tab &t : std::as_const(m_tabs))
        t.session->shutdown();
    QWidget::closeEvent(event);
}
