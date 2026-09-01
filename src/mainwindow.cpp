#include "mainwindow.h"

#include "connectdialog.h"
#include "fileviews.h"
#include "format.h"
#include "ftpworker.h"
#include "pathbar.h"
#include "remotefilemodel.h"
#include "sftpworker.h"
#include "transferpanel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace {
void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QPushButton *iconButton(const QString &glyph, const QString &tooltip)
{
    auto *button = new QPushButton(glyph);
    button->setObjectName(QStringLiteral("iconButton"));
    button->setToolTip(tooltip);
    button->setFixedSize(Theme::ControlHeight, Theme::ControlHeight);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QLineEdit *filterField(const QString &placeholder)
{
    auto *field = new QLineEdit;
    field->setPlaceholderText(placeholder);
    field->setClearButtonEnabled(true);
    field->setFixedWidth(190);
    return field;
}

QWidget *emptyState(const QString &glyph, const QString &title,
                    const QString &text)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(Theme::SpaceXl, Theme::SpaceXl * 2,
                               Theme::SpaceXl, Theme::SpaceXl * 2);
    layout->setSpacing(Theme::SpaceSm);
    layout->addStretch();
    auto *icon = new QLabel(glyph);
    icon->setObjectName(QStringLiteral("emptyStateIcon"));
    icon->setAlignment(Qt::AlignCenter);
    auto *titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("emptyStateTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    auto *textLabel = new QLabel(text);
    textLabel->setObjectName(QStringLiteral("emptyStateText"));
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setWordWrap(true);
    layout->addWidget(icon);
    layout->addWidget(titleLabel);
    layout->addWidget(textLabel);
    layout->addStretch();
    return widget;
}

QString appendName(const QString &directory, const QString &name)
{
    return directory.endsWith(QLatin1Char('/'))
               ? directory + name
               : directory + QLatin1Char('/') + name;
}

class LocalFileSystemModel final : public QFileSystemModel {
public:
    using QFileSystemModel::QFileSystemModel;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
            case NameColumn:
                return MainWindow::tr("Имя");
            case SizeColumn:
                return MainWindow::tr("Размер");
            case TypeColumn:
                return MainWindow::tr("Тип");
            case DateColumn:
                return MainWindow::tr("Изменён");
            default:
                break;
            }
        }
        return QFileSystemModel::headerData(section, orientation, role);
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == Qt::TextAlignmentRole && index.column() == SizeColumn)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        if (role != Qt::DisplayRole || !index.isValid())
            return QFileSystemModel::data(index, role);

        const QFileInfo info = fileInfo(index);
        switch (index.column()) {
        case SizeColumn:
            return info.isDir() ? QStringLiteral("—") : Format::size(info.size());
        case TypeColumn: {
            if (info.isDir())
                return MainWindow::tr("Папка");
            if (info.isSymLink())
                return MainWindow::tr("Ссылка");
            const QString suffix = info.suffix().toUpper();
            return suffix.isEmpty() ? MainWindow::tr("Файл") : suffix;
        }
        case DateColumn:
            return Format::timestamp(info.lastModified());
        default:
            return QFileSystemModel::data(index, role);
        }
    }

private:
    enum Column { NameColumn, SizeColumn, TypeColumn, DateColumn };
};

QString protocolName(Protocol protocol)
{
    return protocol == Protocol::Ftp ? QStringLiteral("FTP")
                                     : QStringLiteral("SFTP");
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    qRegisterMetaType<ConnectionProfile>();
    qRegisterMetaType<RemoteEntries>();
    qRegisterMetaType<TransferStatus>();
    qRegisterMetaType<QVector<TransferJob>>();

    Theme::setMode(Theme::loadSavedMode());
    buildUi();
    applyTheme();
    createWorkers(m_workerProtocol);

    setConnectionState(false);
    QTimer::singleShot(0, this, [this] {
        const auto saved = ConnectDialog::lastProfile();
        if (!saved)
            return;
        m_activeProfile = *saved;
        ensureWorkers(m_activeProfile.protocol);
        m_connectButton->setEnabled(false);
        m_connectionLabel->setText(tr("●  Подключение…"));
        showNotice(tr("Восстанавливаю сохранённую сессию %1…")
                       .arg(protocolName(m_activeProfile.protocol)),
                   Notice::Info);
        emit connectRequested(m_activeProfile);
    });
}

MainWindow::~MainWindow()
{
    releaseWorkers();
}

