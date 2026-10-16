#ifdef _WIN32
#include <winsock2.h>
#include <mstcpip.h>
#endif

#include "sftpconnection.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QUrl>

#include <libssh/libssh.h>

#include <algorithm>
#include <cstring>

using namespace sftp;

namespace {
QString fingerprintString(const unsigned char *hash, size_t length)
{
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(length));
    for (size_t i = 0; i < length; ++i)
        parts.append(QStringLiteral("%1").arg(hash[i], 2, 16, QLatin1Char('0')));
    return parts.join(QLatin1Char(':'));
}

QByteArray bytes(const QString &path)
{
    return QDir::fromNativeSeparators(path).toUtf8();
}

QByteArray pathRequest(Type type, quint32 id, const QString &path)
{
    Builder builder(type, id, path.size() + 8);
    builder.string(bytes(path));
    return builder.finish();
}

RemoteEntry entryFrom(const QString &directory, const QByteArray &name,
                      const QByteArray &longName, const FileAttrs &attrs)
{
    RemoteEntry entry;
    entry.name = QString::fromUtf8(name);
    entry.path = RemoteConnection::join(directory, entry.name);
    entry.size = static_cast<qint64>(attrs.size);
    entry.permissions = attrs.permissions;
    const bool longDir = !longName.isEmpty() && longName.at(0) == 'd';
    const bool longLink = !longName.isEmpty() && longName.at(0) == 'l';
    entry.symlink = attrs.isLink() || (!(attrs.flags & AttrPermissions) && longLink);
    entry.directory = !entry.symlink &&
                      (attrs.isDirectory() ||
                       (!(attrs.flags & AttrPermissions) && longDir));
    if (attrs.flags & AttrTimes && attrs.mtime > 0)
        entry.modified = QDateTime::fromSecsSinceEpoch(attrs.mtime);
    return entry;
}

QString hostKeySetting(const ConnectionProfile &profile)
{
    return QStringLiteral("hostKeys/%1:%2").arg(profile.host).arg(profile.port);
}
}

SftpConnection::~SftpConnection()
{
    close();
}

QString SftpConnection::sshError(const QString &prefix) const
{
    if (!m_session)
        return prefix;
    const char *message = ssh_get_error(m_session);
    const QString details = message ? QString::fromUtf8(message) : QString();
    return details.isEmpty() ? prefix
                             : QStringLiteral("%1: %2").arg(prefix, details);
}

bool SftpConnection::channelFailed(const QString &prefix)
{
    if (m_channel.broken()) {
        fail(QStringLiteral("%1: %2").arg(prefix, m_channel.lastError()), true);
        return true;
    }
    return false;
}

RemoteConnection::OpenResult SftpConnection::open(const ConnectionProfile &profile,
                                                  bool interactiveTrust)
{
    close();
    m_profile = profile;
    m_error.clear();
    m_broken = false;

    if (m_profile.host.contains(QStringLiteral("://"))) {
        const QUrl url(m_profile.host.trimmed());
        if (url.isValid() && !url.host().isEmpty()) {
            m_profile.host = url.host();
            if (url.port() > 0)
                m_profile.port = static_cast<quint16>(url.port());
        }
    }

    m_session = ssh_new();
    if (!m_session) {
        fail(tr("Не удалось создать SSH-сессию."));
        return OpenResult::Failed;
    }

    const QByteArray host = m_profile.host.toUtf8();
    const QByteArray user = m_profile.username.toUtf8();
    unsigned int port = m_profile.port;
    long timeout = 20;
    int enabled = 1;
    int disabled = 0;
    ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData());
    ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeout);
    ssh_options_set(m_session, SSH_OPTIONS_NODELAY, &enabled);
    ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &disabled);
    const char *ciphers = "aes128-gcm@openssh.com,aes256-gcm@openssh.com,"
                          "chacha20-poly1305@openssh.com,aes128-ctr,aes256-ctr";
    ssh_options_set(m_session, SSH_OPTIONS_CIPHERS_C_S, ciphers);
    ssh_options_set(m_session, SSH_OPTIONS_CIPHERS_S_C, ciphers);
    ssh_set_blocking(m_session, 1);

    if (ssh_connect(m_session) != SSH_OK) {
        fail(sshError(tr("Не удалось подключиться к %1:%2")
                          .arg(m_profile.host)
                          .arg(m_profile.port)),
             true);
        close();
        return OpenResult::Failed;
    }
    tuneSocket();
    if (const char *banner = ssh_get_serverbanner(m_session))
        m_serverInfo = QString::fromUtf8(banner);

    ssh_key key = nullptr;
    unsigned char *hash = nullptr;
    size_t hashLength = 0;
    if (ssh_get_server_publickey(m_session, &key) != SSH_OK ||
        ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash,
                               &hashLength) != SSH_OK) {
        if (key)
            ssh_key_free(key);
        fail(sshError(tr("Не удалось получить ключ сервера")), true);
        close();
        return OpenResult::Failed;
    }
    m_pendingFingerprint = fingerprintString(hash, hashLength);
    QString keyType;
    if (const char *type = ssh_key_type_to_char(ssh_key_type(key)))
        keyType = QString::fromUtf8(type);
    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(key);

    const QString known = QSettings().value(hostKeySetting(m_profile)).toString();
    if (known == m_pendingFingerprint ||
        m_profile.trustedFingerprint == m_pendingFingerprint) {
        m_trustedFingerprint = m_pendingFingerprint;
        return finishOpen();
    }
    if (!interactiveTrust) {
        fail(tr("Ключ сервера %1 не совпадает с подтверждённым.")
                 .arg(m_profile.host));
        close();
        return OpenResult::Failed;
    }
    m_trust.host = QStringLiteral("%1:%2").arg(m_profile.host).arg(m_profile.port);
    m_trust.fingerprint = m_pendingFingerprint;
    m_trust.keyType = keyType;
    m_trust.changed = !known.isEmpty();
    m_waitingForTrust = true;
    return OpenResult::NeedsTrust;
}

