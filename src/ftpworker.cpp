#include "ftpworker.h"

#include "transferplan.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStringList>
#include <QThread>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>

namespace {
constexpr int ConnectTimeout = 15000;
constexpr int ControlTimeout = 20000;
constexpr int DataTimeout = 30000;
constexpr qint64 ChunkSize = 64 * 1024;

const QLatin1String PartialSuffix(".nexapart");

QString fingerprintText(const QByteArray &digest)
{
    QStringList parts;
    parts.reserve(digest.size());
    for (const char byte : digest) {
        parts.append(QStringLiteral("%1").arg(static_cast<quint8>(byte), 2, 16,
                                              QLatin1Char('0')));
    }
    return parts.join(QLatin1Char(':'));
}

int monthFromName(const QString &name)
{
    static const QStringList names{
        QStringLiteral("jan"), QStringLiteral("feb"), QStringLiteral("mar"),
        QStringLiteral("apr"), QStringLiteral("may"), QStringLiteral("jun"),
        QStringLiteral("jul"), QStringLiteral("aug"), QStringLiteral("sep"),
        QStringLiteral("oct"), QStringLiteral("nov"), QStringLiteral("dec")};
    return static_cast<int>(names.indexOf(name.toLower())) + 1;
}

quint32 permissionsFromRwx(const QString &value)
{
    if (value.size() < 9)
        return 0;
    quint32 mode = 0;
    for (int i = 0; i < 9; ++i) {
        if (value.at(i) != QLatin1Char('-'))
            mode |= 1u << static_cast<unsigned int>(8 - i);
    }
    return mode;
}

QDateTime machineTime(const QString &value)
{
    QDateTime stamp =
        QDateTime::fromString(value.left(14), QStringLiteral("yyyyMMddHHmmss"));
    if (!stamp.isValid())
        return {};
    stamp.setTimeZone(QTimeZone::UTC);
    return stamp.toLocalTime();
}

QDateTime listingTime(const QString &month, const QString &day,
                      const QString &clockOrYear)
{
    const int monthIndex = monthFromName(month);
    if (monthIndex <= 0)
        return {};
    bool ok = false;
    const int dayValue = day.toInt(&ok);
    if (!ok)
        return {};

    const QDate today = QDate::currentDate();
    int year = today.year();
    QTime time(0, 0);
    if (clockOrYear.contains(QLatin1Char(':'))) {
        const QStringList parts = clockOrYear.split(QLatin1Char(':'));
        if (parts.size() < 2)
            return {};
        time = QTime(parts.at(0).toInt(), parts.at(1).toInt());
        const QDate candidate(year, monthIndex, dayValue);
        if (candidate.isValid() && candidate > today.addDays(1))
            --year;
    } else {
        year = clockOrYear.toInt(&ok);
        if (!ok)
            return {};
    }

    const QDate date(year, monthIndex, dayValue);
    if (!date.isValid())
        return {};
    return QDateTime(date, time);
}

bool parseMachineLine(const QString &line, RemoteEntry &entry)
{
    const qsizetype split = line.indexOf(QLatin1Char(' '));
    if (split <= 0)
        return false;
    const QString name = line.mid(split + 1);
    if (name.isEmpty())
        return false;

    QString type;
    const QStringList facts =
        line.left(split).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &fact : facts) {
        const qsizetype assign = fact.indexOf(QLatin1Char('='));
        if (assign <= 0)
            continue;
        const QString key = fact.left(assign).toLower();
        const QString value = fact.mid(assign + 1);
        if (key == QStringLiteral("type"))
            type = value.toLower();
        else if (key == QStringLiteral("size"))
            entry.size = value.toLongLong();
        else if (key == QStringLiteral("modify"))
            entry.modified = machineTime(value);
        else if (key == QStringLiteral("unix.mode"))
            entry.permissions = value.toUInt(nullptr, 8);
    }
    if (type == QStringLiteral("cdir") || type == QStringLiteral("pdir"))
        return false;

    entry.name = name;
    entry.directory = type.startsWith(QStringLiteral("dir"));
    entry.symlink = type.contains(QStringLiteral("link"));
    return true;
}

bool parseUnixLine(const QString &line, RemoteEntry &entry)
{
    static const QRegularExpression pattern(QStringLiteral(
        "^([-bcdlps])([rwxstST-]{9})[.+@]?\\s+\\d+\\s+(?:\\S+\\s+)*?"
        "(\\d+)\\s+([A-Za-z]{3})\\s+(\\d{1,2})\\s+([\\d:]+)\\s+(.+)$"));
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch())
        return false;

    QString name = match.captured(7);
    const qsizetype arrow = name.indexOf(QStringLiteral(" -> "));
    if (arrow > 0)
        name = name.left(arrow);
    if (name.isEmpty())
        return false;

    entry.name = name;
    entry.size = match.captured(3).toLongLong();
    entry.permissions = permissionsFromRwx(match.captured(2));
    entry.directory = match.captured(1) == QStringLiteral("d");
    entry.symlink = match.captured(1) == QStringLiteral("l");
    entry.modified =
        listingTime(match.captured(4), match.captured(5), match.captured(6));
    return true;
}