void MainWindow::buildUi()
{
    resize(1420, 860);
    setMinimumSize(1080, 660);
    setWindowTitle(QStringLiteral("Nexa"));

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("appRoot"));
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildSidebar());

    auto *workspace = new QWidget;
    workspace->setObjectName(QStringLiteral("workspace"));
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(Theme::SpaceXl, Theme::SpaceXl,
                                        Theme::SpaceXl, Theme::SpaceLg);
    workspaceLayout->setSpacing(Theme::SpaceLg);

    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(Theme::SpaceSm);
    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(2);
    auto *workspaceTitle = new QLabel(tr("Файлы и сервер"));
    workspaceTitle->setObjectName(QStringLiteral("workspaceTitle"));
    auto *workspaceSubtitle = new QLabel(
        tr("Перетаскивайте объекты между панелями — передача начнётся "
           "автоматически"));
    workspaceSubtitle->setObjectName(QStringLiteral("workspaceSubtitle"));
    titleLayout->addWidget(workspaceTitle);
    titleLayout->addWidget(workspaceSubtitle);
    topBar->addLayout(titleLayout);
    topBar->addStretch();

    m_themeButton = iconButton(QString(), tr("Сменить тему (Ctrl+T)"));
    m_disconnectButton = new QPushButton(tr("Отключиться"));
    m_disconnectButton->setObjectName(QStringLiteral("secondaryButton"));
    m_connectButton = new QPushButton(tr("＋  Подключиться"));
    m_connectButton->setObjectName(QStringLiteral("primaryButton"));
    m_disconnectButton->setMinimumHeight(Theme::ControlHeight + 4);
    m_connectButton->setMinimumHeight(Theme::ControlHeight + 4);
    topBar->addWidget(m_themeButton);
    topBar->addWidget(m_disconnectButton);
    topBar->addWidget(m_connectButton);
    workspaceLayout->addLayout(topBar);

    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(m_connectButton, &QPushButton::clicked,
            this, &MainWindow::showConnectDialog);
    connect(m_disconnectButton, &QPushButton::clicked, this, [this] {
        emit disconnectRequested();
    });

    m_notification = new QFrame;
    m_notification->setObjectName(QStringLiteral("notificationBanner"));
    m_notification->setVisible(false);
    auto *noticeLayout = new QHBoxLayout(m_notification);
    noticeLayout->setContentsMargins(Theme::SpaceLg, Theme::SpaceMd,
                                     Theme::SpaceMd, Theme::SpaceMd);
    noticeLayout->setSpacing(Theme::SpaceMd);
    auto *noticeText = new QVBoxLayout;
    noticeText->setSpacing(2);
    m_notificationText = new QLabel;
    m_notificationText->setObjectName(QStringLiteral("notificationText"));
    m_notificationText->setWordWrap(true);
    m_notificationDetail = new QLabel;
    m_notificationDetail->setObjectName(QStringLiteral("notificationDetail"));
    m_notificationDetail->setWordWrap(true);
    m_notificationDetail->setVisible(false);
    noticeText->addWidget(m_notificationText);
    noticeText->addWidget(m_notificationDetail);
    noticeLayout->addLayout(noticeText, 1);
    auto *dismiss = iconButton(QStringLiteral("✕"), tr("Скрыть"));
    dismiss->setFixedSize(26, 26);
    noticeLayout->addWidget(dismiss, 0, Qt::AlignTop);
    connect(dismiss, &QPushButton::clicked, this,
            [this] { m_notification->setVisible(false); });
    workspaceLayout->addWidget(m_notification);

    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setSingleShot(true);
    connect(m_notificationTimer, &QTimer::timeout, this,
            [this] { m_notification->setVisible(false); });

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildFilesPage());

    m_transferPanel = new TransferPanel;
    auto *transferPage = new QWidget;
    transferPage->setObjectName(QStringLiteral("workspacePage"));
    auto *transferLayout = new QVBoxLayout(transferPage);
    transferLayout->setContentsMargins(0, 0, 0, 0);
    transferLayout->addWidget(m_transferPanel);
    m_pages->addWidget(transferPage);
    workspaceLayout->addWidget(m_pages, 1);

    rootLayout->addWidget(workspace, 1);
    setCentralWidget(root);

    m_statusLeft = new QLabel(tr("Готово"));
    statusBar()->addWidget(m_statusLeft, 1);
    m_progress = new QProgressBar;
    m_progress->setFixedWidth(190);
    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_progress);

    connect(m_transferPanel, &TransferPanel::cancelAllRequested, this, [this] {
        m_transferWorker->cancelAllJobs();
        showNotice(tr("Отмена активных передач…"), Notice::Warning);
    });
    connect(m_transferPanel, &TransferPanel::cancelRequested, this,
            [this](quint64 id) { m_transferWorker->cancelJob(id); });
    connect(m_transferPanel, &TransferPanel::clearFinishedRequested, this,
            [this] { m_transferPanel->model()->removeFinished(); });
    connect(m_transferPanel, &TransferPanel::revealRequested, this,
            [](const TransferStatus &status) {
                const QString directory =
                    QFileInfo(status.localPath).absolutePath();
                QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
            });
    connect(m_transferPanel->model(), &TransferModel::summaryChanged,
            this, &MainWindow::updateTransferSummary);
    connect(m_transferPanel, &TransferPanel::overwritePolicyChanged, this,
            [this](OverwritePolicy policy) {
                m_transferWorker->setOverwritePolicy(policy);
            });

    const auto shortcut = [this](QKeySequence sequence, auto slot) {
        auto *action = new QShortcut(sequence, this);
        connect(action, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence(Qt::Key_F5), &MainWindow::refreshRemote);
    shortcut(QKeySequence(Qt::Key_F2), &MainWindow::renameRemoteSelected);
    shortcut(QKeySequence(Qt::Key_Delete), &MainWindow::removeRemoteSelected);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_L),
             [this] { m_remotePathBar->beginEdit(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L),
             [this] { m_localPathBar->beginEdit(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_F),
             [this] { m_remoteFilter->setFocus(); });
    shortcut(QKeySequence(Qt::ALT | Qt::Key_Left),
             [this] { m_remotePathBar->goBack(); });
    shortcut(QKeySequence(Qt::ALT | Qt::Key_Right),
             [this] { m_remotePathBar->goForward(); });
    shortcut(QKeySequence(Qt::ALT | Qt::Key_Up),
             [this] { m_remotePathBar->goUp(); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_T), &MainWindow::toggleTheme);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_1), [this] { setActivePage(0); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_2), [this] { setActivePage(1); });
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(224);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(Theme::SpaceLg, Theme::SpaceXl, Theme::SpaceLg,
                               Theme::SpaceLg);
    layout->setSpacing(Theme::SpaceSm);

    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(Theme::SpaceMd);
    auto *brandMark = new QLabel(QStringLiteral("N"));
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setAlignment(Qt::AlignCenter);
    brandMark->setFixedSize(38, 38);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto *brand = new QLabel(QStringLiteral("Nexa"));
    brand->setObjectName(QStringLiteral("brandTitle"));
    auto *edition = new QLabel(tr("SFTP и FTP"));
    edition->setObjectName(QStringLiteral("brandSubtitle"));
    brandText->addWidget(brand);
    brandText->addWidget(edition);
    brandRow->addWidget(brandMark);
    brandRow->addLayout(brandText);
    brandRow->addStretch();
    layout->addLayout(brandRow);
    layout->addSpacing(Theme::SpaceXl);

    auto *caption = new QLabel(tr("РАБОЧЕЕ ПРОСТРАНСТВО"));
    caption->setObjectName(QStringLiteral("sectionCaption"));
    layout->addWidget(caption);

    const QStringList navigationTitles{tr("  ▣   Файлы"), tr("  ⇅   Передачи")};
    for (int i = 0; i < navigationTitles.size(); ++i) {
        auto *button = new QPushButton(navigationTitles.at(i));
        button->setObjectName(QStringLiteral("navigationButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("active", i == 0);
        connect(button, &QPushButton::clicked, this,
                [this, i] { setActivePage(i); });
        m_navigationButtons.append(button);
        if (i == 1) {
            auto *row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(Theme::SpaceXs);
            row->addWidget(button, 1);
            m_navigationBadge = new QLabel;
            m_navigationBadge->setObjectName(QStringLiteral("navigationBadge"));
            m_navigationBadge->setVisible(false);
            row->addWidget(m_navigationBadge);
            layout->addLayout(row);
        } else {
            layout->addWidget(button);
        }
    }
    layout->addStretch();

    auto *connectionCard = new QFrame;
    connectionCard->setObjectName(QStringLiteral("connectionCard"));
    auto *connectionLayout = new QVBoxLayout(connectionCard);
    connectionLayout->setContentsMargins(Theme::SpaceMd, Theme::SpaceMd,
                                         Theme::SpaceMd, Theme::SpaceMd);
    connectionLayout->setSpacing(Theme::SpaceXs);
    auto *connectionCaption = new QLabel(tr("СОЕДИНЕНИЕ"));
    connectionCaption->setObjectName(QStringLiteral("sectionCaption"));
    m_connectionLabel = new QLabel(tr("●  Не подключено"));
    m_connectionLabel->setObjectName(QStringLiteral("connectionLabel"));
    m_connectionDetail = new QLabel(tr("Сервер не выбран"));
    m_connectionDetail->setObjectName(QStringLiteral("connectionDetail"));
    m_connectionDetail->setWordWrap(true);
    connectionLayout->addWidget(connectionCaption);
    connectionLayout->addWidget(m_connectionLabel);
    connectionLayout->addWidget(m_connectionDetail);
    layout->addWidget(connectionCard);

    auto *version = new QLabel(QStringLiteral("Nexa %1")
                                   .arg(QApplication::applicationVersion()));
    version->setObjectName(QStringLiteral("versionLabel"));
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);
    return sidebar;
}

