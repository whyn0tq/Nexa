#include "fileviews.h"

#include "remotefilemodel.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QApplication>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include <cstring>
#include <array>
#include <iterator>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <oleidl.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#ifdef Q_OS_WIN
class ExplorerDropTarget final : public IDropTarget {
public:
    explicit ExplorerDropTarget(RemoteFileView *view) : m_view(view) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,
                                             void **object) override
    {
        if (!object)
            return E_POINTER;
        *object = nullptr;
        if (id == IID_IUnknown || id == IID_IDropTarget) {
            *object = static_cast<IDropTarget *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(
            InterlockedIncrement(&m_referenceCount));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG references =
            InterlockedDecrement(&m_referenceCount);
        if (references == 0)
            delete this;
        return static_cast<ULONG>(references);
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *dataObject, DWORD,
                                        POINTL, DWORD *effect) override
    {
        m_hasFiles = containsFileDrop(dataObject);
        setEffect(effect);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL,
                                       DWORD *effect) override
    {
        setEffect(effect);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        m_hasFiles = false;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject *dataObject, DWORD,
                                   POINTL point, DWORD *effect) override
    {
        QList<QUrl> urls = physicalFiles(dataObject);
        if (urls.isEmpty())
            urls = materializeVirtualFiles(dataObject);
        m_hasFiles = false;
        if (!urls.isEmpty() && m_view) {
            m_view->acceptExplorerDrop(
                urls, QPoint(static_cast<int>(point.x),
                             static_cast<int>(point.y)));
            if (effect)
                *effect = DROPEFFECT_COPY;
        } else if (effect) {
            *effect = DROPEFFECT_NONE;
        }
        return S_OK;
    }

private:
    static QList<QUrl> physicalFiles(IDataObject *dataObject)
    {
        QList<QUrl> urls;
        FORMATETC format{
            CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (!dataObject ||
            FAILED(dataObject->GetData(&format, &medium)))
            return urls;
        const HDROP drop = static_cast<HDROP>(medium.hGlobal);
        const UINT count =
            DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        urls.reserve(static_cast<qsizetype>(count));
        for (UINT index = 0; index < count; ++index) {
            const UINT length =
                DragQueryFileW(drop, index, nullptr, 0);
            std::wstring path(
                static_cast<size_t>(length) + 1, L'\0');
            if (DragQueryFileW(drop, index, path.data(),
                               length + 1) > 0) {
                urls.append(QUrl::fromLocalFile(
                    QString::fromWCharArray(
                        path.data(),
                        static_cast<qsizetype>(length))));
            }
        }
        ReleaseStgMedium(&medium);
        return urls;
    }

    static bool writeVirtualFile(IDataObject *dataObject, UINT index,
                                 const QString &path)
    {
        const CLIPFORMAT contentsFormat = static_cast<CLIPFORMAT>(
            RegisterClipboardFormatW(CFSTR_FILECONTENTS));
        FORMATETC format{contentsFormat, nullptr, DVASPECT_CONTENT,
                        static_cast<LONG>(index),
                        TYMED_ISTREAM | TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (FAILED(dataObject->GetData(&format, &medium)))
            return false;

        QFile output(path);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            ReleaseStgMedium(&medium);
            return false;
        }
        bool success = true;
        if (medium.tymed == TYMED_ISTREAM && medium.pstm) {
            std::array<char, 128 * 1024> buffer{};
            while (true) {
                ULONG read = 0;
                const HRESULT status = medium.pstm->Read(
                    buffer.data(), static_cast<ULONG>(buffer.size()),
                    &read);
                if (read > 0 &&
                    output.write(buffer.data(),
                                 static_cast<qint64>(read)) !=
                        static_cast<qint64>(read)) {
                    success = false;
                    break;
                }
                if (status == S_FALSE || read == 0)
                    break;
                if (FAILED(status)) {
                    success = false;
                    break;
                }
            }
        } else if (medium.tymed == TYMED_HGLOBAL &&
                   medium.hGlobal) {
            const SIZE_T size = GlobalSize(medium.hGlobal);
            const void *data = GlobalLock(medium.hGlobal);
            success = data &&
                      output.write(static_cast<const char *>(data),
                                   static_cast<qint64>(size)) ==
                          static_cast<qint64>(size);
            if (data)
                GlobalUnlock(medium.hGlobal);
        } else {
            success = false;
        }
        output.close();
        ReleaseStgMedium(&medium);
        if (!success)
            output.remove();
        return success;
    }

    static QList<QUrl> materializeVirtualFiles(
        IDataObject *dataObject)
    {
        QList<QUrl> urls;
        if (!dataObject)
            return urls;
        const CLIPFORMAT descriptorFormat = static_cast<CLIPFORMAT>(
            RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW));
        FORMATETC format{descriptorFormat, nullptr, DVASPECT_CONTENT,
                        -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (FAILED(dataObject->GetData(&format, &medium)))
            return urls;

        const auto *group = static_cast<const FILEGROUPDESCRIPTORW *>(
            GlobalLock(medium.hGlobal));
        if (!group) {
            ReleaseStgMedium(&medium);
            return urls;
        }

        const QString stagingPath =
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation) +
            QStringLiteral("/Nexa-drop/") +
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        QDir().mkpath(stagingPath);
        QSet<QString> roots;
        for (UINT index = 0; index < group->cItems; ++index) {
            QString relative =
                QString::fromWCharArray(group->fgd[index].cFileName)
                    .replace(QLatin1Char('\\'), QLatin1Char('/'));
            relative = QDir::cleanPath(relative);
            if (relative.isEmpty() ||
                relative == QStringLiteral(".") ||
                QDir::isAbsolutePath(relative) ||
                relative == QStringLiteral("..") ||
                relative.startsWith(QStringLiteral("../")))
                continue;

            roots.insert(relative.section(QLatin1Char('/'), 0, 0));
            const QString destination =
                QDir(stagingPath).filePath(relative);
            const bool directory =
                (group->fgd[index].dwFlags & FD_ATTRIBUTES) &&
                (group->fgd[index].dwFileAttributes &
                 FILE_ATTRIBUTE_DIRECTORY);
            if (directory) {
                QDir().mkpath(destination);
                continue;
            }
            QDir().mkpath(QFileInfo(destination).absolutePath());
            if (!writeVirtualFile(dataObject, index, destination))
                roots.remove(
                    relative.section(QLatin1Char('/'), 0, 0));
        }
        GlobalUnlock(medium.hGlobal);
        ReleaseStgMedium(&medium);

        for (const QString &root : roots) {
            const QString path = QDir(stagingPath).filePath(root);
            if (QFileInfo::exists(path))
                urls.append(QUrl::fromLocalFile(path));
        }
        return urls;
    }

    static bool containsFileDrop(IDataObject *dataObject)
    {
        if (!dataObject)
            return false;
        FORMATETC format{
            CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        if (SUCCEEDED(dataObject->QueryGetData(&format)))
            return true;
        const CLIPFORMAT descriptorFormat = static_cast<CLIPFORMAT>(
            RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW));
        format.cfFormat = descriptorFormat;
        return SUCCEEDED(dataObject->QueryGetData(&format));
    }

    void setEffect(DWORD *effect) const
    {
        if (effect)
            *effect = m_hasFiles ? DROPEFFECT_COPY
                                 : DROPEFFECT_NONE;
    }

    LONG m_referenceCount = 1;
    RemoteFileView *m_view = nullptr;
    bool m_hasFiles = false;
};
#endif

