#include "sftpchannel.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <libssh/libssh.h>

#include <algorithm>
#include <cstring>

namespace sftp {

namespace {
void putU32(char *target, quint32 value)
{
    target[0] = static_cast<char>((value >> 24) & 0xff);
    target[1] = static_cast<char>((value >> 16) & 0xff);
    target[2] = static_cast<char>((value >> 8) & 0xff);
    target[3] = static_cast<char>(value & 0xff);
}

quint32 getU32(const char *source)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(source);
    return (static_cast<quint32>(bytes[0]) << 24) |
           (static_cast<quint32>(bytes[1]) << 16) |
           (static_cast<quint32>(bytes[2]) << 8) |
           static_cast<quint32>(bytes[3]);
}

constexpr qsizetype MaxPacket = 4 * 1024 * 1024;
constexpr qsizetype ReadChunk = 2 * 1024 * 1024;
}

Builder::Builder(Type type, quint32 id, qsizetype reserve)
{
    m_data.reserve(reserve + 9);
    m_data.append(4, '\0');
    u8(type);
    if (type != Init)
        u32(id);
}

Builder &Builder::u8(quint8 value)
{
    m_data.append(static_cast<char>(value));
    return *this;
}

Builder &Builder::u32(quint32 value)
{
    char bytes[4];
    putU32(bytes, value);
    m_data.append(bytes, 4);
    return *this;
}

Builder &Builder::u64(quint64 value)
{
    u32(static_cast<quint32>(value >> 32));
    return u32(static_cast<quint32>(value & 0xffffffffu));
}

Builder &Builder::string(const QByteArray &value)
{
    return string(value.constData(), value.size());
}

Builder &Builder::string(const char *data, qsizetype size)
{
    u32(static_cast<quint32>(size));
    m_data.append(data, size);
    return *this;
}

char *Builder::stringSpace(qsizetype size)
{
    u32(static_cast<quint32>(size));
    const qsizetype at = m_data.size();
    m_data.resize(at + size);
    return m_data.data() + at;
}

Builder &Builder::attrs(const FileAttrs &value)
{
    u32(value.flags & ~AttrExtended);
    if (value.flags & AttrSize)
        u64(value.size);
    if (value.flags & AttrUidGid) {
        u32(0);
        u32(0);
    }
    if (value.flags & AttrPermissions)
        u32(value.permissions);
    if (value.flags & AttrTimes) {
        u32(value.atime);
        u32(value.mtime);
    }
    return *this;
}

QByteArray Builder::finish()
{
    putU32(m_data.data(), static_cast<quint32>(m_data.size() - 4));
    return std::move(m_data);
}

quint8 Packet::u8()
{
    if (position + 1 > data.size()) {
        valid = false;
        return 0;
    }
    return static_cast<quint8>(data.at(position++));
}

quint32 Packet::u32()
{
    if (position + 4 > data.size()) {
        valid = false;
        return 0;
    }
    const quint32 value = getU32(data.constData() + position);
    position += 4;
    return value;
}

quint64 Packet::u64()
{
    const quint64 high = u32();
    const quint64 low = u32();
    return (high << 32) | low;
}

const char *Packet::stringView(qsizetype &size)
{
    const quint32 length = u32();
    if (!valid || length > static_cast<quint32>(data.size() - position)) {
        valid = false;
        size = 0;
        return nullptr;
    }
    const char *start = data.constData() + position;
    size = static_cast<qsizetype>(length);
    position += size;
    return start;
}

QByteArray Packet::string()
{
    qsizetype size = 0;
    const char *start = stringView(size);
    return start ? QByteArray(start, size) : QByteArray();
}

FileAttrs Packet::attrs()
{
    FileAttrs result;
    result.flags = u32();
    if (result.flags & AttrSize)
        result.size = u64();
    if (result.flags & AttrUidGid) {
        u32();
        u32();
    }
    if (result.flags & AttrPermissions)
        result.permissions = u32();
    if (result.flags & AttrTimes) {
        result.atime = u32();
        result.mtime = u32();
    }
    if (result.flags & AttrExtended) {
        const quint32 count = u32();
        for (quint32 i = 0; i < count && valid; ++i) {
            qsizetype ignored = 0;
            stringView(ignored);
            stringView(ignored);
        }
    }
    return result;
}