RemoteConnection::OpenResult SftpConnection::continueAfterTrust(bool accepted,
                                                                bool remember)
{
    if (!m_waitingForTrust)
        return OpenResult::Failed;
    m_waitingForTrust = false;
    if (!accepted) {
        fail(tr("Подключение отменено: ключ сервера не принят."));
        close();
        return OpenResult::Failed;
    }
    m_trustedFingerprint = m_pendingFingerprint;
    if (remember)
        QSettings().setValue(hostKeySetting(m_profile), m_pendingFingerprint);
    return finishOpen();
}

RemoteConnection::OpenResult SftpConnection::finishOpen()
{
    if (!authenticate()) {
        close();
        return OpenResult::AuthFailed;
    }
    QString error;
    if (!m_channel.open(m_session, error)) {
        fail(error, true);
        close();
        return OpenResult::Failed;
    }
    return OpenResult::Ok;
}

void SftpConnection::tuneSocket()
{
#ifdef _WIN32
    const socket_t fd = ssh_get_fd(m_session);
    if (fd == SSH_INVALID_SOCKET)
        return;
    BOOL on = TRUE;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&on),
               sizeof(on));
    const int sendBuffer = 8 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char *>(&sendBuffer),
               sizeof(sendBuffer));
    tcp_keepalive settings{1, 30000, 5000};
    DWORD returned = 0;
    WSAIoctl(fd, SIO_KEEPALIVE_VALS, &settings, sizeof(settings), nullptr, 0,
             &returned, nullptr, nullptr);
#endif
}

bool SftpConnection::authenticate()
{
    switch (m_profile.auth) {
    case AuthMethod::Password:
        if (!m_profile.password.isEmpty()) {
            const QByteArray password = m_profile.password.toUtf8();
            if (ssh_userauth_password(m_session, nullptr, password.constData()) ==
                SSH_AUTH_SUCCESS)
                return true;
        }
        break;
    case AuthMethod::PrivateKey:
        if (authenticateWithKey(0xffffffffu))
            return true;
        break;
    case AuthMethod::Agent:
        if (ssh_userauth_agent(m_session, nullptr) == SSH_AUTH_SUCCESS)
            return true;
        break;
    }
    if (!m_error.isEmpty())
        return false;
    if (!ssh_is_connected(m_session)) {
        fail(sshError(tr("Сервер закрыл соединение при входе")), true);
        return false;
    }

    int listed = ssh_userauth_list(m_session, nullptr);
    if (listed <= 0) {
        if (ssh_userauth_none(m_session, nullptr) == SSH_AUTH_SUCCESS)
            return true;
        listed = ssh_userauth_list(m_session, nullptr);
    }
    const unsigned int methods =
        listed <= 0 ? 0xffffffffu : static_cast<unsigned int>(listed);
    if (m_profile.auth == AuthMethod::Password && (methods & SSH_AUTH_METHOD_INTERACTIVE) &&
        authenticateInteractive())
        return true;

    QStringList offered;
    if (methods & SSH_AUTH_METHOD_PASSWORD)
        offered.append(tr("пароль"));
    if (methods & SSH_AUTH_METHOD_PUBLICKEY)
        offered.append(tr("ключ"));
    if (methods & SSH_AUTH_METHOD_INTERACTIVE)
        offered.append(tr("интерактивный вход"));
    fail(offered.isEmpty()
             ? tr("Неверный логин или пароль.")
             : tr("Сервер не принял данные для входа. Доступно: %1.")
                   .arg(offered.join(QStringLiteral(", "))));
    return false;
}

