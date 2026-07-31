#pragma once

#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QLineEdit;
class QStackedWidget;

class PathBar final : public QWidget {
    Q_OBJECT

public:
    enum class Style { Remote, Local };

    explicit PathBar(Style style, QWidget *parent = nullptr);

    QString path() const { return m_path; }

    void setPath(const QString &path, bool recordHistory = true);

    bool canGoBack() const { return m_historyIndex > 0; }
    bool canGoForward() const { return m_historyIndex + 1 < m_history.size(); }
    QString parentPath() const;

    void beginEdit();

public slots:
    void goBack();
    void goForward();
    void goUp();

signals:
    void pathActivated(const QString &path);
    void historyChanged();

private:
    void rebuildCrumbs();
    QString normalize(const QString &value) const;

    const Style m_style;
    QString m_path;
    QStringList m_history;
    int m_historyIndex = -1;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_crumbHost = nullptr;
    QHBoxLayout *m_crumbLayout = nullptr;
    QLineEdit *m_editor = nullptr;
};