namespace {
bool hasPotentialLocalFiles(const QMimeData *mimeData)
{
    if (!mimeData)
        return false;
    if (mimeData->hasUrls() || mimeData->hasText())
        return true;
    for (const QString &format : mimeData->formats()) {
        if (format.contains(QStringLiteral("FileNameW"),
                            Qt::CaseInsensitive) ||
            format.contains(QStringLiteral("FileName"),
                            Qt::CaseInsensitive) ||
            format.contains(QStringLiteral("Shell IDList"),
                            Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QList<QUrl> extractLocalUrls(const QMimeData *mimeData)
{
    QList<QUrl> result;
    if (!mimeData)
        return result;
    for (const QUrl &url : mimeData->urls()) {
        if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile()))
            result.append(url);
    }
    if (!result.isEmpty())
        return result;

#ifdef Q_OS_WIN
    for (const QString &format : mimeData->formats()) {
        if (!format.contains(QStringLiteral("Shell IDList"),
                             Qt::CaseInsensitive))
            continue;
        const QByteArray raw = mimeData->data(format);
        if (raw.size() < static_cast<qsizetype>(2 * sizeof(UINT)))
            continue;

        const auto *bytes =
            reinterpret_cast<const unsigned char *>(raw.constData());
        UINT childCount = 0;
        memcpy(&childCount, bytes, sizeof(childCount));
        const quint64 offsetsBytes =
            static_cast<quint64>(childCount + 1) * sizeof(UINT);
        if (childCount == 0 || childCount > 65535 ||
            sizeof(UINT) + offsetsBytes >
                static_cast<quint64>(raw.size()))
            continue;

        const auto offsetAt = [&](UINT index) {
            UINT offset = 0;
            memcpy(&offset,
                   bytes + sizeof(UINT) +
                       static_cast<size_t>(index) * sizeof(UINT),
                   sizeof(offset));
            return offset;
        };
        const UINT parentOffset = offsetAt(0);
        if (parentOffset >= static_cast<UINT>(raw.size()))
            continue;
        const auto *parent = reinterpret_cast<const ITEMIDLIST *>(
            bytes + parentOffset);
        for (UINT index = 1; index <= childCount; ++index) {
            const UINT childOffset = offsetAt(index);
            if (childOffset >= static_cast<UINT>(raw.size()))
                continue;
            const auto *child = reinterpret_cast<const ITEMIDLIST *>(
                bytes + childOffset);
            PIDLIST_ABSOLUTE absolute = ILCombine(parent, child);
            if (!absolute)
                continue;
            wchar_t path[32768] = {};
            if (SHGetPathFromIDListEx(absolute, path,
                                      static_cast<DWORD>(std::size(path)),
                                      GPFIDL_DEFAULT) &&
                QFileInfo::exists(QString::fromWCharArray(path))) {
                result.append(QUrl::fromLocalFile(
                    QString::fromWCharArray(path)));
            }
            ILFree(absolute);
        }
        if (!result.isEmpty())
            return result;
    }
#endif

    for (const QString &format : mimeData->formats()) {
        const QByteArray raw = mimeData->data(format);
        QStringList paths;
        if (format.contains(QStringLiteral("FileNameW"),
                            Qt::CaseInsensitive) &&
            raw.size() >= 2) {
            const qsizetype length = raw.size() / 2;
            std::vector<char16_t> utf16(static_cast<size_t>(length));
            memcpy(utf16.data(), raw.constData(),
                   static_cast<size_t>(length) * sizeof(char16_t));
            paths = QString::fromUtf16(utf16.data(), length)
                        .split(QChar(u'\0'), Qt::SkipEmptyParts);
        } else if (format.contains(QStringLiteral("FileName"),
                                   Qt::CaseInsensitive) &&
                   !format.contains(QStringLiteral("FileNameW"),
                                    Qt::CaseInsensitive)) {
            paths = QString::fromLocal8Bit(raw)
                        .split(QChar(u'\0'), Qt::SkipEmptyParts);
        }
        for (const QString &path : paths) {
            QString cleanPath = path.trimmed();
            if (cleanPath.startsWith(QChar(0xfeff)))
                cleanPath.remove(0, 1);
            if (!cleanPath.isEmpty() && QFileInfo::exists(cleanPath))
                result.append(QUrl::fromLocalFile(cleanPath));
        }
        if (!result.isEmpty())
            return result;
    }

    if (mimeData->hasText()) {
        for (const QString &line :
             mimeData->text().split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                    Qt::SkipEmptyParts)) {
            const QUrl url = QUrl::fromUserInput(line.trimmed());
            if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile()))
                result.append(url);
        }
    }
    return result;
}
}

