#include "sftpworker.h"

#include "transferplan.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSettings>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <fcntl.h>

namespace {
constexpr quint32 AttrSize = 0x00000001;
constexpr quint32 AttrUidGid = 0x00000002;
constexpr quint32 AttrPermissions = 0x00000004;
constexpr quint32 AttrAcModTime = 0x00000008;
constexpr quint32 AttrExtended = 0x80000000;

const QLatin1String PartialSuffix(".nexapart");

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

QString joinRemote(const QString &directory, const QString &name)
{
    if (directory.endsWith(QLatin1Char('/')))
        return directory + name;
    return directory + QLatin1Char('/') + name;
}

QString fingerprintString(const unsigned char *hash, size_t length)
{
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(length));
    for (size_t i = 0; i < length; ++i)
        parts.append(QStringLiteral("%1").arg(hash[i], 2, 16, QLatin1Char('0')));
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

qint64 attributeMTime(const sftp_attributes attributes)
{
    if (!attributes)
        return 0;
    return static_cast<qint64>(attributes->mtime64 ? attributes->mtime64
                                                   : attributes->mtime);
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
    const auto *bytes =
        reinterpret_cast<const unsigned char *>(data.constData() + offset);
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

bool skipAttributes(const QByteArray &data, qsizetype &offset, qint64 &size,
                    quint32 &permissions, quint32 &mtime)
{
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
        if (!takeU32(data, offset, ignored) || !takeU32(data, offset, ignored))
            return false;
    }
    if ((flags & AttrPermissions) && !takeU32(data, offset, permissions))
        return false;
    if (flags & AttrAcModTime) {
        quint32 ignored = 0;
        if (!takeU32(data, offset, ignored) || !takeU32(data, offset, mtime))
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

void sortEntries(RemoteEntries &entries)
{
    std::sort(entries.begin(), entries.end(),
              [](const RemoteEntry &a, const RemoteEntry &b) {
                  if (a.directory != b.directory)
                      return a.directory > b.directory;
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
}

QString numberedVariant(const QString &path, int index)
{
    const qsizetype slash =
        std::max(path.lastIndexOf(QLatin1Char('/')),
                 path.lastIndexOf(QLatin1Char('\\')));
    const QString directory = slash < 0 ? QString() : path.left(slash + 1);
    const QString name = path.mid(slash + 1);
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot <= 0 ? name : name.left(dot);
    const QString suffix = dot <= 0 ? QString() : name.mid(dot);
    return QStringLiteral("%1%2 (%3)%4")
        .arg(directory, stem, QString::number(index), suffix);
}
}

SftpWorker::SftpWorker(Role role, QObject *parent)
    : QObject(parent), m_role(role)
{
}

SftpWorker::~SftpWorker()
{
    cleanup();
}

quint64 SftpWorker::beginDirectoryRequest() noexcept
{
    return m_directoryGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
}

void SftpWorker::requestStop() noexcept
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    m_cancelAll.store(true, std::memory_order_relaxed);
    beginDirectoryRequest();
}

void SftpWorker::cancelJob(quint64 id)
{
    QMutexLocker locker(&m_cancelMutex);
    m_canceledJobs.insert(id);
}

void SftpWorker::cancelAllJobs()
{
    m_cancelAll.store(true, std::memory_order_relaxed);
}

void SftpWorker::setOverwritePolicy(OverwritePolicy policy) noexcept
{
    m_overwritePolicy.store(static_cast<int>(policy), std::memory_order_relaxed);
}

bool SftpWorker::isBusy() const noexcept
{
    return m_busy.load(std::memory_order_relaxed);
}

bool SftpWorker::stopping() const noexcept
{
    return m_stopRequested.load(std::memory_order_relaxed);
}

bool SftpWorker::jobCanceled(quint64 id) const
{
    if (m_cancelAll.load(std::memory_order_relaxed) || stopping())
        return true;
    QMutexLocker locker(&m_cancelMutex);
    return m_canceledJobs.contains(id);
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
    if (!openSession())
        return;
    if (m_waitingForHostKey)
        return;
    finishConnect();
}

void SftpWorker::finishConnect()
{
    if (!authenticate() || !startSftp())
        return;
    if (m_reconnecting) {
        emit reconnected();
        return;
    }
    char *canonical = sftp_canonicalize_path(m_sftp, ".");
    const QString home =
        canonical ? QString::fromUtf8(canonical) : QStringLiteral("/");
    if (canonical)
        ssh_string_free_char(canonical);
    emit connected(home, m_serverBanner, m_runtimeTrustedFingerprint);
}

bool SftpWorker::openSession()
{
    QElapsedTimer timer;
    timer.start();
    m_session = ssh_new();
    if (!m_session) {
        if (!m_reconnecting)
            emit errorOccurred(tr("Не удалось создать SSH-сессию."));
        return false;
    }

    const QByteArray host = m_profile.host.toUtf8();
    const QByteArray user = m_profile.username.toUtf8();
    unsigned int port = m_profile.port;
    long timeoutSeconds = 15;
    int enabled = 1;
    int disabled = 0;
    if (ssh_options_set(m_session, SSH_OPTIONS_HOST, host.constData()) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_PORT, &port) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData()) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeoutSeconds) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_NODELAY, &enabled) != SSH_OK ||
        ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &disabled) != SSH_OK) {
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Не удалось настроить SSH-сессию")));
        cleanup();
        return false;
    }
    ssh_set_blocking(m_session, 1);

    if (ssh_connect(m_session) != SSH_OK) {
        if (!m_reconnecting) {
            emit errorOccurred(sessionError(tr("Не удалось подключиться к %1:%2")
                                                .arg(m_profile.host)
                                                .arg(m_profile.port)));
        }
        cleanup();
        return false;
    }
    emit performanceMetric(tr("SSH-соединение"), timer.elapsed());

    if (const char *banner = ssh_get_serverbanner(m_session))
        m_serverBanner = QString::fromUtf8(banner);

    ssh_key serverKey = nullptr;
    unsigned char *hash = nullptr;
    size_t hashLength = 0;
    if (ssh_get_server_publickey(m_session, &serverKey) != SSH_OK ||
        ssh_get_publickey_hash(serverKey, SSH_PUBLICKEY_HASH_SHA256, &hash,
                               &hashLength) != SSH_OK) {
        if (serverKey)
            ssh_key_free(serverKey);
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Не удалось проверить ключ сервера")));
        cleanup();
        return false;
    }
    m_pendingFingerprint = fingerprintString(hash, hashLength);
    if (const char *keyType = ssh_key_type_to_char(ssh_key_type(serverKey)))
        m_pendingKeyType = QString::fromUtf8(keyType);
    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(serverKey);

    const QString settingsKey =
        QStringLiteral("hostKeys/%1:%2").arg(m_profile.host).arg(m_profile.port);
    const QString known = QSettings().value(settingsKey).toString();
    if (known == m_pendingFingerprint ||
        m_runtimeTrustedFingerprint == m_pendingFingerprint ||
        m_profile.trustedFingerprint == m_pendingFingerprint) {
        m_runtimeTrustedFingerprint = m_pendingFingerprint;
        return true;
    }

    if (m_role == Role::Transfers) {
        emit errorOccurred(
            tr("Ключ сервера %1 не совпадает с подтверждённым. Соединение для "
               "передач не открыто.")
                .arg(m_profile.host));
        cleanup();
        return false;
    }

    m_waitingForHostKey = true;
    emit hostKeyConfirmationRequired(
        QStringLiteral("%1:%2").arg(m_profile.host).arg(m_profile.port),
        m_pendingFingerprint, m_pendingKeyType, !known.isEmpty());
    return true;
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
        QSettings().setValue(QStringLiteral("hostKeys/%1:%2")
                                 .arg(m_profile.host)
                                 .arg(m_profile.port),
                             m_pendingFingerprint);
    }
    finishConnect();
}

