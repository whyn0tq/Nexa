#include "fileviews.h"
#include "remotefilemodel.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFile>
#include <QTemporaryDir>

#include <cstring>
#include <memory>
#include <string>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    RemoteFileView remoteView;
    remoteView.resize(320, 200);
    remoteView.show();
    QApplication::processEvents();
    bool localDropReceived = false;
    QTemporaryDir temporaryDirectory;
    const QString qtDropPath =
        temporaryDirectory.filePath(QStringLiteral("example.txt"));
    QFile qtDropFile(qtDropPath);
    if (!temporaryDirectory.isValid() ||
        !qtDropFile.open(QIODevice::WriteOnly))
        return 2;
    qtDropFile.close();
    QObject::connect(&remoteView, &RemoteFileView::localUrlsDropped,
                     [&localDropReceived](const QList<QUrl> &urls,
                                          const QModelIndex &) {
                         localDropReceived =
                             urls.size() == 1 && urls.first().isLocalFile();
                     });
    QMimeData localMime;
    localMime.setUrls({QUrl::fromLocalFile(qtDropPath)});
    QDragEnterEvent localEnter(
        QPoint(5, 5), Qt::CopyAction, &localMime,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(remoteView.viewport(), &localEnter);
    QDropEvent localDrop(
        QPointF(5, 5), Qt::CopyAction, &localMime,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(remoteView.viewport(), &localDrop);
    const bool qtLocalDropReceived = localDropReceived;

    localDropReceived = false;
    QMimeData explorerMime;
    const QString explorerPath =
        temporaryDirectory.filePath(
            QStringLiteral("explorer-example.txt"));
    QFile explorerDropFile(explorerPath);
    if (!explorerDropFile.open(QIODevice::WriteOnly))
        return 3;
    explorerDropFile.close();
    QByteArray explorerPathData(
        reinterpret_cast<const char *>(explorerPath.utf16()),
        (explorerPath.size() + 1) *
            static_cast<qsizetype>(sizeof(char16_t)));
    explorerMime.setData(
        QStringLiteral(
            "application/x-qt-windows-mime;value=\"FileNameW\""),
        explorerPathData);
    QDragEnterEvent explorerEnter(
        QPoint(5, 5), Qt::CopyAction, &explorerMime,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(remoteView.viewport(), &explorerEnter);
    QDropEvent explorerDrop(
        QPointF(5, 5), Qt::CopyAction, &explorerMime,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(remoteView.viewport(), &explorerDrop);
    const bool explorerLocalDropReceived = localDropReceived;

    LocalFileView localView;
    bool remoteDropReceived = false;
    QObject::connect(&localView, &LocalFileView::remoteDataDropped,
                     [&remoteDropReceived](const QByteArray &data,
                                           const QModelIndex &) {
                         remoteDropReceived = !data.isEmpty();
                     });
    RemoteFileModel model;
    RemoteEntry entry;
    entry.name = QStringLiteral("remote.txt");
    entry.path = QStringLiteral("/remote.txt");
    model.setEntries({entry});
    std::unique_ptr<QMimeData> remoteMime(
        model.mimeData({model.index(0, 0)}));
    QDragEnterEvent remoteEnter(
        QPoint(5, 5), Qt::CopyAction, remoteMime.get(),
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(localView.viewport(), &remoteEnter);
    QDropEvent remoteDrop(
        QPointF(5, 5), Qt::CopyAction, remoteMime.get(),
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(localView.viewport(), &remoteDrop);

    return qtLocalDropReceived && explorerLocalDropReceived &&
                   remoteDropReceived
               ? 0
               : 1;
}