LocalFileView::LocalFileView(QWidget *parent) : QTreeView(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
}

void LocalFileView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(RemoteFileModel::remoteMimeType())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

void LocalFileView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(RemoteFileModel::remoteMimeType())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTreeView::dragMoveEvent(event);
}

void LocalFileView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(RemoteFileModel::remoteMimeType())) {
        emit remoteDataDropped(
            event->mimeData()->data(RemoteFileModel::remoteMimeType()),
            indexAt(event->position().toPoint()));
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTreeView::dropEvent(event);
}

RemoteFileView::RemoteFileView(QWidget *parent) : QTableView(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
#ifdef Q_OS_WIN
    viewport()->setAttribute(Qt::WA_NativeWindow);
    const HWND handle =
        reinterpret_cast<HWND>(viewport()->winId());
    DragAcceptFiles(handle, TRUE);
    qApp->installNativeEventFilter(this);
    const HRESULT oleResult = OleInitialize(nullptr);
    m_oleInitialized = SUCCEEDED(oleResult);
    m_explorerDropTarget = new ExplorerDropTarget(this);
    RevokeDragDrop(handle);
    if (FAILED(RegisterDragDrop(handle, m_explorerDropTarget))) {
        m_explorerDropTarget->Release();
        m_explorerDropTarget = nullptr;
    }
#endif
}