bool SftpWorker::authenticate()
{
    QElapsedTimer timer;
    timer.start();

    int result = ssh_userauth_none(m_session, nullptr);
    if (result == SSH_AUTH_SUCCESS) {
        emit performanceMetric(tr("Аутентификация"), timer.elapsed());
        return true;
    }
    if (result == SSH_AUTH_ERROR && connectionLost()) {
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Ошибка аутентификации")));
        cleanup();
        return false;
    }

    const int listed = ssh_userauth_list(m_session, nullptr);
    const unsigned int methods =
        listed < 0 ? 0xffffffffu : static_cast<unsigned int>(listed);

    if (m_profile.usePrivateKey && authenticateWithKey(methods)) {
        emit performanceMetric(tr("Аутентификация по ключу"), timer.elapsed());
        return true;
    }
    if (m_profile.useAgent && (methods & SSH_AUTH_METHOD_PUBLICKEY) &&
        ssh_userauth_agent(m_session, nullptr) == SSH_AUTH_SUCCESS) {
        emit performanceMetric(tr("Аутентификация через агент"), timer.elapsed());
        return true;
    }
    if (!m_profile.usePrivateKey && !m_profile.password.isEmpty()) {
        if (methods & SSH_AUTH_METHOD_PASSWORD) {
            const QByteArray password = m_profile.password.toUtf8();
            if (ssh_userauth_password(m_session, nullptr, password.constData()) ==
                SSH_AUTH_SUCCESS) {
                emit performanceMetric(tr("Аутентификация по паролю"),
                                       timer.elapsed());
                return true;
            }
        }

        if ((methods & SSH_AUTH_METHOD_INTERACTIVE) && authenticateInteractive()) {
            emit performanceMetric(tr("Интерактивная аутентификация"),
                                   timer.elapsed());
            return true;
        }
    }

    if (!m_reconnecting) {
        QStringList offered;
        if (methods & SSH_AUTH_METHOD_PASSWORD)
            offered.append(tr("пароль"));
        if (methods & SSH_AUTH_METHOD_PUBLICKEY)
            offered.append(tr("публичный ключ"));
        if (methods & SSH_AUTH_METHOD_INTERACTIVE)
            offered.append(tr("клавиатурный диалог"));
        const QString prefix =
            offered.isEmpty()
                ? tr("Ошибка аутентификации")
                : tr("Ошибка аутентификации. Сервер принимает: %1")
                      .arg(offered.join(QStringLiteral(", ")));
        emit errorOccurred(sessionError(prefix));
    }
    cleanup();
    return false;
}

bool SftpWorker::authenticateWithKey(unsigned int methods)
{
    if (!(methods & SSH_AUTH_METHOD_PUBLICKEY) ||
        m_profile.privateKeyPath.isEmpty())
        return false;

    const QByteArray identity = QFile::encodeName(m_profile.privateKeyPath);
    const QByteArray passphrase = m_profile.keyPassphrase.toUtf8();
    ssh_key key = nullptr;
    if (ssh_pki_import_privkey_file(
            identity.constData(),
            passphrase.isEmpty() ? nullptr : passphrase.constData(), nullptr,
            nullptr, &key) != SSH_OK) {
        if (!m_reconnecting) {
            emit errorOccurred(
                tr("Не удалось прочитать приватный ключ «%1». Проверьте путь "
                   "и кодовую фразу.")
                    .arg(m_profile.privateKeyPath));
        }
        return false;
    }
    const int result = ssh_userauth_publickey(m_session, nullptr, key);
    ssh_key_free(key);
    return result == SSH_AUTH_SUCCESS;
}

bool SftpWorker::authenticateInteractive()
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

bool SftpWorker::startSftp()
{
    QElapsedTimer timer;
    timer.start();
    m_sftp = sftp_new(m_session);
    if (!m_sftp || sftp_init(m_sftp) != SSH_OK) {
        if (!m_reconnecting)
            emit errorOccurred(sessionError(tr("Не удалось запустить SFTP")));
        cleanup();
        return false;
    }
    refreshTransferLimits();
    emit performanceMetric(tr("Инициализация SFTP"), timer.elapsed());
    return true;
}