bool parseDosLine(const QString &line, RemoteEntry &entry)
{
    static const QRegularExpression pattern(QStringLiteral(
        "^(\\d{2}-\\d{2}-\\d{2,4})\\s+(\\d{1,2}:\\d{2}(?:[AaPp][Mm])?)\\s+"
        "(<[Dd][Ii][Rr]>|\\d+)\\s+(.+)$"));
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch())
        return false;

    const QString name = match.captured(4).trimmed();
    if (name.isEmpty())
        return false;

    const QString marker = match.captured(3);
    entry.name = name;
    entry.directory = marker.startsWith(QLatin1Char('<'));
    entry.size = entry.directory ? 0 : marker.toLongLong();

    const QString stamp = match.captured(1) + QLatin1Char(' ') + match.captured(2);
    for (const auto &format : {QStringLiteral("MM-dd-yy hh:mmAP"),
                               QStringLiteral("MM-dd-yyyy hh:mmAP"),
                               QStringLiteral("MM-dd-yy hh:mmap"),
                               QStringLiteral("MM-dd-yyyy hh:mmap"),
                               QStringLiteral("MM-dd-yy hh:mm"),
                               QStringLiteral("MM-dd-yyyy hh:mm")}) {
        const QDateTime parsed = QDateTime::fromString(stamp, format);
        if (parsed.isValid()) {
            entry.modified = parsed;
            break;
        }
    }
    return true;
}
}

FtpWorker::FtpWorker(Role role, QObject *parent) : RemoteWorker(role, parent)
{
}

FtpWorker::~FtpWorker()
{
    cleanup();
}

QByteArray FtpWorker::encodePath(const QString &path) const
{
    return m_utf8 ? path.toUtf8() : path.toLatin1();
}

QString FtpWorker::decodeText(const QByteArray &data) const
{
    return m_utf8 ? QString::fromUtf8(data) : QString::fromLatin1(data);
}

bool FtpWorker::controlReady() const
{
    return m_control &&
           m_control->state() == QAbstractSocket::ConnectedState;
}

QString FtpWorker::failureText(const QString &prefix, const Reply &reply) const
{
    if (!reply.valid) {
        return tr("%1: сервер закрыл FTP-соединение. Подключитесь повторно.")
            .arg(prefix);
    }
    const QString detail = reply.text.trimmed();
    return detail.isEmpty()
               ? prefix
               : QStringLiteral("%1: %2 (%3)").arg(prefix, detail).arg(reply.code);
}

bool FtpWorker::waitForData(QTcpSocket *socket, int milliseconds) const
{
    if (!socket)
        return false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        if (socket->bytesAvailable() > 0)
            return true;
        if (stopping())
            return false;
        if (socket->waitForReadyRead(250))
            return true;
        if (socket->state() != QAbstractSocket::ConnectedState)
            break;
    }
    return socket->bytesAvailable() > 0;
}

bool FtpWorker::readControlLine(QByteArray &line)
{
    forever {
        const qsizetype end = m_controlBuffer.indexOf('\n');
        if (end >= 0) {
            line = m_controlBuffer.left(end);
            m_controlBuffer.remove(0, end + 1);
            while (line.endsWith('\r'))
                line.chop(1);
            return true;
        }
        if (!m_control)
            return false;
        if (!waitForData(m_control, ControlTimeout))
            return false;
        const QByteArray incoming = m_control->readAll();
        if (incoming.isEmpty())
            return false;
        m_controlBuffer.append(incoming);
    }
}

FtpWorker::Reply FtpWorker::readReply()
{
    Reply reply;
    QByteArray line;
    if (!readControlLine(line))
        return reply;

    const auto codeOf = [](const QByteArray &value) {
        if (value.size() < 4 || (value.at(3) != ' ' && value.at(3) != '-'))
            return 0;
        bool ok = false;
        const int parsed = value.left(3).toInt(&ok);
        return ok ? parsed : 0;
    };

    reply.code = codeOf(line);
    if (reply.code == 0) {
        if (line.size() != 3)
            return reply;
        bool ok = false;
        reply.code = line.toInt(&ok);
        if (!ok)
            return reply;
    }

    const auto textOf = [&codeOf](const QByteArray &value) {
        return codeOf(value) == 0 ? value : value.mid(4);
    };

    QStringList collected;
    collected.append(decodeText(textOf(line)));
    if (line.size() > 3 && line.at(3) == '-') {
        forever {
            if (!readControlLine(line))
                return Reply{};
            collected.append(decodeText(textOf(line)));
            if (codeOf(line) == reply.code && line.at(3) == ' ')
                break;
        }
    }

    reply.valid = true;
    reply.text = collected.join(QLatin1Char('\n'));
    m_lastReply = reply.text;
    return reply;
}

FtpWorker::Reply FtpWorker::command(const QString &verb, const QString &argument)
{
    if (!controlReady())
        return {};
    QByteArray payload = verb.toLatin1();
    if (!argument.isEmpty()) {
        payload.append(' ');
        payload.append(encodePath(argument));
    }
    payload.append("\r\n");
    if (m_control->write(payload) != payload.size() ||
        !m_control->waitForBytesWritten(ControlTimeout))
        return {};
    return readReply();
}

void FtpWorker::connectToHost(const ConnectionProfile &profile)
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
            if (url.scheme().compare(QStringLiteral("ftps"),
                                     Qt::CaseInsensitive) == 0)
                m_profile.useTls = true;
        }
    }
    if (!openControl())
        return;
    if (m_waitingForHostKey)
        return;
    finishConnect();
}

