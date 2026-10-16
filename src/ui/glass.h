#pragma once

#include "icons.h"

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QPropertyAnimation;
class FrameAnimation;
class QVBoxLayout;
class QTimer;

namespace Glass {

QImage blurred(const QImage &source, int radius);

enum class Tone { Panel, Raised, Sheet, Bar };
void paintPanel(QPainter &painter, const QRectF &rect, qreal radius,
                Tone tone = Tone::Panel);

}

class Backdrop final : public QWidget {
    Q_OBJECT

public:
    explicit Backdrop(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPixmap m_cache;
};

class GlassPanel : public QFrame {
    Q_OBJECT

public:
    explicit GlassPanel(Glass::Tone tone = Glass::Tone::Panel,
                        int radius = 16, QWidget *parent = nullptr);
    void setTone(Glass::Tone tone);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Glass::Tone m_tone;
    int m_radius;
};

class Sheet : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)

public:
    Sheet(QWidget *host, int width);

    QVBoxLayout *body() const { return m_body; }
    QWidget *card() const { return m_card; }
    void open();
    void dismiss();
    void setTitle(const QString &title, const QString &subtitle = QString(),
                  Icon icon = Icon::Info, const QColor &iconColor = QColor());

    qreal progress() const { return m_progress; }
    void setProgress(qreal value);
    void relayout();

signals:
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void rejected() { dismiss(); }

private:
    void layoutCard();
    void positionCard();
    void freezeCard();

    QPointer<QWidget> m_host;
    QWidget *m_card = nullptr;
    QVBoxLayout *m_body = nullptr;
    QWidget *m_header = nullptr;
    QPixmap m_snapshot;
    QPixmap m_plain;
    QPixmap m_cardImage;
    bool m_cardFrozen = false;
    FrameAnimation *m_animation = nullptr;
    qreal m_progress = 0;
    int m_width;
    bool m_closing = false;
};

class ToastHost final : public QWidget {
    Q_OBJECT

public:
    enum class Kind { Info, Success, Warning, Error };

    explicit ToastHost(QWidget *parent);
    int notify(const QString &text, Kind kind = Kind::Info,
               const QString &detail = QString());
    void dismiss(int id);
    void setBottomMargin(int margin);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void relayout();

    QVector<QWidget *> m_toasts;
    int m_nextId = 1;
    int m_bottomMargin = 16;
};