void SftpWorker::refreshTransferLimits()
{
    m_readChunk = 64 * 1024;
    m_writeChunk = 64 * 1024;
    if (sftp_limits_t limits = sftp_limits(m_sftp)) {
        if (limits->max_read_length > 0) {
            m_readChunk = static_cast<size_t>(
                std::min<uint64_t>(limits->max_read_length, 256 * 1024));
        }
        if (limits->max_write_length > 0) {
            m_writeChunk = static_cast<size_t>(
                std::min<uint64_t>(limits->max_write_length, 256 * 1024));
        }
        sftp_limits_free(limits);
    }
    m_readChunk = std::max<size_t>(m_readChunk, 16 * 1024);
    m_writeChunk = std::max<size_t>(m_writeChunk, 16 * 1024);
}

bool SftpWorker::reconnectSession()
{
    if (stopping() || m_reconnecting || m_profile.host.isEmpty())
        return false;
    const ConnectionProfile savedProfile = m_profile;
    m_reconnecting = true;
    emit reconnecting();
    bool success = false;
    for (int attempt = 0; attempt < 3 && !stopping(); ++attempt) {
        if (attempt > 0)
            QThread::msleep(static_cast<unsigned long>(500 * attempt));
        connectToHost(savedProfile);
        success = m_sftp && m_session && ssh_is_connected(m_session);
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
    const QString details = message ? QString::fromUtf8(message) : QString();
    return details.contains(QStringLiteral("Socket error"), Qt::CaseInsensitive) ||
           details.contains(QStringLiteral("connection reset"), Qt::CaseInsensitive) ||
           details.contains(QStringLiteral("connection closed"), Qt::CaseInsensitive) ||
           details.contains(QStringLiteral("Broken pipe"), Qt::CaseInsensitive);
}

void SftpWorker::disconnectFromHost()
{
    requestStop();
    cleanup();
    emit disconnected();
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
    const QString details = message ? QString::fromUtf8(message) : QString();
    if (!ssh_is_connected(m_session) ||
        details.contains(QStringLiteral("Socket error: No error"),
                         Qt::CaseInsensitive)) {
        return tr("%1: сервер закрыл SFTP-соединение. Подключитесь повторно.")
            .arg(prefix);
    }
    if (m_sftp) {
        const int sftpError = sftp_get_error(m_sftp);
        if (sftpError == SSH_FX_PERMISSION_DENIED)
            return tr("%1: недостаточно прав на сервере.").arg(prefix);
        if (sftpError == SSH_FX_NO_SUCH_FILE)
            return tr("%1: объект не найден на сервере.").arg(prefix);
    }
    return details.isEmpty() ? prefix
                             : QStringLiteral("%1: %2").arg(prefix, details);
}

QString SftpWorker::normalizedRemotePath(const QString &path) const
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (!result.startsWith(QLatin1Char('/')))
        result.prepend(QLatin1Char('/'));
    return result;
}

