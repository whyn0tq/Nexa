#include "sftpworker.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QThread>
#include <QUrl>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <algorithm>
#include <deque>
#include <fcntl.h>

namespace {
QByteArray remoteBytes(const QString &path)
{
    return QDir::fromNativeSeparators(path).toUtf8();
}

QString remoteParentPath(const QString &value)
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(value));
    if (path == QStringLiteral("/"))
        return path;
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : path.left(slash);
}

QString fingerprintString(const unsigned char *hash, size_t length)
{
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(length));
    for (size_t i = 0; i < length; ++i) {
        parts.append(QStringLiteral("%1")
                         .arg(hash[i], 2, 16, QLatin1Char('0')));
    }
    return parts.join(QLatin1Char(':'));
}

bool isDirectory(const sftp_attributes attributes)
{
    return attributes &&
           (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY ||
            (attributes->permissions & SSH_S_IFMT) == SSH_S_IFDIR);
}

bool isSymlink(const sftp_attributes attributes)
{
    return attributes &&
           (attributes->type == SSH_FILEXFER_TYPE_SYMLINK ||
            (attributes->permissions & SSH_S_IFMT) == SSH_S_IFLNK);
}

void appendU32(QByteArray &data, quint32 value)
{
    data.append(static_cast<char>((value >> 24) & 0xff));
    data.append(static_cast<char>((value >> 16) & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
    data.append(static_cast<char>(value & 0xff));
}

void appendString(QByteArray &data, const QByteArray &value)
{
    appendU32(data, static_cast<quint32>(value.size()));
    data.append(value);
}

bool takeU32(const QByteArray &data, qsizetype &offset, quint32 &value)
{
    if (offset < 0 || offset + 4 > data.size())
        return false;
    const auto *bytes = reinterpret_cast<const unsigned char *>(
        data.constData() + offset);
    value = (static_cast<quint32>(bytes[0]) << 24) |
            (static_cast<quint32>(bytes[1]) << 16) |
            (static_cast<quint32>(bytes[2]) << 8) |
            static_cast<quint32>(bytes[3]);
    offset += 4;
    return true;
}

bool takeU64(const QByteArray &data, qsizetype &offset, quint64 &value)
{
    quint32 high = 0;
    quint32 low = 0;
    if (!takeU32(data, offset, high) || !takeU32(data, offset, low))
        return false;
    value = (static_cast<quint64>(high) << 32) | low;
    return true;
}

bool takeString(const QByteArray &data, qsizetype &offset, QByteArray &value)
{
    quint32 length = 0;
    if (!takeU32(data, offset, length) ||
        length > static_cast<quint32>(data.size() - offset))
        return false;
    value = data.mid(offset, static_cast<qsizetype>(length));
    offset += static_cast<qsizetype>(length);
    return true;
}

bool skipAttributes(const QByteArray &data, qsizetype &offset,
                    qint64 &size, quint32 &permissions, quint32 &mtime)
{
    constexpr quint32 AttrSize = 0x00000001;
    constexpr quint32 AttrUidGid = 0x00000002;
    constexpr quint32 AttrPermissions = 0x00000004;
    constexpr quint32 AttrAcmodtime = 0x00000008;
    constexpr quint32 AttrExtended = 0x80000000;
    quint32 flags = 0;
    if (!takeU32(data, offset, flags))
        return false;
    if (flags & AttrSize) {
        quint64 rawSize = 0;
        if (!takeU64(data, offset, rawSize))
            return false;
        size = static_cast<qint64>(rawSize);
    }
    if (flags & AttrUidGid) {
        quint32 ignored = 0;
        if (!takeU32(data, offset, ignored) ||
            !takeU32(data, offset, ignored))
            return false;
    }
    if (flags & AttrPermissions) {
        if (!takeU32(data, offset, permissions))
            return false;
    }
    if (flags & AttrAcmodtime) {
        quint32 ignored = 0;
        if (!takeU32(data, offset, ignored) ||
            !takeU32(data, offset, mtime))
            return false;
    }
    if (flags & AttrExtended) {
        quint32 count = 0;
        if (!takeU32(data, offset, count))
            return false;
        for (quint32 i = 0; i < count; ++i) {
            QByteArray ignored;
            if (!takeString(data, offset, ignored) ||
                !takeString(data, offset, ignored))
                return false;
        }
    }
    return true;
}
}

SftpWorker::SftpWorker(QObject *parent) : QObject(parent)
{
    ssh_init();
}

SftpWorker::~SftpWorker()
{
    cleanup();
    ssh_finalize();
}

quint64 SftpWorker::beginDirectoryRequest() noexcept
{
    return m_directoryGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
}

void SftpWorker::requestStop() noexcept
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    beginDirectoryRequest();
}

