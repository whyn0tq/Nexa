#include "sessionview.h"

#include "filepane.h"
#include "editors.h"
#include "format.h"
#include "glass.h"
#include "transferengine.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

quint64 nextTransferId();

namespace {
QString parentOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}
}

void SessionView::openSelected()
{
    const RemoteEntries selected = m_remote->selectedEntries();
    if (selected.size() != 1 || selected.first().directory) {
        m_toasts->notify(tr("Выберите один файл на сервере"), ToastHost::Kind::Info);
        return;
    }
    if (!m_session->online()) {
        m_toasts->notify(tr("Дождитесь подключения к серверу"), ToastHost::Kind::Warning);
        return;
    }
    openRemoteFile(selected.first(), true);
}

namespace {
constexpr qint64 QuickLimit = 8LL * 1024 * 1024;
constexpr qint64 PrefetchLimit = 1024 * 1024;
constexpr int CacheFiles = 24;

QString elapsedText(qint64 ms)
{
    if (ms < 100)
        return QCoreApplication::translate("SessionView", "мгновенно");
    return QCoreApplication::translate("SessionView", "за %1 с")
        .arg(QString::number(ms / 1000.0, 'f', 1));
}

bool writeLocal(const QString &local, const QByteArray &data)
{
    QFile file(local);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(data) == data.size();
}
}

void SessionView::openRemoteFile(const RemoteEntry &entry, bool edit)
{
    const QString folder =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/Nexa-edit/%1/%2")
            .arg(m_session->id())
            .arg(qHash(parentOf(entry.path)), 8, 16, QLatin1Char('0'));
    QDir().mkpath(folder);
    const QString local = QDir(folder).filePath(entry.name);

    if (m_edits.contains(local))
        m_editWatcher->removePath(local);
    EditedFile file;
    file.remotePath = entry.path;
    file.watch = edit;
    m_edits.insert(local, file);

    if (m_session->supportsQuickFiles() && entry.size <= QuickLimit) {
        QElapsedTimer clock;
        clock.start();
        const auto cached = m_fileCache.constFind(entry.path);
        if (cached != m_fileCache.cend() && cached->size == entry.size &&
            cached->modified == entry.modified) {
            openLocalCopy(local, cached->data, clock.elapsed());
            return;
        }
        const int toast = m_toasts->notify(tr("Открываю %1…").arg(entry.name),
                                           ToastHost::Kind::Info,
                                           tr("%1 с сервера").arg(Format::size(entry.size)));
        for (auto it = m_quickOps.begin(); it != m_quickOps.end(); ++it) {
            if (!it->upload && it->local.isEmpty() && it->remote == entry.path) {
                it->local = local;
                it->clock = clock;
                it->toast = toast;
                return;
            }
        }
        QuickOp op;
        op.remote = entry.path;
        op.local = local;
        op.clock = clock;
        op.size = entry.size;
        op.modified = entry.modified;
        op.toast = toast;
        m_quickOps.insert(m_session->fetchFile(entry.path, entry.size), op);
        return;
    }

    TransferJob job;
    job.id = nextTransferId();
    job.remotePath = entry.path;
    job.localPath = local;
    job.size = entry.size;
    job.permissions = entry.permissions;
    job.modified = entry.modified.isValid() ? entry.modified.toSecsSinceEpoch() : 0;
    job.overwrite = true;
    m_editJobs.insert(job.id, local);
    m_session->transfers()->enqueue({job});
    m_toasts->notify(tr("Открываю %1…").arg(entry.name), ToastHost::Kind::Info,
                     tr("Большой файл — %1").arg(Format::size(entry.size)));
}