bool SftpConnection::authenticateWithKey(unsigned int methods)
{
    if (!(methods & SSH_AUTH_METHOD_PUBLICKEY) || m_profile.privateKeyPath.isEmpty())
        return false;
    const QByteArray identity = QFile::encodeName(m_profile.privateKeyPath);
    const QByteArray passphrase = m_profile.keyPassphrase.toUtf8();
    ssh_key key = nullptr;
    if (ssh_pki_import_privkey_file(
            identity.constData(),
            passphrase.isEmpty() ? nullptr : passphrase.constData(), nullptr,
            nullptr, &key) != SSH_OK) {
        fail(tr("Не удалось прочитать ключ «%1». Проверьте путь и кодовую фразу.")
                 .arg(m_profile.privateKeyPath));
        return false;
    }
    const int result = ssh_userauth_publickey(m_session, nullptr, key);
    ssh_key_free(key);
    return result == SSH_AUTH_SUCCESS;
}

bool SftpConnection::authenticateInteractive()
{
    const QByteArray password = m_profile.password.toUtf8();
    int result = ssh_userauth_kbdint(m_session, nullptr, nullptr);
    for (int round = 0; result == SSH_AUTH_INFO && round < 8; ++round) {
        const int prompts = ssh_userauth_kbdint_getnprompts(m_session);
        for (int i = 0; i < prompts; ++i) {
            char echo = 0;
            ssh_userauth_kbdint_getprompt(m_session, static_cast<unsigned int>(i),
                                          &echo);
            ssh_userauth_kbdint_setanswer(m_session, static_cast<unsigned int>(i),
                                          echo ? "" : password.constData());
        }
        result = ssh_userauth_kbdint(m_session, nullptr, nullptr);
    }
    return result == SSH_AUTH_SUCCESS;
}

void SftpConnection::close()
{
    m_waitingForTrust = false;
    m_channel.close();
    if (m_session) {
        if (ssh_is_connected(m_session))
            ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
}

bool SftpConnection::isOpen() const
{
    return m_session && ssh_is_connected(m_session) && m_channel.isOpen();
}

bool SftpConnection::ping()
{
    if (!isOpen())
        return false;
    const quint32 id = m_channel.nextId();
    Packet reply;
    if (!m_channel.call(pathRequest(Realpath, id, QStringLiteral(".")), id, reply,
                        20000)) {
        fail(tr("Сервер перестал отвечать."), true);
        return false;
    }
    return true;
}

QString SftpConnection::homePath()
{
    const quint32 id = m_channel.nextId();
    Packet reply;
    if (!m_channel.call(pathRequest(Realpath, id, QStringLiteral(".")), id, reply) ||
        reply.type != Name) {
        return QStringLiteral("/");
    }
    if (reply.u32() < 1)
        return QStringLiteral("/");
    const QString path = QString::fromUtf8(reply.string());
    return reply.valid && !path.isEmpty() ? normalized(path) : QStringLiteral("/");
}

bool SftpConnection::listMany(
    const QStringList &directories,
    const std::function<void(const QString &, RemoteEntries &)> &onDirectory,
    QString &failedDirectory, QString &failure)
{
    struct Listing {
        QString path;
        QByteArray handle;
        RemoteEntries entries;
        int inFlight = 0;
        bool eof = false;
    };
    enum class Op { Open, Read, Close };
    struct Pending {
        int index;
        Op op;
    };

    constexpr int MaxOpen = 16;
    constexpr int ReadsPerHandle = 3;
    QVector<Listing> listings(directories.size());
    for (int i = 0; i < directories.size(); ++i)
        listings[i].path = normalized(directories.at(i));
    QHash<quint32, Pending> pending;
    int nextToOpen = 0;
    int open = 0;
    QElapsedTimer idle;
    idle.start();

    const auto sendRead = [&](int index) {
        const quint32 id = m_channel.nextId();
        Builder builder(Readdir, id);
        builder.string(listings[index].handle);
        if (!m_channel.send(builder.finish()))
            return false;
        pending.insert(id, {index, Op::Read});
        ++listings[index].inFlight;
        return true;
    };

    while (nextToOpen < listings.size() || !pending.isEmpty()) {
        if (canceledByUser()) {
            fail(tr("Отменено."));
            return false;
        }
        while (open < MaxOpen && nextToOpen < listings.size()) {
            const quint32 id = m_channel.nextId();
            if (!m_channel.send(pathRequest(Opendir, id, listings[nextToOpen].path)))
                return !channelFailed(tr("Ошибка чтения папки"));
            pending.insert(id, {nextToOpen++, Op::Open});
            ++open;
        }

        Packet reply;
        const auto result = m_channel.read(reply, 500);
        if (result == Channel::ReadResult::Error) {
            channelFailed(tr("Ошибка чтения папки"));
            return false;
        }
        if (result == Channel::ReadResult::Timeout) {
            if (idle.elapsed() > 60000) {
                fail(tr("Сервер не отвечает."), true);
                return false;
            }
            continue;
        }
        idle.restart();
        const auto found = pending.find(reply.id);
        if (found == pending.end())
            continue;
        const Pending request = found.value();
        pending.erase(found);
        Listing &listing = listings[request.index];

        switch (request.op) {
        case Op::Open:
            if (reply.type != Handle) {
                failedDirectory = listing.path;
                failure = describeStatus(reply.status(), reply.statusMessage());
                while (!pending.isEmpty()) {
                    Packet rest;
                    const auto drained = m_channel.read(rest, 2000);
                    if (drained == Channel::ReadResult::Error)
                        return false;
                    if (drained == Channel::ReadResult::Timeout)
                        break;
                    pending.remove(rest.id);
                }
                return false;
            }
            listing.handle = reply.string();
            for (int i = 0; i < ReadsPerHandle; ++i) {
                if (!sendRead(request.index))
                    return !channelFailed(tr("Ошибка чтения папки"));
            }
            break;
        case Op::Read:
            --listing.inFlight;
            if (reply.type == Name) {
                const quint32 count = reply.u32();
                for (quint32 i = 0; i < count && reply.valid; ++i) {
                    const QByteArray name = reply.string();
                    const QByteArray longName = reply.string();
                    const FileAttrs attrs = reply.attrs();
                    if (!reply.valid || name == "." || name == "..")
                        continue;
                    listing.entries.append(
                        entryFrom(listing.path, name, longName, attrs));
                }
                if (!listing.eof && listing.inFlight == 0 &&
                    (!sendRead(request.index) || !sendRead(request.index) ||
                     !sendRead(request.index)))
                    return !channelFailed(tr("Ошибка чтения папки"));
            } else {
                listing.eof = true;
            }
            if (listing.eof && listing.inFlight == 0) {
                const quint32 id = m_channel.nextId();
                Builder builder(Close, id);
                builder.string(listing.handle);
                if (!m_channel.send(builder.finish()))
                    return !channelFailed(tr("Ошибка чтения папки"));
                --open;
                onDirectory(listing.path, listing.entries);
                listing.entries = RemoteEntries();
            }
            break;
        case Op::Close:
            --open;
            break;
        }
    }
    return true;
}

void SftpConnection::resolveLinks(RemoteEntries &entries)
{
    QVector<int> links;
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).symlink)
            links.append(i);
    }
    if (links.isEmpty())
        return;
    m_channel.batch(
        static_cast<int>(links.size()),
        [&](int index, quint32 id) {
            return pathRequest(Stat, id, entries.at(links.at(index)).path);
        },
        [&](int index, Packet &reply) {
            if (reply.type != Attrs)
                return;
            const FileAttrs attrs = reply.attrs();
            RemoteEntry &entry = entries[links.at(index)];
            entry.directory = attrs.isDirectory();
            if (!entry.directory)
                entry.size = static_cast<qint64>(attrs.size);
        });
}