bool FtpWorker::openControl()
{
    QElapsedTimer timer;
    timer.start();

    m_control = new QSslSocket;
    m_control->setPeerVerifyMode(QSslSocket::VerifyNone);
    m_control->connectToHost(m_profile.host, m_profile.port);
    if (!m_control->waitForConnected(ConnectTimeout)) {
        if (!m_reconnecting) {
            emit errorOccurred(tr("Не удалось подключиться к %1:%2: %3")
                                   .arg(m_profile.host)
                                   .arg(m_profile.port)
                                   .arg(m_control->errorString()));
        }
        cleanup();
        return false;
    }
    m_control->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_control->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    const Reply greeting = readReply();
    if (greeting.code != 220) {
        if (!m_reconnecting) {
            emit errorOccurred(
                failureText(tr("Сервер отклонил подключение"), greeting));
        }
        cleanup();
        return false;
    }
    m_serverBanner = greeting.text.split(QLatin1Char('\n')).first().trimmed();
    emit performanceMetric(tr("FTP-соединение"), timer.elapsed());

    if (!m_profile.useTls)
        return true;
    if (!negotiateTls()) {
        cleanup();
        return false;
    }
    return acceptCertificate();
}

bool FtpWorker::negotiateTls()
{
    if (!QSslSocket::supportsSsl()) {
        if (!m_reconnecting) {
            emit errorOccurred(
                tr("Сборка Qt собрана без поддержки TLS, FTPS недоступен."));
        }
        return false;
    }

    Reply reply = command(QStringLiteral("AUTH"), QStringLiteral("TLS"));
    if (reply.code != 234)
        reply = command(QStringLiteral("AUTH"), QStringLiteral("SSL"));
    if (reply.code != 234) {
        if (!m_reconnecting)
            emit errorOccurred(failureText(tr("Сервер не поддерживает FTPS"), reply));
        return false;
    }

    QSslConfiguration configuration = m_control->sslConfiguration();
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
    configuration.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
    m_control->setSslConfiguration(configuration);
    m_control->startClientEncryption();
    if (!m_control->waitForEncrypted(ConnectTimeout)) {
        if (!m_reconnecting) {
            emit errorOccurred(tr("Не удалось установить TLS-сеанс: %1")
                                   .arg(m_control->errorString()));
        }
        return false;
    }
    m_tlsActive = true;
    return true;
}

bool FtpWorker::acceptCertificate()
{
    const QSslCertificate certificate = m_control->peerCertificate();
    if (certificate.isNull()) {
        if (!m_reconnecting)
            emit errorOccurred(tr("Сервер не предъявил TLS-сертификат."));
        cleanup();
        return false;
    }

    m_pendingFingerprint =
        fingerprintText(certificate.digest(QCryptographicHash::Sha256));
    const QString settingsKey = QStringLiteral("tlsCertificates/%1:%2")
                                    .arg(m_profile.host)
                                    .arg(m_profile.port);
    const QString known = QSettings().value(settingsKey).toString();
    if (known == m_pendingFingerprint ||
        m_runtimeTrustedFingerprint == m_pendingFingerprint ||
        m_profile.trustedFingerprint == m_pendingFingerprint) {
        m_runtimeTrustedFingerprint = m_pendingFingerprint;
        return true;
    }

    if (m_role == Role::Transfers) {
        emit errorOccurred(
            tr("Сертификат %1 не совпадает с подтверждённым. Соединение для "
               "передач не открыто.")
                .arg(m_profile.host));
        cleanup();
        return false;
    }

    m_waitingForHostKey = true;
    const QString subject = certificate.subjectDisplayName();
    emit hostKeyConfirmationRequired(
        QStringLiteral("%1:%2").arg(m_profile.host).arg(m_profile.port),
        m_pendingFingerprint,
        subject.isEmpty() ? tr("TLS-сертификат")
                          : tr("TLS-сертификат, %1").arg(subject),
        !known.isEmpty());
    return true;
}

void FtpWorker::confirmHostKey(bool accepted, bool remember)
{
    if (!m_waitingForHostKey)
        return;
    m_waitingForHostKey = false;
    if (!accepted) {
        emit errorOccurred(
            tr("Подключение отменено: сертификат сервера не принят."));
        cleanup();
        return;
    }
    m_runtimeTrustedFingerprint = m_pendingFingerprint;
    if (remember) {
        QSettings().setValue(QStringLiteral("tlsCertificates/%1:%2")
                                 .arg(m_profile.host)
                                 .arg(m_profile.port),
                             m_pendingFingerprint);
    }
    finishConnect();
}

void FtpWorker::finishConnect()
{
    if (!login())
        return;
    loadFeatures();
    if (m_reconnecting) {
        emit reconnected();
        return;
    }
    const QString home = currentDirectory();
    emit connected(home, m_serverBanner, m_runtimeTrustedFingerprint);
}

