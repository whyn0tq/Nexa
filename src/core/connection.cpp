#include "connection.h"

#include "ftpconnection.h"
#include "sftpconnection.h"

#include <QDir>

#include <algorithm>

namespace {
RemoteConnection::Factory &factoryOverride()
{
    static RemoteConnection::Factory factory;
    return factory;
}
}

void RemoteConnection::setFactory(Factory factory)
{
    factoryOverride() = std::move(factory);
}

std::unique_ptr<RemoteConnection> RemoteConnection::create(Protocol protocol)
{
    if (const Factory &factory = factoryOverride())
        return factory(protocol);
    if (protocol == Protocol::Ftp)
        return std::make_unique<FtpConnection>();
    return std::make_unique<SftpConnection>();
}

QString RemoteConnection::parentPath(const QString &path)
{
    const QString clean = normalized(path);
    if (clean == QStringLiteral("/"))
        return clean;
    const qsizetype slash = clean.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : clean.left(slash);
}

QString RemoteConnection::join(const QString &directory, const QString &name)
{
    if (directory.endsWith(QLatin1Char('/')))
        return directory + name;
    return directory + QLatin1Char('/') + name;
}

QString RemoteConnection::normalized(const QString &path)
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!result.startsWith(QLatin1Char('/')))
        result.prepend(QLatin1Char('/'));
    return result;
}

QString RemoteConnection::numberedVariant(const QString &path, int index)
{
    const qsizetype slash = std::max(path.lastIndexOf(QLatin1Char('/')),
                                     path.lastIndexOf(QLatin1Char('\\')));
    const QString directory = slash < 0 ? QString() : path.left(slash + 1);
    const QString name = path.mid(slash + 1);
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot <= 0 ? name : name.left(dot);
    const QString suffix = dot <= 0 ? QString() : name.mid(dot);
    return QStringLiteral("%1%2 (%3)%4")
        .arg(directory, stem, QString::number(index), suffix);
}

void RemoteConnection::sortEntries(RemoteEntries &entries)
{
    std::sort(entries.begin(), entries.end(),
              [](const RemoteEntry &a, const RemoteEntry &b) {
                  if (a.directory != b.directory)
                      return a.directory > b.directory;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
}
