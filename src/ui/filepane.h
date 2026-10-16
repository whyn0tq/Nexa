#pragma once

#include "glass.h"
#include "types.h"

#include <QStringList>
#include <QWidget>

class FileFilterProxy;
class FileListModel;
class FileView;
class IconButton;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class Breadcrumb final : public QWidget {
    Q_OBJECT

public:
    explicit Breadcrumb(bool remote, QWidget *parent = nullptr);
    void setPath(const QString &path);
    void beginEdit();
    QSize sizeHint() const override;

signals:
    void pathActivated(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Segment {
        QString label;
        QString path;
        QRectF rect;
        bool root = false;
    };
    void relayout();
    int segmentAt(const QPoint &point) const;

    bool m_remote;
    QString m_path;
    QVector<Segment> m_segments;
    int m_hovered = -1;
    int m_firstVisible = 0;
    QLineEdit *m_editor = nullptr;
};

class FilePane final : public GlassPanel {
    Q_OBJECT

public:
    explicit FilePane(bool remote, QWidget *parent = nullptr);

    bool isRemote() const { return m_remote; }
    void setCaption(const QString &caption, Icon icon);
    void setPath(const QString &path, bool record = true);
    QString path() const { return m_path; }
    void setEntries(const RemoteEntries &entries);
    void removeEntries(const QStringList &paths);
    void showLoading(const QString &text);
    void showMessage(Icon icon, const QString &title, const QString &text,
                     const QString &action = QString());
    void showList();
    void setInteractive(bool interactive);

    FileView *view() const { return m_view; }
    FileListModel *model() const { return m_model; }
    RemoteEntries selectedEntries() const;
    const RemoteEntry *entryAt(const QModelIndex &proxyIndex) const;
    const RemoteEntry *entryAtViewport(const QPoint &point) const;
    void focusFilter();
    void editPath();
    void goBack();
    void goForward();
    void goUp();

signals:
    void navigateRequested(const QString &path);
    void refreshRequested();
    void entryOpened(const RemoteEntry &entry);
    void contextMenuRequested(const QPoint &globalPosition, bool onItem);
    void messageActionTriggered();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateFooter();
    void updateButtons();
    static QString parentOf(const QString &path, bool remote);

    bool m_remote;
    QString m_path;
    QStringList m_back;
    QStringList m_forward;

    QLabel *m_captionIcon = nullptr;
    QLabel *m_caption = nullptr;
    QLabel *m_footer = nullptr;
    IconButton *m_backButton = nullptr;
    IconButton *m_forwardButton = nullptr;
    IconButton *m_upButton = nullptr;
    IconButton *m_refreshButton = nullptr;
    Breadcrumb *m_crumbs = nullptr;
    QLineEdit *m_filter = nullptr;
    QStackedWidget *m_stack = nullptr;
    FileView *m_view = nullptr;
    FileListModel *m_model = nullptr;
    FileFilterProxy *m_proxy = nullptr;
    QWidget *m_messagePage = nullptr;
    QLabel *m_messageIcon = nullptr;
    QLabel *m_messageTitle = nullptr;
    QLabel *m_messageText = nullptr;
    QPushButton *m_messageAction = nullptr;
};
