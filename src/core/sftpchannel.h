#pragma once

#include "types.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <functional>

struct ssh_session_struct;
struct ssh_channel_struct;
typedef struct ssh_session_struct *ssh_session;
typedef struct ssh_channel_struct *ssh_channel;

namespace sftp {

enum Type : quint8 {
    Init = 1,
    Version = 2,
    Open = 3,
    Close = 4,
    Read = 5,
    Write = 6,
    Lstat = 7,
    Fstat = 8,
    Setstat = 9,
    Fsetstat = 10,
    Opendir = 11,
    Readdir = 12,
    Remove = 13,
    Mkdir = 14,
    Rmdir = 15,
    Realpath = 16,
    Stat = 17,
    Rename = 18,
    Status = 101,
    Handle = 102,
    Data = 103,
    Name = 104,
    Attrs = 105,
    Extended = 200,
    ExtendedReply = 201
};

enum StatusCode : quint32 {
    Ok = 0,
    Eof = 1,
    NoSuchFile = 2,
    PermissionDenied = 3,
    Failure = 4,
    BadMessage = 5,
    NoConnection = 6,
    ConnectionLost = 7,
    Unsupported = 8
};

enum OpenFlag : quint32 {
    OpenRead = 0x01,
    OpenWrite = 0x02,
    OpenAppend = 0x04,
    OpenCreate = 0x08,
    OpenTruncate = 0x10,
    OpenExclusive = 0x20
};

enum AttrFlag : quint32 {
    AttrSize = 0x01,
    AttrUidGid = 0x02,
    AttrPermissions = 0x04,
    AttrTimes = 0x08,
    AttrExtended = 0x80000000u
};

constexpr quint32 TypeMask = 0170000;
constexpr quint32 TypeDirectory = 0040000;
constexpr quint32 TypeLink = 0120000;

struct FileAttrs {
    quint32 flags = 0;
    quint64 size = 0;
    quint32 permissions = 0;
    quint32 atime = 0;
    quint32 mtime = 0;

    bool isDirectory() const
    {
        return (flags & AttrPermissions) &&
               (permissions & TypeMask) == TypeDirectory;
    }
    bool isLink() const
    {
        return (flags & AttrPermissions) && (permissions & TypeMask) == TypeLink;
    }
};

class Builder {
public:
    Builder(Type type, quint32 id, qsizetype reserve = 64);
    Builder &u8(quint8 value);
    Builder &u32(quint32 value);
    Builder &u64(quint64 value);
    Builder &string(const QByteArray &value);
    Builder &string(const char *data, qsizetype size);
    Builder &attrs(const FileAttrs &value);
    char *stringSpace(qsizetype size);
    QByteArray finish();

private:
    QByteArray m_data;
};

class Packet {
public:
    quint8 type = 0;
    quint32 id = 0;
    QByteArray data;
    qsizetype position = 0;
    bool valid = true;

    quint8 u8();
    quint32 u32();
    quint64 u64();
    QByteArray string();
    const char *stringView(qsizetype &size);
    FileAttrs attrs();

    quint32 status();
    QString statusMessage();
};

class Channel {
public:
    Channel() = default;
    ~Channel();
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    bool open(ssh_session session, QString &error);
    void close();
    bool isOpen() const { return m_channel != nullptr && !m_broken; }
    bool broken() const { return m_broken; }

    quint32 nextId() { return m_nextId++; }
    bool send(const QByteArray &framed);

    enum class ReadResult { Packet, Timeout, Error };
    ReadResult read(Packet &packet, int timeoutMs);

    bool call(const QByteArray &framed, quint32 id, Packet &reply,
              int timeoutMs = 45000);

    bool batch(int count, const std::function<QByteArray(int, quint32)> &make,
               const std::function<void(int, Packet &)> &onReply,
               int window = 64);

    bool hasExtension(const QByteArray &name) const
    {
        return m_extensions.contains(name);
    }

    void ensureLimits();

    quint32 maxRead() const { return m_maxRead; }
    quint32 maxWrite() const { return m_maxWrite; }
    QString lastError() const { return m_error; }

private:
    bool fillBuffer(int timeoutMs, bool &timedOut);
    bool takePacket(Packet &packet);
    bool m_limitsLoaded = false;

    ssh_channel m_channel = nullptr;
    QByteArray m_buffer;
    qsizetype m_offset = 0;
    QByteArray m_scratch;
    quint32 m_nextId = 1;
    quint32 m_maxRead = 64 * 1024;
    quint32 m_maxWrite = 32 * 1024;
    QHash<QByteArray, QByteArray> m_extensions;
    QString m_error;
    bool m_broken = false;
};

QString describeStatus(quint32 code, const QString &message);

}
