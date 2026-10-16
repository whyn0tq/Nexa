#include "hoststore.h"

#include "listingcache.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <limits>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#endif

namespace {
const QString HostsKey = QStringLiteral("hosts/v1");
const QString OpenKey = QStringLiteral("hosts/openSessions");

QString protocolName(const ConnectionProfile &profile)
{
    if (profile.protocol == Protocol::Ftp)
        return profile.useTls ? QStringLiteral("ftps") : QStringLiteral("ftp");
    return QStringLiteral("sftp");
}

QJsonObject toJson(const SavedHost &host)
{
    const ConnectionProfile &p = host.profile;
    return QJsonObject{
        {QStringLiteral("id"), host.id},
        {QStringLiteral("name"), host.name},
        {QStringLiteral("color"), host.color},
        {QStringLiteral("lastUsed"), host.lastUsed.toString(Qt::ISODate)},
        {QStringLiteral("lastPath"), host.lastPath},
        {QStringLiteral("protocol"), protocolName(p)},
        {QStringLiteral("host"), p.host},
        {QStringLiteral("port"), p.port},
        {QStringLiteral("username"), p.username},
        {QStringLiteral("auth"), static_cast<int>(p.auth)},
        {QStringLiteral("privateKeyPath"), p.privateKeyPath},
        {QStringLiteral("initialPath"), p.initialPath},
        {QStringLiteral("password"), HostStore::protectSecret(p.password)},
        {QStringLiteral("passphrase"), HostStore::protectSecret(p.keyPassphrase)},
    };
}

SavedHost fromJson(const QJsonObject &object)
{
    SavedHost host;
    host.id = object.value(QStringLiteral("id")).toString();
    host.name = object.value(QStringLiteral("name")).toString();
    host.color = object.value(QStringLiteral("color")).toInt();
    host.lastUsed = QDateTime::fromString(
        object.value(QStringLiteral("lastUsed")).toString(), Qt::ISODate);
    host.lastPath = object.value(QStringLiteral("lastPath")).toString();
    ConnectionProfile &p = host.profile;
    const QString protocol = object.value(QStringLiteral("protocol")).toString();
    p.protocol = protocol.startsWith(QStringLiteral("ftp")) ? Protocol::Ftp : Protocol::Sftp;
    p.useTls = protocol == QStringLiteral("ftps");
    p.host = object.value(QStringLiteral("host")).toString();
    p.port = static_cast<quint16>(std::clamp(
        object.value(QStringLiteral("port")).toInt(defaultPort(p.protocol)), 1, 65535));
    p.username = object.value(QStringLiteral("username")).toString();
    p.auth = static_cast<AuthMethod>(
        std::clamp(object.value(QStringLiteral("auth")).toInt(), 0, 2));
    p.privateKeyPath = object.value(QStringLiteral("privateKeyPath")).toString();
    p.initialPath = object.value(QStringLiteral("initialPath")).toString();
    p.password = HostStore::unprotectSecret(object.value(QStringLiteral("password")).toString());
    p.keyPassphrase =
        HostStore::unprotectSecret(object.value(QStringLiteral("passphrase")).toString());
    if (p.protocol == Protocol::Ftp)
        p.auth = AuthMethod::Password;
    return host;
}
}

HostStore &HostStore::instance()
{
    static HostStore store;
    return store;
}

HostStore::HostStore()
{
    load();
}

void HostStore::load()
{
    QSettings settings;
    if (!settings.contains(HostsKey)) {
        migrateRecentProfiles();
        return;
    }
    const QJsonArray array =
        QJsonDocument::fromJson(settings.value(HostsKey).toByteArray()).array();
    m_hosts.clear();
    for (const QJsonValue &value : array) {
        SavedHost host = fromJson(value.toObject());
        if (!host.id.isEmpty() && !host.profile.host.isEmpty())
            m_hosts.append(host);
    }
}

void HostStore::migrateRecentProfiles()
{
    QSettings settings;
    const QJsonArray recent = QJsonDocument::fromJson(
                                  settings.value(QStringLiteral("recentProfiles")).toByteArray())
                                  .array();
    for (const QJsonValue &value : recent) {
        const QJsonObject object = value.toObject();
        SavedHost host;
        host.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ConnectionProfile &p = host.profile;
        p.protocol = object.value(QStringLiteral("protocol")).toString() == QStringLiteral("ftp")
                         ? Protocol::Ftp
                         : Protocol::Sftp;
        p.host = object.value(QStringLiteral("host")).toString();
        p.port = static_cast<quint16>(object.value(QStringLiteral("port")).toInt(22));
        p.username = object.value(QStringLiteral("username")).toString();
        p.useTls = object.value(QStringLiteral("useTls")).toBool();
        p.auth = object.value(QStringLiteral("usePrivateKey")).toBool() ? AuthMethod::PrivateKey
                                                                      : AuthMethod::Password;
        p.privateKeyPath = object.value(QStringLiteral("privateKeyPath")).toString();
        p.password = unprotectSecret(object.value(QStringLiteral("passwordProtected")).toString());
        p.keyPassphrase =
            unprotectSecret(object.value(QStringLiteral("keyPassphraseProtected")).toString());
        if (!p.host.isEmpty())
            m_hosts.append(host);
    }
    persist();
}

