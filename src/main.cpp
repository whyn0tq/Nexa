#include "mainwindow.h"

#include <QApplication>

#include <libssh/libssh.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_UNUSED(app);
    QApplication::setOrganizationName(QStringLiteral("Nexa"));
    QApplication::setApplicationName(QStringLiteral("Nexa"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    if (ssh_init() != SSH_OK)
        return 1;

    int result = 0;
    {
        MainWindow window;
        window.show();
        result = QApplication::exec();
    }
    ssh_finalize();
    return result;
}
