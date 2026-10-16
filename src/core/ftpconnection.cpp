#include "ftpconnection.h"

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
#include <QTimeZone>
#include <QUrl>

#include <algorithm>

namespace {
constexpr int ConnectTimeout = 15000;
constexpr int ControlTimeout = 20000;
constexpr int DataTimeout = 30000;
constexpr qint64 ChunkSize = 256 * 1024;

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

FtpConnection::~FtpConnection()
{
    close();
}

QByteArray FtpConnection::encodePath(const QString &path) const
{
    return m_utf8 ? path.toUtf8() : path.toLatin1();
}

QString FtpConnection::decodeText(const QByteArray &data) const
{
    return m_utf8 ? QString::fromUtf8(data) : QString::fromLatin1(data);
}

bool FtpConnection::isOpen() const
{
    return m_control && m_control->state() == QAbstractSocket::ConnectedState &&
           !m_waitingForTrust;
}

QString FtpConnection::failureText(const QString &prefix, const Reply &reply) const
{
    if (!reply.valid)
        return tr("%1: сервер закрыл FTP-соединение.").arg(prefix);
    const QString detail = reply.text.trimmed();
    return detail.isEmpty()
               ? prefix
               : QStringLiteral("%1: %2 (%3)").arg(prefix, detail).arg(reply.code);
}

bool FtpConnection::waitForData(QTcpSocket *socket, int milliseconds) const
{
    if (!socket)
        return false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        if (socket->bytesAvailable() > 0)
            return true;
        if (m_abort && m_abort())
            return false;
        if (socket->waitForReadyRead(250))
            return true;
        if (socket->state() != QAbstractSocket::ConnectedState)
            break;
    }
    return socket->bytesAvailable() > 0;
}

bool FtpConnection::readControlLine(QByteArray &line)
{
    forever {
        const qsizetype end = m_buffer.indexOf('\n');
        if (end >= 0) {
            line = m_buffer.left(end);
            m_buffer.remove(0, end + 1);
            while (line.endsWith('\r'))
                line.chop(1);
            return true;
        }
        if (!m_control || !waitForData(m_control, ControlTimeout))
            return false;
        const QByteArray incoming = m_control->readAll();
        if (incoming.isEmpty())
            return false;
        m_buffer.append(incoming);
    }
}

FtpConnection::Reply FtpConnection::readReply()
{
    Reply reply;
    QByteArray line;
    if (!readControlLine(line)) {
        m_broken = true;
        return reply;
    }
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
            if (!readControlLine(line)) {
                m_broken = true;
                return Reply{};
            }
            collected.append(decodeText(textOf(line)));
            if (codeOf(line) == reply.code && line.at(3) == ' ')
                break;
        }
    }
    reply.valid = true;
    reply.text = collected.join(QLatin1Char('\n'));
    m_lastReply = reply.text;
    if (reply.code == 421)
        m_broken = true;
    return reply;
}

FtpConnection::Reply FtpConnection::command(const QString &verb,
                                            const QString &argument)
{
    if (!m_control || m_control->state() != QAbstractSocket::ConnectedState) {
        m_broken = true;
        return {};
    }
    QByteArray payload = verb.toLatin1();
    if (!argument.isEmpty()) {
        payload.append(' ');
        payload.append(encodePath(argument));
    }
    payload.append("\r\n");
    if (m_control->write(payload) != payload.size() ||
        !m_control->waitForBytesWritten(ControlTimeout)) {
        m_broken = true;
        return {};
    }
    return readReply();
}