quint32 Packet::status()
{
    const qsizetype saved = position;
    position = 5;
    const quint32 code = u32();
    position = saved;
    return valid ? code : Failure;
}

QString Packet::statusMessage()
{
    const qsizetype saved = position;
    position = 9;
    const QByteArray text = string();
    position = saved;
    valid = true;
    return QString::fromUtf8(text).trimmed();
}

Channel::~Channel()
{
    close();
}

bool Channel::open(ssh_session session, QString &error)
{
    close();
    m_broken = false;
    m_buffer.clear();
    m_offset = 0;
    m_nextId = 1;
    m_extensions.clear();

    m_channel = ssh_channel_new(session);
    if (!m_channel || ssh_channel_open_session(m_channel) != SSH_OK ||
        ssh_channel_request_sftp(m_channel) != SSH_OK) {
        error = QCoreApplication::translate(
                    "sftp", "Сервер не открыл подсистему SFTP: %1")
                    .arg(QString::fromUtf8(ssh_get_error(session)));
        close();
        return false;
    }

    Builder init(Init, 0);
    init.u32(3);
    Packet version;
    bool timedOut = false;
    if (!send(init.finish())) {
        error = m_error;
        close();
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (!takePacket(version)) {
        if (timer.elapsed() > 30000 || !fillBuffer(1000, timedOut)) {
            error = QCoreApplication::translate("sftp",
                                                "Сервер не ответил на SFTP INIT.");
            close();
            return false;
        }
    }
    if (version.type != Version) {
        error = QCoreApplication::translate("sftp",
                                            "Неожиданный ответ SFTP-сервера.");
        close();
        return false;
    }
    version.position = 5;
    while (version.valid && version.position < version.data.size()) {
        const QByteArray name = version.string();
        const QByteArray value = version.string();
        if (version.valid)
            m_extensions.insert(name, value);
    }
    m_maxRead = 64 * 1024;
    m_maxWrite = 32 * 1024;
    m_limitsLoaded = false;
    return true;
}

void Channel::ensureLimits()
{
    if (m_limitsLoaded || !m_channel || m_broken)
        return;
    m_limitsLoaded = true;
    if (!hasExtension("limits@openssh.com"))
        return;
    const quint32 id = nextId();
    Builder request(Extended, id);
    request.string(QByteArrayLiteral("limits@openssh.com"));
    Packet reply;
    if (!call(request.finish(), id, reply, 15000) || reply.type != ExtendedReply)
        return;
    const quint64 packet = reply.u64();
    const quint64 read = reply.u64();
    const quint64 write = reply.u64();
    Q_UNUSED(packet);
    if (!reply.valid)
        return;
    constexpr quint64 Cap = 255 * 1024;
    if (read > 0)
        m_maxRead = static_cast<quint32>(std::min(read, Cap));
    if (write > 0)
        m_maxWrite = static_cast<quint32>(std::min(write, Cap));
}

void Channel::close()
{
    if (!m_channel)
        return;
    if (!m_broken && ssh_channel_is_open(m_channel))
        ssh_channel_close(m_channel);
    ssh_channel_free(m_channel);
    m_channel = nullptr;
}

qint64 Channel::writeWindow() const
{
    return m_channel ? ssh_channel_window_size(m_channel) : 0;
}

bool Channel::send(const QByteArray &framed)
{
    if (!m_channel || m_broken)
        return false;
    qsizetype offset = 0;
    while (offset < framed.size()) {
        const int written = ssh_channel_write(
            m_channel, framed.constData() + offset,
            static_cast<uint32_t>(framed.size() - offset));
        if (written <= 0) {
            m_broken = true;
            m_error = QCoreApplication::translate("sftp",
                                                  "Соединение с сервером прервано.");
            return false;
        }
        offset += written;
    }
    return true;
}

bool Channel::fillBuffer(int timeoutMs, bool &timedOut)
{
    timedOut = false;
    if (!m_channel || m_broken)
        return false;
    if (m_offset > 0 && m_offset >= m_buffer.size() / 2) {
        m_buffer.remove(0, m_offset);
        m_offset = 0;
    }
    const qsizetype used = m_buffer.size();
    m_buffer.resize(used + ReadChunk);
    const int count = ssh_channel_read_timeout(
        m_channel, m_buffer.data() + used, static_cast<uint32_t>(ReadChunk), 0, timeoutMs);
    m_buffer.resize(used + std::max(count, 0));
    if (count == SSH_ERROR || (count == 0 && ssh_channel_is_eof(m_channel))) {
        m_broken = true;
        m_error = QCoreApplication::translate("sftp",
                                              "Соединение с сервером прервано.");
        return false;
    }
    timedOut = count <= 0;
    return true;
}

bool Channel::takePacket(Packet &packet)
{
    const qsizetype available = m_buffer.size() - m_offset;
    if (available < 4)
        return false;
    const quint32 length = getU32(m_buffer.constData() + m_offset);
    if (length == 0 || length > MaxPacket) {
        m_broken = true;
        m_error = QCoreApplication::translate("sftp",
                                              "Повреждённый пакет SFTP.");
        return false;
    }
    if (available < static_cast<qsizetype>(length) + 4)
        return false;
    packet.data = m_buffer.mid(m_offset + 4, length);
    m_offset += static_cast<qsizetype>(length) + 4;
    if (m_offset == m_buffer.size()) {
        m_buffer.resize(0);
        m_offset = 0;
    }
    packet.valid = true;
    packet.position = 0;
    packet.type = packet.u8();
    packet.id = packet.type == Version ? 0 : packet.u32();
    return packet.valid;
}

Channel::ReadResult Channel::read(Packet &packet, int timeoutMs)
{
    if (takePacket(packet))
        return ReadResult::Packet;
    if (m_broken)
        return ReadResult::Error;
    bool timedOut = false;
    if (!fillBuffer(timeoutMs, timedOut))
        return ReadResult::Error;
    if (takePacket(packet))
        return ReadResult::Packet;
    if (m_broken)
        return ReadResult::Error;
    return ReadResult::Timeout;
}

bool Channel::call(const QByteArray &framed, quint32 id, Packet &reply,
                   int timeoutMs)
{
    if (!send(framed))
        return false;
    QElapsedTimer timer;
    timer.start();
    forever {
        switch (read(reply, 500)) {
        case ReadResult::Packet:
            if (reply.id == id)
                return true;
            break;
        case ReadResult::Error:
            return false;
        case ReadResult::Timeout:
            if (timer.elapsed() > timeoutMs) {
                m_broken = true;
                m_error = QCoreApplication::translate(
                    "sftp", "Сервер не отвечает.");
                return false;
            }
            break;
        }
    }
}

bool Channel::batch(int count,
                    const std::function<QByteArray(int, quint32)> &make,
                    const std::function<void(int, Packet &)> &onReply,
                    int window)
{
    QHash<quint32, int> inFlight;
    int nextIndex = 0;
    QElapsedTimer idle;
    idle.start();
    while (nextIndex < count || !inFlight.isEmpty()) {
        while (nextIndex < count && inFlight.size() < window) {
            const quint32 id = nextId();
            if (!send(make(nextIndex, id)))
                return false;
            inFlight.insert(id, nextIndex++);
        }
        Packet reply;
        switch (read(reply, 500)) {
        case ReadResult::Packet: {
            idle.restart();
            const auto found = inFlight.find(reply.id);
            if (found == inFlight.end())
                break;
            const int index = found.value();
            inFlight.erase(found);
            onReply(index, reply);
            break;
        }
        case ReadResult::Error:
            return false;
        case ReadResult::Timeout:
            if (idle.elapsed() > 60000) {
                m_broken = true;
                m_error = QCoreApplication::translate("sftp",
                                                      "Сервер не отвечает.");
                return false;
            }
            break;
        }
    }
    return true;
}

QString describeStatus(quint32 code, const QString &message)
{
    QString text;
    switch (code) {
    case NoSuchFile:
        text = QCoreApplication::translate("sftp", "объект не найден");
        break;
    case PermissionDenied:
        text = QCoreApplication::translate("sftp", "недостаточно прав");
        break;
    case Unsupported:
        text = QCoreApplication::translate("sftp",
                                           "операция не поддерживается сервером");
        break;
    case NoConnection:
    case ConnectionLost:
        text = QCoreApplication::translate("sftp", "соединение потеряно");
        break;
    default:
        text = message.isEmpty()
                   ? QCoreApplication::translate("sftp", "ошибка сервера (%1)")
                         .arg(code)
                   : message;
        return text;
    }
    return text;
}

}