void SftpWorker::connectToHost(const ConnectionProfile &profile)
{
    m_stopRequested.store(false, std::memory_order_relaxed);
    cleanup();
    m_profile = profile;
    if (m_profile.host.contains(QStringLiteral("://"))) {
        const QUrl url(m_profile.host.trimmed());
        if (url.isValid() && !url.host().isEmpty()) {
            m_profile.host = url.host();
            if (url.port() > 0)
                m_profile.port = static_cast<quint16>(url.port());
            if (m_profile.username.isEmpty() && !url.userName().isEmpty())
                m_profile.username =
                    QUrl::fromPercentEncoding(url.userName().toUtf8());
        }
    }

    QElapsedTimer timer;
    timer.start();
    m_session = ssh_new();
    if (!m_session) {
        if (!m_reconnecting)
            emit errorOccurred(tr("Не удалось создать SSH-сессию."));
        return;
    }

    const QByteArray host = m_profile.host.toUtf8();
    const QByteArray user = m_profile.username.toUtf8();
    unsigned int port = m_profile.port;
    long timeoutSeconds = 12;
    int enabled = 1;
    int disabled = 0;
    if (ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData()) !=
            SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_PORT, &port) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData()) !=
            SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeoutSeconds) !=
            SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_NODELAY, &enabled) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &disabled) !=
            SSH_OK) {
        if (!m_reconnecting)
            emit errorOccurred(
                sessionError(tr("Не удалось настроить SSH-сессию")));
        cleanup();
        return;
    }
    ssh_set_blocking(m_session, 1);

    if (ssh_connect(m_session) != SSH_OK) {
        if (!m_reconnecting) {
            emit errorOccurred(sessionError(
                tr("Не удалось подключиться к %1:%2")
                    .arg(m_profile.host).arg(m_profile.port)));
        }
        cleanup();
        return;
    }
    emit performanceMetric(tr("SSH-соединение"), timer.elapsed());

    ssh_key serverKey = nullptr;
    unsigned char *hash = nullptr;
    size_t hashLength = 0;
    if (ssh_get_server_publickey(m_session, &serverKey) != SSH_OK ||
        ssh_get_publickey_hash(serverKey, SSH_PUBLICKEY_HASH_SHA256,
                               &hash, &hashLength) != SSH_OK) {
        if (serverKey)
            ssh_key_free(serverKey);
        if (!m_reconnecting)
            emit errorOccurred(
                sessionError(tr("Не удалось проверить ключ сервера")));
        cleanup();
        return;
    }
    m_pendingFingerprint = fingerprintString(hash, hashLength);
    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(serverKey);

    const QString key = QStringLiteral("hostKeys/%1:%2")
                            .arg(m_profile.host).arg(m_profile.port);
    QSettings settings;
    const QString known = settings.value(key).toString();
    if (known == m_pendingFingerprint ||
        m_runtimeTrustedFingerprint == m_pendingFingerprint) {
        m_runtimeTrustedFingerprint = m_pendingFingerprint;
        authenticate();
        return;
    }

    m_waitingForHostKey = true;
    emit hostKeyConfirmationRequired(
        QStringLiteral("%1:%2").arg(m_profile.host).arg(m_profile.port),
        m_pendingFingerprint, !known.isEmpty());
}

void SftpWorker::confirmHostKey(bool accepted, bool remember)
{
    if (!m_waitingForHostKey)
        return;
    m_waitingForHostKey = false;
    if (!accepted) {
        emit errorOccurred(tr("Подключение отменено: ключ сервера не принят."));
        cleanup();
        return;
    }
    m_runtimeTrustedFingerprint = m_pendingFingerprint;
    if (remember) {
        QSettings settings;
        settings.setValue(QStringLiteral("hostKeys/%1:%2")
                              .arg(m_profile.host).arg(m_profile.port),
                          m_pendingFingerprint);
    }
    authenticate();
}

void SftpWorker::authenticate()
{
    QElapsedTimer timer;
    timer.start();
    int result = SSH_AUTH_ERROR;
    if (m_profile.usePrivateKey) {
        const QByteArray identity = QFile::encodeName(m_profile.privateKeyPath);
        if (ssh_options_set(m_session, SSH_OPTIONS_IDENTITY,
                            identity.constData()) != SSH_OK) {
            if (!m_reconnecting)
                emit errorOccurred(
                    sessionError(tr("Не удалось настроить приватный ключ")));
            cleanup();
            return;
        }
        const QByteArray passphrase = m_profile.keyPassphrase.toUtf8();
        result = ssh_userauth_publickey_auto(
            m_session, nullptr,
            passphrase.isEmpty() ? nullptr : passphrase.constData());
    } else {
        const QByteArray password = m_profile.password.toUtf8();
        result = ssh_userauth_password(m_session, nullptr,
                                       password.constData());
    }
    if (result != SSH_AUTH_SUCCESS) {
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Ошибка аутентификации")));
        cleanup();
        return;
    }

    m_sftp = sftp_new(m_session);
    if (!m_sftp || sftp_init(m_sftp) != SSH_OK) {
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Не удалось запустить SFTP")));
        cleanup();
        return;
    }
    emit performanceMetric(tr("Аутентификация и SFTP"), timer.elapsed());

    char *canonical = sftp_canonicalize_path(m_sftp, ".");
    const QString home = canonical
        ? QString::fromUtf8(canonical) : QStringLiteral("/");
    if (canonical)
        ssh_string_free_char(canonical);
    if (m_reconnecting)
        emit reconnected();
    else
        emit connected(home);
}