RemoteConnection::OpenResult FtpConnection::open(const ConnectionProfile &profile,
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
            if (url.scheme().compare(QStringLiteral("ftps"), Qt::CaseInsensitive) == 0)
                m_profile.useTls = true;
        }
    }

    m_control = new QSslSocket;
    m_control->setPeerVerifyMode(QSslSocket::VerifyNone);
    m_control->connectToHost(m_profile.host, m_profile.port);
    if (!m_control->waitForConnected(ConnectTimeout)) {
        fail(tr("Не удалось подключиться к %1:%2: %3")
                 .arg(m_profile.host)
                 .arg(m_profile.port)
                 .arg(m_control->errorString()),
             true);
        close();
        return OpenResult::Failed;
    }
    m_control->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_control->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    const Reply greeting = readReply();
    if (greeting.code != 220) {
        fail(failureText(tr("Сервер отклонил подключение"), greeting), true);
        close();
        return OpenResult::Failed;
    }
    m_serverInfo = greeting.text.split(QLatin1Char('\n')).first().trimmed();
    if (!m_profile.useTls)
        return finishOpen();
    if (!negotiateTls()) {
        close();
        return OpenResult::Failed;
    }

    const QSslCertificate certificate = m_control->peerCertificate();
    if (certificate.isNull()) {
        fail(tr("Сервер не предъявил TLS-сертификат."));
        close();
        return OpenResult::Failed;
    }
    m_pendingFingerprint = fingerprintText(certificate.digest(QCryptographicHash::Sha256));
    const QString key = QStringLiteral("tlsCertificates/%1:%2")
                            .arg(m_profile.host)
                            .arg(m_profile.port);
    const QString known = QSettings().value(key).toString();
    if (known == m_pendingFingerprint ||
        m_profile.trustedFingerprint == m_pendingFingerprint) {
        m_trustedFingerprint = m_pendingFingerprint;
        return finishOpen();
    }
    if (!interactiveTrust) {
        fail(tr("Сертификат %1 не совпадает с подтверждённым.").arg(m_profile.host));
        close();
        return OpenResult::Failed;
    }
    const QString subject = certificate.subjectDisplayName();
    m_trust.host = QStringLiteral("%1:%2").arg(m_profile.host).arg(m_profile.port);
    m_trust.fingerprint = m_pendingFingerprint;
    m_trust.keyType = subject.isEmpty() ? tr("TLS-сертификат")
                                        : tr("TLS-сертификат, %1").arg(subject);
    m_trust.changed = !known.isEmpty();
    m_waitingForTrust = true;
    return OpenResult::NeedsTrust;
}

RemoteConnection::OpenResult FtpConnection::continueAfterTrust(bool accepted,
                                                               bool remember)
{
    if (!m_waitingForTrust)
        return OpenResult::Failed;
    m_waitingForTrust = false;
    if (!accepted) {
        fail(tr("Подключение отменено: сертификат сервера не принят."));
        close();
        return OpenResult::Failed;
    }
    m_trustedFingerprint = m_pendingFingerprint;
    if (remember) {
        QSettings().setValue(QStringLiteral("tlsCertificates/%1:%2")
                                 .arg(m_profile.host)
                                 .arg(m_profile.port),
                             m_pendingFingerprint);
    }
    return finishOpen();
}

bool FtpConnection::negotiateTls()
{
    if (!QSslSocket::supportsSsl()) {
        fail(tr("В сборке нет поддержки TLS, FTPS недоступен."));
        return false;
    }
    Reply reply = command(QStringLiteral("AUTH"), QStringLiteral("TLS"));
    if (reply.code != 234)
        reply = command(QStringLiteral("AUTH"), QStringLiteral("SSL"));
    if (reply.code != 234) {
        fail(failureText(tr("Сервер не поддерживает FTPS"), reply));
        return false;
    }
    QSslConfiguration configuration = m_control->sslConfiguration();
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
    configuration.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
    m_control->setSslConfiguration(configuration);
    m_control->startClientEncryption();
    if (!m_control->waitForEncrypted(ConnectTimeout)) {
        fail(tr("Не удалось установить TLS-сеанс: %1").arg(m_control->errorString()),
             true);
        return false;
    }
    m_tlsActive = true;
    return true;
}

RemoteConnection::OpenResult FtpConnection::finishOpen()
{
    if (!login()) {
        const bool network = m_broken;
        close();
        return network ? OpenResult::Failed : OpenResult::AuthFailed;
    }
    loadFeatures();
    return OpenResult::Ok;
}