void SessionView::prefetchSelected()
{
    const RemoteEntries selected = m_remote->selectedEntries();
    if (selected.size() != 1 || !m_session || !m_session->online() ||
        !m_session->supportsQuickFiles())
        return;
    const RemoteEntry &entry = selected.first();
    if (entry.directory || entry.size < 0 || entry.size > PrefetchLimit)
        return;
    const auto cached = m_fileCache.constFind(entry.path);
    if (cached != m_fileCache.cend() && cached->size == entry.size &&
        cached->modified == entry.modified)
        return;
    for (const QuickOp &op : std::as_const(m_quickOps)) {
        if (!op.upload && op.remote == entry.path)
            return;
    }
    QuickOp op;
    op.remote = entry.path;
    op.size = entry.size;
    op.modified = entry.modified;
    m_quickOps.insert(m_session->fetchFile(entry.path, entry.size), op);
}

void SessionView::quickFetched(quint64 token, const QString &path, const QByteArray &content,
                               const QString &error)
{
    Q_UNUSED(path);
    const auto found = m_quickOps.find(token);
    if (found == m_quickOps.end())
        return;
    const QuickOp op = found.value();
    m_quickOps.erase(found);
    if (op.toast)
        m_toasts->dismiss(op.toast);
    if (!error.isEmpty()) {
        if (!op.local.isEmpty()) {
            m_toasts->notify(tr("Не удалось открыть %1").arg(QFileInfo(op.local).fileName()),
                             ToastHost::Kind::Error, error);
            m_edits.remove(op.local);
        }
        return;
    }
    if (content.size() <= PrefetchLimit * 2) {
        m_fileCache.insert(op.remote, {content, op.size, op.modified});
        m_fileCacheOrder.removeAll(op.remote);
        m_fileCacheOrder.append(op.remote);
        while (m_fileCacheOrder.size() > CacheFiles)
            m_fileCache.remove(m_fileCacheOrder.takeFirst());
    }
    if (!op.local.isEmpty())
        openLocalCopy(op.local, content, op.clock.elapsed());
}

void SessionView::openLocalCopy(const QString &local, const QByteArray &content, qint64 elapsedMs)
{
    auto edit = m_edits.find(local);
    if (edit == m_edits.end())
        return;
    const QString name = QFileInfo(local).fileName();
    if (!writeLocal(local, content)) {
        m_toasts->notify(tr("Не удалось открыть %1").arg(name), ToastHost::Kind::Error,
                         tr("Не получилось записать временный файл."));
        m_edits.erase(edit);
        return;
    }
    const QFileInfo info(local);
    edit->stamp = info.lastModified();
    edit->size = info.size();
    const QString how = launch(local);
    if (edit->watch)
        m_editWatcher->addPath(local);
    m_toasts->notify(tr("%1 · %2").arg(name, elapsedText(elapsedMs)), ToastHost::Kind::Success,
                     edit->watch ? tr("%1. Сохраните файл — изменения сразу уйдут на сервер.").arg(how)
                                 : how);
}

void SessionView::quickStored(quint64 token, const QString &path, const QString &error)
{
    const auto found = m_quickOps.find(token);
    if (found == m_quickOps.end())
        return;
    const QuickOp op = found.value();
    m_quickOps.erase(found);
    auto edit = m_edits.find(op.local);
    const QString name = QFileInfo(op.local).fileName();
    if (edit != m_edits.end())
        edit->uploading = false;
    if (error.isEmpty()) {
        m_toasts->notify(tr("Сохранено на сервере · %1").arg(elapsedText(op.clock.elapsed())),
                         ToastHost::Kind::Success, path);
        if (parentOf(path) == m_remotePath)
            refreshRemote();
    } else {
        m_toasts->notify(tr("Не удалось сохранить %1 на сервере").arg(name),
                         ToastHost::Kind::Error, error);
        if (edit != m_edits.end())
            edit->dirty = true;
    }
    if (edit != m_edits.end() && edit->dirty)
        m_editSettle.start();
}