bool SftpConnection::list(const QString &path, RemoteEntries &entries)
{
    entries.clear();
    QString failedDirectory;
    QString failure;
    const bool listed = listMany(
        {path},
        [&entries](const QString &, RemoteEntries &found) { entries = found; },
        failedDirectory, failure);
    if (!listed) {
        if (!failure.isEmpty())
            fail(tr("Не удалось открыть «%1»: %2").arg(normalized(path), failure));
        else if (m_error.isEmpty())
            fail(tr("Не удалось открыть «%1».").arg(normalized(path)));
        return false;
    }
    resolveLinks(entries);
    sortEntries(entries);
    return true;
}

bool SftpConnection::listStart(const QString &path, QString &absolute, RemoteEntries &entries)
{
    entries.clear();
    const bool home = path.isEmpty();
    absolute = home ? QString() : normalized(path);
    const QString target = home ? QStringLiteral(".") : absolute;

    quint32 realpathId = 0;
    if (home) {
        realpathId = m_channel.nextId();
        if (!m_channel.send(pathRequest(Realpath, realpathId, target)))
            return !channelFailed(tr("Ошибка чтения папки"));
    }
    const quint32 openId = m_channel.nextId();
    if (!m_channel.send(pathRequest(Opendir, openId, target)))
        return !channelFailed(tr("Ошибка чтения папки"));

    constexpr int ReadsAhead = 3;
    QByteArray handle;
    QSet<quint32> reads;
    bool opened = false;
    bool eof = false;
    QString failure;
    QElapsedTimer idle;
    idle.start();
    const auto sendReads = [&] {
        for (int i = 0; i < ReadsAhead; ++i) {
            const quint32 id = m_channel.nextId();
            Builder builder(Readdir, id);
            builder.string(handle);
            if (!m_channel.send(builder.finish()))
                return false;
            reads.insert(id);
        }
        return true;
    };

    while (!opened || !reads.isEmpty()) {
        Packet reply;
        const auto result = m_channel.read(reply, 500);
        if (result == Channel::ReadResult::Error)
            return !channelFailed(tr("Ошибка чтения папки"));
        if (result == Channel::ReadResult::Timeout) {
            if (idle.elapsed() > 60000) {
                fail(tr("Сервер не отвечает."), true);
                return false;
            }
            continue;
        }
        idle.restart();
        if (reply.id == realpathId && realpathId) {
            QString resolved;
            if (reply.type == Name && reply.u32() >= 1)
                resolved = QString::fromUtf8(reply.string());
            absolute = reply.valid && !resolved.isEmpty() ? normalized(resolved)
                                                          : QStringLiteral("/");
            realpathId = 0;
        } else if (reply.id == openId && !opened) {
            opened = true;
            if (reply.type != Handle) {
                failure = describeStatus(reply.status(), reply.statusMessage());
                break;
            }
            handle = reply.string();
            if (!sendReads())
                return !channelFailed(tr("Ошибка чтения папки"));
        } else if (reads.remove(reply.id)) {
            if (reply.type != Name) {
                eof = true;
            } else {
                const quint32 count = reply.u32();
                for (quint32 i = 0; i < count && reply.valid; ++i) {
                    const QByteArray name = reply.string();
                    const QByteArray longName = reply.string();
                    const FileAttrs attrs = reply.attrs();
                    if (!reply.valid || name == "." || name == "..")
                        continue;
                    entries.append(entryFrom(absolute, name, longName, attrs));
                }
            }
            if (reads.isEmpty() && !eof && !sendReads())
                return !channelFailed(tr("Ошибка чтения папки"));
        }
    }

    if (opened && failure.isEmpty()) {
        const quint32 id = m_channel.nextId();
        Builder builder(Close, id);
        builder.string(handle);
        if (!m_channel.send(builder.finish()))
            return !channelFailed(tr("Ошибка чтения папки"));
    }
    if (!failure.isEmpty()) {
        if (absolute.isEmpty())
            absolute = QStringLiteral("/");
        fail(tr("Не удалось открыть «%1»: %2").arg(home ? QStringLiteral("~") : absolute,
                                                    failure));
        entries.clear();
        return false;
    }
    resolveLinks(entries);
    sortEntries(entries);
    return true;
}

