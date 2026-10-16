#include "listingcache.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace ListingCache {
namespace {

constexpr quint32 Magic = 0x4e584c43;
constexpr quint16 Version = 1;
constexpr int MaxFolders = 200;
constexpr int MaxEntries = 20000;

QString directory()
{
    const QSettings settings;
    QString base;
    if (settings.format() == QSettings::IniFormat)
        base = QFileInfo(settings.fileName()).absolutePath();
    else
        base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return base + QStringLiteral("/listings");
}

QString fileFor(const QString &hostId)
{
    QString safe = hostId;
    for (QChar &c : safe) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('-') && c != QLatin1Char('_'))
            c = QLatin1Char('_');
    }
    return directory() + QLatin1Char('/') + safe + QStringLiteral(".bin");
}

}

Snapshot load(const QString &hostId)
{
    Snapshot snapshot;
    if (hostId.isEmpty())
        return snapshot;
    QFile file(fileFor(hostId));
    if (!file.open(QIODevice::ReadOnly))
        return snapshot;
    const QByteArray packed = qUncompress(file.readAll());
    QDataStream in(packed);
    in.setVersion(QDataStream::Qt_6_5);
    quint32 magic = 0;
    quint16 version = 0;
    qint32 folders = 0;
    in >> magic >> version >> folders;
    if (magic != Magic || version != Version || folders < 0 || folders > MaxFolders)
        return snapshot;
    for (qint32 i = 0; i < folders && in.status() == QDataStream::Ok; ++i) {
        QString path;
        qint32 count = 0;
        in >> path >> count;
        if (count < 0 || count > MaxEntries)
            return {};
        RemoteEntries entries;
        entries.reserve(count);
        for (qint32 j = 0; j < count && in.status() == QDataStream::Ok; ++j) {
            RemoteEntry entry;
            qint64 modified = 0;
            quint8 flags = 0;
            in >> entry.name >> entry.size >> modified >> entry.permissions >> flags;
            entry.path = path == QLatin1String("/") ? QLatin1Char('/') + entry.name
                                                    : path + QLatin1Char('/') + entry.name;
            if (modified)
                entry.modified = QDateTime::fromMSecsSinceEpoch(modified);
            entry.directory = flags & 1;
            entry.symlink = flags & 2;
            entries.append(entry);
        }
        snapshot.listings.insert(path, entries);
        snapshot.order.append(path);
    }
    if (in.status() != QDataStream::Ok)
        return {};
    return snapshot;
}

void save(const QString &hostId, const Snapshot &snapshot)
{
    if (hostId.isEmpty())
        return;
    QStringList order;
    for (const QString &path : snapshot.order) {
        if (snapshot.listings.contains(path) && !order.contains(path))
            order.append(path);
    }
    if (order.size() > MaxFolders)
        order = order.mid(order.size() - MaxFolders);

    QByteArray raw;
    QDataStream out(&raw, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << Magic << Version << static_cast<qint32>(order.size());
    for (const QString &path : std::as_const(order)) {
        const RemoteEntries &entries = snapshot.listings.value(path);
        const qint32 count = static_cast<qint32>(std::min<qsizetype>(entries.size(), MaxEntries));
        out << path << count;
        for (qint32 i = 0; i < count; ++i) {
            const RemoteEntry &entry = entries.at(i);
            const quint8 flags = (entry.directory ? 1 : 0) | (entry.symlink ? 2 : 0);
            out << entry.name << entry.size
                << (entry.modified.isValid() ? entry.modified.toMSecsSinceEpoch() : qint64(0))
                << entry.permissions << flags;
        }
    }

    QDir().mkpath(directory());
    QSaveFile file(fileFor(hostId));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(qCompress(raw, 6));
    file.commit();
}

void forget(const QString &hostId)
{
    if (!hostId.isEmpty())
        QFile::remove(fileFor(hostId));
}

}
