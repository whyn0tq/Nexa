#pragma once

#include "hoststore.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QScrollArea;
class QVBoxLayout;

class HostRow final : public QWidget {
    Q_OBJECT

public:
    HostRow(const SavedHost &host, bool open, QWidget *parent = nullptr);
    static constexpr int RowHeight = 52;

signals:
    void connectRequested(const QString &id);
    void menuRequested(const QString &id, const QPoint &globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    SavedHost m_host;
    bool m_open;
    bool m_hovered = false;
};

class LauncherPage final : public QWidget {
    Q_OBJECT

public:
    explicit LauncherPage(QWidget *parent = nullptr);
    void setOpenHosts(const QStringList &ids);
    void focusQuickConnect();

signals:
    void quickConnect(const QString &address);
    void connectHost(const QString &id);
    void hostMenu(const QString &id, const QPoint &globalPosition);
    void addHost();

private:
    void submit();
    void showError(const QString &text);
    void rebuild();

    QWidget *m_bar = nullptr;
    QLineEdit *m_quick = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_count = nullptr;
    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_rows = nullptr;
    QLabel *m_empty = nullptr;
    QStringList m_open;
};