bool FtpWorker::login()
{
    QElapsedTimer timer;
    timer.start();

    const QString user = m_profile.username.isEmpty()
                             ? QStringLiteral("anonymous")
                             : m_profile.username;
    Reply reply = command(QStringLiteral("USER"), user);
    if (reply.code == 331 || reply.code == 332) {
        QString secret = m_profile.password;
        if (secret.isEmpty() && user == QStringLiteral("anonymous"))
            secret = QStringLiteral("anonymous@");
        reply = command(QStringLiteral("PASS"), secret);
    }
    if (reply.code != 230 && reply.code != 202) {
        if (!m_reconnecting)
            emit errorOccurred(failureText(tr("Ошибка аутентификации"), reply));
        cleanup();
        return false;
    }

    if (m_tlsActive) {
        command(QStringLiteral("PBSZ"), QStringLiteral("0"));
        const Reply protection =
            command(QStringLiteral("PROT"), QStringLiteral("P"));
        if (protection.code != 200) {
            if (!m_reconnecting) {
                emit errorOccurred(failureText(
                    tr("Сервер отказал в защите канала данных"), protection));
            }
            cleanup();
            return false;
        }
    }

    const Reply binary = command(QStringLiteral("TYPE"), QStringLiteral("I"));
    if (binary.code != 200) {
        if (!m_reconnecting) {
            emit errorOccurred(
                failureText(tr("Сервер не принял двоичный режим"), binary));
        }
        cleanup();
        return false;
    }
    emit performanceMetric(tr("Аутентификация"), timer.elapsed());
    return true;
}

void FtpWorker::loadFeatures()
{
    m_hasMlsd = false;
    m_hasMfmt = false;
    m_hasSize = true;
    m_useEpsv = true;

    const Reply reply = command(QStringLiteral("FEAT"));
    if (reply.code != 211)
        return;

    bool utf8 = false;
    const QStringList lines = reply.text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString feature = line.trimmed().toUpper();
        if (feature.startsWith(QStringLiteral("MLSD")) ||
            feature.startsWith(QStringLiteral("MLST")))
            m_hasMlsd = true;
        else if (feature.startsWith(QStringLiteral("MFMT")))
            m_hasMfmt = true;
        else if (feature.startsWith(QStringLiteral("UTF8")))
            utf8 = true;
    }

    if (utf8) {
        m_utf8 = true;
        command(QStringLiteral("OPTS"), QStringLiteral("UTF8 ON"));
    }
}

void FtpWorker::cleanup()
{
    m_waitingForHostKey = false;
    m_tlsActive = false;
    m_utf8 = false;
    m_workingDirectory.clear();
    m_controlBuffer.clear();
    beginDirectoryRequest();
    if (!m_control)
        return;
    if (m_control->state() == QAbstractSocket::ConnectedState) {
        m_control->write("QUIT\r\n");
        m_control->waitForBytesWritten(1000);
        m_control->waitForReadyRead(1000);
        m_control->readAll();
        m_control->disconnectFromHost();
        if (m_control->state() != QAbstractSocket::UnconnectedState)
            m_control->waitForDisconnected(2000);
    }
    delete m_control;
    m_control = nullptr;
}

void FtpWorker::disconnectFromHost()
{
    requestStop();
    cleanup();
    emit disconnected();
}

bool FtpWorker::reconnectSession()
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
        success = controlReady() && !m_waitingForHostKey;
        if (success || m_waitingForHostKey)
            break;
    }
    m_reconnecting = false;
    return success;
}

bool FtpWorker::enterPassiveMode(QHostAddress &address, quint16 &port)
{
    if (m_useEpsv) {
        const Reply reply = command(QStringLiteral("EPSV"));
        if (reply.code == 229) {
            static const QRegularExpression pattern(
                QStringLiteral("\\((.)\\1\\1(\\d+)\\1\\)"));
            const QRegularExpressionMatch match = pattern.match(reply.text);
            if (match.hasMatch()) {
                const int parsed = match.captured(2).toInt();
                if (parsed > 0 && parsed <= 65535) {
                    address = m_control->peerAddress();
                    port = static_cast<quint16>(parsed);
                    return true;
                }
            }
        }
        m_useEpsv = false;
    }

    const Reply reply = command(QStringLiteral("PASV"));
    if (reply.code != 227)
        return false;
    static const QRegularExpression pattern(QStringLiteral(
        "(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*"
        "(\\d+)\\s*,\\s*(\\d+)"));
    const QRegularExpressionMatch match = pattern.match(reply.text);
    if (!match.hasMatch())
        return false;

    const int high = match.captured(5).toInt();
    const int low = match.captured(6).toInt();
    if (high < 0 || high > 255 || low < 0 || low > 255)
        return false;
    port = static_cast<quint16>(high * 256 + low);

    const QHostAddress announced(QStringLiteral("%1.%2.%3.%4")
                                     .arg(match.captured(1),
                                          match.captured(2),
                                          match.captured(3),
                                          match.captured(4)));
    const QHostAddress control = m_control->peerAddress();
    address = (announced.isNull() ||
               (!announced.isGlobal() && control.isGlobal()))
                  ? control
                  : announced;
    return port > 0;
}