QWidget *MainWindow::buildFilesPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("workspacePage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter;
    splitter->setObjectName(QStringLiteral("fileSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(buildLocalPanel());
    splitter->addWidget(buildRemotePanel());
    splitter->setSizes({580, 580});
    layout->addWidget(splitter, 1);
    return page;
}

QWidget *MainWindow::buildLocalPanel()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("fileCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("panelHeader"));
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::SpaceLg, Theme::SpaceLg,
                                     Theme::SpaceLg, Theme::SpaceMd);
    headerLayout->setSpacing(Theme::SpaceSm);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(Theme::SpaceSm);
    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(0);
    auto *title = new QLabel(tr("Этот компьютер"));
    title->setObjectName(QStringLiteral("panelTitle"));
    m_localMeta = new QLabel;
    m_localMeta->setObjectName(QStringLiteral("panelMeta"));
    titleColumn->addWidget(title);
    titleColumn->addWidget(m_localMeta);
    titleRow->addLayout(titleColumn);
    titleRow->addStretch();
    m_localFilter = filterField(tr("Фильтр по имени"));
    titleRow->addWidget(m_localFilter);
    headerLayout->addLayout(titleRow);

    auto *navigationRow = new QHBoxLayout;
    navigationRow->setSpacing(Theme::SpaceXs);
    m_localBack = iconButton(QStringLiteral("‹"), tr("Назад"));
    m_localForward = iconButton(QStringLiteral("›"), tr("Вперёд"));
    m_localUp = iconButton(QStringLiteral("↑"), tr("На уровень выше"));
    auto *localHome = iconButton(QStringLiteral("⌂"), tr("Домашняя папка"));
    m_localPathBar = new PathBar(PathBar::Style::Local);
    navigationRow->addWidget(m_localBack);
    navigationRow->addWidget(m_localForward);
    navigationRow->addWidget(m_localUp);
    navigationRow->addWidget(localHome);
    navigationRow->addWidget(m_localPathBar, 1);
    headerLayout->addLayout(navigationRow);
    cardLayout->addWidget(header);

    m_localModel = new LocalFileSystemModel(this);
    m_localModel->setOption(QFileSystemModel::DontWatchForChanges, true);
    m_localModel->setOption(QFileSystemModel::DontResolveSymlinks, true);
    m_localModel->setOption(QFileSystemModel::DontUseCustomDirectoryIcons, true);
    m_localModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_localModel->setNameFilterDisables(false);
    m_localModel->setRootPath(QDir::homePath());

    m_localView = new LocalFileView;
    m_localView->setModel(m_localModel);
    m_localView->setRootIndex(m_localModel->index(QDir::homePath()));
    m_localView->setSortingEnabled(true);
    m_localView->sortByColumn(0, Qt::AscendingOrder);
    m_localView->setAlternatingRowColors(true);
    m_localView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_localView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_localView->setUniformRowHeights(true);

    m_localView->setRootIsDecorated(false);
    m_localView->setItemsExpandable(false);
    m_localView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_localView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_localView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_localView->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    cardLayout->addWidget(m_localView, 1);

    m_localPathBar->setPath(QDir::homePath());

    connect(m_localView, &QTreeView::doubleClicked,
            this, &MainWindow::openLocal);

    connect(m_localModel, &QFileSystemModel::directoryLoaded, this,
            [this](const QString &path) {
                if (QDir(path) == QDir(m_localPathBar->path()))
                    refreshPanelMeta();
            });
    connect(m_localPathBar, &PathBar::pathActivated,
            this, &MainWindow::navigateLocal);
    connect(m_localPathBar, &PathBar::historyChanged,
            this, &MainWindow::refreshNavigationButtons);
    connect(m_localBack, &QPushButton::clicked,
            m_localPathBar, &PathBar::goBack);
    connect(m_localForward, &QPushButton::clicked,
            m_localPathBar, &PathBar::goForward);
    connect(m_localUp, &QPushButton::clicked, m_localPathBar, &PathBar::goUp);
    connect(localHome, &QPushButton::clicked, this,
            [this] { navigateLocal(QDir::homePath()); });
    connect(m_localFilter, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                m_localModel->setNameFilters(
                    text.trimmed().isEmpty()
                        ? QStringList()
                        : QStringList{QStringLiteral("*%1*").arg(text.trimmed())});
            });
    connect(m_localView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
                QMenu menu(this);
                auto *upload = menu.addAction(tr("Загрузить на сервер"));
                upload->setEnabled(
                    m_connected &&
                    !m_localView->selectionModel()->selectedRows().isEmpty());
                connect(upload, &QAction::triggered,
                        this, &MainWindow::uploadSelected);
                menu.addSeparator();
                auto *reveal = menu.addAction(tr("Открыть в Проводнике"));
                connect(reveal, &QAction::triggered, this, [this] {
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(m_localPathBar->path()));
                });
                menu.exec(m_localView->viewport()->mapToGlobal(position));
            });
    connect(m_localView, &LocalFileView::remoteDataDropped, this,
            [this](const QByteArray &encodedEntries,
                   const QModelIndex &targetIndex) {
                if (!m_connected)
                    return;
                QString destination = m_localPathBar->path();
                if (targetIndex.isValid()) {
                    const QFileInfo target = m_localModel->fileInfo(targetIndex);
                    if (target.isDir())
                        destination = target.absoluteFilePath();
                }
                QMimeData mimeData;
                mimeData.setData(RemoteFileModel::remoteMimeType(),
                                 encodedEntries);
                const RemoteEntries entries =
                    RemoteFileModel::entriesFromMimeData(&mimeData);
                QVector<TransferJob> jobs;
                jobs.reserve(entries.size());
                for (const RemoteEntry &entry : entries)
                    jobs.append(downloadJobFor(entry, destination));
                enqueueJobs(jobs);
            });
    return card;
}

