#pragma once

#include "icons.h"
#include "session.h"
#include "types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class FilePane;
class IconButton;
class QFileSystemWatcher;
class QLabel;
class QMimeData;
class QPushButton;
class QSplitter;
class Session;
class StatusDot;
class ToastHost;
class GlassPanel;

class DropOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit DropOverlay(QWidget *parent);
    void showTarget(const QRect &area, Icon icon, const QString &text);
    void hideSoon();
    void hideNow();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRect m_area;
    Icon m_icon = Icon::Upload;
    QString m_text;
    QTimer m_hideTimer;
};

class SessionView final : public QWidget {
    Q_OBJECT

public:
    SessionView(Session *session, ToastHost *toasts, QWidget *parent = nullptr);
    ~SessionView() override;

    void showLocalPlaceholder(const QString &path, const RemoteEntries &entries);

    Session *session() const { return m_session; }
    QString remotePath() const { return m_remotePath; }
    QString localPath() const { return m_localPath; }

    void refreshRemote();
    void refreshLocal();
    void uploadPaths(const QStringList &paths, const QString &remoteDirectory = QString());
    void uploadDialog(bool folder);
    void createRemoteFolder();
    void createRemoteFile();
    void renameRemote();
    void deleteRemote();
    void downloadSelected();
    void uploadSelected();
    void focusRemoteFilter();
    void editRemotePath();
    void navigateBack();
    void navigateForward();
    void openSelected();
    void deleteFocused();
    void renameFocused();
    void deleteLocal();
    void renameLocal();
    void createLocalFolder();

signals:
    void titleChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct DropPlan {
        enum class Kind { None, Upload, Download } kind = Kind::None;
        QString directory;
        int row = -1;
        FilePane *pane = nullptr;
    };

    QWidget *buildHeader();
    void updateHeader();
    void onState();
    void onReady(bool reconnected);
    void navigateRemote(const QString &path, bool record = true);
    void rememberListing(const QString &path, const RemoteEntries &entries);
    void saveListings();
    void navigateLocal(const QString &path, bool record = true);
    void openRemoteEntry(const RemoteEntry &entry);
    void openLocalEntry(const RemoteEntry &entry);
    void remoteMenu(const QPoint &position, bool onItem);
    void localMenu(const QPoint &position, bool onItem);
    void download(const RemoteEntries &entries, const QString &localDirectory);
    void enqueue(const QVector<TransferJob> &jobs);

    DropPlan planDrop(QWidget *target, const QPoint &position, const QMimeData *mime) const;
    bool handleDrag(QWidget *target, QEvent *event);
    static QStringList localPaths(const QMimeData *data);
    bool localFocused() const;

    struct EditedFile {
        QString remotePath;
        QDateTime stamp;
        qint64 size = -1;
        bool watch = false;
        bool uploading = false;
        bool dirty = false;
    };
    struct QuickOp {
        QString remote;
        QString local;
        QElapsedTimer clock;
        qint64 size = -1;
        QDateTime modified;
        bool upload = false;
        int toast = 0;
    };
    struct CachedFile {
        QByteArray data;
        qint64 size = -1;
        QDateTime modified;
    };
    void openRemoteFile(const RemoteEntry &entry, bool edit);
    void prefetchSelected();
    void quickFetched(quint64 token, const QString &path, const QByteArray &content,
                      const QString &error);
    void quickStored(quint64 token, const QString &path, const QString &error);
    void openLocalCopy(const QString &local, const QByteArray &content, qint64 elapsedMs);
    void editJobFinished(const TransferStatus &status);
    void localFileChanged(const QString &path);
    void uploadEdits();
    QString launch(const QString &path);

    QPointer<Session> m_session;
    ToastHost *m_toasts;
    GlassPanel *m_header = nullptr;
    StatusDot *m_dot = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_endpoint = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_reconnect = nullptr;
    QSplitter *m_splitter = nullptr;
    FilePane *m_local = nullptr;
    FilePane *m_remote = nullptr;
    DropOverlay *m_overlay = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_localRefresh;
    QFileSystemWatcher *m_editWatcher = nullptr;
    QTimer m_editSettle;
    QHash<QString, EditedFile> m_edits;
    QHash<quint64, QString> m_editJobs;
    QHash<quint64, QuickOp> m_quickOps;
    QHash<QString, CachedFile> m_fileCache;
    QStringList m_fileCacheOrder;
    QTimer m_selectionPrefetch;

    QString m_localPath;
    QString m_remotePath;
    quint64 m_listGeneration = 0;
    QHash<QString, RemoteEntries> m_cache;
    QHash<QString, qint64> m_cacheStamp;
    QElapsedTimer m_cacheClock;
    QStringList m_cacheOrder;
    QTimer m_cacheSave;
    bool m_navigatedWhileConnecting = false;
    bool m_localPlaceholder = false;
    bool m_navigatedOnce = false;
    bool m_remoteLoading = false;
};
