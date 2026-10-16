#pragma once

#include <QColor>
#include <QTimer>
#include <QVector>
#include <QWidget>

class IconButton;

class TabStrip final : public QWidget {
    Q_OBJECT

public:
    enum class Status { None, Online, Busy, Offline };

    struct Tab {
        quint64 id = 0;
        QString title;
        QString tooltip;
        QColor label;
        Status status = Status::None;
    };

    explicit TabStrip(QWidget *parent = nullptr);

    void setTabs(const QVector<Tab> &tabs);
    void updateTab(const Tab &tab);
    void setCurrent(quint64 id);
    quint64 current() const { return m_current; }
    bool isDragArea(const QPoint &point) const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void activated(quint64 id);
    void closeRequested(quint64 id);
    void newRequested();
    void contextMenuRequested(quint64 id, const QPoint &globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    bool event(QEvent *event) override;

private:
    void relayout();
    int tabAt(const QPoint &point) const;
    QRectF closeRect(int index) const;

    QVector<Tab> m_tabs;
    QVector<QRectF> m_rects;
    QRectF m_plusRect;
    quint64 m_current = 0;
    int m_hovered = -1;
    int m_pressed = -1;
    bool m_hoverClose = false;
    bool m_hoverPlus = false;
    int m_dragHover = -1;
    QTimer m_dragTimer;
    int m_pulseTimer = 0;
    qreal m_phase = 0;
};

class TitleBar final : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    TabStrip *tabs() const { return m_tabs; }
    IconButton *maximizeButton() const { return m_maximize; }
    bool isCaption(const QPoint &point) const;
    void setMaximized(bool maximized);
    void setTransferBadge(int active);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void paletteRequested();
    void transfersRequested();
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    TabStrip *m_tabs = nullptr;
    QWidget *m_search = nullptr;
    IconButton *m_transfers = nullptr;
    IconButton *m_settings = nullptr;
    IconButton *m_minimize = nullptr;
    IconButton *m_maximize = nullptr;
    IconButton *m_close = nullptr;
    QWidget *m_logo = nullptr;
    int m_transferBadge = 0;
};