bool SftpConnection::makeDirectory(const QString &path)
{
    const quint32 id = m_channel.nextId();
    Builder builder(Mkdir, id);
    builder.string(bytes(path)).attrs({});
    Packet reply;
    if (!m_channel.call(builder.finish(), id, reply))
        return !channelFailed(tr("Не удалось создать папку"));
    if (reply.status() != Ok) {
        fail(tr("Не удалось создать папку «%1»: %2")
                 .arg(path, describeStatus(reply.status(), reply.statusMessage())));
        return false;
    }
    return true;
}

bool SftpConnection::createFile(const QString &path)
{
    const quint32 id = m_channel.nextId();
    Builder builder(Open, id);
    builder.string(bytes(path))
        .u32(OpenWrite | OpenCreate | OpenExclusive)
        .attrs({});
    Packet reply;
    if (!m_channel.call(builder.finish(), id, reply))
        return !channelFailed(tr("Не удалось создать файл"));
    if (reply.type != Handle) {
        fail(tr("Не удалось создать файл «%1»: %2")
                 .arg(path, describeStatus(reply.status(), reply.statusMessage())));
        return false;
    }
    const QByteArray handle = reply.string();
    const quint32 closeId = m_channel.nextId();
    Builder close(Close, closeId);
    close.string(handle);
    m_channel.call(close.finish(), closeId, reply);
    return true;
}

bool SftpConnection::rename(const QString &from, const QString &to)
{
    const quint32 id = m_channel.nextId();
    Builder builder(Rename, id);
    builder.string(bytes(from)).string(bytes(to));
    Packet reply;
    if (!m_channel.call(builder.finish(), id, reply))
        return !channelFailed(tr("Не удалось переименовать"));
    if (reply.status() != Ok) {
        fail(tr("Не удалось переименовать «%1»: %2")
                 .arg(from, describeStatus(reply.status(), reply.statusMessage())));
        return false;
    }
    return true;
}

