#pragma once

#include "hoststore.h"

#include <QWidget>

class QGridLayout;
class QLineEdit;
class QLabel;
class QScrollArea;

class HostCard final : public QWidget {
    Q_OBJECT

public:
    HostCard(const SavedHost &host, bool open, QWidget *parent = nullptr);

signals:
    void connectRequested(const QString &id);
    void editRequested(const QString &id);
    void menuRequested(const QString &id, const QPoint &globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QRectF moreRect() const;

    SavedHost m_host;
    bool m_open;
    bool m_hovered = false;
    bool m_hoverMore = false;
};

class LauncherPage final : public QWidget {
    Q_OBJECT

public:
    explicit LauncherPage(QWidget *parent = nullptr);
    void setOpenHosts(const QStringList &ids);
    void focusQuickConnect();

signals:
    void connectHost(const QString &id);
    void editHost(const QString &id);
    void hostMenu(const QString &id, const QPoint &globalPosition);
    void addHost();
    void quickConnect(const QString &address);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();

    QLineEdit *m_quick = nullptr;
    QLineEdit *m_search = nullptr;
    QWidget *m_grid = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    QWidget *m_empty = nullptr;
    QLabel *m_count = nullptr;
    QScrollArea *m_scroll = nullptr;
    QStringList m_open;
    int m_columns = 0;
};