QWidget *MainWindow::buildRemotePanel()
{
    m_remoteCard = new QFrame;
    m_remoteCard->setObjectName(QStringLiteral("fileCard"));
    auto *cardLayout = new QVBoxLayout(m_remoteCard);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("panelHeader"));
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::SpaceLg, Theme::SpaceLg,
                                     Theme::SpaceLg, Theme::SpaceMd);
    headerLayout->setSpacing(Theme::SpaceSm);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(Theme::SpaceSm);
    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(0);
    auto *title = new QLabel(tr("Удалённый сервер"));
    title->setObjectName(QStringLiteral("panelTitle"));
    m_remoteMeta = new QLabel;
    m_remoteMeta->setObjectName(QStringLiteral("panelMeta"));
    titleColumn->addWidget(title);
    titleColumn->addWidget(m_remoteMeta);
    titleRow->addLayout(titleColumn);
    titleRow->addStretch();
    m_remoteFilter = filterField(tr("Фильтр по имени"));
    titleRow->addWidget(m_remoteFilter);
    headerLayout->addLayout(titleRow);

    auto *navigationRow = new QHBoxLayout;
    navigationRow->setSpacing(Theme::SpaceXs);
    m_remoteBack = iconButton(QStringLiteral("‹"), tr("Назад"));
    m_remoteForward = iconButton(QStringLiteral("›"), tr("Вперёд"));
    m_remoteUp = iconButton(QStringLiteral("↑"), tr("На уровень выше"));
    auto *remoteRefresh = iconButton(QStringLiteral("⟳"), tr("Обновить (F5)"));
    auto *remoteEdit = iconButton(QStringLiteral("⌨"), tr("Ввести путь (Ctrl+L)"));
    m_remotePathBar = new PathBar(PathBar::Style::Remote);
    navigationRow->addWidget(m_remoteBack);
    navigationRow->addWidget(m_remoteForward);
    navigationRow->addWidget(m_remoteUp);
    navigationRow->addWidget(remoteRefresh);
    navigationRow->addWidget(m_remotePathBar, 1);
    navigationRow->addWidget(remoteEdit);
    headerLayout->addLayout(navigationRow);
    cardLayout->addWidget(header);

    m_remoteModel = new RemoteFileModel(this);
    m_remoteProxy = new RemoteSortFilterProxy(this);
    m_remoteProxy->setSourceModel(m_remoteModel);

    m_remoteView = new RemoteFileView;
    m_remoteView->setModel(m_remoteProxy);
    m_remoteView->setAlternatingRowColors(true);
    m_remoteView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_remoteView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_remoteView->setShowGrid(false);
    m_remoteView->setSortingEnabled(true);
    m_remoteView->sortByColumn(RemoteFileModel::NameColumn, Qt::AscendingOrder);
    m_remoteView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_remoteView->setWordWrap(false);
    m_remoteView->verticalHeader()->hide();
    m_remoteView->verticalHeader()->setDefaultSectionSize(Theme::RowHeight);
    m_remoteView->horizontalHeader()->setStretchLastSection(false);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        RemoteFileModel::NameColumn, QHeaderView::Stretch);
    for (int column = RemoteFileModel::SizeColumn;
         column < RemoteFileModel::ColumnCount; ++column) {
        m_remoteView->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    cardLayout->addWidget(m_remoteView, 1);

    m_remoteEmptyState = emptyState(
        QStringLiteral("◈"), tr("Нет подключения"),
        tr("Нажмите «Подключиться», чтобы открыть сессию SFTP или FTP. "
           "Отпечаток ключа или сертификата сервера будет показан для "
           "проверки."));
    cardLayout->addWidget(m_remoteEmptyState, 1);
    m_remoteView->setVisible(false);

    connect(m_remoteView, &QTableView::doubleClicked,
            this, &MainWindow::openRemote);
    connect(m_remotePathBar, &PathBar::pathActivated,
            this, &MainWindow::navigateRemote);
    connect(m_remotePathBar, &PathBar::historyChanged,
            this, &MainWindow::refreshNavigationButtons);
    connect(m_remoteBack, &QPushButton::clicked,
            m_remotePathBar, &PathBar::goBack);
    connect(m_remoteForward, &QPushButton::clicked,
            m_remotePathBar, &PathBar::goForward);
    connect(m_remoteUp, &QPushButton::clicked, m_remotePathBar, &PathBar::goUp);
    connect(remoteRefresh, &QPushButton::clicked,
            this, &MainWindow::refreshRemote);
    connect(remoteEdit, &QPushButton::clicked,
            m_remotePathBar, &PathBar::beginEdit);
    connect(m_remoteFilter, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                m_remoteProxy->setNameFilter(text);
                refreshPanelMeta();
            });
    connect(m_remoteView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &position) {
                const QModelIndex targetIndex = m_remoteView->indexAt(position);
                const QString destination = remoteDestinationFor(targetIndex);
                const int selected = static_cast<int>(
                    m_remoteView->selectionModel()->selectedRows().size());

                QMenu menu(this);
                auto *download = menu.addAction(tr("Скачать"));
                auto *uploadFiles = menu.addAction(tr("Загрузить файлы сюда…"));
                auto *uploadFolder = menu.addAction(tr("Загрузить папку сюда…"));
                menu.addSeparator();
                auto *newFile = menu.addAction(tr("Создать файл"));
                auto *newFolder = menu.addAction(tr("Создать папку"));
                menu.addSeparator();
                auto *rename = menu.addAction(tr("Переименовать"));
                auto *remove = menu.addAction(tr("Удалить"));
                menu.addSeparator();
                auto *copyPath = menu.addAction(tr("Копировать путь"));
                auto *refresh = menu.addAction(tr("Обновить"));

                download->setEnabled(m_connected && selected > 0);
                uploadFiles->setEnabled(m_connected);
                uploadFolder->setEnabled(m_connected);
                newFile->setEnabled(m_connected);
                newFolder->setEnabled(m_connected);
                rename->setEnabled(m_connected && selected == 1);
                remove->setEnabled(m_connected && selected > 0);
                copyPath->setEnabled(selected > 0);
                refresh->setEnabled(m_connected);

                connect(download, &QAction::triggered,
                        this, &MainWindow::downloadSelected);
                connect(uploadFiles, &QAction::triggered, this,
                        [this, destination] {
                            const QStringList files =
                                QFileDialog::getOpenFileNames(
                                    this, tr("Выберите файлы для загрузки"),
                                    m_localPathBar->path());
                            QVector<TransferJob> jobs;
                            for (const QString &file : files) {
                                const QFileInfo info(file);
                                TransferJob job;
                                job.id = m_nextJobId++;
                                job.upload = true;
                                job.localPath = info.absoluteFilePath();
                                job.remotePath =
                                    appendName(destination, info.fileName());
                                jobs.append(job);
                            }
                            enqueueJobs(jobs);
                        });
                connect(uploadFolder, &QAction::triggered, this,
                        [this, destination] {
                            const QString folder =
                                QFileDialog::getExistingDirectory(
                                    this, tr("Выберите папку для загрузки"),
                                    m_localPathBar->path());
                            if (folder.isEmpty())
                                return;
                            const QFileInfo info(folder);
                            TransferJob job;
                            job.id = m_nextJobId++;
                            job.upload = true;
                            job.directory = true;
                            job.localPath = info.absoluteFilePath();
                            job.remotePath =
                                appendName(destination, info.fileName());
                            enqueueJobs({job});
                        });
                connect(newFile, &QAction::triggered,
                        this, &MainWindow::createRemoteFile);
                connect(newFolder, &QAction::triggered,
                        this, &MainWindow::createRemoteFolder);
                connect(rename, &QAction::triggered,
                        this, &MainWindow::renameRemoteSelected);
                connect(remove, &QAction::triggered,
                        this, &MainWindow::removeRemoteSelected);
                connect(copyPath, &QAction::triggered, this, [this] {
                    QStringList paths;
                    for (const RemoteEntry &entry : selectedRemoteEntries())
                        paths.append(entry.path);
                    QApplication::clipboard()->setText(
                        paths.join(QLatin1Char('\n')));
                });
                connect(refresh, &QAction::triggered,
                        this, &MainWindow::refreshRemote);
                menu.exec(m_remoteView->viewport()->mapToGlobal(position));
            });
    connect(m_remoteView, &RemoteFileView::localUrlsDropped, this,
            [this](const QList<QUrl> &urls, const QModelIndex &targetIndex) {
                if (!m_connected) {
                    showNotice(tr("Сначала подключитесь к серверу."),
                               Notice::Warning);
                    return;
                }
                const QString destination = remoteDestinationFor(targetIndex);
                QVector<TransferJob> jobs;
                jobs.reserve(urls.size());
                for (const QUrl &url : urls) {
                    if (!url.isLocalFile())
                        continue;
                    const QFileInfo info(url.toLocalFile());
                    if (!info.exists())
                        continue;
                    TransferJob job;
                    job.id = m_nextJobId++;
                    job.upload = true;
                    job.localPath = info.absoluteFilePath();
                    job.remotePath = appendName(destination, info.fileName());
                    job.directory = info.isDir() && !info.isSymLink();
                    jobs.append(job);
                }
                enqueueJobs(jobs);
            });
    return m_remoteCard;
}

