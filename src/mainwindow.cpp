#include "mainwindow.h"

#include "connectdialog.h"
#include "fileviews.h"
#include "remotefilemodel.h"
#include "sftpworker.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QShortcut>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QWidget *panelHeader(const QString &caption, QLineEdit *path,
                     QPushButton *up, QWidget *parent)
{
    auto *widget = new QWidget(parent);
    widget->setObjectName(QStringLiteral("panelHeader"));
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(18, 17, 18, 13);
    layout->setSpacing(10);
    auto *label = new QLabel(caption);
    label->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(label);
    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    up->setText(QStringLiteral("←"));
    up->setObjectName(QStringLiteral("iconButton"));
    up->setToolTip(QObject::tr("На уровень выше"));
    up->setFixedSize(38, 38);
    path->setObjectName(QStringLiteral("pathInput"));
    row->addWidget(up);
    row->addWidget(path);
    layout->addLayout(row);
    return widget;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    qRegisterMetaType<ConnectionProfile>();
    qRegisterMetaType<RemoteEntries>();
    buildUi();
    applyTheme();

    m_workerThread = new QThread(this);
    m_worker = new SftpWorker;
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &MainWindow::connectRequested,
            m_worker, &SftpWorker::connectToHost);
    connect(this, &MainWindow::hostKeyConfirmed,
            m_worker, &SftpWorker::confirmHostKey);
    connect(this, &MainWindow::disconnectRequested,
            m_worker, &SftpWorker::disconnectFromHost);
    connect(this, &MainWindow::remoteListRequested,
            m_worker, &SftpWorker::listDirectory);
    connect(this, &MainWindow::downloadRequested,
            m_worker, &SftpWorker::downloadFile);
    connect(this, &MainWindow::uploadRequested,
            m_worker, &SftpWorker::uploadFile);
    connect(this, &MainWindow::remoteMkdirRequested,
            m_worker, &SftpWorker::createDirectory);
    connect(this, &MainWindow::remoteFileCreateRequested,
            m_worker, &SftpWorker::createFile);
    connect(this, &MainWindow::remoteRemoveRequested,
            m_worker, &SftpWorker::removeEntry);
    connect(this, &MainWindow::remoteRenameRequested,
            m_worker, &SftpWorker::renameEntry);

    connect(m_worker, &SftpWorker::connected,
            this, &MainWindow::onConnected);
    connect(m_worker, &SftpWorker::disconnected,
            this, &MainWindow::onDisconnected);
    connect(m_worker, &SftpWorker::hostKeyConfirmationRequired,
            this, &MainWindow::onHostKeyRequired);
    connect(m_worker, &SftpWorker::directoryListed,
            this, &MainWindow::onRemoteListed);
    connect(m_worker, &SftpWorker::directoryBatchReceived,
            this, &MainWindow::onRemoteBatchReceived);
    connect(m_worker, &SftpWorker::errorOccurred,
            this, &MainWindow::onError);
    connect(m_worker, &SftpWorker::performanceMetric, this,
            [this](const QString &operation, qint64 milliseconds) {
                statusBar()->showMessage(
                    tr("%1 — %2 мс").arg(operation).arg(milliseconds), 5000);
            });
    connect(m_worker, &SftpWorker::reconnecting, this, [this] {
        m_connectionLabel->setText(tr("●  Переподключение…"));
        m_connectionLabel->setStyleSheet(QStringLiteral("color:#fbbf24;"));
        statusBar()->showMessage(
            tr("Соединение потеряно. Переподключаюсь автоматически…"));
    });
    connect(m_worker, &SftpWorker::reconnected, this, [this] {
        m_connectionLabel->setText(tr("●  Подключено"));
        m_connectionLabel->setStyleSheet(QStringLiteral("color:#4ade80;"));
        statusBar()->showMessage(
            tr("Соединение восстановлено. Операция продолжена."), 5000);
    });
    connect(m_worker, &SftpWorker::transferProgress, this,
            [this](const QString &label, qint64 done, qint64 total) {
                m_progress->setVisible(true);
                m_progress->setMaximum(total > 0 ? 1000 : 0);
                if (total > 0)
                    m_progress->setValue(
                        static_cast<int>((done * 1000) / total));
                const double seconds =
                    std::max(0.001,
                             static_cast<double>(m_transferTimer.elapsed()) /
                                 1000.0);
                const double megabytesPerSecond =
                    static_cast<double>(done) / (1024.0 * 1024.0) / seconds;
                statusBar()->showMessage(
                    tr("Передача %1 — %2 / %3 — %4 МБ/с")
                        .arg(label, humanSize(done), humanSize(total))
                        .arg(megabytesPerSecond, 0, 'f', 1));
            });
    connect(m_worker, &SftpWorker::transferStarted, this,
            [this](const QString &label, bool upload) {
                m_transferTimer.restart();
                statusBar()->showMessage(
                    upload ? tr("Загрузка «%1» на сервер…").arg(label)
                           : tr("Скачивание «%1»…").arg(label));
                m_progress->setVisible(true);
                m_progress->setRange(0, 0);
            });
    connect(m_worker, &SftpWorker::transferFinished, this,
            [this](const QString &label) {
                m_progress->setVisible(false);
                statusBar()->showMessage(tr("Передача «%1» завершена.").arg(label),
                                         4000);
                requestRemoteDirectory(m_currentRemotePath);
                m_localModel->setRootPath(m_localPath->text());
            });
    connect(m_worker, &SftpWorker::deletionProgress, this,
            [this](qint64 count, const QString &path) {
                statusBar()->showMessage(
                    tr("Удаление… обработано объектов: %1 — %2")
                        .arg(count).arg(path));
            });

    m_workerThread->start();
    setConnectionState(false);
    QTimer::singleShot(0, this, [this] {
        const auto saved = ConnectDialog::lastProfile();
        if (!saved)
            return;
        m_connectButton->setEnabled(false);
        m_connectionLabel->setText(tr("●  Подключение…"));
        statusBar()->showMessage(
            tr("Восстанавливаю сохранённую SFTP-сессию…"));
        emit connectRequested(*saved);
    });
}

