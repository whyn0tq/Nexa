#include "connection.h"
#include "sftpchannel.h"

#include <QString>

#include <cstdio>
#include <cstring>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

sftp::Packet parse(const QByteArray &framed)
{
    sftp::Packet packet;
    packet.data = framed.mid(4);
    packet.type = packet.u8();
    packet.id = packet.u32();
    return packet;
}

void framing()
{
    sftp::Builder builder(sftp::Open, 7);
    sftp::FileAttrs attrs;
    attrs.flags = sftp::AttrPermissions | sftp::AttrTimes;
    attrs.permissions = 0644;
    attrs.mtime = 1700000000;
    builder.string(QByteArrayLiteral("/srv/файл.txt"))
        .u32(sftp::OpenWrite | sftp::OpenCreate)
        .attrs(attrs);
    const QByteArray framed = builder.finish();

    const quint32 length = (quint32(quint8(framed[0])) << 24) |
                           (quint32(quint8(framed[1])) << 16) |
                           (quint32(quint8(framed[2])) << 8) | quint8(framed[3]);
    check(length == quint32(framed.size() - 4), "length prefix covers the payload");

    sftp::Packet packet = parse(framed);
    check(packet.type == sftp::Open, "type survives");
    check(packet.id == 7, "request id survives");
    check(packet.string() == QByteArrayLiteral("/srv/файл.txt"), "UTF-8 path survives");
    check(packet.u32() == (sftp::OpenWrite | sftp::OpenCreate), "open flags survive");
    const sftp::FileAttrs decoded = packet.attrs();
    check(decoded.permissions == 0644, "permissions survive");
    check(decoded.mtime == 1700000000, "mtime survives");
    check(packet.valid && packet.position == packet.data.size(), "nothing left over");
}

void writePayloadInPlace()
{
    sftp::Builder builder(sftp::Write, 42, 1024);
    builder.string(QByteArrayLiteral("H")).u64(0x0102030405060708ull);
    char *target = builder.stringSpace(5);
    std::memcpy(target, "hello", 5);
    sftp::Packet packet = parse(builder.finish());
    check(packet.string() == QByteArrayLiteral("H"), "handle");
    check(packet.u64() == 0x0102030405060708ull, "64-bit offset");
    qsizetype size = 0;
    const char *data = packet.stringView(size);
    check(size == 5 && data && QByteArray(data, size) == "hello", "in-place payload");
}

void truncatedPacketsAreRejected()
{
    sftp::Packet packet;
    packet.data = QByteArray("\x65\x00\x00", 3);
    packet.type = packet.u8();
    packet.u32();
    check(!packet.valid, "short packet is flagged invalid");

    sftp::Packet text;
    text.data = QByteArray::fromHex("6800000001000000ff");
    text.type = text.u8();
    text.id = text.u32();
    qsizetype size = 0;
    check(text.stringView(size) == nullptr && !text.valid,
          "string longer than the packet is rejected");
}

void remotePaths()
{
    check(RemoteConnection::normalized(QStringLiteral("game//csgo/")) ==
              QStringLiteral("/game/csgo"),
          "paths are normalized");
    check(RemoteConnection::parentPath(QStringLiteral("/a/b")) == QStringLiteral("/a"),
          "parent of nested path");
    check(RemoteConnection::parentPath(QStringLiteral("/a")) == QStringLiteral("/"),
          "parent of top-level path");
    check(RemoteConnection::join(QStringLiteral("/"), QStringLiteral("x")) ==
              QStringLiteral("/x"),
          "join at root");
    check(RemoteConnection::numberedVariant(QStringLiteral("/d/report.txt"), 2) ==
              QStringLiteral("/d/report (2).txt"),
          "numbered variant keeps the suffix");
}
}

int main()
{
    framing();
    writePayloadInPlace();
    truncatedPacketsAreRejected();
    remotePaths();
    if (failures == 0)
        std::printf("sftp codec: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