void MainWindow::createWorkers(Protocol protocol)
{
    const auto makeWorker = [protocol](RemoteWorker::Role role) -> RemoteWorker * {
        if (protocol == Protocol::Ftp)
            return new FtpWorker(role);
        return new SftpWorker(role);
    };

    m_workerProtocol = protocol;

    m_browseThread = new QThread(this);
    m_browseWorker = makeWorker(RemoteWorker::Role::Browsing);
    m_browseWorker->moveToThread(m_browseThread);

    m_transferThread = new QThread(this);
    m_transferWorker = makeWorker(RemoteWorker::Role::Transfers);
    m_transferWorker->moveToThread(m_transferThread);

    connect(this, &MainWindow::connectRequested,
            m_browseWorker, &RemoteWorker::connectToHost);
    connect(this, &MainWindow::hostKeyConfirmed,
            m_browseWorker, &RemoteWorker::confirmHostKey);
    connect(this, &MainWindow::remoteListRequested,
            m_browseWorker, &RemoteWorker::listDirectory);
    connect(this, &MainWindow::remoteMkdirRequested,
            m_browseWorker, &RemoteWorker::createDirectory);
    connect(this, &MainWindow::remoteFileCreateRequested,
            m_browseWorker, &RemoteWorker::createFile);
    connect(this, &MainWindow::remoteRemoveRequested,
            m_browseWorker, &RemoteWorker::removeEntry);
    connect(this, &MainWindow::remoteRenameRequested,
            m_browseWorker, &RemoteWorker::renameEntry);
    connect(this, &MainWindow::disconnectRequested,
            m_browseWorker, &RemoteWorker::disconnectFromHost);
    connect(this, &MainWindow::disconnectRequested,
            m_transferWorker, &RemoteWorker::disconnectFromHost);
    connect(this, &MainWindow::transferConnectRequested,
            m_transferWorker, &RemoteWorker::connectToHost);
    connect(this, &MainWindow::transfersRequested,
            m_transferWorker, &RemoteWorker::enqueueTransfers);

    connect(m_browseWorker, &RemoteWorker::connected,
            this, &MainWindow::onConnected);
    connect(m_browseWorker, &RemoteWorker::disconnected,
            this, &MainWindow::onDisconnected);
    connect(m_browseWorker, &RemoteWorker::hostKeyConfirmationRequired,
            this, &MainWindow::onHostKeyRequired);
    connect(m_browseWorker, &RemoteWorker::directoryListed,
            this, &MainWindow::onRemoteListed);
    connect(m_browseWorker, &RemoteWorker::directoryBatchReceived,
            this, &MainWindow::onRemoteBatchReceived);
    connect(m_browseWorker, &RemoteWorker::errorOccurred,
            this, &MainWindow::onError);
    connect(m_transferWorker, &RemoteWorker::errorOccurred,
            this, &MainWindow::onError);

    connect(m_browseWorker, &RemoteWorker::performanceMetric, this,
            [this](const QString &operation, qint64 milliseconds) {
                m_statusLeft->setText(
                    tr("%1 — %2 мс").arg(operation).arg(milliseconds));
            });
    connect(m_browseWorker, &RemoteWorker::deletionProgress, this,
            [this](qint64 count, const QString &path) {
                m_statusLeft->setText(
                    tr("Удаление… обработано: %1 — %2").arg(count).arg(path));
            });
    connect(m_browseWorker, &RemoteWorker::reconnecting, this, [this] {
        m_connectionLabel->setText(tr("●  Переподключение…"));
        m_connectionLabel->setStyleSheet(
            QStringLiteral("color:%1;").arg(Theme::palette().warning.name()));
        showNotice(tr("Соединение потеряно. Переподключаюсь автоматически…"),
                   Notice::Warning);
    });
    connect(m_browseWorker, &RemoteWorker::reconnected, this, [this] {
        setConnectionState(true);
        showNotice(tr("Соединение восстановлено."), Notice::Success);
    });

    connect(m_transferWorker, &RemoteWorker::connected, this,
            [this](const QString &, const QString &, const QString &) {
                m_transferSessionReady = true;
            });
    connect(m_transferWorker, &RemoteWorker::transferQueued, this,
            [this](const TransferStatus &status) {
                m_transferPanel->applyStatus(status);
            });
    connect(m_transferWorker, &RemoteWorker::transferUpdated, this,
            [this](const TransferStatus &status) {
                m_transferPanel->applyStatus(status);
                if (status.state == TransferState::Running) {
                    m_statusLeft->setText(
                        tr("%1 «%2» — %3 из %4 — %5")
                            .arg(status.upload ? tr("Загрузка") : tr("Скачивание"),
                                 status.name, Format::size(status.done),
                                 Format::size(status.total),
                                 Format::speed(status.bytesPerSecond)));
                }
            });
    connect(m_transferWorker, &RemoteWorker::queueDrained, this,
            [this](int completed, int failed, int skipped) {
                m_progress->setVisible(false);
                if (failed > 0) {
                    showNotice(tr("Очередь завершена: успешно %1, с ошибками %2.")
                                   .arg(completed)
                                   .arg(failed),
                               Notice::Error,
                               tr("Подробности — на вкладке «Передачи»."));
                } else if (completed > 0) {
                    showNotice(tr("Передачи завершены: %1.").arg(completed),
                               Notice::Success);
                } else if (skipped > 0) {
                    showNotice(tr("Передачи отменены или пропущены: %1.")
                                   .arg(skipped),
                               Notice::Info);
                }
                if (m_connected) {
                    requestRemoteDirectory(m_currentRemotePath, false);
                    m_localModel->setRootPath(m_localPathBar->path());
                }
            });

    m_transferWorker->setOverwritePolicy(m_transferPanel->overwritePolicy());

    m_browseThread->start();
    m_transferThread->start();
}