bool SftpConnection::remove(const RemoteEntries &entries,
                            const std::function<void(qint64)> &progress)
{
    QStringList files;
    QVector<QPair<QString, int>> directories;
    QStringList level;
    for (const RemoteEntry &entry : entries) {
        if (entry.directory && !entry.symlink) {
            directories.append({entry.path, 0});
            level.append(entry.path);
        } else {
            files.append(entry.path);
        }
    }

    for (int depth = 1; !level.isEmpty(); ++depth) {
        QStringList next;
        QString failedDirectory;
        QString failure;
        const bool listed = listMany(
            level,
            [&](const QString &, RemoteEntries &children) {
                for (const RemoteEntry &child : children) {
                    if (child.directory && !child.symlink) {
                        next.append(child.path);
                        directories.append({child.path, depth});
                    } else {
                        files.append(child.path);
                    }
                }
            },
            failedDirectory, failure);
        if (!listed) {
            if (m_error.isEmpty())
                fail(tr("Не удалось прочитать «%1»: %2").arg(failedDirectory, failure));
            return false;
        }
        level = next;
    }

    qint64 removed = 0;
    QString firstError;
    const auto check = [&](const QString &path, Packet &reply) {
        const quint32 status = reply.status();
        if (status != Ok && status != NoSuchFile && firstError.isEmpty()) {
            firstError = tr("Не удалось удалить «%1»: %2")
                             .arg(path, describeStatus(status, reply.statusMessage()));
        }
        if (++removed % 25 == 0 && progress)
            progress(removed);
    };

    if (!m_channel.batch(
            static_cast<int>(files.size()),
            [&](int index, quint32 id) {
                return pathRequest(sftp::Remove, id, files.at(index));
            },
            [&](int index, Packet &reply) { check(files.at(index), reply); }, 128))
        return !channelFailed(tr("Ошибка удаления"));
    if (!firstError.isEmpty()) {
        fail(firstError);
        return false;
    }

    int maxDepth = 0;
    for (const auto &directory : directories)
        maxDepth = std::max(maxDepth, directory.second);
    for (int depth = maxDepth; depth >= 0; --depth) {
        QStringList atDepth;
        for (const auto &directory : directories) {
            if (directory.second == depth)
                atDepth.append(directory.first);
        }
        if (!m_channel.batch(
                static_cast<int>(atDepth.size()),
                [&](int index, quint32 id) {
                    return pathRequest(Rmdir, id, atDepth.at(index));
                },
                [&](int index, Packet &reply) { check(atDepth.at(index), reply); },
                128))
            return !channelFailed(tr("Ошибка удаления"));
        if (!firstError.isEmpty()) {
            fail(firstError);
            return false;
        }
    }
    if (progress)
        progress(removed);
    return true;
}

bool SftpConnection::expand(const TransferJob &job, TransferHost &host)
{
    m_error.clear();
    return job.upload ? expandUpload(job, host) : expandDownload(job, host);
}

bool SftpConnection::expandUpload(const TransferJob &job, TransferHost &host)
{
    const QFileInfo source(job.localPath);
    if (!source.exists()) {
        host.expansionFailed(job.id, tr("Локальный объект «%1» больше не существует.")
                                         .arg(job.localPath));
        return true;
    }
    if (!job.directory) {
        FileTask task;
        task.jobId = job.id;
        task.overwrite = job.overwrite;
        task.upload = true;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = source.size();
        task.permissions = source.isExecutable() ? 0755 : 0644;
        task.modified = source.lastModified().toSecsSinceEpoch();
        host.addTasks(job.id, {task});
        host.expansionFinished(job.id);
        return true;
    }

    QVector<QPair<QString, QString>> level{{job.localPath, normalized(job.remotePath)}};
    bool anyAdded = false;
    const auto broken = [&] {
        channelFailed(tr("Ошибка создания папок"));
        if (anyAdded)
            host.expansionFailed(job.id, m_error);
        else
            host.requeueJob(job);
        return false;
    };
    while (!level.isEmpty()) {
        if (host.canceled(job.id) || host.stopping()) {
            host.expansionFinished(job.id);
            return true;
        }
        QVector<bool> created(level.size(), false);
        QVector<int> toCheck;
        const bool sent = m_channel.batch(
            static_cast<int>(level.size()),
            [&](int index, quint32 id) {
                Builder builder(Mkdir, id);
                builder.string(bytes(level.at(index).second)).attrs({});
                return builder.finish();
            },
            [&](int index, Packet &reply) {
                if (reply.status() == Ok)
                    created[index] = true;
                else
                    toCheck.append(index);
            });
        if (!sent)
            return broken();

        QString failure;
        if (!toCheck.isEmpty()) {
            const bool checked = m_channel.batch(
                static_cast<int>(toCheck.size()),
                [&](int index, quint32 id) {
                    return pathRequest(Stat, id, level.at(toCheck.at(index)).second);
                },
                [&](int index, Packet &reply) {
                    if (reply.type == Attrs && reply.attrs().isDirectory())
                        return;
                    if (failure.isEmpty()) {
                        failure = tr("Не удалось создать папку «%1» на сервере.")
                                      .arg(level.at(toCheck.at(index)).second);
                    }
                });
            if (!checked)
                return broken();
        }
        if (!failure.isEmpty()) {
            host.expansionFailed(job.id, failure);
            return true;
        }

        QVector<QPair<QString, QString>> next;
        QVector<FileTask> tasks;
        for (int i = 0; i < level.size(); ++i) {
            const auto &[localDirectory, remoteDirectory] = level.at(i);
            const QFileInfoList children =
                QDir(localDirectory)
                    .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                       QDir::Hidden | QDir::System,
                                   QDir::DirsFirst | QDir::Name);
            for (const QFileInfo &child : children) {
                const QString remotePath = join(remoteDirectory, child.fileName());
                if (child.isDir() && !child.isSymLink()) {
                    next.append({child.absoluteFilePath(), remotePath});
                    continue;
                }
                FileTask task;
                task.jobId = job.id;
                task.overwrite = job.overwrite;
                task.upload = true;
                task.localPath = child.absoluteFilePath();
                task.remotePath = remotePath;
                task.size = child.size();
                task.permissions = child.isExecutable() ? 0755 : 0644;
                task.modified = child.lastModified().toSecsSinceEpoch();
                task.remoteAbsent = created.at(i);
                tasks.append(task);
            }
        }
        if (!tasks.isEmpty()) {
            host.addTasks(job.id, tasks);
            anyAdded = true;
        }
        level = next;
    }
    host.expansionFinished(job.id);
    return true;
}

