#pragma once

#include "icons.h"

#include <QAbstractButton>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QTimer>
#include <QLabel>
#include <QStringList>
#include <QWidget>

class QPushButton;

class IconButton : public QAbstractButton {
    Q_OBJECT

public:
    enum class Style { Ghost, Glass, Accent, Danger };

    IconButton(Icon icon, const QString &tooltip, QWidget *parent = nullptr);
    void setIcon(Icon icon);
    void setStyle(Style style);
    void setIconSize(int size);
    void setForcedHover(bool hovered);
    void setForcedPressed(bool pressed);
    void setBadge(int count);
    QSize sizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    Icon m_icon;
    Style m_style = Style::Ghost;
    int m_iconSize = 18;
    bool m_hovered = false;
    bool m_keyboardFocus = false;
    bool m_forcedHover = false;
    bool m_forcedPressed = false;
    int m_badge = 0;
};

class Segmented final : public QWidget {
    Q_OBJECT

public:
    explicit Segmented(const QStringList &options, QWidget *parent = nullptr);
    int current() const { return m_current; }
    void setCurrent(int index);
    void setOptionEnabled(int index, bool enabled);
    void setCounts(const QVector<int> &counts, int alertIndex = -1);
    QSize sizeHint() const override;

signals:
    void changed(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int indexAt(const QPoint &point) const;

    QStringList m_options;
    QVector<bool> m_enabled;
    QVector<int> m_counts;
    int m_alert = -1;
    int m_current = 0;
    int m_hovered = -1;
};

class Dropdown final : public QWidget {
    Q_OBJECT

public:
    explicit Dropdown(const QStringList &items, QWidget *parent = nullptr);
    int current() const { return m_current; }
    void setCurrent(int index);
    void setPrefix(const QString &prefix);
    void setDetails(const QStringList &details);
    void setCompact(bool compact);
    QSize sizeHint() const override;

signals:
    void changed(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void openMenu();

    QStringList m_items;
    QStringList m_details;
    QString m_prefix;
    int m_current = 0;
    bool m_hovered = false;
    bool m_open = false;
    bool m_compact = false;
};

class StatusDot final : public QWidget {
    Q_OBJECT

public:
    explicit StatusDot(QWidget *parent = nullptr);
    void setColor(const QColor &color, bool pulsing = false);

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    QColor m_color;
    bool m_pulsing = false;
    int m_timer = 0;
    qreal m_phase = 0;
};

QPushButton *makeButton(const QString &text, const QString &variant = QString(),
                        Icon icon = Icon::Plus, bool withIcon = false);
QLabel *makeLabel(const QString &text, int pixelSize, const QColor &color,
                  QFont::Weight weight = QFont::Normal);
QString badgeHtml(const QString &text, const QColor &color);

class QMenu;
QMenu *makeMenu(QWidget *parent);
QAction *addMenuAction(QMenu *menu, Icon icon, const QString &text,
                       const QString &shortcut = QString());

int frameInterval(const QWidget *widget);
qreal animationClock();

class FrameAnimation final : public QObject {
    Q_OBJECT

public:
    explicit FrameAnimation(QWidget *owner);
    void start(qreal from, qreal to, int milliseconds,
               QEasingCurve::Type curve = QEasingCurve::OutCubic);
    void stop();
    bool isRunning() const { return m_timer.isActive(); }
    qreal value() const { return m_value; }

signals:
    void valueChanged(qreal value);
    void finished();

private:
    void tick();

    QWidget *m_owner;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QEasingCurve m_curve;
    qreal m_from = 0;
    qreal m_to = 0;
    qreal m_value = 0;
    int m_duration = 1;
};
