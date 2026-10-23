#pragma once

#include "glass.h"

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QStackedLayout;

class CommandPalette final : public Sheet {
    Q_OBJECT

public:
    struct Command {
        Icon icon = Icon::Command;
        QString title;
        QString subtitle;
        QString group;
        std::function<void()> run;
    };

    CommandPalette(QWidget *host, const QVector<Command> &commands);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refill();
    void runCurrent();

    QVector<Command> m_commands;
    QVector<int> m_visible;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QStackedLayout *m_results = nullptr;
    QLabel *m_nothing = nullptr;
};