bool FtpConnection::login()
{
    const QString user =
        m_profile.username.isEmpty() ? QStringLiteral("anonymous") : m_profile.username;
    Reply reply = command(QStringLiteral("USER"), user);
    if (reply.code == 331 || reply.code == 332) {
        QString secret = m_profile.password;
        if (secret.isEmpty() && user == QStringLiteral("anonymous"))
            secret = QStringLiteral("anonymous@");
        reply = command(QStringLiteral("PASS"), secret);
    }
    if (reply.code != 230 && reply.code != 202) {
        fail(failureText(tr("Ошибка входа"), reply), !reply.valid);
        return false;
    }
    if (m_tlsActive) {
        command(QStringLiteral("PBSZ"), QStringLiteral("0"));
        const Reply protection = command(QStringLiteral("PROT"), QStringLiteral("P"));
        if (protection.code != 200) {
            fail(failureText(tr("Сервер отказал в защите канала данных"), protection));
            return false;
        }
    }
    const Reply binary = command(QStringLiteral("TYPE"), QStringLiteral("I"));
    if (binary.code != 200) {
        fail(failureText(tr("Сервер не принял двоичный режим"), binary), !binary.valid);
        return false;
    }
    return true;
}

void FtpConnection::loadFeatures()
{
    m_hasMlsd = false;
    m_hasMfmt = false;
    m_hasSize = true;
    m_useEpsv = true;
    const Reply reply = command(QStringLiteral("FEAT"));
    if (reply.code != 211)
        return;
    bool utf8 = false;
    for (const QString &line : reply.text.split(QLatin1Char('\n'))) {
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

void FtpConnection::close()
{
    m_waitingForTrust = false;
    m_tlsActive = false;
    m_utf8 = false;
    m_workingDirectory.clear();
    m_buffer.clear();
    if (!m_control)
        return;
    if (m_control->state() == QAbstractSocket::ConnectedState && !m_broken) {
        m_control->write("QUIT\r\n");
        m_control->waitForBytesWritten(500);
    }
    m_control->abort();
    delete m_control;
    m_control = nullptr;
}

bool FtpConnection::ping()
{
    if (!isOpen())
        return false;
    const Reply reply = command(QStringLiteral("NOOP"));
    if (!reply.valid) {
        fail(tr("Сервер перестал отвечать."), true);
        return false;
    }
    return true;
}

QString FtpConnection::homePath()
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
    return normalized(path);
}

bool FtpConnection::enterPassiveMode(QHostAddress &address, quint16 &port)
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
        if (!reply.valid)
            return false;
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
                                     .arg(match.captured(1), match.captured(2),
                                          match.captured(3), match.captured(4)));
    const QHostAddress control = m_control->peerAddress();
    address = (announced.isNull() || (!announced.isGlobal() && control.isGlobal()))
                  ? control
                  : announced;
    return port > 0;
}

QSslSocket *FtpConnection::openData(const QString &verb, const QString &argument,
                                    qint64 *restartOffset)
{
    QHostAddress address;
    quint16 port = 0;
    if (!enterPassiveMode(address, port)) {
        fail(tr("Сервер не открыл канал данных: %1").arg(m_lastReply.trimmed()),
             m_broken);
        return nullptr;
    }
    auto *socket = new QSslSocket;
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);
    if (m_tlsActive) {
        QSslConfiguration configuration = socket->sslConfiguration();
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        configuration.setSslOption(QSsl::SslOptionDisableSessionPersistence, false);
        const QByteArray ticket = m_control->sslConfiguration().sessionTicket();
        if (!ticket.isEmpty())
            configuration.setSessionTicket(ticket);
        socket->setSslConfiguration(configuration);
    }
    socket->connectToHost(address, port);
    if (!socket->waitForConnected(ConnectTimeout)) {
        fail(tr("Не удалось открыть канал данных %1:%2: %3")
                 .arg(address.toString())
                 .arg(port)
                 .arg(socket->errorString()),
             true);
        delete socket;
        return nullptr;
    }
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 8 * 1024 * 1024);
    socket->setReadBufferSize(0);

    if (restartOffset && *restartOffset > 0) {
        const Reply marker =
            command(QStringLiteral("REST"), QString::number(*restartOffset));
        if (marker.code != 350)
            *restartOffset = 0;
    }
    const Reply reply = command(verb, argument);
    if (reply.code != 150 && reply.code != 125) {
        fail(failureText(tr("Сервер отклонил %1").arg(verb), reply), !reply.valid);
        delete socket;
        return nullptr;
    }
    if (m_tlsActive) {
        socket->startClientEncryption();
        if (!socket->waitForEncrypted(ConnectTimeout)) {
            fail(tr("Не удалось защитить канал данных: %1").arg(socket->errorString()));
            closeData(socket);
            finishData();
            return nullptr;
        }
    }
    return socket;
}