void MainWindow::releaseWorkers()
{
    const QVector<QPair<QThread *, RemoteWorker *>> sessions{
        {m_transferThread, m_transferWorker},
        {m_browseThread, m_browseWorker}};
    for (const auto &[thread, worker] : sessions) {
        if (!thread)
            continue;
        if (worker)
            worker->requestStop();
        thread->quit();

        if (!thread->wait(5000)) {
            thread->terminate();
            thread->wait();
        }
        delete worker;
        delete thread;
    }
    m_transferWorker = nullptr;
    m_browseWorker = nullptr;
    m_transferThread = nullptr;
    m_browseThread = nullptr;
}

void MainWindow::ensureWorkers(Protocol protocol)
{
    if (m_browseWorker && m_transferWorker && m_workerProtocol == protocol)
        return;
    releaseWorkers();
    createWorkers(protocol);
}

void MainWindow::showConnectDialog()
{
    ConnectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_activeProfile = dialog.profile();
    ensureWorkers(m_activeProfile.protocol);
    m_connectButton->setEnabled(false);
    m_connectionLabel->setText(tr("●  Подключение…"));
    m_connectionDetail->setText(QStringLiteral("%1@%2:%3")
                                    .arg(m_activeProfile.username,
                                         m_activeProfile.host)
                                    .arg(m_activeProfile.port));
    emit connectRequested(m_activeProfile);
}

void MainWindow::onConnected(const QString &homePath, const QString &banner,
                             const QString &fingerprint)
{
    setConnectionState(true);
    m_activeProfile.trustedFingerprint = fingerprint;
    m_transferSessionReady = false;

    emit transferConnectRequested(m_activeProfile);

    m_connectionDetail->setText(
        banner.isEmpty()
            ? QStringLiteral("%1@%2:%3")
                  .arg(m_activeProfile.username, m_activeProfile.host)
                  .arg(m_activeProfile.port)
            : banner);

    QString restored =
        QSettings().value(QStringLiteral("session/remotePath"), homePath)
            .toString();
    if (restored.isEmpty())
        restored = homePath;
    showNotice(tr("Подключение %1 установлено.")
                   .arg(protocolName(m_activeProfile.protocol)),
               Notice::Success);
    requestRemoteDirectory(restored, false);
}

void MainWindow::onDisconnected()
{
    if (!m_connected && m_directoryCache.isEmpty())
        return;
    setConnectionState(false);
    m_transferSessionReady = false;
    m_directoryCache.clear();
    m_remoteModel->clear();
    refreshPanelMeta();
    showNotice(tr("Соединение закрыто."), Notice::Info);
}

void MainWindow::onHostKeyRequired(const QString &host,
                                   const QString &fingerprint,
                                   const QString &keyType, bool changed)
{
    const bool ftp = m_activeProfile.protocol == Protocol::Ftp;
    QMessageBox box(this);
    box.setIcon(changed ? QMessageBox::Critical : QMessageBox::Question);
    box.setWindowTitle(changed ? (ftp ? tr("Сертификат сервера изменился")
                                      : tr("Ключ сервера изменился"))
                               : tr("Новый сервер"));
    box.setText(
        changed ? (ftp ? tr("Внимание: сертификат сервера %1 отличается от "
                            "сохранённого. Это может указывать на атаку. "
                            "Продолжайте, только если вы ожидали его смену.")
                             .arg(host)
                       : tr("Внимание: ключ сервера %1 отличается от "
                            "сохранённого. Это может указывать на атаку. "
                            "Продолжайте, только если вы ожидали смену ключа.")
                             .arg(host))
                : tr("Подтвердите подлинность сервера %1.").arg(host));
    box.setInformativeText(keyType.isEmpty()
                               ? tr("SHA-256 отпечаток:\n%1").arg(fingerprint)
                               : tr("Тип ключа: %1\nSHA-256 отпечаток:\n%2")
                                     .arg(keyType, fingerprint));
    auto *once = box.addButton(tr("Подключиться один раз"),
                               QMessageBox::AcceptRole);
    auto *remember = box.addButton(tr("Принять и запомнить"),
                                   QMessageBox::YesRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == remember)
        emit hostKeyConfirmed(true, true);
    else if (box.clickedButton() == once)
        emit hostKeyConfirmed(true, false);
    else
        emit hostKeyConfirmed(false, false);
}

void MainWindow::setConnectionState(bool connected)
{
    m_connected = connected;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_remotePathBar->setEnabled(connected);
    m_remoteFilter->setEnabled(connected);
    m_remoteView->setVisible(connected);
    m_remoteEmptyState->setVisible(!connected);
    m_connectionLabel->setText(connected ? tr("●  Подключено")
                                         : tr("●  Не подключено"));
    m_connectionLabel->setStyleSheet(
        QStringLiteral("color:%1;")
            .arg(connected ? Theme::palette().success.name()
                           : Theme::palette().textMuted.name()));
    if (!connected)
        m_connectionDetail->setText(tr("Сервер не выбран"));
    refreshNavigationButtons();
}