QString SessionView::launch(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
    QFile probe(path);
    const bool binary = probe.open(QIODevice::ReadOnly) && probe.read(8192).contains('\0');
    probe.close();
    if (!binary) {
        Editors::Choice used;
        if (Editors::open(path, &used)) {
            if (used.fellBack)
                return tr("%1 не найден — открыто в программе «%2»")
                    .arg(Editors::nameOf(Editors::preferred()), used.editor.name);
            return tr("Открыто в программе «%1»").arg(used.editor.name);
        }
    }
#ifdef Q_OS_WIN
    const auto result = reinterpret_cast<qintptr>(
        ShellExecuteW(nullptr, L"open", reinterpret_cast<const wchar_t *>(native.utf16()), nullptr,
                      nullptr, SW_SHOWNORMAL));
    if (result > 32)
        return tr("Открыто программой по умолчанию");
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    return tr("Открыто программой по умолчанию");
}

void SessionView::editJobFinished(const TransferStatus &status)
{
    const auto found = m_editJobs.constFind(status.id);
    if (found == m_editJobs.cend())
        return;
    const QString local = found.value();
    m_editJobs.remove(status.id);
    auto edit = m_edits.find(local);
    if (edit == m_edits.end())
        return;
    const QString name = QFileInfo(local).fileName();

    if (!status.upload) {
        if (status.state != TransferState::Completed) {
            m_toasts->notify(tr("Не удалось открыть %1").arg(name), ToastHost::Kind::Error,
                             status.detail);
            m_edits.erase(edit);
            return;
        }
        const QFileInfo info(local);
        edit->stamp = info.lastModified();
        edit->size = info.size();
        const QString how = launch(local);
        if (edit->watch) {
            m_editWatcher->addPath(local);
            m_toasts->notify(tr("Правка: %1").arg(name), ToastHost::Kind::Success,
                             tr("%1. Сохраните файл — изменения сразу уйдут на сервер.").arg(how));
        }
        return;
    }

    edit->uploading = false;
    if (status.state == TransferState::Completed) {
        m_toasts->notify(tr("Сохранено на сервере: %1").arg(name), ToastHost::Kind::Success,
                         edit->remotePath);
    } else {
        m_toasts->notify(tr("Не удалось сохранить %1 на сервере").arg(name),
                         ToastHost::Kind::Error, status.detail);
        edit->dirty = true;
    }
    if (edit->dirty)
        m_editSettle.start();
}

void SessionView::localFileChanged(const QString &path)
{
    auto edit = m_edits.find(path);
    if (edit == m_edits.end() || !edit->watch)
        return;
    if (!m_editWatcher->files().contains(path) && QFileInfo::exists(path))
        m_editWatcher->addPath(path);
    edit->dirty = true;
    m_editSettle.start();
}

void SessionView::uploadEdits()
{
    bool waiting = false;
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        EditedFile &edit = it.value();
        if (!edit.dirty || !edit.watch)
            continue;
        const QFileInfo info(it.key());
        if (!info.exists()) {
            waiting = true;
            continue;
        }
        if (!m_editWatcher->files().contains(it.key()))
            m_editWatcher->addPath(it.key());
        if (info.lastModified() == edit.stamp && info.size() == edit.size) {
            edit.dirty = false;
            continue;
        }
        if (edit.uploading || !m_session->online()) {
            waiting = true;
            continue;
        }
        edit.dirty = false;
        edit.uploading = true;
        edit.stamp = info.lastModified();
        edit.size = info.size();
        m_fileCache.remove(edit.remotePath);

        if (m_session->supportsQuickFiles() && info.size() <= QuickLimit) {
            QFile source(it.key());
            if (source.open(QIODevice::ReadOnly)) {
                QuickOp op;
                op.remote = edit.remotePath;
                op.local = it.key();
                op.upload = true;
                op.clock.start();
                m_quickOps.insert(m_session->storeFile(edit.remotePath, source.readAll()), op);
                continue;
            }
        }

        TransferJob job;
        job.id = nextTransferId();
        job.upload = true;
        job.localPath = it.key();
        job.remotePath = edit.remotePath;
        job.size = info.size();
        job.overwrite = true;
        m_editJobs.insert(job.id, it.key());
        m_session->transfers()->enqueue({job});
    }
    if (waiting)
        m_editSettle.start(1500);
}