bool SftpWorker::ensureListingChannel()
{
    if (m_listingChannel && ssh_channel_is_open(m_listingChannel))
        return true;
    closeListingChannel();
    if (!m_session || !ssh_is_connected(m_session))
        return false;

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
        const int written =
            ssh_channel_write(m_listingChannel, packet.constData() + offset,
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
            const int count =
                ssh_channel_read(m_listingChannel, target + done,
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
    const auto *bytes = reinterpret_cast<const unsigned char *>(lengthBytes);
    const quint32 length = (static_cast<quint32>(bytes[0]) << 24) |
                           (static_cast<quint32>(bytes[1]) << 16) |
                           (static_cast<quint32>(bytes[2]) << 8) |
                           static_cast<quint32>(bytes[3]);
    if (length == 0 || length > 32 * 1024 * 1024)
        return false;
    payload.resize(static_cast<qsizetype>(length));
    return readExact(payload.data(), payload.size());
}

bool SftpWorker::listDirectoryPipelined(const QString &path, quint64 generation,
                                        RemoteEntries *result, int pipelineDepth,
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
    if (!takeU32(response, offset, responseId) || responseId != openId ||
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
        if (stopping()) {
            closeListingChannel();
            return false;
        }
        if (generation != m_directoryGeneration.load(std::memory_order_relaxed)) {
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
        const quint8 type = static_cast<unsigned char>(response.at(offset++));
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
                    !skipAttributes(response, offset, size, permissions,
                                    modified)) {
                    closeListingChannel();
                    return false;
                }
                const QString name = QString::fromUtf8(nameBytes);
                if (name == QStringLiteral(".") || name == QStringLiteral(".."))
                    continue;
                RemoteEntry entry;
                entry.name = name;
                entry.path = joinRemote(cleanPath, name);
                entry.size = size;
                entry.permissions = permissions;
                entry.directory = (permissions & SSH_S_IFMT) == SSH_S_IFDIR ||
                                  (!longName.isEmpty() && longName.at(0) == 'd');
                entry.symlink = (permissions & SSH_S_IFMT) == SSH_S_IFLNK ||
                                (!longName.isEmpty() && longName.at(0) == 'l');
                if (modified > 0)
                    entry.modified = QDateTime::fromSecsSinceEpoch(modified);
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

            if (status != SSH_FX_EOF) {
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
    if (!sendListingPacket(closeRequest) || !readListingPacket(response))
        closeListingChannel();

    if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
        return true;
    sortEntries(entries);
    if (result)
        *result = entries;
    if (!background) {
        emit performanceMetric(tr("Каталог %1 (%2 объектов)")
                                   .arg(cleanPath)
                                   .arg(entries.size()),
                               timer.elapsed());
    }
    emit directoryListed(cleanPath, entries);
    return true;
}

void SftpWorker::listDirectory(const QString &path, quint64 generation)
{
    listDirectoryWithRetry(path, generation, 2);
}

void SftpWorker::listDirectoryWithRetry(const QString &path, quint64 generation,
                                        int retries)
{
    if (stopping())
        return;
    if (!m_sftp) {
        if (retries > 0 && reconnectSession()) {
            listDirectoryWithRetry(path, beginDirectoryRequest(), retries - 1);
            return;
        }
        emit errorOccurred(tr("Нет активного SFTP-подключения."));
        return;
    }
    if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
        return;

    m_busy.store(true, std::memory_order_relaxed);
    RemoteEntries entries;
    if (listDirectoryPipelined(path, generation, &entries, 16, false)) {
        int prefetched = 0;
        for (const auto &entry : entries) {
            if (!entry.directory || entry.symlink)
                continue;
            if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
                break;
            RemoteEntries ignored;
            listDirectoryPipelined(entry.path, generation, &ignored, 8, true);
            if (++prefetched >= 8)
                break;
        }
        m_busy.store(false, std::memory_order_relaxed);
        return;
    }
    if (retries > 0 && connectionLost() && reconnectSession()) {
        m_busy.store(false, std::memory_order_relaxed);
        listDirectoryWithRetry(path, beginDirectoryRequest(), retries - 1);
        return;
    }
    listDirectoryFallback(path, generation, retries);
    m_busy.store(false, std::memory_order_relaxed);
}

bool SftpWorker::listDirectoryFallback(const QString &path, quint64 generation,
                                       int retries)
{
    QElapsedTimer timer;
    timer.start();
    const QString cleanPath = normalizedRemotePath(path);
    const QByteArray encoded = remoteBytes(cleanPath);
    sftp_dir directory = sftp_opendir(m_sftp, encoded.constData());
    if (!directory) {
        if (retries > 0 && connectionLost() && reconnectSession()) {
            listDirectoryWithRetry(path, beginDirectoryRequest(), retries - 1);
            return true;
        }
        if (generation == m_directoryGeneration.load(std::memory_order_relaxed)) {
            emit errorOccurred(sessionError(
                tr("Не удалось открыть удалённую папку «%1»").arg(cleanPath)));
        }
        return false;
    }

    RemoteEntries entries;
    RemoteEntries batch;
    batch.reserve(64);
    QElapsedTimer flushTimer;
    flushTimer.start();
    while (generation == m_directoryGeneration.load(std::memory_order_relaxed)) {
        sftp_attributes attributes = sftp_readdir(m_sftp, directory);
        if (!attributes)
            break;
        const QString name = QString::fromUtf8(attributes->name);
        if (name != QStringLiteral(".") && name != QStringLiteral("..")) {
            RemoteEntry entry;
            entry.name = name;
            entry.path = joinRemote(cleanPath, name);
            entry.size = static_cast<qint64>(attributes->size);
            entry.permissions = attributes->permissions;
            entry.directory = isDirectory(attributes);
            entry.symlink = isSymlink(attributes);
            const qint64 modified = attributeMTime(attributes);
            if (modified > 0)
                entry.modified = QDateTime::fromSecsSinceEpoch(modified);
            entries.append(entry);
            batch.append(entry);
            if (batch.size() >= 64 || flushTimer.elapsed() >= 40) {
                emit directoryBatchReceived(cleanPath, batch);
                batch.clear();
                flushTimer.restart();
            }
        }
        sftp_attributes_free(attributes);
    }
    const bool directoryFailed = !sftp_dir_eof(directory);
    sftp_closedir(directory);

    if (directoryFailed && retries > 0 && connectionLost() && reconnectSession()) {
        listDirectoryWithRetry(path, beginDirectoryRequest(), retries - 1);
        return true;
    }
    if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
        return true;
    if (!batch.isEmpty())
        emit directoryBatchReceived(cleanPath, batch);

    sortEntries(entries);
    emit performanceMetric(
        tr("Каталог %1 (%2 объектов)").arg(cleanPath).arg(entries.size()),
        timer.elapsed());
    emit directoryListed(cleanPath, entries);
    return true;
}

bool SftpWorker::readRemoteDirectory(const QString &path, RemoteEntries &entries)
{
    const QByteArray encoded = remoteBytes(path);
    sftp_dir directory = sftp_opendir(m_sftp, encoded.constData());
    if (!directory)
        return false;
    while (!stopping()) {
        sftp_attributes attributes = sftp_readdir(m_sftp, directory);
        if (!attributes)
            break;
        const QString name = QString::fromUtf8(attributes->name);
        if (name != QStringLiteral(".") && name != QStringLiteral("..")) {
            RemoteEntry entry;
            entry.name = name;
            entry.path = joinRemote(path, name);
            entry.size = static_cast<qint64>(attributes->size);
            entry.permissions = attributes->permissions;
            entry.directory = isDirectory(attributes) && !isSymlink(attributes);
            entry.symlink = isSymlink(attributes);
            const qint64 modified = attributeMTime(attributes);
            if (modified > 0)
                entry.modified = QDateTime::fromSecsSinceEpoch(modified);
            entries.append(entry);
        }
        sftp_attributes_free(attributes);
    }
    const bool complete = sftp_dir_eof(directory);
    sftp_closedir(directory);
    return complete && !stopping();
}

void SftpWorker::enqueueTransfers(const QVector<TransferJob> &jobs)
{
    if (jobs.isEmpty())
        return;
    m_cancelAll.store(false, std::memory_order_relaxed);
    m_stopRequested.store(false, std::memory_order_relaxed);
    for (const TransferJob &job : jobs) {
        m_queue.append(job);
        TransferStatus status;
        status.id = job.id;
        status.name = job.upload ? QFileInfo(job.localPath).fileName()
                                 : QFileInfo(job.remotePath).fileName();
        status.localPath = job.localPath;
        status.remotePath = job.remotePath;
        status.upload = job.upload;
        status.directory = job.directory;
        status.state = TransferState::Queued;
        emit transferQueued(status);
    }
    if (!m_draining)
        drainQueue();
}

void SftpWorker::drainQueue()
{
    m_draining = true;
    m_busy.store(true, std::memory_order_relaxed);
    int completed = 0;
    int failed = 0;
    int skipped = 0;

    while (!m_queue.isEmpty()) {
        const TransferJob job = m_queue.takeFirst();
        TransferState outcome = TransferState::Canceled;
        if (jobCanceled(job.id) || stopping()) {
            TransferStatus status;
            status.id = job.id;
            status.name = job.upload ? QFileInfo(job.localPath).fileName()
                                     : QFileInfo(job.remotePath).fileName();
            status.localPath = job.localPath;
            status.remotePath = job.remotePath;
            status.upload = job.upload;
            status.directory = job.directory;
            status.state = TransferState::Canceled;
            emit transferUpdated(status);
        } else {
            outcome = runJob(job);
        }
        switch (outcome) {
        case TransferState::Completed:
            ++completed;
            break;
        case TransferState::Failed:
            ++failed;
            break;
        default:
            ++skipped;
            break;
        }
    }

    {
        QMutexLocker locker(&m_cancelMutex);
        m_canceledJobs.clear();
    }
    m_cancelAll.store(false, std::memory_order_relaxed);
    m_busy.store(false, std::memory_order_relaxed);
    m_draining = false;
    emit queueDrained(completed, failed, skipped);
}

TransferState SftpWorker::runJob(const TransferJob &job)
{
    JobContext context;
    context.status.id = job.id;
    context.status.name = job.upload ? QFileInfo(job.localPath).fileName()
                                     : QFileInfo(job.remotePath).fileName();
    context.status.localPath = job.localPath;
    context.status.remotePath = job.remotePath;
    context.status.upload = job.upload;
    context.status.directory = job.directory;
    context.status.state = TransferState::Running;
    context.status.total = -1;
    context.clock.start();
    context.emitClock.start();
    publish(context, true);

    if (!m_sftp && !reconnectSession()) {
        context.status.state = TransferState::Failed;
        context.status.detail = tr("Нет активного SFTP-подключения.");
        publish(context, true);
        return context.status.state;
    }

    QVector<FileTask> tasks;
    QString failure;
    if (!expandJob(job, tasks, failure)) {
        context.status.state =
            jobCanceled(job.id) ? TransferState::Canceled : TransferState::Failed;
        context.status.detail = failure;
        publish(context, true);
        return context.status.state;
    }

    qint64 total = 0;
    for (const FileTask &task : tasks)
        total += std::max<qint64>(task.size, 0);
    context.status.total = total;
    publish(context, true);

    int skippedFiles = 0;
    for (const FileTask &task : tasks) {
        if (jobCanceled(job.id)) {
            context.status.state = TransferState::Canceled;
            publish(context, true);
            return context.status.state;
        }
        context.status.detail = job.upload ? task.localPath : task.remotePath;
        const FileOutcome outcome = job.upload
                                        ? uploadFileTask(task, context, 1)
                                        : downloadFileTask(task, context, 1);
        switch (outcome) {
        case FileOutcome::Completed:
            context.baseDone += std::max<qint64>(task.size, 0);
            break;
        case FileOutcome::Skipped:
            context.baseDone += std::max<qint64>(task.size, 0);
            ++skippedFiles;
            break;
        case FileOutcome::Canceled:
            context.status.state = TransferState::Canceled;
            publish(context, true);
            return context.status.state;
        case FileOutcome::Failed:
            context.status.state = TransferState::Failed;
            publish(context, true);
            return context.status.state;
        }
        publish(context);
    }

    context.status.done = context.baseDone;
    context.status.state = (skippedFiles > 0 && skippedFiles == tasks.size())
                               ? TransferState::Skipped
                               : TransferState::Completed;
    context.status.detail = skippedFiles > 0
                                ? tr("Пропущено файлов: %1").arg(skippedFiles)
                                : QString();
    publish(context, true);
    return context.status.state;
}

void SftpWorker::publish(JobContext &context, bool force)
{
    if (!force && context.emitClock.elapsed() < 100)
        return;
    context.emitClock.restart();
    const qint64 elapsed = std::max<qint64>(context.clock.elapsed(), 1);
    context.status.bytesPerSecond = context.status.done * 1000 / elapsed;
    emit transferUpdated(context.status);
}

bool SftpWorker::expandJob(const TransferJob &job, QVector<FileTask> &tasks,
                           QString &failure)
{
    return job.upload ? expandUpload(job, tasks, failure)
                      : expandDownload(job, tasks, failure);
}

bool SftpWorker::expandDownload(const TransferJob &job, QVector<FileTask> &tasks,
                                QString &failure)
{
    if (!job.directory) {
        FileTask task;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = job.size;
        task.permissions = job.permissions;
        task.modified = job.modified;
        if (task.size < 0) {
            const QByteArray encoded = remoteBytes(job.remotePath);
            if (sftp_attributes attributes =
                    sftp_stat(m_sftp, encoded.constData())) {
                task.size = static_cast<qint64>(attributes->size);
                task.permissions = attributes->permissions;
                task.modified = attributeMTime(attributes);
                sftp_attributes_free(attributes);
            }
        }
        tasks.append(task);
        return true;
    }

    QVector<QPair<QString, QString>> pending;
    pending.append({job.remotePath, job.localPath});
    while (!pending.isEmpty()) {
        if (jobCanceled(job.id)) {
            failure = tr("Отменено пользователем.");
            return false;
        }
        const auto [remoteDirectory, localDirectory] = pending.takeLast();
        if (!QDir().mkpath(localDirectory)) {
            failure = tr("Не удалось создать локальную папку «%1».")
                          .arg(localDirectory);
            return false;
        }
        RemoteEntries entries;
        if (!readRemoteDirectory(remoteDirectory, entries)) {
            failure = sessionError(tr("Не удалось прочитать удалённую папку «%1»")
                                       .arg(remoteDirectory));
            return false;
        }
        for (const RemoteEntry &entry : entries) {
            const QString localPath = QDir(localDirectory).filePath(entry.name);
            if (entry.directory) {
                pending.append({entry.path, localPath});
                continue;
            }
            FileTask task;
            task.localPath = localPath;
            task.remotePath = entry.path;
            task.size = entry.size;
            task.permissions = entry.permissions;
            task.modified = entry.modified.isValid()
                                ? entry.modified.toSecsSinceEpoch()
                                : 0;
            tasks.append(task);
        }
    }
    return true;
}

bool SftpWorker::expandUpload(const TransferJob &job, QVector<FileTask> &tasks,
                              QString &failure)
{
    const QFileInfo source(job.localPath);
    if (!source.exists()) {
        failure = tr("Локальный объект «%1» больше не существует.")
                      .arg(job.localPath);
        return false;
    }
    if (!job.directory) {
        FileTask task;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = source.size();
        task.permissions = source.isExecutable() ? 0755 : 0644;
        task.modified = source.lastModified().toSecsSinceEpoch();
        tasks.append(task);
        return true;
    }

    QVector<QPair<QString, QString>> pending;
    pending.append({job.localPath, job.remotePath});
    while (!pending.isEmpty()) {
        if (jobCanceled(job.id)) {
            failure = tr("Отменено пользователем.");
            return false;
        }
        const auto [localDirectory, remoteDirectory] = pending.takeLast();
        if (!ensureRemoteDirectory(remoteDirectory)) {
            failure = sessionError(tr("Не удалось создать удалённую папку «%1»")
                                       .arg(remoteDirectory));
            return false;
        }
        const QFileInfoList children =
            QDir(localDirectory)
                .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                               QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &child : children) {
            const QString remotePath =
                joinRemote(remoteDirectory, child.fileName());
            if (child.isDir() && !child.isSymLink()) {
                pending.append({child.absoluteFilePath(), remotePath});
                continue;
            }
            FileTask task;
            task.localPath = child.absoluteFilePath();
            task.remotePath = remotePath;
            task.size = child.size();
            task.permissions = child.isExecutable() ? 0755 : 0644;
            task.modified = child.lastModified().toSecsSinceEpoch();
            tasks.append(task);
        }
    }
    return true;
}

SftpWorker::FileOutcome SftpWorker::downloadFileTask(const FileTask &task,
                                                     JobContext &context,
                                                     int retries)
{
    if (jobCanceled(context.status.id))
        return FileOutcome::Canceled;

    const auto policy =
        static_cast<OverwritePolicy>(m_overwritePolicy.load(std::memory_order_relaxed));
    const QFileInfo existing(task.localPath);
    const QFileInfo partial(task.localPath + PartialSuffix);
    TransferPlan::Destination state;
    state.exists = existing.exists();
    state.size = state.exists ? existing.size() : 0;
    state.partialSize = partial.exists() ? partial.size() : -1;

    const TransferPlan plan = TransferPlan::make(
        policy, task.localPath, task.size, state, [&task] {
            for (int index = 2; index < 1000; ++index) {
                const QString candidate = numberedVariant(task.localPath, index);
                if (!QFileInfo::exists(candidate))
                    return candidate;
            }
            return task.localPath;
        });
    if (plan.skip)
        return FileOutcome::Skipped;

    const QString destination = plan.destination;
    const QString partialPath = destination + PartialSuffix;
    qint64 startOffset = plan.startOffset;
    if (plan.discardPartial || startOffset == 0)
        QFile::remove(partialPath);

    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        emit errorOccurred(tr("Не удалось создать локальную папку для «%1».")
                               .arg(destination));
        return FileOutcome::Failed;
    }

    const QByteArray remote = remoteBytes(task.remotePath);
    sftp_file handle = sftp_open(m_sftp, remote.constData(), O_RDONLY, 0);
    if (!handle) {
        if (retries > 0 && connectionLost() && reconnectSession())
            return downloadFileTask(task, context, retries - 1);
        emit errorOccurred(sessionError(
            tr("Не удалось открыть «%1» для скачивания").arg(task.remotePath)));
        context.status.detail =
            tr("Не удалось открыть «%1».").arg(task.remotePath);
        return FileOutcome::Failed;
    }
    if (startOffset > 0 &&
        sftp_seek64(handle, static_cast<uint64_t>(startOffset)) != SSH_OK) {
        startOffset = 0;
        QFile::remove(partialPath);
    }

    QFile output(partialPath);
    const QIODevice::OpenMode mode =
        startOffset > 0 ? (QIODevice::WriteOnly | QIODevice::Append)
                        : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!output.open(mode)) {
        sftp_close(handle);
        emit errorOccurred(tr("Не удалось создать «%1»: %2")
                               .arg(partialPath, output.errorString()));
        return FileOutcome::Failed;
    }

    struct PendingRead {
        sftp_aio aio = nullptr;
        QByteArray buffer;
        size_t requested = 0;
    };
    constexpr size_t TransferWindow = 64;
    std::deque<PendingRead> pending;
    const qint64 total = task.size;
    qint64 scheduled = startOffset;
    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;
    bool finished = false;
    bool resync = false;

    auto releasePending = [&pending] {
        for (auto &request : pending) {
            if (request.aio)
                sftp_aio_free(request.aio);
        }
        pending.clear();
    };

    while (!finished && !failed) {
        if (jobCanceled(context.status.id)) {
            canceled = true;
            break;
        }
        while (pending.size() < TransferWindow &&
               (total < 0 || scheduled < total)) {
            const size_t wanted =
                total < 0 ? m_readChunk
                          : static_cast<size_t>(std::min<qint64>(
                                static_cast<qint64>(m_readChunk),
                                total - scheduled));
            if (wanted == 0)
                break;
            PendingRead request;
            const ssize_t requested =
                sftp_aio_begin_read(handle, wanted, &request.aio);
            if (requested <= 0) {
                failed = true;
                break;
            }
            request.requested = static_cast<size_t>(requested);
            request.buffer.resize(static_cast<qsizetype>(requested));
            scheduled += requested;
            pending.push_back(std::move(request));
        }
        if (failed || pending.empty())
            break;

        PendingRead request = std::move(pending.front());
        pending.pop_front();
        const ssize_t count =
            sftp_aio_wait_read(&request.aio, request.buffer.data(),
                               static_cast<size_t>(request.buffer.size()));
        if (count < 0) {
            failed = true;
            break;
        }
        if (count == 0) {
            finished = true;
            releasePending();
            break;
        }
        if (output.write(request.buffer.constData(), count) != count) {
            emit errorOccurred(tr("Не удалось записать «%1»: %2")
                                   .arg(partialPath, output.errorString()));
            failed = true;
            break;
        }
        done += count;

        if (static_cast<size_t>(count) < request.requested) {
            releasePending();
            if (sftp_seek64(handle, static_cast<uint64_t>(done)) != SSH_OK) {
                failed = true;
                break;
            }
            scheduled = done;
            resync = true;
        }
        if (total >= 0 && done >= total)
            finished = true;

        context.status.done = context.baseDone + (done - startOffset);
        publish(context);
    }
    releasePending();
    Q_UNUSED(resync);

    if (!failed && !canceled && task.modified > 0) {
        output.setFileTime(QDateTime::fromSecsSinceEpoch(task.modified),
                           QFileDevice::FileModificationTime);
    }
    output.close();
    sftp_close(handle);

    if (canceled) {
        return FileOutcome::Canceled;
    }
    if (failed || (total >= 0 && done != total)) {
        if (retries > 0 && connectionLost() && reconnectSession())
            return downloadFileTask(task, context, retries - 1);
        emit errorOccurred(
            sessionError(tr("Ошибка при скачивании «%1»").arg(task.remotePath)));
        context.status.detail =
            tr("Ошибка при скачивании «%1».").arg(task.remotePath);
        return FileOutcome::Failed;
    }

    QFile::remove(destination);
    if (!QFile::rename(partialPath, destination)) {
        emit errorOccurred(tr("Не удалось сохранить «%1».").arg(destination));
        return FileOutcome::Failed;
    }
    applyLocalMetadata(destination, task);
    context.status.done = context.baseDone + (done - startOffset);
    return FileOutcome::Completed;
}

SftpWorker::FileOutcome SftpWorker::uploadFileTask(const FileTask &task,
                                                   JobContext &context,
                                                   int retries)
{
    if (jobCanceled(context.status.id))
        return FileOutcome::Canceled;

    const auto policy =
        static_cast<OverwritePolicy>(m_overwritePolicy.load(std::memory_order_relaxed));
    TransferPlan::Destination state;
    QByteArray remote = remoteBytes(task.remotePath);
    if (sftp_attributes attributes = sftp_stat(m_sftp, remote.constData())) {
        state.exists = true;
        state.size = static_cast<qint64>(attributes->size);
        sftp_attributes_free(attributes);
    }

    QString renamed = task.remotePath;
    if (state.exists && policy == OverwritePolicy::Rename) {
        for (int index = 2; index < 1000; ++index) {
            renamed = numberedVariant(task.remotePath, index);
            const QByteArray candidate = remoteBytes(renamed);
            sftp_attributes probe = sftp_stat(m_sftp, candidate.constData());
            if (!probe)
                break;
            sftp_attributes_free(probe);
        }
    }

    const TransferPlan plan = TransferPlan::makeUpload(
        policy, task.remotePath, task.size, state, renamed);
    if (plan.skip)
        return FileOutcome::Skipped;
    const QString destination = plan.destination;
    qint64 startOffset = plan.startOffset;
    remote = remoteBytes(destination);

    QFile input(task.localPath);
    if (!input.open(QIODevice::ReadOnly)) {
        emit errorOccurred(
            tr("Не удалось открыть «%1»: %2").arg(task.localPath, input.errorString()));
        context.status.detail = tr("Не удалось открыть «%1».").arg(task.localPath);
        return FileOutcome::Failed;
    }
    if (startOffset > 0 && !input.seek(startOffset))
        startOffset = 0;

    const int flags = startOffset > 0 ? (O_WRONLY | O_CREAT)
                                      : (O_WRONLY | O_CREAT | O_TRUNC);
    sftp_file handle = sftp_open(m_sftp, remote.constData(), flags, 0644);
    if (!handle) {
        if (retries > 0 && connectionLost() && reconnectSession())
            return uploadFileTask(task, context, retries - 1);
        emit errorOccurred(
            sessionError(tr("Не удалось создать «%1» на сервере").arg(destination)));
        context.status.detail = tr("Не удалось создать «%1».").arg(destination);
        return FileOutcome::Failed;
    }
    if (startOffset > 0 &&
        sftp_seek64(handle, static_cast<uint64_t>(startOffset)) != SSH_OK) {
        sftp_close(handle);
        input.seek(0);
        startOffset = 0;
        handle = sftp_open(m_sftp, remote.constData(),
                           O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (!handle) {
            emit errorOccurred(sessionError(
                tr("Не удалось создать «%1» на сервере").arg(destination)));
            return FileOutcome::Failed;
        }
    }

    struct PendingWrite {
        sftp_aio aio = nullptr;
        size_t requested = 0;
    };
    constexpr size_t TransferWindow = 64;
    std::deque<PendingWrite> pending;
    const qint64 total = task.size;
    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;

    auto releasePending = [&pending] {
        for (auto &request : pending) {
            if (request.aio)
                sftp_aio_free(request.aio);
        }
        pending.clear();
    };

    while (!failed && (!input.atEnd() || !pending.empty())) {
        if (jobCanceled(context.status.id)) {
            canceled = true;
            break;
        }
        while (pending.size() < TransferWindow && !input.atEnd()) {
            QByteArray chunk = input.read(static_cast<qint64>(m_writeChunk));
            if (chunk.isEmpty()) {
                if (!input.atEnd())
                    failed = true;
                break;
            }
            PendingWrite request;
            const ssize_t requested = sftp_aio_begin_write(
                handle, chunk.constData(), static_cast<size_t>(chunk.size()),
                &request.aio);
            if (requested <= 0) {
                failed = true;
                break;
            }
            request.requested = static_cast<size_t>(requested);

            if (requested < chunk.size())
                input.seek(input.pos() - (chunk.size() - requested));
            pending.push_back(request);
        }
        if (failed || pending.empty())
            break;

        PendingWrite request = pending.front();
        pending.pop_front();
        const ssize_t written = sftp_aio_wait_write(&request.aio);
        if (written <= 0 || static_cast<size_t>(written) != request.requested) {
            failed = true;
            break;
        }
        done += written;
        context.status.done = context.baseDone + (done - startOffset);
        publish(context);
    }
    releasePending();
    input.close();
    sftp_close(handle);

    if (canceled)
        return FileOutcome::Canceled;
    if (failed || (total >= 0 && done != total)) {
        if (retries > 0 && connectionLost() && reconnectSession())
            return uploadFileTask(task, context, retries - 1);
        emit errorOccurred(
            sessionError(tr("Ошибка при загрузке «%1»").arg(task.localPath)));
        context.status.detail =
            tr("Ошибка при загрузке «%1».").arg(task.localPath);
        return FileOutcome::Failed;
    }

    applyRemoteMetadata(destination, task);
    context.status.done = context.baseDone + (done - startOffset);
    return FileOutcome::Completed;
}

void SftpWorker::applyLocalMetadata(const QString &localPath,
                                    const FileTask &task)
{
    if (task.permissions & 0100) {
        QFile::setPermissions(localPath,
                              QFile::permissions(localPath) |
                                  QFileDevice::ExeOwner | QFileDevice::ExeUser);
    }
}

void SftpWorker::applyRemoteMetadata(const QString &remotePath,
                                     const FileTask &task)
{
    const QByteArray encoded = remoteBytes(remotePath);
    if (task.permissions != 0)
        sftp_chmod(m_sftp, encoded.constData(),
                   static_cast<mode_t>(task.permissions & 07777));
    if (task.modified > 0) {
        struct sftp_attributes_struct attributes;
        std::memset(&attributes, 0, sizeof(attributes));
        attributes.flags = AttrAcModTime;
        attributes.atime = static_cast<uint32_t>(task.modified);
        attributes.mtime = static_cast<uint32_t>(task.modified);
        sftp_setstat(m_sftp, encoded.constData(), &attributes);
    }
}

bool SftpWorker::ensureRemoteDirectory(const QString &path)
{
    const QByteArray encoded = remoteBytes(path);
    if (sftp_mkdir(m_sftp, encoded.constData(), 0755) == SSH_OK)
        return true;
    sftp_attributes attributes = sftp_stat(m_sftp, encoded.constData());
    const bool exists = isDirectory(attributes);
    if (attributes)
        sftp_attributes_free(attributes);
    return exists;
}

void SftpWorker::createDirectory(const QString &path)
{
    const QByteArray encoded = remoteBytes(path);
    if (!m_sftp || sftp_mkdir(m_sftp, encoded.constData(), 0755) != SSH_OK) {
        emit errorOccurred(
            sessionError(tr("Не удалось создать папку «%1»").arg(path)));
        return;
    }
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

void SftpWorker::createFile(const QString &path)
{
    if (!m_sftp)
        return;
    const QByteArray encoded = remoteBytes(path);
    sftp_file handle =
        sftp_open(m_sftp, encoded.constData(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (!handle) {
        emit errorOccurred(
            sessionError(tr("Не удалось создать файл «%1»").arg(path)));
        return;
    }
    sftp_close(handle);
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

void SftpWorker::renameEntry(const QString &oldPath, const QString &newPath)
{
    if (!m_sftp)
        return;
    const QByteArray oldEncoded = remoteBytes(oldPath);
    const QByteArray newEncoded = remoteBytes(newPath);
    if (sftp_rename(m_sftp, oldEncoded.constData(), newEncoded.constData()) !=
        SSH_OK) {
        emit errorOccurred(sessionError(
            tr("Не удалось переименовать «%1» в «%2»").arg(oldPath, newPath)));
        return;
    }
    listDirectory(remoteParentPath(newPath), beginDirectoryRequest());
}

void SftpWorker::removeEntry(const QString &path, bool directory)
{
    if (!m_sftp)
        return;
    m_busy.store(true, std::memory_order_relaxed);
    quint64 removedCount = 0;
    QString failedPath;
    const bool removed =
        removeRecursively(path, directory, removedCount, failedPath);
    m_busy.store(false, std::memory_order_relaxed);
    if (!removed) {
        if (stopping())
            return;
        emit errorOccurred(sessionError(
            tr("Не удалось удалить «%1»")
                .arg(failedPath.isEmpty() ? path : failedPath)));
        return;
    }
    emit deletionProgress(static_cast<qint64>(removedCount), path);
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

bool SftpWorker::removeRecursively(const QString &path, bool directory,
                                   quint64 &removedCount, QString &failedPath)
{
    if (stopping())
        return false;
    const QByteArray rootEncoded = remoteBytes(path);
    if (sftp_attributes attributes = sftp_lstat(m_sftp, rootEncoded.constData())) {
        if (isSymlink(attributes))
            directory = false;
        sftp_attributes_free(attributes);
    }

    if (!directory) {
        if (sftp_unlink(m_sftp, rootEncoded.constData()) != SSH_OK) {
            failedPath = path;
            return false;
        }
        ++removedCount;
        return true;
    }

    QStringList directories;
    QStringList pending{path};
    while (!pending.isEmpty()) {
        if (stopping())
            return false;
        const QString current = pending.takeLast();
        RemoteEntries entries;
        if (!readRemoteDirectory(current, entries)) {
            const QByteArray encoded = remoteBytes(current);
            if (sftp_unlink(m_sftp, encoded.constData()) == SSH_OK) {
                ++removedCount;
                continue;
            }
            failedPath = current;
            return false;
        }
        directories.append(current);
        for (const RemoteEntry &entry : entries) {
            if (entry.directory) {
                pending.append(entry.path);
                continue;
            }
            const QByteArray encoded = remoteBytes(entry.path);
            if (sftp_unlink(m_sftp, encoded.constData()) != SSH_OK) {
                failedPath = entry.path;
                return false;
            }
            if (++removedCount % 50 == 0)
                emit deletionProgress(static_cast<qint64>(removedCount), entry.path);
        }
    }

    for (auto it = directories.crbegin(); it != directories.crend(); ++it) {
        if (stopping())
            return false;
        const QByteArray encoded = remoteBytes(*it);
        if (sftp_rmdir(m_sftp, encoded.constData()) != SSH_OK) {
            failedPath = *it;
            return false;
        }
        ++removedCount;
    }
    return true;
}