void MainWindow::navigateLocal(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isDir())
        return;
    m_localPathBar->setPath(info.absoluteFilePath());
    m_localModel->setRootPath(info.absoluteFilePath());
    m_localView->setRootIndex(m_localModel->index(info.absoluteFilePath()));
    refreshPanelMeta();
    refreshNavigationButtons();
}

void MainWindow::navigateRemote(const QString &path)
{
    if (m_connected)
        requestRemoteDirectory(path);
}

void MainWindow::openLocal(const QModelIndex &index)
{
    const QFileInfo info = m_localModel->fileInfo(index);
    if (info.isDir())
        navigateLocal(info.absoluteFilePath());
    else
        uploadSelected();
}

void MainWindow::openRemote(const QModelIndex &index)
{
    const RemoteEntry *entry = remoteEntryAt(index);
    if (!entry)
        return;
    if (entry->directory)
        requestRemoteDirectory(entry->path);
    else
        downloadSelected();
}

void MainWindow::requestRemoteDirectory(const QString &path, bool useCache)
{
    QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!cleanPath.startsWith(QLatin1Char('/')))
        cleanPath.prepend(QLatin1Char('/'));

    m_currentRemotePath = cleanPath;
    m_remotePathBar->setPath(cleanPath);
    if (m_connected)
        QSettings().setValue(QStringLiteral("session/remotePath"), cleanPath);

    const quint64 generation = m_browseWorker->beginDirectoryRequest();
    const auto cached = m_directoryCache.constFind(cleanPath);
    if (useCache && cached != m_directoryCache.cend()) {
        m_showingCachedDirectory = true;
        m_remoteModel->setEntries(cached.value());
        m_statusLeft->setText(tr("Каталог показан из кэша, идёт обновление…"));
    } else {
        m_showingCachedDirectory = false;
        m_remoteModel->clear();
        m_statusLeft->setText(tr("Загрузка каталога…"));
    }
    refreshPanelMeta();
    refreshNavigationButtons();
    emit remoteListRequested(cleanPath, generation);
}

void MainWindow::onRemoteListed(const QString &path,
                                const RemoteEntries &entries)
{
    m_directoryCache.insert(path, entries);
    if (path != m_currentRemotePath)
        return;
    m_showingCachedDirectory = false;
    m_remoteModel->setEntries(entries);
    refreshPanelMeta();
    m_statusLeft->setText(tr("Загружено объектов: %1").arg(entries.size()));
}

void MainWindow::onRemoteBatchReceived(const QString &path,
                                       const RemoteEntries &entries)
{
    if (path != m_currentRemotePath || m_showingCachedDirectory)
        return;
    m_remoteModel->appendEntries(entries);
    refreshPanelMeta();
}

void MainWindow::refreshRemote()
{
    if (!m_connected)
        return;
    m_directoryCache.remove(m_currentRemotePath);
    requestRemoteDirectory(m_currentRemotePath, false);
}

void MainWindow::refreshNavigationButtons()
{
    m_localBack->setEnabled(m_localPathBar->canGoBack());
    m_localForward->setEnabled(m_localPathBar->canGoForward());
    m_localUp->setEnabled(m_localPathBar->parentPath() !=
                          m_localPathBar->path());
    m_remoteBack->setEnabled(m_connected && m_remotePathBar->canGoBack());
    m_remoteForward->setEnabled(m_connected && m_remotePathBar->canGoForward());
    m_remoteUp->setEnabled(m_connected &&
                           m_currentRemotePath != QStringLiteral("/"));
}

void MainWindow::refreshPanelMeta()
{
    const int shown = m_remoteProxy->rowCount();
    const int total = m_remoteModel->rowCount();
    qint64 bytes = 0;
    for (const RemoteEntry &entry : m_remoteModel->entries()) {
        if (!entry.directory)
            bytes += entry.size;
    }
    m_remoteMeta->setText(
        shown == total
            ? tr("%1 объектов · %2").arg(total).arg(Format::size(bytes))
            : tr("%1 из %2 объектов").arg(shown).arg(total));

    const QModelIndex localRoot = m_localModel->index(m_localPathBar->path());
    const int localCount = m_localModel->rowCount(localRoot);
    qint64 localBytes = 0;
    for (int row = 0; row < localCount; ++row) {
        const QFileInfo info =
            m_localModel->fileInfo(m_localModel->index(row, 0, localRoot));
        if (!info.isDir())
            localBytes += info.size();
    }
    m_localMeta->setText(
        tr("%1 объектов · %2").arg(localCount).arg(Format::size(localBytes)));
}

void MainWindow::enqueueJobs(const QVector<TransferJob> &jobs)
{
    if (jobs.isEmpty())
        return;
    if (!m_connected) {
        showNotice(tr("Сначала подключитесь к серверу."), Notice::Warning);
        return;
    }
    emit transfersRequested(jobs);
    showNotice(tr("Добавлено в очередь: %1").arg(jobs.size()), Notice::Info);
    updateTransferSummary();
}

void MainWindow::uploadSelected()
{
    if (!m_connected) {
        showNotice(tr("Сначала подключитесь к серверу."), Notice::Warning);
        return;
    }
    const auto rows = m_localView->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        showNotice(tr("Выберите локальные файлы для загрузки."), Notice::Info);
        return;
    }
    QVector<TransferJob> jobs;
    jobs.reserve(rows.size());
    for (const QModelIndex &index : rows) {
        const QFileInfo info = m_localModel->fileInfo(index);
        TransferJob job;
        job.id = m_nextJobId++;
        job.upload = true;
        job.localPath = info.absoluteFilePath();
        job.remotePath = appendName(m_currentRemotePath, info.fileName());
        job.directory = info.isDir() && !info.isSymLink();
        jobs.append(job);
    }
    enqueueJobs(jobs);
}

void MainWindow::downloadSelected()
{
    if (!m_connected)
        return;
    const RemoteEntries entries = selectedRemoteEntries();
    if (entries.isEmpty())
        return;
    const QString destination = m_localPathBar->path();
    QVector<TransferJob> jobs;
    jobs.reserve(entries.size());
    for (const RemoteEntry &entry : entries)
        jobs.append(downloadJobFor(entry, destination));
    enqueueJobs(jobs);
}

TransferJob MainWindow::downloadJobFor(const RemoteEntry &entry,
                                       const QString &destinationDirectory)
{
    TransferJob job;
    job.id = m_nextJobId++;
    job.upload = false;
    job.remotePath = entry.path;
    job.localPath = QDir(destinationDirectory).filePath(entry.name);
    job.directory = entry.directory && !entry.symlink;
    job.permissions = entry.permissions;
    if (!job.directory) {
        job.size = entry.size;
        job.modified =
            entry.modified.isValid() ? entry.modified.toSecsSinceEpoch() : 0;
    }
    return job;
}