QSslSocket *FtpWorker::openDataConnection(const QString &verb,
                                          const QString &argument,
                                          qint64 *restartOffset)
{
    QHostAddress address;
    quint16 port = 0;
    if (!enterPassiveMode(address, port)) {
        emit errorOccurred(tr("Сервер не открыл пассивный канал данных: %1")
                               .arg(m_lastReply.trimmed()));
        return nullptr;
    }

    auto *socket = new QSslSocket;
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    if (m_tlsActive) {
        QSslConfiguration configuration = socket->sslConfiguration();
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        configuration.setSslOption(QSsl::SslOptionDisableSessionPersistence,
                                   false);
        const QByteArray ticket = m_control->sslConfiguration().sessionTicket();
        if (!ticket.isEmpty())
            configuration.setSessionTicket(ticket);
        socket->setSslConfiguration(configuration);
    }
    socket->connectToHost(address, port);
    if (!socket->waitForConnected(ConnectTimeout)) {
        emit errorOccurred(tr("Не удалось открыть канал данных %1:%2: %3")
                               .arg(address.toString())
                               .arg(port)
                               .arg(socket->errorString()));
        delete socket;
        return nullptr;
    }
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    if (restartOffset && *restartOffset > 0) {
        const Reply marker = command(QStringLiteral("REST"),
                                     QString::number(*restartOffset));
        if (marker.code != 350)
            *restartOffset = 0;
    }

    const Reply reply = command(verb, argument);
    if (reply.code != 150 && reply.code != 125) {
        delete socket;
        return nullptr;
    }

    if (m_tlsActive) {
        socket->startClientEncryption();
        if (!socket->waitForEncrypted(ConnectTimeout)) {
            emit errorOccurred(tr("Не удалось защитить канал данных: %1")
                                   .arg(socket->errorString()));
            closeDataConnection(socket);
            finishDataTransfer();
            return nullptr;
        }
    }
    return socket;
}

void FtpWorker::closeDataConnection(QSslSocket *socket)
{
    if (!socket)
        return;
    if (socket->state() == QAbstractSocket::ConnectedState) {
        while (socket->bytesToWrite() > 0 &&
               socket->waitForBytesWritten(DataTimeout)) {
        }
        for (int pass = 0; pass < 4; ++pass) {
            socket->readAll();
            if (!socket->waitForReadyRead(1))
                break;
        }
    }
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
        if (socket->state() != QAbstractSocket::UnconnectedState)
            socket->waitForDisconnected(DataTimeout);
    }
    delete socket;
}

bool FtpWorker::finishDataTransfer()
{
    const Reply reply = readReply();
    return reply.valid && (reply.code == 226 || reply.code == 250);
}

bool FtpWorker::changeDirectory(const QString &path)
{
    const QString target = normalizedRemotePath(path);
    if (m_workingDirectory == target)
        return true;
    const Reply reply = command(QStringLiteral("CWD"), target);
    if (reply.code != 250 && reply.code != 200)
        return false;
    m_workingDirectory = target;
    return true;
}

QString FtpWorker::currentDirectory()
{
    const Reply reply = command(QStringLiteral("PWD"));
    if (reply.code != 257)
        return QStringLiteral("/");
    const qsizetype first = reply.text.indexOf(QLatin1Char('"'));
    const qsizetype last = reply.text.lastIndexOf(QLatin1Char('"'));
    if (first < 0 || last <= first)
        return QStringLiteral("/");
    QString path = reply.text.mid(first + 1, last - first - 1);
    path.replace(QStringLiteral("\"\""), QStringLiteral("\""));
    return normalizedRemotePath(path);
}

qint64 FtpWorker::remoteFileSize(const QString &path)
{
    if (!m_hasSize)
        return -1;
    const Reply reply = command(QStringLiteral("SIZE"),
                                normalizedRemotePath(path));
    if (reply.code == 500 || reply.code == 502)
        m_hasSize = false;
    if (reply.code != 213)
        return -1;
    bool ok = false;
    const qint64 size = reply.text.trimmed().toLongLong(&ok);
    return ok ? size : -1;
}

qint64 FtpWorker::remoteModifiedTime(const QString &path)
{
    const Reply reply = command(QStringLiteral("MDTM"),
                                normalizedRemotePath(path));
    if (reply.code != 213)
        return 0;
    const QDateTime stamp = machineTime(reply.text.trimmed());
    return stamp.isValid() ? stamp.toSecsSinceEpoch() : 0;
}

bool FtpWorker::remoteIsDirectory(const QString &path)
{
    const QString previous = m_workingDirectory;
    if (!changeDirectory(path))
        return false;
    if (!previous.isEmpty())
        changeDirectory(previous);
    return true;
}