bool SftpWorker::reconnectSession()
{
    if (m_stopRequested.load(std::memory_order_relaxed) ||
        m_reconnecting || m_profile.host.isEmpty())
        return false;
    const ConnectionProfile savedProfile = m_profile;
    m_reconnecting = true;
    emit reconnecting();
    bool success = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0)
            QThread::msleep(static_cast<unsigned long>(500 * attempt));
        connectToHost(savedProfile);
        success = m_sftp != nullptr &&
                  m_session != nullptr &&
                  ssh_is_connected(m_session);
        if (success || m_waitingForHostKey)
            break;
    }
    m_reconnecting = false;
    return success;
}

bool SftpWorker::connectionLost() const
{
    if (!m_session || !ssh_is_connected(m_session))
        return true;
    const char *message = ssh_get_error(m_session);
    const QString details =
        message ? QString::fromUtf8(message) : QString();
    return details.contains(QStringLiteral("Socket error"),
                            Qt::CaseInsensitive) ||
           details.contains(QStringLiteral("connection reset"),
                            Qt::CaseInsensitive) ||
           details.contains(QStringLiteral("connection closed"),
                            Qt::CaseInsensitive);
}

void SftpWorker::disconnectFromHost()
{
    requestStop();
    cleanup();
    emit disconnected();
}

bool SftpWorker::ensureListingChannel()
{
    if (m_listingChannel && ssh_channel_is_open(m_listingChannel))
        return true;
    closeListingChannel();

    m_listingChannel = ssh_channel_new(m_session);
    if (!m_listingChannel ||
        ssh_channel_open_session(m_listingChannel) != SSH_OK ||
        ssh_channel_request_sftp(m_listingChannel) != SSH_OK) {
        closeListingChannel();
        return false;
    }

    QByteArray init;
    init.append(static_cast<char>(SSH_FXP_INIT));
    appendU32(init, 3);
    QByteArray version;
    if (!sendListingPacket(init) || !readListingPacket(version) ||
        version.isEmpty() ||
        static_cast<unsigned char>(version.at(0)) != SSH_FXP_VERSION) {
        closeListingChannel();
        return false;
    }
    m_listingRequestId = 1;
    return true;
}

void SftpWorker::closeListingChannel()
{
    if (!m_listingChannel)
        return;
    if (ssh_channel_is_open(m_listingChannel))
        ssh_channel_close(m_listingChannel);
    ssh_channel_free(m_listingChannel);
    m_listingChannel = nullptr;
}

bool SftpWorker::sendListingPacket(const QByteArray &payload)
{
    if (!m_listingChannel)
        return false;
    QByteArray packet;
    packet.reserve(payload.size() + 4);
    appendU32(packet, static_cast<quint32>(payload.size()));
    packet.append(payload);
    qsizetype offset = 0;
    while (offset < packet.size()) {
        const int written = ssh_channel_write(
            m_listingChannel, packet.constData() + offset,
            static_cast<uint32_t>(packet.size() - offset));
        if (written <= 0)
            return false;
        offset += written;
    }
    return true;
}

bool SftpWorker::readListingPacket(QByteArray &payload)
{
    payload.clear();
    auto readExact = [this](char *target, qsizetype length) {
        qsizetype done = 0;
        while (done < length) {
            const int count = ssh_channel_read(
                m_listingChannel, target + done,
                static_cast<uint32_t>(length - done), 0);
            if (count <= 0)
                return false;
            done += count;
        }
        return true;
    };

    char lengthBytes[4]{};
    if (!readExact(lengthBytes, 4))
        return false;
    const auto *bytes =
        reinterpret_cast<const unsigned char *>(lengthBytes);
    const quint32 length =
        (static_cast<quint32>(bytes[0]) << 24) |
        (static_cast<quint32>(bytes[1]) << 16) |
        (static_cast<quint32>(bytes[2]) << 8) |
        static_cast<quint32>(bytes[3]);
    if (length == 0 || length > 32 * 1024 * 1024)
        return false;
    payload.resize(static_cast<qsizetype>(length));
    return readExact(payload.data(), payload.size());
}