bool SftpConnection::expandDownload(const TransferJob &job, TransferHost &host)
{
    if (!job.directory) {
        FileTask task;
        task.jobId = job.id;
        task.overwrite = job.overwrite;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = job.size;
        task.modified = job.modified;
        task.permissions = job.permissions;
        if (task.size < 0) {
            const quint32 id = m_channel.nextId();
            Packet reply;
            if (!m_channel.call(pathRequest(Stat, id, job.remotePath), id, reply)) {
                channelFailed(tr("Ошибка чтения"));
                host.requeueJob(job);
                return false;
            }
            if (reply.type == Attrs) {
                const FileAttrs attrs = reply.attrs();
                if (attrs.flags & AttrSize)
                    task.size = static_cast<qint64>(attrs.size);
                task.modified = attrs.mtime;
            }
        }
        host.addTasks(job.id, {task});
        host.expansionFinished(job.id);
        return true;
    }

    QHash<QString, QString> localFor{{normalized(job.remotePath), job.localPath}};
    QStringList level{normalized(job.remotePath)};
    if (!QDir().mkpath(job.localPath)) {
        host.expansionFailed(job.id, tr("Не удалось создать папку «%1».")
                                         .arg(job.localPath));
        return true;
    }
    bool anyAdded = false;
    while (!level.isEmpty()) {
        if (host.canceled(job.id) || host.stopping()) {
            host.expansionFinished(job.id);
            return true;
        }
        QStringList next;
        QVector<FileTask> tasks;
        QString failedDirectory;
        QString failure;
        bool localFailure = false;
        const bool listed = listMany(
            level,
            [&](const QString &directory, RemoteEntries &children) {
                const QString localDirectory = localFor.value(directory);
                for (const RemoteEntry &child : children) {
                    const QString localPath = QDir(localDirectory).filePath(child.name);
                    if (child.directory && !child.symlink) {
                        if (!QDir().mkpath(localPath))
                            localFailure = true;
                        localFor.insert(child.path, localPath);
                        next.append(child.path);
                        continue;
                    }
                    FileTask task;
                    task.jobId = job.id;
                    task.overwrite = job.overwrite;
                    task.localPath = localPath;
                    task.remotePath = child.path;
                    task.size = child.size;
                    task.permissions = child.permissions;
                    task.modified = child.modified.isValid()
                                        ? child.modified.toSecsSinceEpoch()
                                        : 0;
                    tasks.append(task);
                }
            },
            failedDirectory, failure);
        if (!tasks.isEmpty())
            host.addTasks(job.id, tasks);
        if (!listed) {
            if (m_broken) {
                if (anyAdded || !tasks.isEmpty())
                    host.expansionFailed(job.id, m_error);
                else
                    host.requeueJob(job);
                return false;
            }
            host.expansionFailed(job.id, tr("Не удалось прочитать «%1»: %2")
                                             .arg(failedDirectory, failure));
            return true;
        }
        anyAdded = anyAdded || !tasks.isEmpty();
        if (localFailure) {
            host.expansionFailed(job.id, tr("Не удалось создать локальные папки."));
            return true;
        }
        level = next;
    }
    host.expansionFinished(job.id);
    return true;
}

void SftpConnection::prefetch(
    const QStringList &directories,
    const std::function<void(const QString &, const RemoteEntries &)> &onDirectory)
{
    if (directories.isEmpty() || !isOpen())
        return;
    QVector<QPair<QString, RemoteEntries>> results;
    QString failedDirectory;
    QString failure;
    listMany(
        directories,
        [&results](const QString &directory, RemoteEntries &entries) {
            results.append({directory, entries});
        },
        failedDirectory, failure);
    if (m_channel.broken())
        return;
    for (auto &[directory, entries] : results) {
        resolveLinks(entries);
        sortEntries(entries);
        onDirectory(directory, entries);
    }
}

