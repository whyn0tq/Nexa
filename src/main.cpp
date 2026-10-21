#include "demo.h"
#include "icons.h"
#include "i18n.h"
#include "mainwindow.h"
#include "nativewindow.h"
#include "theme.h"

#include <QApplication>
#include <QProcess>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QSettings>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTimer>

#include <libssh/libssh.h>

#include <optional>

namespace {
bool exportIcon(const QString &path)
{
    const QList<int> sizes{16, 20, 24, 32, 40, 48, 64, 128, 256};
    QList<QByteArray> images;
    for (const int size : sizes) {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        Icons::logoImage(size).save(&buffer, "PNG");
        images.append(png);
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out << quint16(0) << quint16(1) << quint16(sizes.size());
    quint32 offset = 6 + 16 * static_cast<quint32>(sizes.size());
    for (int i = 0; i < sizes.size(); ++i) {
        const quint8 dimension = sizes.at(i) >= 256 ? 0 : static_cast<quint8>(sizes.at(i));
        out << dimension << dimension << quint8(0) << quint8(0) << quint16(1) << quint16(32)
            << quint32(images.at(i).size()) << offset;
        offset += static_cast<quint32>(images.at(i).size());
    }
    for (const QByteArray &image : images)
        out.writeRawData(image.constData(), static_cast<int>(image.size()));
    return true;
}
}

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Nexa"));
    QApplication::setApplicationName(QStringLiteral("Nexa"));
    QApplication::setApplicationVersion(QStringLiteral(NEXA_VERSION));

    const QStringList arguments = QApplication::arguments();
    const auto option = [&arguments](const char *name) {
        const qsizetype at = arguments.indexOf(QLatin1String(name));
        return at >= 0 && at + 1 < arguments.size() ? arguments.at(at + 1) : QString();
    };
    const QString demoShots = option("--demo-shots");
    std::optional<QTemporaryDir> demoProfile;
    if (!demoShots.isEmpty()) {
        demoProfile.emplace();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, demoProfile->path());
    }
    if (const QString directory = option("--settings"); !directory.isEmpty() && demoShots.isEmpty()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
    }
    const qsizetype exportAt = arguments.indexOf(QStringLiteral("--export-icon"));
    if (exportAt >= 0 && exportAt + 1 < arguments.size())
        return exportIcon(arguments.at(exportAt + 1)) ? 0 : 1;

    if (const QString language = option("--language"); !language.isEmpty())
        I18n::setLanguage(I18n::fromCode(language));
    I18n::install();
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    QPalette palette;
    palette.setColor(QPalette::Window, Theme::Base);
    palette.setColor(QPalette::Base, QColor(18, 18, 21));
    palette.setColor(QPalette::AlternateBase, QColor(24, 24, 28));
    palette.setColor(QPalette::Text, Theme::Text);
    palette.setColor(QPalette::WindowText, Theme::Text);
    palette.setColor(QPalette::ButtonText, Theme::Text);
    palette.setColor(QPalette::Button, QColor(28, 28, 32));
    palette.setColor(QPalette::Highlight, Theme::Accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, Theme::TextMuted);
    palette.setColor(QPalette::ToolTipBase, QColor(20, 20, 23));
    palette.setColor(QPalette::ToolTipText, Theme::Text);
    palette.setColor(QPalette::Disabled, QPalette::Text, Theme::TextFaint);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, Theme::TextFaint);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, Theme::TextFaint);
    QApplication::setPalette(palette);
    QApplication::setFont(Theme::font(13));
    app.setStyleSheet(Theme::styleSheet());
    QApplication::setWindowIcon(Icons::appIcon());

    if (ssh_init() != SSH_OK)
        return 1;
    Win::installVirtualFileSupport();

    int result = 0;
    {
        if (!demoShots.isEmpty())
            Demo::install();
        MainWindow window;
        window.show();
        if (demoShots.isEmpty())
            window.restoreSessions();
        else
            Demo::tour(window, demoShots);
        const qsizetype snapshotAt = arguments.indexOf(QStringLiteral("--snapshot"));
        if (snapshotAt >= 0 && snapshotAt + 1 < arguments.size()) {
            const QString target = arguments.at(snapshotAt + 1);
            QTimer::singleShot(2500, &window, [&window, target] {
                window.grab().save(target);
                QApplication::quit();
            });
        }
        result = QApplication::exec();
    }
    if (I18n::restartRequested() && demoShots.isEmpty()) {
        QStringList restart = arguments.mid(1);
        if (const qsizetype at = restart.indexOf(QStringLiteral("--language")); at >= 0)
            restart.remove(at, std::min<qsizetype>(2, restart.size() - at));
        QProcess::startDetached(QApplication::applicationFilePath(), restart);
    }
    ssh_finalize();
    return result;
}
