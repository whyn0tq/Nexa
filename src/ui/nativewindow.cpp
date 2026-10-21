#include "nativewindow.h"

#include "controls.h"
#include "filelist.h"
#include "titlebar.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QWidget>

#ifdef Q_OS_WIN
#include <QWindowsMimeConverter>
#include <qt_windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <array>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#endif

namespace Win {

#ifdef Q_OS_WIN
namespace {
HWND handleOf(QWidget *window)
{
    return reinterpret_cast<HWND>(window->winId());
}

int frameThickness(HWND hwnd, bool horizontal)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(horizontal ? SM_CXSIZEFRAME : SM_CYSIZEFRAME, dpi) +
           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

bool s_maxHovered = false;

void setMaxHover(TitleBar *bar, bool hovered, bool pressed = false)
{
    s_maxHovered = hovered;
    bar->maximizeButton()->setForcedHover(hovered);
    bar->maximizeButton()->setForcedPressed(pressed);
}
}
#endif

void prepareFrameless(QWidget *window)
{
#ifdef Q_OS_WIN
    const HWND hwnd = handleOf(window);
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE,
                      style | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX |
                          WS_MINIMIZEBOX);
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    const int round = 2;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof(round));
    const COLORREF border = RGB(40, 40, 46);
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    const MARGINS margins{0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#else
    Q_UNUSED(window);
#endif
}

bool handleNativeEvent(QWidget *window, TitleBar *titleBar, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    if (!window->internalWinId() ||
        hwnd != reinterpret_cast<HWND>(window->internalWinId()))
        return false;

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (!msg->wParam)
            return false;
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        if (IsZoomed(hwnd)) {
            const int x = frameThickness(hwnd, true);
            const int y = frameThickness(hwnd, false);
            params->rgrc[0].left += x;
            params->rgrc[0].right -= x;
            params->rgrc[0].top += y;
            params->rgrc[0].bottom -= y;
        }
        *result = 0;
        return true;
    }
    case WM_NCHITTEST: {
        RECT bounds;
        GetWindowRect(hwnd, &bounds);
        const int px = GET_X_LPARAM(msg->lParam) - bounds.left;
        const int py = GET_Y_LPARAM(msg->lParam) - bounds.top;
        const qreal ratio = window->devicePixelRatioF();
        const QPoint local(qRound(px / ratio), qRound(py / ratio));
        const int w = window->width();
        const int h = window->height();
        if (!IsZoomed(hwnd)) {
            const int edge = 6;
            const bool left = local.x() < edge;
            const bool right = local.x() >= w - edge;
            const bool top = local.y() < edge;
            const bool bottom = local.y() >= h - edge;
            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
        }
        const QPoint inBar = titleBar->mapFrom(window, local);
        if (titleBar->rect().contains(inBar)) {
            const QRect maxRect(titleBar->maximizeButton()->mapTo(titleBar, QPoint(0, 0)),
                                titleBar->maximizeButton()->size());
            if (maxRect.contains(inBar)) {
                *result = HTMAXBUTTON;
                return true;
            }
            if (titleBar->isCaption(inBar)) {
                *result = HTCAPTION;
                return true;
            }
        }
        *result = HTCLIENT;
        return true;
    }
    case WM_NCMOUSEMOVE:
        if (msg->wParam == HTMAXBUTTON) {
            if (!s_maxHovered) {
                setMaxHover(titleBar, true);
                TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
                TrackMouseEvent(&track);
            }
        } else if (s_maxHovered) {
            setMaxHover(titleBar, false);
        }
        return false;
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE:
        if (s_maxHovered)
            setMaxHover(titleBar, false);
        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            setMaxHover(titleBar, true, true);
            *result = 0;
            return true;
        }
        return false;
    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            setMaxHover(titleBar, false);
            if (window->isMaximized())
                window->showNormal();
            else
                window->showMaximized();
            *result = 0;
            return true;
        }
        return false;
    default:
        return false;
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(titleBar);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
#endif
}

