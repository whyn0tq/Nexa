#pragma once

#include "hoststore.h"
#include "types.h"

#include <QVector>
#include <QWidget>

class Backdrop;
class LauncherPage;
class QStackedWidget;
class Session;
class SessionView;
class TitleBar;
class ToastHost;
class TransferDock;

class MainWindow final : public QWidget {
    Q_OBJECT
    friend class DemoTour;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void restoreSessions();

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct Tab {
        Session *session = nullptr;
        SessionView *view = nullptr;
    };

    void buildShortcuts();
    void openHost(const QString &id, bool forceNew = false);
    void openProfile(const ConnectionProfile &profile, const QString &title,
                     const QString &hostId, int color);
    void closeSession(quint64 id, bool confirmTransfers = true);
    void activate(quint64 id);
    void refreshTabs();
    void editHost(const QString &id);
    void addHost(const ConnectionProfile &prefill = ConnectionProfile());
    void hostMenu(const QString &id, const QPoint &position);
    void tabMenu(quint64 id, const QPoint &position);
    void quickConnect(const QString &address);
    void showPalette();
    void saveOpenSessions();
    void updateDock();
    Tab *tab(quint64 id);
    SessionView *currentView() const;
    int activeTransfers() const;

    Backdrop *m_root = nullptr;
    TitleBar *m_titleBar = nullptr;
    QStackedWidget *m_pages = nullptr;
    LauncherPage *m_launcher = nullptr;
    TransferDock *m_dock = nullptr;
    ToastHost *m_toasts = nullptr;
    QVector<Tab> m_tabs;
    quint64 m_current = 0;
    quint64 m_nextSessionId = 1;
    bool m_framePrepared = false;
    bool m_closing = false;
};