RemoteFileView::~RemoteFileView()
{
#ifdef Q_OS_WIN
    const HWND handle =
        reinterpret_cast<HWND>(viewport()->winId());
    if (m_explorerDropTarget) {
        RevokeDragDrop(handle);
        m_explorerDropTarget->Release();
        m_explorerDropTarget = nullptr;
    }
    qApp->removeNativeEventFilter(this);
    DragAcceptFiles(handle, FALSE);
    if (m_oleInitialized)
        OleUninitialize();
#endif
}

void RemoteFileView::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasPotentialLocalFiles(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTableView::dragEnterEvent(event);
}

void RemoteFileView::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasPotentialLocalFiles(event->mimeData())) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTableView::dragMoveEvent(event);
}

void RemoteFileView::dropEvent(QDropEvent *event)
{
    if (hasPotentialLocalFiles(event->mimeData())) {
        emit localUrlsDropped(extractLocalUrls(event->mimeData()),
                              indexAt(event->position().toPoint()));
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    QTableView::dropEvent(event);
}

#ifdef Q_OS_WIN
void RemoteFileView::acceptExplorerDrop(
    const QList<QUrl> &urls, const QPoint &globalPosition)
{
    emit localUrlsDropped(
        urls, indexAt(viewport()->mapFromGlobal(globalPosition)));
}

bool RemoteFileView::nativeEventFilter(const QByteArray &eventType,
                                       void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    auto *nativeMessage = static_cast<MSG *>(message);
    if (nativeMessage &&
        nativeMessage->hwnd ==
            reinterpret_cast<HWND>(viewport()->winId()) &&
        nativeMessage->message == WM_DROPFILES) {
        const auto drop = reinterpret_cast<HDROP>(nativeMessage->wParam);
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        QList<QUrl> urls;
        urls.reserve(static_cast<qsizetype>(count));
        for (UINT i = 0; i < count; ++i) {
            const UINT length = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring path(static_cast<size_t>(length) + 1, L'\0');
            if (DragQueryFileW(drop, i, path.data(), length + 1) > 0) {
                urls.append(QUrl::fromLocalFile(
                    QString::fromWCharArray(path.c_str(),
                                           static_cast<qsizetype>(length))));
            }
        }
        POINT point{};
        DragQueryPoint(drop, &point);
        DragFinish(drop);
        if (!urls.isEmpty())
            emit localUrlsDropped(urls, indexAt(QPoint(point.x, point.y)));
        if (result)
            *result = 0;
        return true;
    }
    return false;
}
#endif