#ifdef Q_OS_WIN
namespace {
class VirtualFiles final : public QWindowsMimeConverter {
public:
    VirtualFiles()
    {
        m_descriptor = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW));
        m_contents = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_FILECONTENTS));
    }

    bool canConvertFromMime(const FORMATETC &, const QMimeData *) const override { return false; }
    bool convertFromMime(const FORMATETC &, const QMimeData *, STGMEDIUM *) const override
    {
        return false;
    }
    QList<FORMATETC> formatsForMime(const QString &, const QMimeData *) const override
    {
        return {};
    }

    bool canConvertToMime(const QString &mimeType, IDataObject *data) const override
    {
        if (mimeType != Mime::Virtual || !data)
            return false;
        FORMATETC format{m_descriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        if (data->QueryGetData(&format) != S_OK)
            return false;
        FORMATETC drop{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        // Explorer's zip folder answers S_FALSE here, which FAILED() counts as success.
        return data->QueryGetData(&drop) != S_OK;
    }

    QString mimeForFormat(const FORMATETC &format) const override
    {
        return format.cfFormat == m_descriptor ? Mime::Virtual : QString();
    }

    QVariant convertToMime(const QString &mimeType, IDataObject *data,
                           QMetaType preferredType) const override
    {
        Q_UNUSED(preferredType);
        if (!canConvertToMime(mimeType, data))
            return {};
        return materialize(data).join(QLatin1Char('\n')).toUtf8();
    }

private:
    bool writeFile(IDataObject *data, UINT index, const QString &path) const
    {
        FORMATETC format{m_contents, nullptr, DVASPECT_CONTENT, static_cast<LONG>(index),
                         TYMED_ISTREAM | TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (FAILED(data->GetData(&format, &medium)))
            return false;
        QFile output(path);
        bool ok = output.open(QIODevice::WriteOnly | QIODevice::Truncate);
        if (ok && medium.tymed == TYMED_ISTREAM && medium.pstm) {
            std::array<char, 256 * 1024> buffer{};
            forever {
                ULONG read = 0;
                const HRESULT status = medium.pstm->Read(buffer.data(),
                                                         static_cast<ULONG>(buffer.size()), &read);
                if (read > 0 && output.write(buffer.data(), read) != static_cast<qint64>(read)) {
                    ok = false;
                    break;
                }
                if (status == S_FALSE || read == 0)
                    break;
                if (FAILED(status)) {
                    ok = false;
                    break;
                }
            }
        } else if (ok && medium.tymed == TYMED_HGLOBAL && medium.hGlobal) {
            const SIZE_T size = GlobalSize(medium.hGlobal);
            const void *bytes = GlobalLock(medium.hGlobal);
            ok = bytes && output.write(static_cast<const char *>(bytes),
                                       static_cast<qint64>(size)) == static_cast<qint64>(size);
            if (bytes)
                GlobalUnlock(medium.hGlobal);
        } else {
            ok = false;
        }
        output.close();
        ReleaseStgMedium(&medium);
        if (!ok)
            output.remove();
        return ok;
    }

    QStringList materialize(IDataObject *data) const
    {
        QStringList paths;
        FORMATETC format{m_descriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        if (FAILED(data->GetData(&format, &medium)))
            return paths;
        const auto *group = static_cast<const FILEGROUPDESCRIPTORW *>(GlobalLock(medium.hGlobal));
        if (!group) {
            ReleaseStgMedium(&medium);
            return paths;
        }
        const QString staging = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                                QStringLiteral("/Nexa-drop/") +
                                QUuid::createUuid().toString(QUuid::WithoutBraces);
        QDir().mkpath(staging);
        QSet<QString> roots;
        for (UINT i = 0; i < group->cItems; ++i) {
            QString relative = QDir::cleanPath(
                QString::fromWCharArray(group->fgd[i].cFileName).replace(QLatin1Char('\\'),
                                                                         QLatin1Char('/')));
            if (relative.isEmpty() || relative == QStringLiteral(".") ||
                QDir::isAbsolutePath(relative) || relative.startsWith(QStringLiteral("..")))
                continue;
            const QString root = relative.section(QLatin1Char('/'), 0, 0);
            const QString target = QDir(staging).filePath(relative);
            const bool directory = (group->fgd[i].dwFlags & FD_ATTRIBUTES) &&
                                   (group->fgd[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            if (directory) {
                QDir().mkpath(target);
                roots.insert(root);
                continue;
            }
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (writeFile(data, i, target))
                roots.insert(root);
        }
        GlobalUnlock(medium.hGlobal);
        ReleaseStgMedium(&medium);
        for (const QString &root : std::as_const(roots))
            paths.append(QDir(staging).filePath(root));
        return paths;
    }

    CLIPFORMAT m_descriptor = 0;
    CLIPFORMAT m_contents = 0;
};
}
#endif

void installVirtualFileSupport()
{
#ifdef Q_OS_WIN
    static VirtualFiles *converter = new VirtualFiles;
    Q_UNUSED(converter);
#endif
}

}