bool FtpWorker::readRemoteDirectory(const QString &path, RemoteEntries &entries,
                                    QString &failure)
{
    const QString cleanPath = normalizedRemotePath(path);
    if (!changeDirectory(cleanPath)) {
        failure = tr("Не удалось открыть удалённую папку «%1»: %2")
                      .arg(cleanPath, m_lastReply.trimmed());
        return false;
    }

    const QString verb = m_hasMlsd ? QStringLiteral("MLSD")
                                   : QStringLiteral("LIST");
    QSslSocket *data = openDataConnection(verb, QString());
    if (!data) {
        if (m_hasMlsd) {
            m_hasMlsd = false;
            data = openDataConnection(QStringLiteral("LIST"), QString());
        }
        if (!data) {
            failure = tr("Не удалось получить список папки «%1»: %2")
                          .arg(cleanPath, m_lastReply.trimmed());
            return false;
        }
    }

    QByteArray payload;
    QElapsedTimer idle;
    idle.start();
    bool failed = false;
    forever {
        if (stopping()) {
            failed = true;
            break;
        }
        if (data->bytesAvailable() == 0) {
            if (!data->waitForReadyRead(250)) {
                if (data->state() != QAbstractSocket::ConnectedState &&
                    data->bytesAvailable() == 0)
                    break;
                if (idle.elapsed() > DataTimeout) {
                    failed = true;
                    break;
                }
                continue;
            }
        }
        const QByteArray chunk = data->readAll();
        if (chunk.isEmpty()) {
            if (data->state() != QAbstractSocket::ConnectedState)
                break;
            continue;
        }
        payload.append(chunk);
        idle.restart();
    }
    closeDataConnection(data);
    const bool acknowledged = finishDataTransfer();
    if (failed || !acknowledged) {
        failure = tr("Не удалось получить список папки «%1»: %2")
                      .arg(cleanPath, m_lastReply.trimmed());
        return false;
    }

    static const QRegularExpression newline(QStringLiteral("\r\n|\n|\r"));
    const QStringList lines =
        decodeText(payload).split(newline, Qt::SkipEmptyParts);
    const bool machine = m_hasMlsd;
    for (const QString &line : lines) {
        RemoteEntry entry;
        const bool parsed = machine ? parseMachineLine(line, entry)
                                    : (parseUnixLine(line, entry) ||
                                       parseDosLine(line, entry));
        if (!parsed)
            continue;
        if (entry.name == QStringLiteral(".") ||
            entry.name == QStringLiteral(".."))
            continue;
        entry.path = joinRemote(cleanPath, entry.name);
        entries.append(entry);
    }
    return true;
}

void FtpWorker::listDirectory(const QString &path, quint64 generation)
{
    if (stopping())
        return;
    if (!controlReady()) {
        if (!reconnectSession()) {
            emit errorOccurred(tr("Нет активного FTP-подключения."));
            return;
        }
        generation = beginDirectoryRequest();
    }
    if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
        return;

    QElapsedTimer timer;
    timer.start();
    m_busy.store(true, std::memory_order_relaxed);
    const QString cleanPath = normalizedRemotePath(path);
    RemoteEntries entries;
    QString failure;
    const bool listed = readRemoteDirectory(cleanPath, entries, failure);
    m_busy.store(false, std::memory_order_relaxed);

    if (generation != m_directoryGeneration.load(std::memory_order_relaxed))
        return;
    if (!listed) {
        emit errorOccurred(failure);
        return;
    }

    sortEntries(entries);
    emit performanceMetric(
        tr("Каталог %1 (%2 объектов)").arg(cleanPath).arg(entries.size()),
        timer.elapsed());
    emit directoryListed(cleanPath, entries);
}