void MainWindow::updateTransferSummary()
{
    TransferModel *model = m_transferPanel->model();
    const int active = model->activeCount() + model->queuedCount();
    if (m_navigationBadge) {
        m_navigationBadge->setVisible(active > 0);
        m_navigationBadge->setText(QString::number(active));
    }
    const int progress = model->aggregateProgress();
    if (progress < 0) {
        m_progress->setVisible(false);
        return;
    }
    m_progress->setVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(progress);
    m_progress->setFormat(
        tr("%1%  ·  %2").arg(progress).arg(Format::speed(model->aggregateSpeed())));
}

const RemoteEntry *MainWindow::remoteEntryAt(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return nullptr;
    const QModelIndex source = m_remoteProxy->mapToSource(proxyIndex);
    return m_remoteModel->entryAt(source.row());
}

RemoteEntries MainWindow::selectedRemoteEntries() const
{
    RemoteEntries result;
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    result.reserve(rows.size());
    for (const QModelIndex &index : rows) {
        if (const RemoteEntry *entry = remoteEntryAt(index))
            result.append(*entry);
    }
    return result;
}

QString MainWindow::remoteDestinationFor(const QModelIndex &proxyIndex) const
{
    const RemoteEntry *entry = remoteEntryAt(proxyIndex);
    return entry && entry->directory && !entry->symlink ? entry->path
                                                        : m_currentRemotePath;
}

void MainWindow::createRemoteFolder()
{
    if (!m_connected)
        return;
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Новая папка"), tr("Имя папки:"),
                              QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || name.isEmpty() || name.contains(QLatin1Char('/')))
        return;
    emit remoteMkdirRequested(appendName(m_currentRemotePath, name));
}

void MainWindow::createRemoteFile()
{
    if (!m_connected)
        return;
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Новый файл"), tr("Имя файла:"),
                              QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || name.isEmpty() || name.contains(QLatin1Char('/')))
        return;
    emit remoteFileCreateRequested(appendName(m_currentRemotePath, name));
}

void MainWindow::removeRemoteSelected()
{
    if (!m_connected)
        return;
    const RemoteEntries entries = selectedRemoteEntries();
    if (entries.isEmpty())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Подтверждение удаления"));
    box.setText(tr("Удалить выбранные объекты (%1)?").arg(entries.size()));
    QStringList names;
    for (int i = 0; i < entries.size() && i < 8; ++i)
        names.append(entries.at(i).name);
    if (entries.size() > 8)
        names.append(tr("и ещё %1…").arg(entries.size() - 8));
    box.setDetailedText(names.join(QLatin1Char('\n')));
    box.setInformativeText(
        tr("Все файлы внутри выбранных папок также будут удалены. "
           "Это действие нельзя отменить."));
    auto *deleteButton =
        box.addButton(tr("Удалить"), QMessageBox::DestructiveRole);
    box.addButton(tr("Отмена"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != deleteButton)
        return;
    for (const RemoteEntry &entry : entries)
        emit remoteRemoveRequested(entry.path, entry.directory && !entry.symlink);
}

void MainWindow::renameRemoteSelected()
{
    if (!m_connected)
        return;
    const RemoteEntries entries = selectedRemoteEntries();
    if (entries.size() != 1)
        return;
    const RemoteEntry &entry = entries.first();
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Переименование"), tr("Новое имя:"),
                              QLineEdit::Normal, entry.name, &ok)
            .trimmed();
    if (!ok || name.isEmpty() || name == entry.name ||
        name.contains(QLatin1Char('/')))
        return;
    emit remoteRenameRequested(entry.path,
                               appendName(m_currentRemotePath, name));
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(Theme::styleSheet());
    for (QWidget *widget : QApplication::allWidgets())
        repolish(widget);

    m_themeButton->setText(Theme::mode() == Theme::Mode::Dark
                               ? QStringLiteral("☀")
                               : QStringLiteral("☾"));
    setConnectionState(m_connected);
}

void MainWindow::toggleTheme()
{
    const Theme::Mode next = Theme::mode() == Theme::Mode::Dark
                                 ? Theme::Mode::Light
                                 : Theme::Mode::Dark;
    Theme::setMode(next);
    Theme::saveMode(next);
    applyTheme();
    showNotice(next == Theme::Mode::Light ? tr("Включена светлая тема")
                                          : tr("Включена тёмная тема"),
               Notice::Info);
}

void MainWindow::setActivePage(int index)
{
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < m_navigationButtons.size(); ++i) {
        m_navigationButtons.at(i)->setProperty("active", i == index);
        repolish(m_navigationButtons.at(i));
    }
}

void MainWindow::showNotice(const QString &text, Notice severity,
                            const QString &detail)
{
    static const QHash<Notice, QString> severityNames{
        {Notice::Info, QStringLiteral("info")},
        {Notice::Success, QStringLiteral("success")},
        {Notice::Warning, QStringLiteral("warning")},
        {Notice::Error, QStringLiteral("error")}};

    m_notificationText->setText(text);
    m_notificationDetail->setText(detail);
    m_notificationDetail->setVisible(!detail.isEmpty());
    m_notification->setProperty("severity", severityNames.value(severity));
    repolish(m_notification);
    m_notification->setVisible(true);

    if (severity == Notice::Error)
        m_notificationTimer->stop();
    else
        m_notificationTimer->start(6000);
    m_statusLeft->setText(text);
}

void MainWindow::onError(const QString &message)
{
    m_connectButton->setEnabled(!m_connected);
    m_errorLog.append(QStringLiteral("%1  %2")
                          .arg(QDateTime::currentDateTime().toString(
                                   QStringLiteral("HH:mm:ss")),
                               message));

    while (m_errorLog.size() > 50)
        m_errorLog.removeFirst();
    const int recent = std::min<int>(3, static_cast<int>(m_errorLog.size()));
    QStringList detail;
    for (int i = static_cast<int>(m_errorLog.size()) - recent;
         i < m_errorLog.size(); ++i)
        detail.append(m_errorLog.at(i));
    showNotice(message, Notice::Error,
               m_errorLog.size() > 1
                   ? tr("Последние сообщения:\n%1")
                         .arg(detail.join(QLatin1Char('\n')))
                   : QString());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    TransferModel *model = m_transferPanel->model();
    if (!m_closing && model->activeCount() + model->queuedCount() > 0) {
        const auto answer = QMessageBox::question(
            this, tr("Передачи не завершены"),
            tr("Ещё выполняются передачи (%1). Закрыть Nexa и прервать их?")
                .arg(model->activeCount() + model->queuedCount()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    m_closing = true;
    m_browseWorker->requestStop();
    m_transferWorker->requestStop();
    QMainWindow::closeEvent(event);
}