MainWindow::~MainWindow()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_worker->requestStop();
        QMetaObject::invokeMethod(m_worker, "disconnectFromHost",
                                  Qt::BlockingQueuedConnection);
    }
    m_workerThread->quit();
    m_workerThread->wait();
}

void MainWindow::buildUi()
{
    resize(1380, 820);
    setMinimumSize(1040, 640);
    setWindowTitle(QStringLiteral("Nexa"));

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("appRoot"));
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(216);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 22, 18, 18);
    sidebarLayout->setSpacing(8);

    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(11);
    auto *brandMark = new QLabel(QStringLiteral("N"));
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setAlignment(Qt::AlignCenter);
    brandMark->setFixedSize(38, 38);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto *brand = new QLabel(QStringLiteral("Nexa"));
    brand->setObjectName(QStringLiteral("brandTitle"));
    auto *edition = new QLabel(tr("Secure workspace"));
    edition->setObjectName(QStringLiteral("brandSubtitle"));
    brandText->addWidget(brand);
    brandText->addWidget(edition);
    brandRow->addWidget(brandMark);
    brandRow->addLayout(brandText);
    sidebarLayout->addLayout(brandRow);
    sidebarLayout->addSpacing(26);

    auto *navigationCaption = new QLabel(tr("РАБОЧЕЕ ПРОСТРАНСТВО"));
    navigationCaption->setObjectName(QStringLiteral("sectionCaption"));
    sidebarLayout->addWidget(navigationCaption);
    auto *filesNavigation = new QPushButton(tr("  ▣  Файлы"));
    filesNavigation->setObjectName(QStringLiteral("navigationActive"));
    filesNavigation->setCursor(Qt::ArrowCursor);
    sidebarLayout->addWidget(filesNavigation);
    auto *hostsNavigation = new QPushButton(tr("  ◈  Подключения"));
    hostsNavigation->setObjectName(QStringLiteral("navigationButton"));
    hostsNavigation->setToolTip(
        tr("Менеджер подключений появится в следующей версии"));
    sidebarLayout->addWidget(hostsNavigation);
    auto *transfersNavigation = new QPushButton(tr("  ⇅  Передачи"));
    transfersNavigation->setObjectName(QStringLiteral("navigationButton"));
    transfersNavigation->setToolTip(
        tr("Постоянная очередь появится в следующей версии"));
    sidebarLayout->addWidget(transfersNavigation);
    connect(hostsNavigation, &QPushButton::clicked, this, [this] {
        statusBar()->showMessage(
            tr("Менеджер подключений запланирован для Nexa 0.2"),
            3500);
    });
    connect(transfersNavigation, &QPushButton::clicked, this, [this] {
        statusBar()->showMessage(
            tr("Постоянная очередь передач запланирована для Nexa 0.2"),
            3500);
    });
    sidebarLayout->addStretch();

    auto *connectionCard = new QFrame;
    connectionCard->setObjectName(QStringLiteral("connectionCard"));
    auto *connectionLayout = new QVBoxLayout(connectionCard);
    connectionLayout->setContentsMargins(13, 13, 13, 13);
    connectionLayout->setSpacing(9);
    auto *connectionCaption = new QLabel(tr("СОЕДИНЕНИЕ"));
    connectionCaption->setObjectName(QStringLiteral("sectionCaption"));
    connectionLayout->addWidget(connectionCaption);
    m_connectionLabel = new QLabel(tr("●  Не подключено"));
    m_connectionLabel->setObjectName(QStringLiteral("connectionLabel"));
    connectionLayout->addWidget(m_connectionLabel);
    sidebarLayout->addWidget(connectionCard);
    auto *versionLabel =
        new QLabel(QStringLiteral("Nexa 0.1.0 Preview"));
    versionLabel->setObjectName(QStringLiteral("versionLabel"));
    versionLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout->addWidget(versionLabel);
    rootLayout->addWidget(sidebar);

    auto *workspace = new QWidget;
    workspace->setObjectName(QStringLiteral("workspace"));
    auto *workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(24, 20, 24, 16);
    workspaceLayout->setSpacing(16);

    auto *topBar = new QHBoxLayout;
    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(2);
    auto *eyebrow = new QLabel(tr("ФАЙЛОВЫЙ МЕНЕДЖЕР"));
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto *workspaceTitle = new QLabel(tr("Ваши файлы и сервер"));
    workspaceTitle->setObjectName(QStringLiteral("workspaceTitle"));
    auto *workspaceSubtitle = new QLabel(
        tr("Перетаскивайте файлы между панелями — передача начнётся автоматически"));
    workspaceSubtitle->setObjectName(QStringLiteral("workspaceSubtitle"));
    titleLayout->addWidget(eyebrow);
    titleLayout->addWidget(workspaceTitle);
    titleLayout->addWidget(workspaceSubtitle);
    topBar->addLayout(titleLayout);
    topBar->addStretch();

    m_connectButton = new QPushButton(tr("＋  Подключиться"));
    m_connectButton->setObjectName(QStringLiteral("primaryButton"));
    m_disconnectButton = new QPushButton(tr("Отключиться"));
    m_disconnectButton->setObjectName(QStringLiteral("secondaryButton"));
    m_connectButton->setMinimumHeight(40);
    m_disconnectButton->setMinimumHeight(40);
    topBar->addWidget(m_disconnectButton);
    topBar->addWidget(m_connectButton);
    workspaceLayout->addLayout(topBar);

    connect(m_connectButton, &QPushButton::clicked,
            this, &MainWindow::showConnectDialog);
    connect(m_disconnectButton, &QPushButton::clicked,
            this, [this] { emit disconnectRequested(); });

    auto *splitter = new QSplitter;
    splitter->setObjectName(QStringLiteral("fileSplitter"));
    splitter->setChildrenCollapsible(false);

    auto *localPanel = new QFrame;
    localPanel->setObjectName(QStringLiteral("fileCard"));
    auto *localLayout = new QVBoxLayout(localPanel);
    localLayout->setContentsMargins(0, 0, 0, 0);
    localLayout->setSpacing(0);
    m_localPath = new QLineEdit(QDir::homePath());
    auto *localUp = new QPushButton;
    localLayout->addWidget(panelHeader(tr("Этот компьютер"), m_localPath,
                                       localUp, localPanel));

    m_localModel = new QFileSystemModel(this);
    m_localModel->setOption(QFileSystemModel::DontWatchForChanges, true);
    m_localModel->setOption(QFileSystemModel::DontResolveSymlinks, true);
    m_localModel->setOption(QFileSystemModel::DontUseCustomDirectoryIcons, true);
    m_localModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    m_localModel->setRootPath(QDir::homePath());
    m_localView = new LocalFileView;
    m_localView->setModel(m_localModel);
    m_localView->setRootIndex(m_localModel->index(QDir::homePath()));
    m_localView->setSortingEnabled(true);
    m_localView->sortByColumn(0, Qt::AscendingOrder);
    m_localView->setAlternatingRowColors(true);
    m_localView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_localView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_localView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    localLayout->addWidget(m_localView);

    connect(m_localView, &QTreeView::doubleClicked,
            this, &MainWindow::openLocal);
    connect(m_localPath, &QLineEdit::returnPressed, this, [this] {
        const QFileInfo info(m_localPath->text());
        if (info.isDir())
            m_localView->setRootIndex(m_localModel->index(info.absoluteFilePath()));
    });
    connect(localUp, &QPushButton::clicked, this, [this] {
        QDir dir(m_localPath->text());
        if (dir.cdUp()) {
            m_localPath->setText(dir.absolutePath());
            m_localView->setRootIndex(m_localModel->index(dir.absolutePath()));
        }
    });
    connect(m_localView, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &position) {
                QMenu menu(this);
                auto *upload = menu.addAction(tr("Загрузить на сервер"));
                upload->setEnabled(m_connected &&
                                   !m_localView->selectionModel()
                                        ->selectedRows().isEmpty());
                connect(upload, &QAction::triggered,
                        this, &MainWindow::uploadSelected);
                menu.exec(m_localView->viewport()->mapToGlobal(position));
            });

    auto *remotePanel = new QFrame;
    remotePanel->setObjectName(QStringLiteral("fileCard"));
    auto *remoteLayout = new QVBoxLayout(remotePanel);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    remoteLayout->setSpacing(0);
    m_remotePath = new QLineEdit(QStringLiteral("/"));
    auto *remoteUp = new QPushButton;
    remoteLayout->addWidget(panelHeader(tr("Удалённый сервер"), m_remotePath,
                                        remoteUp, remotePanel));

    m_remoteView = new RemoteFileView;
    m_remoteModel = new RemoteFileModel(this);
    m_remoteView->setModel(m_remoteModel);
    m_remoteView->setAlternatingRowColors(true);
    m_remoteView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_remoteView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_remoteView->setShowGrid(false);
    m_remoteView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_remoteView->verticalHeader()->hide();
    m_remoteView->horizontalHeader()->setStretchLastSection(false);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    remoteLayout->addWidget(m_remoteView);

    connect(m_remoteView, &QTableView::doubleClicked,
            this, &MainWindow::openRemote);
    connect(m_remotePath, &QLineEdit::returnPressed,
            this, &MainWindow::goToRemotePath);
    connect(remoteUp, &QPushButton::clicked,
            this, &MainWindow::goRemoteUp);
    connect(m_remoteView, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &position) {
                QMenu menu(this);
                QString destinationDirectory = m_currentRemotePath;
                const QModelIndex targetIndex =
                    m_remoteView->indexAt(position);
                if (const RemoteEntry *target =
                        m_remoteModel->entryAt(targetIndex.row());
                    target && target->directory)
                    destinationDirectory = target->path;

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
                auto *refresh = menu.addAction(tr("Обновить"));
                const int selected = static_cast<int>(
                    m_remoteView->selectionModel()->selectedRows().size());
                download->setEnabled(m_connected && selected > 0);
                uploadFiles->setEnabled(m_connected);
                uploadFolder->setEnabled(m_connected);
                newFile->setEnabled(m_connected);
                newFolder->setEnabled(m_connected);
                rename->setEnabled(m_connected && selected == 1);
                remove->setEnabled(m_connected && selected > 0);
                refresh->setEnabled(m_connected);
                connect(download, &QAction::triggered,
                        this, &MainWindow::downloadSelected);
                connect(uploadFiles, &QAction::triggered, this,
                        [this, destinationDirectory] {
                            const QStringList files =
                                QFileDialog::getOpenFileNames(
                                    this, tr("Выберите файлы для загрузки"),
                                    m_localPath->text());
                            if (files.isEmpty())
                                return;
                            m_worker->beginDirectoryRequest();
                            for (const QString &file : files) {
                                const QFileInfo info(file);
                                const QString remote =
                                    (destinationDirectory.endsWith(
                                         QLatin1Char('/'))
                                         ? destinationDirectory
                                         : destinationDirectory +
                                               QLatin1Char('/'))
                                    + info.fileName();
                                emit uploadRequested(
                                    info.absoluteFilePath(), remote, false);
                            }
                        });
                connect(uploadFolder, &QAction::triggered, this,
                        [this, destinationDirectory] {
                            const QString folder =
                                QFileDialog::getExistingDirectory(
                                    this, tr("Выберите папку для загрузки"),
                                    m_localPath->text());
                            if (folder.isEmpty())
                                return;
                            m_worker->beginDirectoryRequest();
                            const QFileInfo info(folder);
                            const QString remote =
                                (destinationDirectory.endsWith(QLatin1Char('/'))
                                     ? destinationDirectory
                                     : destinationDirectory +
                                           QLatin1Char('/'))
                                + info.fileName();
                            emit uploadRequested(
                                info.absoluteFilePath(), remote, true);
                        });
                connect(newFile, &QAction::triggered,
                        this, &MainWindow::createRemoteFile);
                connect(newFolder, &QAction::triggered,
                        this, &MainWindow::createRemoteFolder);
                connect(rename, &QAction::triggered,
                        this, &MainWindow::renameRemoteSelected);
                connect(remove, &QAction::triggered,
                        this, &MainWindow::removeRemoteSelected);
                connect(refresh, &QAction::triggered,
                        this, &MainWindow::refreshRemote);
                menu.exec(m_remoteView->viewport()->mapToGlobal(position));
            });

    connect(m_remoteView, &RemoteFileView::localUrlsDropped, this,
            [this](const QList<QUrl> &urls, const QModelIndex &targetIndex) {
                if (!m_connected)
                    return;
                m_worker->beginDirectoryRequest();
                QString destinationDirectory = m_currentRemotePath;
                if (const RemoteEntry *target =
                        m_remoteModel->entryAt(targetIndex.row());
                    target && target->directory)
                    destinationDirectory = target->path;

                int queued = 0;
                for (const QUrl &url : urls) {
                    if (!url.isLocalFile())
                        continue;
                    const QFileInfo info(url.toLocalFile());
                    if (!info.exists())
                        continue;
                    const QString destination =
                        (destinationDirectory.endsWith(QLatin1Char('/'))
                             ? destinationDirectory
                             : destinationDirectory + QLatin1Char('/'))
                        + info.fileName();
                    emit uploadRequested(info.absoluteFilePath(), destination,
                                         info.isDir());
                    ++queued;
                }
                statusBar()->showMessage(
                    tr("Добавлено в загрузку: %1").arg(queued));
            });

    connect(m_localView, &LocalFileView::remoteDataDropped, this,
            [this](const QByteArray &encodedEntries,
                   const QModelIndex &targetIndex) {
                if (!m_connected)
                    return;
                m_worker->beginDirectoryRequest();
                QString destinationDirectory = m_localPath->text();
                if (targetIndex.isValid()) {
                    const QFileInfo target =
                        m_localModel->fileInfo(targetIndex);
                    if (target.isDir())
                        destinationDirectory = target.absoluteFilePath();
                }
                QMimeData mimeData;
                mimeData.setData(RemoteFileModel::remoteMimeType(),
                                 encodedEntries);
                const RemoteEntries entries =
                    RemoteFileModel::entriesFromMimeData(&mimeData);
                for (const auto &entry : entries) {
                    emit downloadRequested(
                        entry.path,
                        QDir(destinationDirectory).filePath(entry.name),
                        entry.directory && !entry.symlink);
                }
                statusBar()->showMessage(
                    tr("Добавлено в скачивание: %1").arg(entries.size()));
            });

    splitter->addWidget(localPanel);
    splitter->addWidget(remotePanel);
    splitter->setSizes({560, 560});
    workspaceLayout->addWidget(splitter, 1);
    rootLayout->addWidget(workspace, 1);
    setCentralWidget(root);

    m_progress = new QProgressBar;
    m_progress->setFixedWidth(170);
    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_progress);

    auto *refreshShortcut = new QShortcut(QKeySequence::Refresh, this);
    connect(refreshShortcut, &QShortcut::activated,
            this, &MainWindow::refreshRemote);
    auto *renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), m_remoteView);
    connect(renameShortcut, &QShortcut::activated,
            this, &MainWindow::renameRemoteSelected);
    auto *deleteShortcut =
        new QShortcut(QKeySequence(Qt::Key_Delete), m_remoteView);
    connect(deleteShortcut, &QShortcut::activated,
            this, &MainWindow::removeRemoteSelected);
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(QStringLiteral(R"(
        * { font-family: "Segoe UI"; font-size: 13px; }
        QMainWindow, QDialog { background: #080c14; color: #e7edf7; }
        QWidget#appRoot, QWidget#workspace { background: #0a0f19; }
        QFrame#sidebar {
            background: #0d131f;
            border: none;
            border-right: 1px solid #1b2637;
        }
        QLabel { color: #b8c3d6; }
        QLabel#brandMark {
            color: white;
            background: #5b5cf0;
            border-radius: 11px;
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#brandTitle { color: #f7f9fc; font-size: 17px; font-weight: 700; }
        QLabel#brandSubtitle { color: #68758a; font-size: 11px; }
        QLabel#sectionCaption, QLabel#eyebrow {
            color: #657187;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#workspaceTitle { color: #f7f9fc; font-size: 24px; font-weight: 700; }
        QLabel#workspaceSubtitle { color: #748197; font-size: 12px; }
        QLabel#panelTitle {
            color: #f4f7fb;
            font-size: 15px;
            font-weight: 650;
        }
        QLabel#dialogTitle {
            color: #f8fafc;
            font-size: 20px;
            font-weight: 700;
            padding: 8px 0 14px 0;
        }
        QLabel#connectionLabel { color: #8b98ad; font-size: 12px; }
        QLabel#versionLabel { color: #465268; font-size: 10px; padding-top: 5px; }
        QFrame#connectionCard {
            background: #111927;
            border: 1px solid #202c3d;
            border-radius: 10px;
        }
        QPushButton#navigationButton, QPushButton#navigationActive {
            border: none;
            border-radius: 8px;
            padding: 10px 11px;
            text-align: left;
            font-weight: 600;
        }
        QPushButton#navigationButton { color: #7f8ba0; background: transparent; }
        QPushButton#navigationButton:hover { color: #d9e1ee; background: #121b2a; }
        QPushButton#navigationActive { color: #dddfff; background: #202147; }
        QFrame#fileCard {
            background: #0f1623;
            border: 1px solid #202b3c;
            border-radius: 13px;
        }
        QWidget#panelHeader { background: transparent; border: none; }
        QLineEdit, QSpinBox, QComboBox {
            background: #111a29;
            color: #dce5f2;
            border: 1px solid #273449;
            border-radius: 8px;
            padding: 9px 11px;
            selection-background-color: #5b5cf0;
        }
        QLineEdit#pathInput { color: #aebbd0; background: #0c1420; }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #6466f1; }
        QPushButton {
            background: #172235;
            color: #dce4f0;
            border: 1px solid #2a384d;
            border-radius: 8px;
            padding: 9px 14px;
            font-weight: 600;
        }
        QPushButton:hover { background: #202e44; border-color: #3a4a62; }
        QPushButton:pressed { background: #121c2b; }
        QPushButton:disabled { color: #4c586c; background: #111925; border-color: #202a39; }
        QPushButton#primaryButton {
            background: #5b5cf0;
            border-color: #7071ff;
            color: white;
            padding-left: 17px;
            padding-right: 17px;
        }
        QPushButton#primaryButton:hover { background: #6d6ef7; }
        QPushButton#secondaryButton { background: transparent; }
        QPushButton#iconButton {
            background: #141e2e;
            color: #9eabc0;
            border-color: #263348;
            padding: 0;
            font-size: 17px;
        }
        QTreeView, QTableView {
            background: #0d1420;
            alternate-background-color: #101927;
            color: #cbd6e6;
            border: none;
            border-top: 1px solid #202b3c;
            outline: none;
            selection-background-color: #30317a;
            selection-color: white;
            show-decoration-selected: 1;
        }
        QTreeView::item, QTableView::item {
            min-height: 32px;
            padding: 4px 8px;
            border: none;
        }
        QTreeView::item:hover, QTableView::item:hover { background: #172236; }
        QHeaderView::section {
            background: #0f1724;
            color: #66758c;
            border: none;
            border-bottom: 1px solid #202b3c;
            padding: 10px 9px;
            font-size: 11px;
            font-weight: 650;
        }
        QSplitter#fileSplitter::handle { background: transparent; width: 12px; }
        QStatusBar {
            background: #0d131f;
            color: #66758c;
            border-top: 1px solid #1b2637;
            min-height: 26px;
        }
        QProgressBar {
            background: #151f30;
            border: 1px solid #28364b;
            border-radius: 5px;
            text-align: center;
            color: white;
        }
        QProgressBar::chunk { background: #5b5cf0; border-radius: 4px; }
        QDialogButtonBox QPushButton { min-width: 110px; }
        QMenu {
            background: #121a28;
            color: #d7dfeb;
            border: 1px solid #2a374a;
            border-radius: 8px;
            padding: 6px;
        }
        QMenu::item { padding: 8px 28px 8px 10px; border-radius: 5px; }
        QMenu::item:selected { background: #30317a; }
        QScrollBar:vertical { background: #0d1420; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical { background: #2b384c; border-radius: 4px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
}

void MainWindow::showConnectDialog()
{
    ConnectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_connectButton->setEnabled(false);
    m_connectionLabel->setText(tr("●  Подключение…"));
    emit connectRequested(dialog.profile());
}

void MainWindow::onConnected(const QString &homePath)
{
    m_connected = true;
    setConnectionState(true);
    QSettings settings;
    QString restoredPath =
        settings.value(QStringLiteral("session/remotePath"),
                       homePath).toString();
    if (restoredPath.isEmpty())
        restoredPath = homePath;
    m_currentRemotePath = restoredPath;
    m_remotePath->setText(restoredPath);
    statusBar()->showMessage(tr("SFTP-подключение установлено."), 4000);
    requestRemoteDirectory(restoredPath, false);
}

void MainWindow::onDisconnected()
{
    m_connected = false;
    setConnectionState(false);
    m_remoteEntries.clear();
    m_directoryCache.clear();
    m_remoteModel->clear();
    statusBar()->showMessage(tr("Соединение закрыто."), 3000);
}

void MainWindow::onHostKeyRequired(const QString &host,
                                   const QString &fingerprint,
                                   bool changed)
{
    QMessageBox box(this);
    box.setIcon(changed ? QMessageBox::Critical : QMessageBox::Question);
    box.setWindowTitle(changed ? tr("Ключ сервера изменился")
                               : tr("Новый сервер"));
    box.setText(changed
        ? tr("Внимание: ключ сервера %1 отличается от сохранённого. "
             "Это может указывать на атаку. Продолжайте только если вы "
             "ожидали смену ключа.").arg(host)
        : tr("Подтвердите подлинность сервера %1.").arg(host));
    box.setInformativeText(tr("SHA-256 отпечаток:\n%1").arg(fingerprint));
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

void MainWindow::onRemoteListed(const QString &path,
                                const RemoteEntries &entries)
{
    m_directoryCache.insert(path, entries);
    if (path != m_currentRemotePath)
        return;
    m_showingCachedDirectory = false;
    m_currentRemotePath = path;
    m_remotePath->setText(path);
    m_remoteEntries = entries;
    m_remoteModel->setEntries(entries);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_remoteView->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    statusBar()->showMessage(
        tr("Загружено объектов: %1").arg(entries.size()), 2000);
}

void MainWindow::onRemoteBatchReceived(const QString &path,
                                       const RemoteEntries &entries)
{
    if (path != m_currentRemotePath || m_showingCachedDirectory)
        return;
    m_remoteEntries.reserve(m_remoteEntries.size() + entries.size());
    for (const auto &entry : entries)
        m_remoteEntries.append(entry);
    m_remoteModel->appendEntries(entries);
    statusBar()->showMessage(
        tr("Загрузка каталога… получено: %1").arg(m_remoteEntries.size()));
}

void MainWindow::onError(const QString &message)
{
    m_connectButton->setEnabled(true);
    if (!m_connected)
        setConnectionState(false);
    m_progress->setVisible(false);
    QMessageBox::critical(this, tr("Ошибка SFTP"), message);
}

void MainWindow::openLocal(const QModelIndex &index)
{
    const QFileInfo info = m_localModel->fileInfo(index);
    if (!info.isDir())
        return;
    m_localPath->setText(info.absoluteFilePath());
    m_localView->setRootIndex(m_localModel->index(info.absoluteFilePath()));
}

void MainWindow::openRemote(const QModelIndex &index)
{
    const int row = index.row();
    if (row < 0 || row >= m_remoteEntries.size())
        return;
    const auto &entry = m_remoteEntries.at(row);
    if (entry.directory)
        requestRemoteDirectory(entry.path);
    else
        downloadSelected();
}

void MainWindow::goToRemotePath()
{
    if (m_connected)
        requestRemoteDirectory(m_remotePath->text());
}

void MainWindow::goRemoteUp()
{
    if (!m_connected)
        return;
    QString path = QDir::cleanPath(
        QDir::fromNativeSeparators(m_currentRemotePath));
    if (path != QStringLiteral("/")) {
        const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
        path = slash <= 0 ? QStringLiteral("/") : path.left(slash);
    }
    requestRemoteDirectory(path);
}

void MainWindow::uploadSelected()
{
    if (!m_connected)
        return;
    const auto rows = m_localView->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Выберите файл"),
                                 tr("Выберите локальный файл для загрузки."));
        return;
    }
    m_worker->beginDirectoryRequest();
    for (const auto &index : rows) {
        const QFileInfo info = m_localModel->fileInfo(index);
        const QString destination =
            (m_currentRemotePath.endsWith(QLatin1Char('/'))
                 ? m_currentRemotePath : m_currentRemotePath + QLatin1Char('/'))
            + info.fileName();
        emit uploadRequested(info.absoluteFilePath(), destination, info.isDir());
    }
}

void MainWindow::downloadSelected()
{
    if (!m_connected)
        return;
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;
    m_worker->beginDirectoryRequest();
    const QString localDirectory = m_localPath->text();
    for (const auto &index : rows) {
        const auto &entry = m_remoteEntries.at(index.row());
        emit downloadRequested(
            entry.path, QDir(localDirectory).filePath(entry.name),
            entry.directory);
    }
}

void MainWindow::createRemoteFolder()
{
    if (!m_connected)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Новая папка"), tr("Имя папки:"), QLineEdit::Normal,
        QString(), &ok).trimmed();
    if (!ok || name.isEmpty() || name.contains(QLatin1Char('/')))
        return;
    const QString path = (m_currentRemotePath.endsWith(QLatin1Char('/'))
        ? m_currentRemotePath : m_currentRemotePath + QLatin1Char('/')) + name;
    emit remoteMkdirRequested(path);
}

void MainWindow::createRemoteFile()
{
    if (!m_connected)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Новый файл"), tr("Имя файла:"), QLineEdit::Normal,
        QString(), &ok).trimmed();
    if (!ok || name.isEmpty() || name.contains(QLatin1Char('/')))
        return;
    const QString path =
        (m_currentRemotePath.endsWith(QLatin1Char('/'))
             ? m_currentRemotePath
             : m_currentRemotePath + QLatin1Char('/')) + name;
    emit remoteFileCreateRequested(path);
}

void MainWindow::removeRemoteSelected()
{
    if (!m_connected)
        return;
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Подтверждение удаления"));
    box.setText(tr("Удалить выбранные объекты?"));
    box.setInformativeText(
        tr("Все файлы внутри выбранных папок также будут удалены. "
           "Это действие нельзя отменить."));
    auto *deleteButton =
        box.addButton(tr("Удалить"), QMessageBox::DestructiveRole);
    box.addButton(tr("Отмена"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != deleteButton)
        return;
    for (const auto &index : rows) {
        const auto &entry = m_remoteEntries.at(index.row());
        emit remoteRemoveRequested(entry.path,
                                   entry.directory && !entry.symlink);
    }
}

void MainWindow::renameRemoteSelected()
{
    if (!m_connected)
        return;
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    if (rows.size() != 1)
        return;
    const auto &entry = m_remoteEntries.at(rows.first().row());
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Переименование"), tr("Новое имя:"), QLineEdit::Normal,
        entry.name, &ok).trimmed();
    if (!ok || name.isEmpty() || name == entry.name ||
        name.contains(QLatin1Char('/')))
        return;
    const QString destination =
        (m_currentRemotePath.endsWith(QLatin1Char('/'))
             ? m_currentRemotePath : m_currentRemotePath + QLatin1Char('/'))
        + name;
    emit remoteRenameRequested(entry.path, destination);
}

void MainWindow::refreshRemote()
{
    if (!m_connected)
        return;
    m_directoryCache.remove(m_currentRemotePath);
    requestRemoteDirectory(m_currentRemotePath, false);
}

void MainWindow::setConnectionState(bool connected)
{
    m_connected = connected;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_remotePath->setEnabled(connected);
    m_connectionLabel->setText(
        connected ? tr("●  Подключено") : tr("●  Не подключено"));
    m_connectionLabel->setStyleSheet(
        connected ? QStringLiteral("color:#4ade80;")
                  : QStringLiteral("color:#94a3b8;"));
}

void MainWindow::requestRemoteDirectory(const QString &path, bool useCache)
{
    QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!cleanPath.startsWith(QLatin1Char('/')))
        cleanPath.prepend(QLatin1Char('/'));

    m_currentRemotePath = cleanPath;
    m_remotePath->setText(cleanPath);
    if (m_connected)
        QSettings().setValue(QStringLiteral("session/remotePath"),
                             cleanPath);
    const quint64 generation = m_worker->beginDirectoryRequest();
    if (useCache) {
        const auto cached = m_directoryCache.constFind(cleanPath);
        if (cached != m_directoryCache.cend()) {
            m_showingCachedDirectory = true;
            m_remoteEntries = cached.value();
            m_remoteModel->setEntries(m_remoteEntries);
            statusBar()->showMessage(tr("Показан каталог из кэша…"));
        } else {
            m_showingCachedDirectory = false;
            m_remoteEntries.clear();
            m_remoteModel->clear();
            statusBar()->showMessage(tr("Загрузка каталога…"));
        }
    } else {
        m_showingCachedDirectory = false;
        m_remoteEntries.clear();
        m_remoteModel->clear();
    }
    emit remoteListRequested(cleanPath, generation);
}

QString MainWindow::selectedRemotePath() const
{
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    return rows.isEmpty() ? QString()
                          : m_remoteEntries.value(rows.first().row()).path;
}

bool MainWindow::selectedRemoteIsDirectory() const
{
    const auto rows = m_remoteView->selectionModel()->selectedRows();
    return !rows.isEmpty() &&
           m_remoteEntries.value(rows.first().row()).directory;
}

QString MainWindow::humanSize(qint64 bytes)
{
    static const QStringList units{
        tr("Б"), tr("КБ"), tr("МБ"), tr("ГБ"), tr("ТБ")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2")
        .arg(unit == 0 ? QString::number(bytes)
                       : QString::number(value, 'f', value < 10 ? 1 : 0),
             units.at(unit));
}