void FtpWorker::createDirectory(const QString &path)
{
    if (!controlReady()) {
        emit errorOccurred(tr("Нет активного FTP-подключения."));
        return;
    }
    const Reply reply = command(QStringLiteral("MKD"),
                                normalizedRemotePath(path));
    if (reply.code != 257) {
        emit errorOccurred(
            failureText(tr("Не удалось создать папку «%1»").arg(path), reply));
        return;
    }
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

void FtpWorker::createFile(const QString &path)
{
    if (!controlReady()) {
        emit errorOccurred(tr("Нет активного FTP-подключения."));
        return;
    }
    const QString target = normalizedRemotePath(path);
    if (remoteFileSize(target) >= 0) {
        emit errorOccurred(tr("Файл «%1» уже существует.").arg(path));
        return;
    }

    QSslSocket *data = openDataConnection(QStringLiteral("STOR"), target);
    if (!data) {
        emit errorOccurred(tr("Не удалось создать файл «%1»: %2")
                               .arg(path, m_lastReply.trimmed()));
        return;
    }
    closeDataConnection(data);
    if (!finishDataTransfer()) {
        emit errorOccurred(tr("Не удалось создать файл «%1»: %2")
                               .arg(path, m_lastReply.trimmed()));
        return;
    }
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

void FtpWorker::renameEntry(const QString &oldPath, const QString &newPath)
{
    if (!controlReady())
        return;
    const Reply from = command(QStringLiteral("RNFR"),
                               normalizedRemotePath(oldPath));
    if (from.code != 350) {
        emit errorOccurred(failureText(
            tr("Не удалось переименовать «%1» в «%2»").arg(oldPath, newPath),
            from));
        return;
    }
    const Reply to = command(QStringLiteral("RNTO"),
                             normalizedRemotePath(newPath));
    if (to.code != 250) {
        emit errorOccurred(failureText(
            tr("Не удалось переименовать «%1» в «%2»").arg(oldPath, newPath),
            to));
        return;
    }
    listDirectory(remoteParentPath(newPath), beginDirectoryRequest());
}

void FtpWorker::removeEntry(const QString &path, bool directory)
{
    if (!controlReady())
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
        emit errorOccurred(tr("Не удалось удалить «%1»: %2")
                               .arg(failedPath.isEmpty() ? path : failedPath,
                                    m_lastReply.trimmed()));
        return;
    }
    emit deletionProgress(static_cast<qint64>(removedCount), path);
    listDirectory(remoteParentPath(path), beginDirectoryRequest());
}

bool FtpWorker::removeRecursively(const QString &path, bool directory,
                                  quint64 &removedCount, QString &failedPath)
{
    if (stopping())
        return false;
    const QString target = normalizedRemotePath(path);

    if (!directory) {
        if (command(QStringLiteral("DELE"), target).code == 250) {
            ++removedCount;
            return true;
        }
        failedPath = path;
        return false;
    }

    QStringList directories;
    QStringList pending{target};
    while (!pending.isEmpty()) {
        if (stopping())
            return false;
        const QString current = pending.takeLast();
        RemoteEntries entries;
        QString failure;
        if (!readRemoteDirectory(current, entries, failure)) {
            if (command(QStringLiteral("DELE"), current).code == 250) {
                ++removedCount;
                continue;
            }
            failedPath = current;
            return false;
        }
        directories.append(current);
        for (const RemoteEntry &entry : entries) {
            if (entry.directory && !entry.symlink) {
                pending.append(entry.path);
                continue;
            }
            if (command(QStringLiteral("DELE"), entry.path).code != 250) {
                failedPath = entry.path;
                return false;
            }
            if (++removedCount % 50 == 0) {
                emit deletionProgress(static_cast<qint64>(removedCount),
                                      entry.path);
            }
        }
    }

    m_workingDirectory.clear();
    changeDirectory(remoteParentPath(target));
    for (auto it = directories.crbegin(); it != directories.crend(); ++it) {
        if (stopping())
            return false;
        if (command(QStringLiteral("RMD"), *it).code != 250) {
            failedPath = *it;
            return false;
        }
        ++removedCount;
    }
    return true;
}

bool FtpWorker::ensureRemoteDirectory(const QString &path)
{
    const QString target = normalizedRemotePath(path);
    if (command(QStringLiteral("MKD"), target).code == 257)
        return true;
    return remoteIsDirectory(target);
}

TransferState FtpWorker::runJob(const TransferJob &job)
{
    JobContext context;
    context.status = statusFor(job);
    context.status.state = TransferState::Running;
    context.status.total = -1;
    context.clock.start();
    context.emitClock.start();
    publish(context, true);

    if (!controlReady() && !reconnectSession()) {
        context.status.state = TransferState::Failed;
        context.status.detail = tr("Нет активного FTP-подключения.");
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

bool FtpWorker::expandJob(const TransferJob &job, QVector<FileTask> &tasks,
                          QString &failure)
{
    return job.upload ? expandUpload(job, tasks, failure)
                      : expandDownload(job, tasks, failure);
}

bool FtpWorker::expandDownload(const TransferJob &job, QVector<FileTask> &tasks,
                               QString &failure)
{
    if (!job.directory) {
        FileTask task;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = job.size;
        task.permissions = job.permissions;
        task.modified = job.modified;
        if (task.size < 0)
            task.size = remoteFileSize(job.remotePath);
        if (task.modified <= 0)
            task.modified = remoteModifiedTime(job.remotePath);
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
        if (!readRemoteDirectory(remoteDirectory, entries, failure))
            return false;
        for (const RemoteEntry &entry : entries) {
            const QString localPath = QDir(localDirectory).filePath(entry.name);
            if (entry.directory && !entry.symlink) {
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

bool FtpWorker::expandUpload(const TransferJob &job, QVector<FileTask> &tasks,
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
            failure = tr("Не удалось создать удалённую папку «%1»: %2")
                          .arg(remoteDirectory, m_lastReply.trimmed());
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

FtpWorker::FileOutcome FtpWorker::downloadFileTask(const FileTask &task,
                                                   JobContext &context,
                                                   int retries)
{
    if (jobCanceled(context.status.id))
        return FileOutcome::Canceled;

    const auto policy = static_cast<OverwritePolicy>(
        m_overwritePolicy.load(std::memory_order_relaxed));
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

    QSslSocket *data = openDataConnection(QStringLiteral("RETR"),
                                          normalizedRemotePath(task.remotePath),
                                          &startOffset);
    if (!data) {
        if (retries > 0 && !controlReady() && reconnectSession())
            return downloadFileTask(task, context, retries - 1);
        emit errorOccurred(tr("Не удалось открыть «%1» для скачивания: %2")
                               .arg(task.remotePath, m_lastReply.trimmed()));
        context.status.detail =
            tr("Не удалось открыть «%1».").arg(task.remotePath);
        return FileOutcome::Failed;
    }

    QFile output(partialPath);
    const QIODevice::OpenMode mode =
        startOffset > 0 ? (QIODevice::WriteOnly | QIODevice::Append)
                        : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!output.open(mode)) {
        closeDataConnection(data);
        finishDataTransfer();
        emit errorOccurred(tr("Не удалось создать «%1»: %2")
                               .arg(partialPath, output.errorString()));
        return FileOutcome::Failed;
    }

    const qint64 total = task.size;
    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;
    QElapsedTimer idle;
    idle.start();

    forever {
        if (jobCanceled(context.status.id)) {
            canceled = true;
            break;
        }
        if (data->bytesAvailable() == 0) {
            if (!data->waitForReadyRead(250)) {
                if (data->state() != QAbstractSocket::ConnectedState &&
                    data->bytesAvailable() == 0)
                    break;
                if (idle.elapsed() > DataTimeout) {
                    failed = true;
                    break;
                }
                continue;
            }
        }
        const QByteArray chunk = data->read(ChunkSize);
        if (chunk.isEmpty()) {
            if (data->state() != QAbstractSocket::ConnectedState)
                break;
            continue;
        }
        if (output.write(chunk) != chunk.size()) {
            emit errorOccurred(tr("Не удалось записать «%1»: %2")
                                   .arg(partialPath, output.errorString()));
            failed = true;
            break;
        }
        done += chunk.size();
        idle.restart();
        context.status.done = context.baseDone + (done - startOffset);
        publish(context);
    }

    if (!failed && !canceled && task.modified > 0) {
        output.setFileTime(QDateTime::fromSecsSinceEpoch(task.modified),
                           QFileDevice::FileModificationTime);
    }
    output.close();
    closeDataConnection(data);
    const bool acknowledged = finishDataTransfer();

    if (canceled)
        return FileOutcome::Canceled;
    if (failed || !acknowledged || (total >= 0 && done != total)) {
        if (retries > 0 && !controlReady() && reconnectSession())
            return downloadFileTask(task, context, retries - 1);
        emit errorOccurred(tr("Ошибка при скачивании «%1»: %2")
                               .arg(task.remotePath, m_lastReply.trimmed()));
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

FtpWorker::FileOutcome FtpWorker::uploadFileTask(const FileTask &task,
                                                 JobContext &context,
                                                 int retries)
{
    if (jobCanceled(context.status.id))
        return FileOutcome::Canceled;

    const auto policy = static_cast<OverwritePolicy>(
        m_overwritePolicy.load(std::memory_order_relaxed));
    TransferPlan::Destination state;
    const qint64 remoteSize = remoteFileSize(task.remotePath);
    if (remoteSize >= 0) {
        state.exists = true;
        state.size = remoteSize;
    }

    QString renamed = task.remotePath;
    if (state.exists && policy == OverwritePolicy::Rename) {
        for (int index = 2; index < 1000; ++index) {
            renamed = numberedVariant(task.remotePath, index);
            if (remoteFileSize(renamed) < 0)
                break;
        }
    }

    const TransferPlan plan = TransferPlan::makeUpload(
        policy, task.remotePath, task.size, state, renamed);
    if (plan.skip)
        return FileOutcome::Skipped;

    const QString destination = normalizedRemotePath(plan.destination);
    qint64 startOffset = plan.startOffset;

    QFile input(task.localPath);
    if (!input.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Не удалось открыть «%1»: %2")
                               .arg(task.localPath, input.errorString()));
        context.status.detail = tr("Не удалось открыть «%1».").arg(task.localPath);
        return FileOutcome::Failed;
    }

    QSslSocket *data = openDataConnection(QStringLiteral("STOR"), destination,
                                          &startOffset);
    if (!data) {
        if (retries > 0 && !controlReady() && reconnectSession())
            return uploadFileTask(task, context, retries - 1);
        emit errorOccurred(tr("Не удалось создать «%1» на сервере: %2")
                               .arg(destination, m_lastReply.trimmed()));
        context.status.detail = tr("Не удалось создать «%1».").arg(destination);
        return FileOutcome::Failed;
    }
    if (!input.seek(startOffset)) {
        input.close();
        closeDataConnection(data);
        finishDataTransfer();
        emit errorOccurred(tr("Не удалось перейти к позиции %1 в «%2».")
                               .arg(startOffset)
                               .arg(task.localPath));
        return FileOutcome::Failed;
    }

    const qint64 total = task.size;
    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;

    while (!input.atEnd()) {
        if (jobCanceled(context.status.id)) {
            canceled = true;
            break;
        }
        const QByteArray chunk = input.read(ChunkSize);
        if (chunk.isEmpty()) {
            failed = !input.atEnd();
            break;
        }
        if (data->write(chunk) != chunk.size() ||
            !data->waitForBytesWritten(DataTimeout)) {
            failed = true;
            break;
        }
        done += chunk.size();
        context.status.done = context.baseDone + (done - startOffset);
        publish(context);
    }

    input.close();
    closeDataConnection(data);
    const bool acknowledged = finishDataTransfer();

    if (canceled)
        return FileOutcome::Canceled;
    if (failed || !acknowledged || (total >= 0 && done != total)) {
        if (retries > 0 && !controlReady() && reconnectSession())
            return uploadFileTask(task, context, retries - 1);
        emit errorOccurred(tr("Ошибка при загрузке «%1»: %2")
                               .arg(task.localPath, m_lastReply.trimmed()));
        context.status.detail =
            tr("Ошибка при загрузке «%1».").arg(task.localPath);
        return FileOutcome::Failed;
    }

    applyRemoteMetadata(destination, task);
    context.status.done = context.baseDone + (done - startOffset);
    return FileOutcome::Completed;
}

void FtpWorker::applyLocalMetadata(const QString &localPath,
                                   const FileTask &task)
{
    if (task.permissions & 0100) {
        QFile::setPermissions(localPath,
                              QFile::permissions(localPath) |
                                  QFileDevice::ExeOwner | QFileDevice::ExeUser);
    }
}

void FtpWorker::applyRemoteMetadata(const QString &remotePath,
                                    const FileTask &task)
{
    if (m_hasMfmt && task.modified > 0) {
        const QString stamp =
            QDateTime::fromSecsSinceEpoch(task.modified, QTimeZone::UTC)
                .toString(QStringLiteral("yyyyMMddHHmmss"));
        command(QStringLiteral("MFMT"),
                QStringLiteral("%1 %2").arg(stamp, remotePath));
    }
    if (task.permissions != 0) {
        command(QStringLiteral("SITE"),
                QStringLiteral("CHMOD %1 %2")
                    .arg(QString::number(task.permissions & 07777, 8),
                         remotePath));
    }
}