bool SftpConnection::readFile(const QString &path, qint64 sizeHint, QByteArray &data)
{
    constexpr qint64 Limit = 64LL * 1024 * 1024;
    data.clear();
    quint32 id = m_channel.nextId();
    Builder open(Open, id);
    open.string(bytes(path)).u32(OpenRead).attrs({});
    Packet reply;
    if (!m_channel.call(open.finish(), id, reply))
        return !channelFailed(tr("Не удалось открыть файл"));
    if (reply.type != Handle) {
        fail(tr("Не удалось открыть «%1»: %2")
                 .arg(path, describeStatus(reply.status(), reply.statusMessage())));
        return false;
    }
    const QByteArray handle = reply.string();

    const qint64 chunk = m_channel.maxRead();
    QHash<quint32, QPair<qint64, qint64>> pending;
    qint64 next = 0;
    qint64 size = 0;
    bool eof = false;
    QString failure;
    const auto request = [&](qint64 offset, qint64 length) {
        const quint32 readId = m_channel.nextId();
        Builder read(Read, readId);
        read.string(handle).u64(static_cast<quint64>(offset)).u32(static_cast<quint32>(length));
        pending.insert(readId, {offset, length});
        return m_channel.send(read.finish());
    };
    const qint64 planned = std::max<qint64>(sizeHint, 0) + chunk;
    while (next < planned) {
        const qint64 length = std::min(chunk, planned - next);
        if (!request(next, length))
            return !channelFailed(tr("Ошибка чтения"));
        next += length;
    }
    QElapsedTimer idle;
    idle.start();
    while (!pending.isEmpty()) {
        Packet answer;
        const auto result = m_channel.read(answer, 500);
        if (result == Channel::ReadResult::Error)
            return !channelFailed(tr("Ошибка чтения"));
        if (result == Channel::ReadResult::Timeout) {
            if (idle.elapsed() > 45000) {
                fail(tr("Сервер не отвечает."), true);
                return false;
            }
            continue;
        }
        idle.restart();
        const auto found = pending.find(answer.id);
        if (found == pending.end())
            continue;
        const auto [offset, length] = found.value();
        pending.erase(found);
        if (answer.type == Data) {
            qsizetype count = 0;
            const char *bytesRead = answer.stringView(count);
            if (count > 0) {
                if (offset + count > Limit) {
                    failure = tr("Файл слишком большой для быстрого открытия.");
                    continue;
                }
                if (data.size() < offset + count)
                    data.resize(offset + count);
                std::memcpy(data.data() + offset, bytesRead, static_cast<size_t>(count));
                size = std::max(size, offset + count);
            }
            if (count < length && !request(offset + count, length - count))
                return !channelFailed(tr("Ошибка чтения"));
        } else if (answer.status() == Eof) {
            eof = true;
        } else if (failure.isEmpty()) {
            failure = describeStatus(answer.status(), answer.statusMessage());
        }
        if (pending.isEmpty() && !eof && failure.isEmpty()) {
            if (!request(next, chunk))
                return !channelFailed(tr("Ошибка чтения"));
            next += chunk;
        }
    }
    Builder close(Close, m_channel.nextId());
    close.string(handle);
    m_channel.send(close.finish());
    if (!failure.isEmpty()) {
        fail(tr("Не удалось прочитать «%1»: %2").arg(path, failure));
        data.clear();
        return false;
    }
    data.resize(size);
    return true;
}

bool SftpConnection::writeFile(const QString &path, const QByteArray &data)
{
    quint32 id = m_channel.nextId();
    Builder open(Open, id);
    open.string(bytes(path)).u32(OpenWrite | OpenCreate | OpenTruncate).attrs({});
    Packet reply;
    if (!m_channel.call(open.finish(), id, reply))
        return !channelFailed(tr("Не удалось сохранить файл"));
    if (reply.type != Handle) {
        fail(tr("Не удалось сохранить «%1»: %2")
                 .arg(path, describeStatus(reply.status(), reply.statusMessage())));
        return false;
    }
    const QByteArray handle = reply.string();

    QSet<quint32> pending;
    const qint64 chunk = m_channel.maxWrite();
    for (qint64 offset = 0; offset < data.size(); offset += chunk) {
        const qint64 length = std::min<qint64>(chunk, data.size() - offset);
        const quint32 writeId = m_channel.nextId();
        Builder write(Write, writeId, length + handle.size() + 32);
        write.string(handle).u64(static_cast<quint64>(offset)).string(data.constData() + offset, length);
        if (!m_channel.send(write.finish()))
            return !channelFailed(tr("Не удалось сохранить файл"));
        pending.insert(writeId);
    }
    const quint32 closeId = m_channel.nextId();
    Builder close(Close, closeId);
    close.string(handle);
    if (!m_channel.send(close.finish()))
        return !channelFailed(tr("Не удалось сохранить файл"));
    pending.insert(closeId);

    QString failure;
    QElapsedTimer idle;
    idle.start();
    while (!pending.isEmpty()) {
        Packet answer;
        const auto result = m_channel.read(answer, 500);
        if (result == Channel::ReadResult::Error)
            return !channelFailed(tr("Не удалось сохранить файл"));
        if (result == Channel::ReadResult::Timeout) {
            if (idle.elapsed() > 45000) {
                fail(tr("Сервер не отвечает."), true);
                return false;
            }
            continue;
        }
        idle.restart();
        if (!pending.remove(answer.id))
            continue;
        if (answer.status() != Ok && failure.isEmpty())
            failure = describeStatus(answer.status(), answer.statusMessage());
    }
    if (!failure.isEmpty()) {
        fail(tr("Не удалось сохранить «%1»: %2").arg(path, failure));
        return false;
    }
    return true;
}