void FtpConnection::closeData(QSslSocket *socket)
{
    if (!socket)
        return;
    if (socket->state() == QAbstractSocket::ConnectedState) {
        while (socket->bytesToWrite() > 0 && socket->waitForBytesWritten(DataTimeout)) {
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

bool FtpConnection::finishData()
{
    const Reply reply = readReply();
    return reply.valid && (reply.code == 226 || reply.code == 250);
}

bool FtpConnection::changeDirectory(const QString &path)
{
    const QString target = normalized(path);
    if (m_workingDirectory == target)
        return true;
    const Reply reply = command(QStringLiteral("CWD"), target);
    if (reply.code != 250 && reply.code != 200)
        return false;
    m_workingDirectory = target;
    return true;
}

qint64 FtpConnection::remoteSize(const QString &path)
{
    if (!m_hasSize)
        return -1;
    const Reply reply = command(QStringLiteral("SIZE"), normalized(path));
    if (reply.code == 500 || reply.code == 502)
        m_hasSize = false;
    if (reply.code != 213)
        return -1;
    bool ok = false;
    const qint64 size = reply.text.trimmed().toLongLong(&ok);
    return ok ? size : -1;
}

qint64 FtpConnection::remoteTime(const QString &path)
{
    const Reply reply = command(QStringLiteral("MDTM"), normalized(path));
    if (reply.code != 213)
        return 0;
    const QDateTime stamp = machineTime(reply.text.trimmed());
    return stamp.isValid() ? stamp.toSecsSinceEpoch() : 0;
}

bool FtpConnection::remoteIsDirectory(const QString &path)
{
    const QString previous = m_workingDirectory;
    if (!changeDirectory(path))
        return false;
    if (!previous.isEmpty())
        changeDirectory(previous);
    return true;
}

bool FtpConnection::readDirectory(const QString &path, RemoteEntries &entries)
{
    const QString cleanPath = normalized(path);
    if (!changeDirectory(cleanPath)) {
        fail(tr("Не удалось открыть «%1»: %2").arg(cleanPath, m_lastReply.trimmed()),
             m_broken);
        return false;
    }
    QSslSocket *data =
        openData(m_hasMlsd ? QStringLiteral("MLSD") : QStringLiteral("LIST"), QString());
    if (!data && m_hasMlsd && !m_broken) {
        m_hasMlsd = false;
        data = openData(QStringLiteral("LIST"), QString());
    }
    if (!data) {
        fail(tr("Не удалось получить список «%1»: %2")
                 .arg(cleanPath, m_lastReply.trimmed()),
             m_broken);
        return false;
    }

    QByteArray payload;
    QElapsedTimer idle;
    idle.start();
    bool failed = false;
    forever {
        if (m_abort && m_abort()) {
            failed = true;
            break;
        }
        if (data->bytesAvailable() == 0 && !data->waitForReadyRead(250)) {
            if (data->state() != QAbstractSocket::ConnectedState &&
                data->bytesAvailable() == 0)
                break;
            if (idle.elapsed() > DataTimeout) {
                failed = true;
                break;
            }
            continue;
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
    closeData(data);
    const bool acknowledged = finishData();
    if (failed || !acknowledged) {
        fail(tr("Не удалось получить список «%1»: %2")
                 .arg(cleanPath, m_lastReply.trimmed()),
             m_broken || failed);
        return false;
    }

    static const QRegularExpression newline(QStringLiteral("\r\n|\n|\r"));
    for (const QString &line : decodeText(payload).split(newline, Qt::SkipEmptyParts)) {
        RemoteEntry entry;
        const bool parsed = m_hasMlsd ? parseMachineLine(line, entry)
                                      : (parseUnixLine(line, entry) ||
                                         parseDosLine(line, entry));
        if (!parsed || entry.name == QStringLiteral(".") ||
            entry.name == QStringLiteral(".."))
            continue;
        entry.path = join(cleanPath, entry.name);
        entries.append(entry);
    }
    return true;
}

bool FtpConnection::list(const QString &path, RemoteEntries &entries)
{
    entries.clear();
    m_abort = m_cancel;
    const bool ok = readDirectory(path, entries);
    if (ok)
        sortEntries(entries);
    return ok;
}

bool FtpConnection::makeDirectory(const QString &path)
{
    const Reply reply = command(QStringLiteral("MKD"), normalized(path));
    if (reply.code != 257) {
        fail(failureText(tr("Не удалось создать папку «%1»").arg(path), reply),
             !reply.valid);
        return false;
    }
    return true;
}

bool FtpConnection::createFile(const QString &path)
{
    const QString target = normalized(path);
    if (remoteSize(target) >= 0) {
        fail(tr("Файл «%1» уже существует.").arg(path));
        return false;
    }
    QSslSocket *data = openData(QStringLiteral("STOR"), target);
    if (!data)
        return false;
    closeData(data);
    if (!finishData()) {
        fail(tr("Не удалось создать файл «%1»: %2").arg(path, m_lastReply.trimmed()),
             m_broken);
        return false;
    }
    return true;
}

bool FtpConnection::rename(const QString &from, const QString &to)
{
    const Reply source = command(QStringLiteral("RNFR"), normalized(from));
    if (source.code != 350) {
        fail(failureText(tr("Не удалось переименовать «%1»").arg(from), source),
             !source.valid);
        return false;
    }
    const Reply target = command(QStringLiteral("RNTO"), normalized(to));
    if (target.code != 250) {
        fail(failureText(tr("Не удалось переименовать «%1»").arg(from), target),
             !target.valid);
        return false;
    }
    return true;
}

bool FtpConnection::remove(const RemoteEntries &entries,
                           const std::function<void(qint64)> &progress)
{
    m_abort = m_cancel;
    qint64 removed = 0;
    for (const RemoteEntry &root : entries) {
        if (!(root.directory && !root.symlink)) {
            const Reply reply = command(QStringLiteral("DELE"), normalized(root.path));
            if (reply.code != 250) {
                fail(failureText(tr("Не удалось удалить «%1»").arg(root.path), reply),
                     !reply.valid);
                return false;
            }
            ++removed;
            continue;
        }
        QStringList directories;
        QStringList pending{normalized(root.path)};
        while (!pending.isEmpty()) {
            if (canceledByUser())
                return false;
            const QString current = pending.takeLast();
            RemoteEntries children;
            if (!readDirectory(current, children))
                return false;
            directories.append(current);
            for (const RemoteEntry &child : children) {
                if (child.directory && !child.symlink) {
                    pending.append(child.path);
                    continue;
                }
                const Reply reply = command(QStringLiteral("DELE"), child.path);
                if (reply.code != 250) {
                    fail(failureText(tr("Не удалось удалить «%1»").arg(child.path),
                                     reply),
                         !reply.valid);
                    return false;
                }
                if (++removed % 25 == 0 && progress)
                    progress(removed);
            }
        }
        m_workingDirectory.clear();
        changeDirectory(parentPath(root.path));
        for (auto it = directories.crbegin(); it != directories.crend(); ++it) {
            const Reply reply = command(QStringLiteral("RMD"), *it);
            if (reply.code != 250) {
                fail(failureText(tr("Не удалось удалить «%1»").arg(*it), reply),
                     !reply.valid);
                return false;
            }
            ++removed;
        }
    }
    if (progress)
        progress(removed);
    return true;
}

bool FtpConnection::ensureDirectory(const QString &path, bool &created)
{
    const QString target = normalized(path);
    created = false;
    if (command(QStringLiteral("MKD"), target).code == 257) {
        created = true;
        return true;
    }
    return remoteIsDirectory(target);
}

bool FtpConnection::expand(const TransferJob &job, TransferHost &host)
{
    m_error.clear();
    m_abort = [&host, &job] { return host.stopping() || host.canceled(job.id); };
    if (job.upload) {
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
            task.modified = source.lastModified().toSecsSinceEpoch();
            host.addTasks(job.id, {task});
            host.expansionFinished(job.id);
            return true;
        }
        bool anyAdded = false;
        QVector<QPair<QString, QString>> pending{{job.localPath, normalized(job.remotePath)}};
        while (!pending.isEmpty()) {
            if (host.canceled(job.id) || host.stopping())
                break;
            const auto [localDirectory, remoteDirectory] = pending.takeFirst();
            bool created = false;
            if (!ensureDirectory(remoteDirectory, created)) {
                if (m_broken && !anyAdded) {
                    host.requeueJob(job);
                    return false;
                }
                host.expansionFailed(job.id, tr("Не удалось создать папку «%1»: %2")
                                                 .arg(remoteDirectory, m_lastReply.trimmed()));
                return !m_broken;
            }
            QVector<FileTask> tasks;
            const QFileInfoList children =
                QDir(localDirectory)
                    .entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                       QDir::Hidden | QDir::System,
                                   QDir::DirsFirst | QDir::Name);
            for (const QFileInfo &child : children) {
                const QString remotePath = join(remoteDirectory, child.fileName());
                if (child.isDir() && !child.isSymLink()) {
                    pending.append({child.absoluteFilePath(), remotePath});
                    continue;
                }
                FileTask task;
                task.jobId = job.id;
                task.overwrite = job.overwrite;
                task.upload = true;
                task.localPath = child.absoluteFilePath();
                task.remotePath = remotePath;
                task.size = child.size();
                task.modified = child.lastModified().toSecsSinceEpoch();
                task.remoteAbsent = created;
                tasks.append(task);
            }
            if (!tasks.isEmpty()) {
                host.addTasks(job.id, tasks);
                anyAdded = true;
            }
        }
        host.expansionFinished(job.id);
        return true;
    }

    if (!job.directory) {
        FileTask task;
        task.jobId = job.id;
        task.overwrite = job.overwrite;
        task.localPath = job.localPath;
        task.remotePath = job.remotePath;
        task.size = job.size >= 0 ? job.size : remoteSize(job.remotePath);
        task.modified = job.modified > 0 ? job.modified : remoteTime(job.remotePath);
        task.permissions = job.permissions;
        host.addTasks(job.id, {task});
        host.expansionFinished(job.id);
        return true;
    }
    bool anyAdded = false;
    QVector<QPair<QString, QString>> pending{{normalized(job.remotePath), job.localPath}};
    while (!pending.isEmpty()) {
        if (host.canceled(job.id) || host.stopping())
            break;
        const auto [remoteDirectory, localDirectory] = pending.takeFirst();
        if (!QDir().mkpath(localDirectory)) {
            host.expansionFailed(job.id, tr("Не удалось создать папку «%1».")
                                             .arg(localDirectory));
            return true;
        }
        RemoteEntries children;
        if (!readDirectory(remoteDirectory, children)) {
            if (m_broken && !anyAdded) {
                host.requeueJob(job);
                return false;
            }
            host.expansionFailed(job.id, m_error);
            return !m_broken;
        }
        QVector<FileTask> tasks;
        for (const RemoteEntry &child : children) {
            const QString localPath = QDir(localDirectory).filePath(child.name);
            if (child.directory && !child.symlink) {
                pending.append({child.path, localPath});
                continue;
            }
            FileTask task;
            task.jobId = job.id;
            task.overwrite = job.overwrite;
            task.localPath = localPath;
            task.remotePath = child.path;
            task.size = child.size;
            task.permissions = child.permissions;
            task.modified =
                child.modified.isValid() ? child.modified.toSecsSinceEpoch() : 0;
            tasks.append(task);
        }
        if (!tasks.isEmpty()) {
            host.addTasks(job.id, tasks);
            anyAdded = true;
        }
    }
    host.expansionFinished(job.id);
    return true;
}

bool FtpConnection::runTransfers(TransferHost &host)
{
    m_error.clear();
    WorkItem item;
    while (!host.stopping() && host.take(item, true)) {
        if (item.kind == WorkItem::Kind::Expand) {
            if (!expand(item.job, host) && m_broken)
                return false;
            continue;
        }
        FileTask &task = item.file;
        host.current(task.jobId, task.upload ? task.localPath : task.remotePath);
        if (host.canceled(task.jobId)) {
            host.finished(task, FileOutcome::Canceled, true, QString());
            continue;
        }
        m_abort = [&host, &task] {
            return host.stopping() || host.canceled(task.jobId);
        };
        QString error;
        const FileOutcome outcome =
            task.upload ? upload(task, host, error) : download(task, host, error);
        if (outcome == FileOutcome::Failed && (m_broken || !isOpen())) {
            host.progress(task.jobId, -task.reported);
            task.reported = 0;
            task.attempt += 1;
            task.restarted = true;
            host.pushFront({task});
            m_broken = true;
            if (m_error.isEmpty())
                m_error = error;
            return false;
        }
        host.finished(task, outcome, true, error);
    }
    return true;
}

FileOutcome FtpConnection::download(FileTask &task, TransferHost &host,
                                    QString &error)
{
    QString destination = task.localPath;
    qint64 startOffset = 0;
    bool discardPartial = false;
    if (task.restarted) {
        const QFileInfo partial(task.localPath + partialSuffix());
        startOffset = partial.exists() ? partial.size() : 0;
    } else {
        const QFileInfo existing(task.localPath);
        const QFileInfo partial(task.localPath + partialSuffix());
        TransferPlan::Destination state;
        state.exists = existing.exists();
        state.size = state.exists ? existing.size() : 0;
        state.partialSize = partial.exists() ? partial.size() : -1;
        const TransferPlan plan = TransferPlan::make(
            task.overwrite ? OverwritePolicy::Overwrite : host.policy(), task.localPath,
            task.size, state, [&task] {
                for (int index = 2; index < 1000; ++index) {
                    const QString candidate = numberedVariant(task.localPath, index);
                    if (!QFileInfo::exists(candidate))
                        return candidate;
                }
                return task.localPath;
            });
        if (plan.skip) {
            host.progress(task.jobId, std::max<qint64>(task.size, 0));
            task.reported += std::max<qint64>(task.size, 0);
            return FileOutcome::Skipped;
        }
        destination = plan.destination;
        startOffset = plan.startOffset;
        discardPartial = plan.discardPartial;
    }
    const QString partialPath = destination + partialSuffix();
    if (discardPartial || startOffset == 0)
        QFile::remove(partialPath);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        error = tr("Не удалось создать папку для «%1».").arg(destination);
        return FileOutcome::Failed;
    }

    QSslSocket *data =
        openData(QStringLiteral("RETR"), normalized(task.remotePath), &startOffset);
    if (!data) {
        error = m_error;
        return FileOutcome::Failed;
    }
    QFile output(partialPath);
    const QIODevice::OpenMode mode = startOffset > 0
                                         ? (QIODevice::WriteOnly | QIODevice::Append)
                                         : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!output.open(mode)) {
        closeData(data);
        finishData();
        error = tr("Не удалось создать «%1»: %2").arg(partialPath, output.errorString());
        return FileOutcome::Failed;
    }
    task.reported += startOffset;
    host.progress(task.jobId, startOffset);

    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;
    QElapsedTimer idle;
    idle.start();
    forever {
        if (host.canceled(task.jobId) || host.stopping()) {
            canceled = true;
            break;
        }
        if (data->bytesAvailable() == 0 && !data->waitForReadyRead(250)) {
            if (data->state() != QAbstractSocket::ConnectedState &&
                data->bytesAvailable() == 0)
                break;
            if (idle.elapsed() > DataTimeout) {
                failed = true;
                break;
            }
            continue;
        }
        const QByteArray chunk = data->read(ChunkSize);
        if (chunk.isEmpty()) {
            if (data->state() != QAbstractSocket::ConnectedState)
                break;
            continue;
        }
        if (output.write(chunk) != chunk.size()) {
            error = tr("Не удалось записать «%1»: %2")
                        .arg(partialPath, output.errorString());
            output.close();
            closeData(data);
            finishData();
            return FileOutcome::Failed;
        }
        done += chunk.size();
        task.reported += chunk.size();
        host.progress(task.jobId, chunk.size());
        idle.restart();
    }
    if (!failed && !canceled && task.modified > 0) {
        output.setFileTime(QDateTime::fromSecsSinceEpoch(task.modified),
                           QFileDevice::FileModificationTime);
    }
    output.close();
    closeData(data);
    const bool acknowledged = finishData();
    if (canceled)
        return FileOutcome::Canceled;
    if (failed || !acknowledged || (task.size >= 0 && done != task.size)) {
        if (failed)
            m_broken = true;
        error = tr("Ошибка при скачивании «%1»: %2")
                    .arg(task.remotePath, m_lastReply.trimmed());
        return FileOutcome::Failed;
    }
    QFile::remove(destination);
    if (!QFile::rename(partialPath, destination)) {
        error = tr("Не удалось сохранить «%1».").arg(destination);
        return FileOutcome::Failed;
    }
    return FileOutcome::Completed;
}

FileOutcome FtpConnection::upload(FileTask &task, TransferHost &host, QString &error)
{
    TransferPlan::Destination state;
    if (!task.remoteAbsent || task.restarted) {
        const qint64 size = remoteSize(task.remotePath);
        if (size >= 0) {
            state.exists = true;
            state.size = size;
        }
    }
    QString destination = task.remotePath;
    qint64 startOffset = 0;
    if (task.restarted) {
        startOffset = state.exists && state.size < task.size ? state.size : 0;
    } else {
        const auto policy = task.overwrite ? OverwritePolicy::Overwrite : host.policy();
        QString renamed = task.remotePath;
        if (state.exists && policy == OverwritePolicy::Rename) {
            for (int index = 2; index < 1000; ++index) {
                renamed = numberedVariant(task.remotePath, index);
                if (remoteSize(renamed) < 0)
                    break;
            }
        }
        const TransferPlan plan = TransferPlan::makeUpload(
            policy, task.remotePath, task.size, state, renamed);
        if (plan.skip) {
            host.progress(task.jobId, std::max<qint64>(task.size, 0));
            task.reported += std::max<qint64>(task.size, 0);
            return FileOutcome::Skipped;
        }
        destination = plan.destination;
        startOffset = plan.startOffset;
    }
    destination = normalized(destination);

    QFile input(task.localPath);
    if (!input.open(QIODevice::ReadOnly)) {
        error = tr("Не удалось открыть «%1»: %2").arg(task.localPath, input.errorString());
        return FileOutcome::Failed;
    }
    QSslSocket *data = openData(QStringLiteral("STOR"), destination, &startOffset);
    if (!data) {
        error = m_error;
        return FileOutcome::Failed;
    }
    if (!input.seek(startOffset)) {
        closeData(data);
        finishData();
        error = tr("Не удалось прочитать «%1».").arg(task.localPath);
        return FileOutcome::Failed;
    }
    task.reported += startOffset;
    host.progress(task.jobId, startOffset);

    qint64 done = startOffset;
    bool failed = false;
    bool canceled = false;
    while (!input.atEnd()) {
        if (host.canceled(task.jobId) || host.stopping()) {
            canceled = true;
            break;
        }
        const QByteArray chunk = input.read(ChunkSize);
        if (chunk.isEmpty()) {
            failed = !input.atEnd();
            break;
        }
        if (data->write(chunk) != chunk.size()) {
            failed = true;
            break;
        }
        while (data->bytesToWrite() > 4 * ChunkSize) {
            if (!data->waitForBytesWritten(DataTimeout)) {
                failed = true;
                break;
            }
        }
        if (failed)
            break;
        done += chunk.size();
        task.reported += chunk.size();
        host.progress(task.jobId, chunk.size());
    }
    input.close();
    closeData(data);
    const bool acknowledged = finishData();
    if (canceled)
        return FileOutcome::Canceled;
    if (failed || !acknowledged || (task.size >= 0 && done != task.size)) {
        if (failed)
            m_broken = true;
        error = tr("Ошибка при загрузке «%1»: %2")
                    .arg(task.localPath, m_lastReply.trimmed());
        return FileOutcome::Failed;
    }
    if (m_hasMfmt && task.modified > 0) {
        const QString stamp = QDateTime::fromSecsSinceEpoch(task.modified, QTimeZone::UTC)
                                  .toString(QStringLiteral("yyyyMMddHHmmss"));
        command(QStringLiteral("MFMT"), QStringLiteral("%1 %2").arg(stamp, destination));
    }
    return FileOutcome::Completed;
}