void HostStore::persist()
{
    QJsonArray array;
    for (const SavedHost &host : std::as_const(m_hosts))
        array.append(toJson(host));
    QSettings().setValue(HostsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
    emit changed();
}

SavedHost HostStore::host(const QString &id) const
{
    for (const SavedHost &host : m_hosts) {
        if (host.id == id)
            return host;
    }
    return {};
}

bool HostStore::contains(const QString &id) const
{
    return std::any_of(m_hosts.cbegin(), m_hosts.cend(),
                       [&id](const SavedHost &host) { return host.id == id; });
}

QString HostStore::save(SavedHost host)
{
    if (host.id.isEmpty())
        host.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (SavedHost &existing : m_hosts) {
        if (existing.id == host.id) {
            host.lastUsed = existing.lastUsed;
            if (host.lastPath.isEmpty())
                host.lastPath = existing.lastPath;
            existing = host;
            persist();
            return host.id;
        }
    }
    m_hosts.append(host);
    persist();
    return host.id;
}

void HostStore::remove(const QString &id)
{
    m_hosts.erase(std::remove_if(m_hosts.begin(), m_hosts.end(),
                                 [&id](const SavedHost &host) { return host.id == id; }),
                  m_hosts.end());
    ListingCache::forget(id);
    persist();
}

void HostStore::touch(const QString &id)
{
    for (SavedHost &host : m_hosts) {
        if (host.id == id) {
            host.lastUsed = QDateTime::currentDateTime();
            persist();
            return;
        }
    }
}

void HostStore::setLastPath(const QString &id, const QString &path)
{
    for (SavedHost &host : m_hosts) {
        if (host.id == id && host.lastPath != path) {
            host.lastPath = path;
            QJsonArray array;
            for (const SavedHost &item : std::as_const(m_hosts))
                array.append(toJson(item));
            QSettings().setValue(HostsKey,
                                 QJsonDocument(array).toJson(QJsonDocument::Compact));
            return;
        }
    }
}

QStringList HostStore::openSessions() const
{
    return QSettings().value(OpenKey).toStringList();
}

void HostStore::setOpenSessions(const QStringList &ids)
{
    QSettings().setValue(OpenKey, ids);
}

ConnectionProfile HostStore::parseAddress(const QString &text, bool *ok)
{
    ConnectionProfile profile;
    QString value = text.trimmed();
    if (ok)
        *ok = false;
    if (value.isEmpty())
        return profile;
    if (!value.contains(QStringLiteral("://")))
        value.prepend(QStringLiteral("sftp://"));
    const QUrl url(value);
    if (!url.isValid() || url.host().isEmpty())
        return profile;
    const QString scheme = url.scheme().toLower();
    profile.protocol = scheme.startsWith(QStringLiteral("ftp")) ? Protocol::Ftp
                                                                : Protocol::Sftp;
    profile.useTls = scheme == QStringLiteral("ftps");
    profile.host = url.host();
    profile.port = url.port() > 0 ? static_cast<quint16>(url.port())
                                  : defaultPort(profile.protocol);
    profile.username = url.userName();
    profile.password = url.password();
    if (!url.path().isEmpty() && url.path() != QStringLiteral("/"))
        profile.initialPath = url.path();
    if (ok)
        *ok = true;
    return profile;
}

QString HostStore::protectSecret(const QString &value)
{
#ifdef Q_OS_WIN
    if (value.isEmpty())
        return {};
    QByteArray plain = value.toUtf8();
    DATA_BLOB input{static_cast<DWORD>(plain.size()),
                    reinterpret_cast<BYTE *>(plain.data())};
    DATA_BLOB output{};
    const BOOL ok = CryptProtectData(&input, L"Nexa credential", nullptr, nullptr,
                                     nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    plain.fill('\0');
    if (!ok)
        return {};
    const QByteArray sealed(reinterpret_cast<const char *>(output.pbData),
                            static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return QString::fromLatin1(sealed.toBase64());
#else
    Q_UNUSED(value);
    return {};
#endif
}

QString HostStore::unprotectSecret(const QString &value)
{
#ifdef Q_OS_WIN
    QByteArray sealed = QByteArray::fromBase64(value.toLatin1());
    if (sealed.isEmpty())
        return {};
    DATA_BLOB input{static_cast<DWORD>(sealed.size()),
                    reinterpret_cast<BYTE *>(sealed.data())};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QString result = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData),
                                             static_cast<qsizetype>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(value);
    return {};
#endif
}