bool SftpWorker::listDirectoryPipelined(const QString &path,
                                        quint64 generation,
                                        RemoteEntries *result,
                                        int pipelineDepth,
                                        bool background)
{
    if (!ensureListingChannel())
        return false;

    QElapsedTimer timer;
    timer.start();
    const QString cleanPath = normalizedRemotePath(path);
    QByteArray request;
    request.append(static_cast<char>(SSH_FXP_OPENDIR));
    const quint32 openId = m_listingRequestId++;
    appendU32(request, openId);
    appendString(request, remoteBytes(cleanPath));
    QByteArray response;
    if (!sendListingPacket(request) || !readListingPacket(response)) {
        closeListingChannel();
        return false;
    }

    qsizetype offset = 0;
    if (response.isEmpty() ||
        static_cast<unsigned char>(response.at(offset++)) != SSH_FXP_HANDLE)
        return false;
    quint32 responseId = 0;
    QByteArray handle;
    if (!takeU32(response, offset, responseId) ||
        responseId != openId ||
        !takeString(response, offset, handle))
        return false;

    int outstanding = 0;
    bool eofReceived = false;
    auto sendReadDirectory = [&]() {
        QByteArray readRequest;
        readRequest.append(static_cast<char>(SSH_FXP_READDIR));
        appendU32(readRequest, m_listingRequestId++);
        appendString(readRequest, handle);
        if (!sendListingPacket(readRequest))
            return false;
        ++outstanding;
        return true;
    };
    for (int i = 0; i < pipelineDepth; ++i) {
        if (!sendReadDirectory()) {
            closeListingChannel();
            return false;
        }
    }

    RemoteEntries entries;
    while (outstanding > 0) {
        if (m_stopRequested.load(std::memory_order_relaxed)) {
            closeListingChannel();
            return false;
        }
        if (generation !=
            m_directoryGeneration.load(std::memory_order_relaxed)) {
            closeListingChannel();
            return true;
        }
        if (!readListingPacket(response)) {
            closeListingChannel();
            return false;
        }
        --outstanding;
        offset = 0;
        if (response.isEmpty()) {
            closeListingChannel();
            return false;
        }
        const quint8 type =
            static_cast<unsigned char>(response.at(offset++));
        quint32 id = 0;
        if (!takeU32(response, offset, id)) {
            closeListingChannel();
            return false;
        }

        if (type == SSH_FXP_NAME) {
            quint32 count = 0;
            if (!takeU32(response, offset, count)) {
                closeListingChannel();
                return false;
            }
            for (quint32 i = 0; i < count; ++i) {
                QByteArray nameBytes;
                QByteArray longName;
                qint64 size = 0;
                quint32 permissions = 0;
                quint32 modified = 0;
                if (!takeString(response, offset, nameBytes) ||
                    !takeString(response, offset, longName) ||
                    !skipAttributes(response, offset, size,
                                    permissions, modified)) {
                    closeListingChannel();
                    return false;
                }
                const QString name = QString::fromUtf8(nameBytes);
                if (name == QStringLiteral(".") ||
                    name == QStringLiteral(".."))
                    continue;
                RemoteEntry entry;
                entry.name = name;
                entry.path = cleanPath == QStringLiteral("/")
                    ? QStringLiteral("/") + name
                    : cleanPath + QStringLiteral("/") + name;
                entry.size = size;
                entry.directory =
                    (permissions & SSH_S_IFMT) == SSH_S_IFDIR ||
                    (!longName.isEmpty() && longName.at(0) == 'd');
                entry.symlink =
                    (permissions & SSH_S_IFMT) == SSH_S_IFLNK ||
                    (!longName.isEmpty() && longName.at(0) == 'l');
                if (modified > 0)
                    entry.modified =
                        QDateTime::fromSecsSinceEpoch(modified);
                entries.append(entry);
            }
            if (!eofReceived && !sendReadDirectory()) {
                closeListingChannel();
                return false;
            }
        } else if (type == SSH_FXP_STATUS) {
            quint32 status = 0;
            if (!takeU32(response, offset, status)) {
                closeListingChannel();
                return false;
            }
            if (status != SSH_FX_EOF && status != SSH_FX_FAILURE) {
                closeListingChannel();
                return false;
            }
            eofReceived = true;
        } else {
            closeListingChannel();
            return false;
        }
    }

    QByteArray closeRequest;
    closeRequest.append(static_cast<char>(SSH_FXP_CLOSE));
    appendU32(closeRequest, m_listingRequestId++);
    appendString(closeRequest, handle);
    if (!sendListingPacket(closeRequest) ||
        !readListingPacket(response)) {
        closeListingChannel();
    }

    if (generation !=
        m_directoryGeneration.load(std::memory_order_relaxed))
        return true;
    std::sort(entries.begin(), entries.end(),
              [](const RemoteEntry &a, const RemoteEntry &b) {
                  if (a.directory != b.directory)
                      return a.directory > b.directory;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    if (result)
        *result = entries;
    if (!background) {
        emit performanceMetric(
            tr("Pipelined-каталог %1 (%2 объектов)")
                .arg(cleanPath).arg(entries.size()),
            timer.elapsed());
    }
    emit directoryListed(cleanPath, entries);
    return true;
}

void SftpWorker::listDirectory(const QString &path, quint64 generation)
{
    listDirectoryWithRetry(path, generation, 2);
}

void SftpWorker::listDirectoryWithRetry(const QString &path,
                                        quint64 generation,
                                        int retries)
{
    if (m_stopRequested.load(std::memory_order_relaxed))
        return;
    if (!m_sftp) {
        if (retries > 0 && reconnectSession()) {
            const quint64 nextGeneration = beginDirectoryRequest();
            listDirectoryWithRetry(path, nextGeneration, retries - 1);
            return;
        }
        emit errorOccurred(tr("Нет активного SFTP-подключения."));
        return;
    }
    if (generation !=
        m_directoryGeneration.load(std::memory_order_relaxed))
        return;
    RemoteEntries pipelinedEntries;
    if (listDirectoryPipelined(path, generation, &pipelinedEntries)) {
        int prefetched = 0;
        for (const auto &entry : pipelinedEntries) {
            if (!entry.directory || entry.symlink)
                continue;
            if (generation !=
                m_directoryGeneration.load(std::memory_order_relaxed))
                break;
            RemoteEntries ignored;
            listDirectoryPipelined(entry.path, generation, &ignored, 8, true);
            if (++prefetched >= 16)
                break;
        }
        return;
    }
    if (retries > 0 && connectionLost() && reconnectSession()) {
        const quint64 nextGeneration = beginDirectoryRequest();
        listDirectoryWithRetry(path, nextGeneration, retries - 1);
        return;
    }

    QElapsedTimer timer;
    timer.start();
    const QString cleanPath = normalizedRemotePath(path);
    const QByteArray encoded = remoteBytes(cleanPath);
    sftp_dir directory = sftp_opendir(m_sftp, encoded.constData());
    if (!directory) {
        if (retries > 0 && connectionLost() && reconnectSession()) {
            const quint64 nextGeneration = beginDirectoryRequest();
            listDirectoryWithRetry(path, nextGeneration, retries - 1);
            return;
        }
        if (generation ==
            m_directoryGeneration.load(std::memory_order_relaxed)) {
            emit errorOccurred(sessionError(
                tr("Не удалось открыть удалённую папку «%1»").arg(cleanPath)));
        }
        return;
    }
    emit performanceMetric(tr("Открытие папки %1").arg(cleanPath),
                           timer.elapsed());

    RemoteEntries entries;
    RemoteEntries batch;
    batch.reserve(32);
    QElapsedTimer flushTimer;
    flushTimer.start();
    bool firstEntryReceived = false;
    while (generation ==
           m_directoryGeneration.load(std::memory_order_relaxed)) {
        sftp_attributes attributes = sftp_readdir(m_sftp, directory);
        if (!attributes)
            break;
        if (!firstEntryReceived) {
            firstEntryReceived = true;
            emit performanceMetric(tr("Первые файлы %1").arg(cleanPath),
                                   timer.elapsed());
        }
        const QString name = QString::fromUtf8(attributes->name);
        if (name != QStringLiteral(".") && name != QStringLiteral("..")) {
            RemoteEntry entry;
            entry.name = name;
            entry.path = cleanPath == QStringLiteral("/")
                ? QStringLiteral("/") + name
                : cleanPath + QStringLiteral("/") + name;
            entry.size = static_cast<qint64>(attributes->size);
            entry.directory = isDirectory(attributes);
            entry.symlink = isSymlink(attributes);
            const uint64_t modified =
                attributes->mtime64 ? attributes->mtime64 : attributes->mtime;
            if (modified > 0)
                entry.modified =
                    QDateTime::fromSecsSinceEpoch(static_cast<qint64>(modified));
            entries.append(entry);
            batch.append(entry);
            if (batch.size() >= 32 || directory->count == 0 ||
                flushTimer.elapsed() >= 40) {
                emit directoryBatchReceived(cleanPath, batch);
                batch.clear();
                flushTimer.restart();
            }
        }
        sftp_attributes_free(attributes);
    }
    const bool directoryFailed = !sftp_dir_eof(directory);
    sftp_closedir(directory);

    if (directoryFailed && retries > 0 && connectionLost() &&
        reconnectSession()) {
        const quint64 nextGeneration = beginDirectoryRequest();
        listDirectoryWithRetry(path, nextGeneration, retries - 1);
        return;
    }

    if (generation !=
        m_directoryGeneration.load(std::memory_order_relaxed))
        return;
    if (!batch.isEmpty())
        emit directoryBatchReceived(cleanPath, batch);

    std::sort(entries.begin(), entries.end(),
              [](const RemoteEntry &a, const RemoteEntry &b) {
                  if (a.directory != b.directory)
                      return a.directory > b.directory;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    emit performanceMetric(
        tr("Каталог %1 (%2 объектов)").arg(cleanPath).arg(entries.size()),
        timer.elapsed());
    emit directoryListed(cleanPath, entries);
}

void SftpWorker::downloadFile(const QString &remotePath,
                              const QString &localPath,
                              bool directory)
{
    if (!m_sftp)
        return;
    emit transferStarted(QFileInfo(remotePath).fileName(), false);
    if (downloadEntry(remotePath, localPath, directory))
        emit transferFinished(QFileInfo(remotePath).fileName());
}

bool SftpWorker::downloadEntry(const QString &remotePath,
                               const QString &localPath,
                               bool directory,
                               int retries)
{
    if (m_stopRequested.load(std::memory_order_relaxed))
        return false;
    if (directory) {
        if (!QDir().mkpath(localPath)) {
            emit errorOccurred(
                tr("Не удалось создать локальную папку «%1».").arg(localPath));
            return false;
        }
        const QByteArray encoded = remoteBytes(remotePath);
        sftp_dir handle = sftp_opendir(m_sftp, encoded.constData());
        if (!handle) {
            if (retries > 0 && connectionLost() &&
                reconnectSession()) {
                return downloadEntry(remotePath, localPath, directory,
                                     retries - 1);
            }
            emit errorOccurred(sessionError(
                tr("Не удалось открыть удалённую папку «%1»").arg(remotePath)));
            return false;
        }
        while (!m_stopRequested.load(std::memory_order_relaxed)) {
            sftp_attributes attributes = sftp_readdir(m_sftp, handle);
            if (!attributes)
                break;
            const QString name = QString::fromUtf8(attributes->name);
            if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
                sftp_attributes_free(attributes);
                continue;
            }
            const bool childDirectory =
                isDirectory(attributes) && !isSymlink(attributes);
            const QString childRemote =
                (remotePath.endsWith(QLatin1Char('/'))
                     ? remotePath : remotePath + QLatin1Char('/')) + name;
            const QString childLocal = QDir(localPath).filePath(name);
            sftp_attributes_free(attributes);
            if (!downloadEntry(childRemote, childLocal, childDirectory,
                               retries)) {
                sftp_closedir(handle);
                return false;
            }
        }
        if (m_stopRequested.load(std::memory_order_relaxed)) {
            sftp_closedir(handle);
            return false;
        }
        const bool directoryComplete = sftp_dir_eof(handle);
        sftp_closedir(handle);
        if (!directoryComplete) {
            if (retries > 0 && connectionLost() && reconnectSession()) {
                return downloadEntry(remotePath, localPath, directory,
                                     retries - 1);
            }
            emit errorOccurred(sessionError(
                tr("Не удалось полностью прочитать удалённую папку «%1»")
                    .arg(remotePath)));
            return false;
        }
        return true;
    }

    const QByteArray remote = remoteBytes(remotePath);
    sftp_file handle = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (!handle) {
        if (retries > 0 && connectionLost() &&
            reconnectSession()) {
            return downloadEntry(remotePath, localPath, directory,
                                 retries - 1);
        }
        emit errorOccurred(sessionError(
            tr("Не удалось открыть «%1» для скачивания").arg(remotePath)));
        return false;
    }
    QFile output(localPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        sftp_close(handle);
        emit errorOccurred(
            tr("Не удалось создать «%1»: %2")
                .arg(localPath, output.errorString()));
        return false;
    }

    sftp_attributes attributes = sftp_fstat(handle);
    const qint64 total =
        attributes ? static_cast<qint64>(attributes->size) : 0;
    if (attributes)
        sftp_attributes_free(attributes);

    size_t chunkSize = 64 * 1024;
    if (sftp_limits_t limits = sftp_limits(m_sftp)) {
        if (limits->max_read_length > 0) {
            chunkSize = static_cast<size_t>(
                std::min<uint64_t>(limits->max_read_length, 256 * 1024));
        }
        sftp_limits_free(limits);
    }
    chunkSize = std::max<size_t>(chunkSize, 16 * 1024);

    struct PendingRead {
        sftp_aio aio = nullptr;
        QByteArray buffer;
    };
    constexpr size_t TransferWindow = 64;
    std::deque<PendingRead> pending;
    qint64 scheduled = 0;
    qint64 done = 0;
    bool transferFailed = false;
    QElapsedTimer progressTimer;
    progressTimer.start();
    while (!m_stopRequested.load(std::memory_order_relaxed) &&
           (done < total || !pending.empty())) {
        while (pending.size() < TransferWindow && scheduled < total) {
            const size_t wanted = static_cast<size_t>(
                std::min<qint64>(static_cast<qint64>(chunkSize),
                                 total - scheduled));
            PendingRead request;
            const ssize_t requested =
                sftp_aio_begin_read(handle, wanted, &request.aio);
            if (requested <= 0) {
                transferFailed = true;
                break;
            }
            request.buffer.resize(static_cast<qsizetype>(requested));
            scheduled += requested;
            pending.push_back(std::move(request));
        }
        if (transferFailed || pending.empty())
            break;

        PendingRead request = std::move(pending.front());
        pending.pop_front();
        const ssize_t count = sftp_aio_wait_read(
            &request.aio, request.buffer.data(),
            static_cast<size_t>(request.buffer.size()));
        if (count <= 0 ||
            output.write(request.buffer.constData(), count) != count) {
            transferFailed = true;
            break;
        }
        done += count;
        if (progressTimer.elapsed() >= 100 || done == total) {
            emit transferProgress(QFileInfo(remotePath).fileName(),
                                  done, total);
            progressTimer.restart();
        }
    }

    for (auto &request : pending) {
        if (request.aio)
            sftp_aio_free(request.aio);
    }
    if (transferFailed || done != total) {
        output.close();
        output.remove();
        sftp_close(handle);
        if (m_stopRequested.load(std::memory_order_relaxed))
            return false;
        if (retries > 0 && connectionLost() &&
            reconnectSession()) {
            return downloadEntry(remotePath, localPath, directory,
                                 retries - 1);
        }
        emit errorOccurred(sessionError(
            tr("Ошибка при скачивании «%1»").arg(remotePath)));
        return false;
    }
    output.close();
    sftp_close(handle);
    return true;
}

void SftpWorker::uploadFile(const QString &localPath,
                            const QString &remotePath,
                            bool directory)
{
    if (!m_sftp)
        return;
    emit transferStarted(QFileInfo(localPath).fileName(), true);
    if (uploadEntry(localPath, remotePath, directory))
        emit transferFinished(QFileInfo(localPath).fileName());
}

bool SftpWorker::uploadEntry(const QString &localPath,
                             const QString &remotePath,
                             bool directory,
                             int retries)
{
    if (m_stopRequested.load(std::memory_order_relaxed))
        return false;
    if (directory) {
        const QByteArray encoded = remoteBytes(remotePath);
        if (sftp_mkdir(m_sftp, encoded.constData(), 0755) != SSH_OK) {
            sftp_attributes attributes =
                sftp_stat(m_sftp, encoded.constData());
            const bool exists = isDirectory(attributes);
            if (attributes)
                sftp_attributes_free(attributes);
            if (!exists) {
                if (retries > 0 && connectionLost() &&
                    reconnectSession()) {
                    return uploadEntry(localPath, remotePath, directory,
                                       retries - 1);
                }
                emit errorOccurred(sessionError(
                    tr("Не удалось создать удалённую папку «%1»")
                        .arg(remotePath)));
                return false;
            }
        }
        const QFileInfoList children = QDir(localPath).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name);
        for (const auto &child : children) {
            const QString childRemote =
                (remotePath.endsWith(QLatin1Char('/'))
                     ? remotePath : remotePath + QLatin1Char('/'))
                + child.fileName();
            if (!uploadEntry(child.absoluteFilePath(), childRemote,
                             child.isDir() && !child.isSymLink(), retries))
                return false;
        }
        return true;
    }

    QFile input(localPath);
    if (!input.open(QIODevice::ReadOnly)) {
        emit errorOccurred(
            tr("Не удалось открыть «%1»: %2")
                .arg(localPath, input.errorString()));
        return false;
    }
    const QByteArray remote = remoteBytes(remotePath);
    sftp_file handle = sftp_open(
        m_sftp, remote.constData(),
        O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!handle) {
        if (retries > 0 && connectionLost() &&
            reconnectSession()) {
            return uploadEntry(localPath, remotePath, directory,
                               retries - 1);
        }
        emit errorOccurred(sessionError(
            tr("Не удалось создать «%1» на сервере").arg(remotePath)));
        return false;
    }

    const qint64 total = input.size();
    size_t chunkSize = 64 * 1024;
    if (sftp_limits_t limits = sftp_limits(m_sftp)) {
        if (limits->max_write_length > 0) {
            chunkSize = static_cast<size_t>(
                std::min<uint64_t>(limits->max_write_length, 256 * 1024));
        }
        sftp_limits_free(limits);
    }
    chunkSize = std::max<size_t>(chunkSize, 16 * 1024);

    struct PendingWrite {
        sftp_aio aio = nullptr;
        QByteArray buffer;
        size_t requested = 0;
    };
    constexpr size_t TransferWindow = 64;
    std::deque<PendingWrite> pending;
    qint64 done = 0;
    bool transferFailed = false;
    QElapsedTimer progressTimer;
    progressTimer.start();
    while (!m_stopRequested.load(std::memory_order_relaxed) &&
           (!input.atEnd() || !pending.empty())) {
        while (pending.size() < TransferWindow && !input.atEnd()) {
            PendingWrite request;
            request.buffer = input.read(static_cast<qint64>(chunkSize));
            if (request.buffer.isEmpty() && !input.atEnd()) {
                transferFailed = true;
                break;
            }
            const ssize_t requested = sftp_aio_begin_write(
                handle, request.buffer.constData(),
                static_cast<size_t>(request.buffer.size()), &request.aio);
            if (requested <= 0) {
                transferFailed = true;
                break;
            }
            request.requested = static_cast<size_t>(requested);
            if (requested < request.buffer.size()) {
                const qint64 unused =
                    request.buffer.size() - static_cast<qint64>(requested);
                input.seek(input.pos() - unused);
                request.buffer.resize(static_cast<qsizetype>(requested));
            }
            pending.push_back(std::move(request));
        }
        if (transferFailed || pending.empty())
            break;

        PendingWrite request = std::move(pending.front());
        pending.pop_front();
        const ssize_t written = sftp_aio_wait_write(&request.aio);
        if (written <= 0 ||
            static_cast<size_t>(written) != request.requested) {
            transferFailed = true;
            break;
        }
        done += written;
        if (progressTimer.elapsed() >= 100 || done == total) {
            emit transferProgress(QFileInfo(localPath).fileName(),
                                  done, total);
            progressTimer.restart();
        }
    }

    for (auto &request : pending) {
        if (request.aio)
            sftp_aio_free(request.aio);
    }
    if (transferFailed || done != total) {
        sftp_close(handle);
        if (m_stopRequested.load(std::memory_order_relaxed))
            return false;
        if (retries > 0 && connectionLost() && reconnectSession()) {
            return uploadEntry(localPath, remotePath, directory,
                               retries - 1);
        }
        emit errorOccurred(sessionError(
            tr("Ошибка при загрузке «%1»").arg(localPath)));
        return false;
    }
    sftp_close(handle);
    return true;
}

void SftpWorker::createDirectory(const QString &path)
{
    const QByteArray encoded = remoteBytes(path);
    if (!m_sftp ||
        sftp_mkdir(m_sftp, encoded.constData(), 0755) != SSH_OK) {
        emit errorOccurred(sessionError(
            tr("Не удалось создать папку «%1»").arg(path)));
        return;
    }
    const quint64 generation = beginDirectoryRequest();
    listDirectory(remoteParentPath(path), generation);
}

void SftpWorker::createFile(const QString &path)
{
    if (!m_sftp)
        return;
    const QByteArray encoded = remoteBytes(path);
    sftp_file handle = sftp_open(
        m_sftp, encoded.constData(),
        O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (!handle) {
        emit errorOccurred(sessionError(
            tr("Не удалось создать файл «%1»").arg(path)));
        return;
    }
    sftp_close(handle);
    const quint64 generation = beginDirectoryRequest();
    listDirectory(remoteParentPath(path), generation);
}

void SftpWorker::renameEntry(const QString &oldPath, const QString &newPath)
{
    if (!m_sftp)
        return;
    const QByteArray oldEncoded = remoteBytes(oldPath);
    const QByteArray newEncoded = remoteBytes(newPath);
    if (sftp_rename(m_sftp, oldEncoded.constData(),
                    newEncoded.constData()) != SSH_OK) {
        emit errorOccurred(sessionError(
            tr("Не удалось переименовать «%1» в «%2»")
                .arg(oldPath, newPath)));
        return;
    }
    const quint64 generation = beginDirectoryRequest();
    listDirectory(remoteParentPath(newPath), generation);
}

void SftpWorker::removeEntry(const QString &path, bool directory)
{
    if (!m_sftp)
        return;
    quint64 removedCount = 0;
    QString failedPath;
    if (!removeRecursively(path, directory, removedCount, failedPath)) {
        if (m_stopRequested.load(std::memory_order_relaxed))
            return;
        emit errorOccurred(sessionError(
            tr("Не удалось удалить «%1»")
                .arg(failedPath.isEmpty() ? path : failedPath)));
        return;
    }
    emit deletionProgress(static_cast<qint64>(removedCount), path);
    const quint64 generation = beginDirectoryRequest();
    listDirectory(remoteParentPath(path), generation);
}

bool SftpWorker::removeRecursively(const QString &path, bool directory,
                                   quint64 &removedCount,
                                   QString &failedPath)
{
    if (m_stopRequested.load(std::memory_order_relaxed))
        return false;
    const QByteArray encoded = remoteBytes(path);

    sftp_attributes ownAttributes =
        sftp_lstat(m_sftp, encoded.constData());
    if (ownAttributes) {
        if (isSymlink(ownAttributes))
            directory = false;
        sftp_attributes_free(ownAttributes);
    }

    if (!directory) {
        if (sftp_unlink(m_sftp, encoded.constData()) != SSH_OK) {
            failedPath = path;
            return false;
        }
        ++removedCount;
        if (removedCount % 25 == 0)
            emit deletionProgress(static_cast<qint64>(removedCount), path);
        return true;
    }

    sftp_dir handle = sftp_opendir(m_sftp, encoded.constData());
    if (!handle) {
        if (sftp_unlink(m_sftp, encoded.constData()) == SSH_OK) {
            ++removedCount;
            return true;
        }
        failedPath = path;
        return false;
    }
    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        sftp_attributes attributes = sftp_readdir(m_sftp, handle);
        if (!attributes)
            break;
        const QString name = QString::fromUtf8(attributes->name);
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
            sftp_attributes_free(attributes);
            continue;
        }
        const bool childDirectory =
            isDirectory(attributes) && !isSymlink(attributes);
        const QString child =
            (path.endsWith(QLatin1Char('/'))
                 ? path : path + QLatin1Char('/')) + name;
        sftp_attributes_free(attributes);
        if (!removeRecursively(child, childDirectory,
                               removedCount, failedPath)) {
            sftp_closedir(handle);
            return false;
        }
    }
    if (m_stopRequested.load(std::memory_order_relaxed)) {
        sftp_closedir(handle);
        return false;
    }
    const bool directoryComplete = sftp_dir_eof(handle);
    sftp_closedir(handle);
    if (!directoryComplete) {
        failedPath = path;
        return false;
    }
    if (sftp_rmdir(m_sftp, encoded.constData()) != SSH_OK) {
        failedPath = path;
        return false;
    }
    ++removedCount;
    if (removedCount % 25 == 0)
        emit deletionProgress(static_cast<qint64>(removedCount), path);
    return true;
}

void SftpWorker::cleanup()
{
    m_waitingForHostKey = false;
    beginDirectoryRequest();
    closeListingChannel();
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
    if (m_session) {
        if (ssh_is_connected(m_session))
            ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }
}

QString SftpWorker::sessionError(const QString &prefix) const
{
    if (!m_session)
        return prefix;
    const char *message = ssh_get_error(m_session);
    const QString details =
        message ? QString::fromUtf8(message) : QString();
    if (!ssh_is_connected(m_session) ||
        details.contains(QStringLiteral("Socket error: No error"),
                         Qt::CaseInsensitive)) {
        return tr("%1: сервер закрыл SFTP-соединение. "
                  "Подключитесь повторно.")
            .arg(prefix);
    }
    return !message || !*message
        ? prefix
        : QStringLiteral("%1: %2").arg(prefix, details);
}

QString SftpWorker::normalizedRemotePath(const QString &path) const
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!result.startsWith(QLatin1Char('/')))
        result.prepend(QLatin1Char('/'));
    return result;
}
