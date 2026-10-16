#include "sftpconnection.h"

#include "transferplan.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <memory>
#include <vector>

using namespace sftp;

namespace {
constexpr int MaxActiveFiles = 64;
constexpr int MaxRequests = 384;
constexpr qint64 ReadBudget = 24LL * 1024 * 1024;
constexpr qint64 WriteBudget = 12LL * 1024 * 1024;
constexpr qint64 SegmentThreshold = 40LL * 1024 * 1024;
constexpr qint64 SegmentTarget = 24LL * 1024 * 1024;
constexpr int SilenceLimitMs = 60000;

QByteArray bytes(const QString &path)
{
    return QDir::fromNativeSeparators(path).toUtf8();
}

void setLocalTime(QFile &file, qint64 modified)
{
    if (modified > 0)
        file.setFileTime(QDateTime::fromSecsSinceEpoch(modified),
                         QFileDevice::FileModificationTime);
}
}

struct SftpConnection::Active {
    enum class Phase { Stat, Probe, Open, Data, Finishing, Done };

    FileTask task;
    Phase phase = Phase::Open;
    QByteArray handle;
    std::unique_ptr<QFile> local;
    QString destination;
    QString partial;
    qint64 rangeEnd = -1;
    qint64 next = 0;
    int inFlight = 0;
    int finishing = 0;
    QVector<QPair<qint64, qint64>> gaps;
    bool eof = false;
    bool failed = false;
    bool canceled = false;
    bool opened = false;
    int probeIndex = 2;
    QString error;
};

struct SftpConnection::Request {
    enum class Kind { Stat, Probe, Open, Read, Write, Fsetstat, Close, Setstat };
    Active *active = nullptr;
    Kind kind = Kind::Open;
    qint64 offset = 0;
    qint64 length = 0;
};

class SftpConnection::Pipeline {
public:
    Pipeline(SftpConnection &connection, TransferHost &transferHost)
        : c(connection), ch(connection.m_channel), host(transferHost)
    {
    }

    bool run();

private:
    using Phase = Active::Phase;
    using Kind = Request::Kind;

    void start(const FileTask &task);
    void startUpload(Active *a);
    void startDownload(Active *a);
    void planUpload(Active *a, bool exists, qint64 remoteSize);
    void openRemote(Active *a, quint32 flags);
    void segmentAfterOpen(Active *a);
    void pump(Active *a);
    void complete(Active *a);
    void conclude(Active *a, FileOutcome outcome, const QString &error = QString());
    void handle(Packet &reply);
    void abortAll();
    bool send(const QByteArray &packet, Active *a, Kind kind, qint64 offset = 0,
              qint64 length = 0, quint32 id = 0);
    void report(Active *a, qint64 delta)
    {
        a->task.reported += delta;
        host.progress(a->task.jobId, delta);
    }

    SftpConnection &c;
    Channel &ch;
    TransferHost &host;
    std::vector<std::unique_ptr<Active>> active;
    QHash<quint32, Request> pending;
    qint64 inFlightBytes = 0;
    QByteArray scratch;
};

bool SftpConnection::runTransfers(TransferHost &host)
{
    m_error.clear();
    if (!isOpen()) {
        fail(tr("Нет соединения."), true);
        return false;
    }
    m_channel.ensureLimits();
    Pipeline pipeline(*this, host);
    return pipeline.run();
}

bool SftpConnection::Pipeline::send(const QByteArray &packet, Active *a, Kind kind,
                                    qint64 offset, qint64 length, quint32 id)
{
    if (!ch.send(packet))
        return false;
    if (id != 0) {
        pending.insert(id, {a, kind, offset, length});
        ++a->inFlight;
        if (kind == Kind::Read || kind == Kind::Write)
            inFlightBytes += length;
    }
    return true;
}

bool SftpConnection::Pipeline::run()
{
    QElapsedTimer silence;
    silence.start();

    forever {
        if (host.stopping()) {
            abortAll();
            return true;
        }

        const qint64 budget = ReadBudget;
        WorkItem item;
        while (static_cast<int>(active.size()) < MaxActiveFiles &&
               inFlightBytes < budget && !ch.broken()) {
            const bool idle = active.empty() && pending.isEmpty();
            if (!host.take(item, idle))
                break;
            if (item.kind == WorkItem::Kind::Expand) {
                if (!c.expand(item.job, host) && c.m_broken) {
                    abortAll();
                    return false;
                }
                continue;
            }
            start(item.file);
        }

        for (auto &a : active) {
            if (!a->failed && !a->canceled && host.canceled(a->task.jobId))
                a->canceled = true;
            if (a->phase == Phase::Data)
                pump(a.get());
            if ((a->failed || a->canceled) && a->phase != Phase::Done &&
                a->inFlight == 0)
                conclude(a.get(), a->canceled ? FileOutcome::Canceled
                                              : FileOutcome::Failed,
                         a->error);
        }
        if (ch.broken()) {
            abortAll();
            c.fail(ch.lastError(), true);
            return false;
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [](const std::unique_ptr<Active> &a) {
                                        return a->phase == Phase::Done &&
                                               a->inFlight == 0;
                                    }),
                     active.end());

        if (active.empty() && pending.isEmpty()) {
            if (!host.take(item, true))
                return true;
            if (item.kind == WorkItem::Kind::Expand) {
                if (!c.expand(item.job, host) && c.m_broken) {
                    abortAll();
                    return false;
                }
            } else {
                start(item.file);
            }
            continue;
        }
        if (pending.isEmpty())
            continue;

        Packet reply;
        switch (ch.read(reply, 200)) {
        case Channel::ReadResult::Packet:
            silence.restart();
            handle(reply);
            break;
        case Channel::ReadResult::Error:
            abortAll();
            c.fail(ch.lastError(), true);
            return false;
        case Channel::ReadResult::Timeout:
            if (silence.elapsed() > SilenceLimitMs) {
                abortAll();
                c.fail(tr("Сервер перестал отвечать во время передачи."), true);
                return false;
            }
            break;
        }
    }
}

void SftpConnection::Pipeline::abortAll()
{
    QVector<FileTask> retry;
    for (auto &a : active) {
        if (a->phase == Phase::Done)
            continue;
        if (a->local)
            a->local->close();
        if (host.stopping())
            continue;
        if (a->phase == Phase::Finishing && a->task.isSegment() &&
            a->task.group->remaining.load() == 0) {
            host.finished(a->task, FileOutcome::Completed, true, QString());
            continue;
        }
        if (a->canceled) {
            host.finished(a->task, FileOutcome::Canceled, !a->task.isSegment(),
                          QString());
            continue;
        }
        FileTask task = a->task;
        host.progress(task.jobId, -task.reported);
        task.reported = 0;
        task.attempt += 1;
        task.restarted = task.restarted || a->opened;
        if (task.isSegment())
            task.restarted = false;
        retry.append(task);
    }
    active.clear();
    pending.clear();
    inFlightBytes = 0;
    if (!retry.isEmpty())
        host.pushFront(retry);
}

void SftpConnection::Pipeline::start(const FileTask &task)
{
    auto owned = std::make_unique<Active>();
    Active *a = owned.get();
    a->task = task;
    active.push_back(std::move(owned));
    host.current(task.jobId, task.upload ? task.localPath : task.remotePath);
    if (host.canceled(task.jobId)) {
        conclude(a, FileOutcome::Canceled);
        return;
    }
    if (task.upload)
        startUpload(a);
    else
        startDownload(a);
}

void SftpConnection::Pipeline::startUpload(Active *a)
{
    a->local = std::make_unique<QFile>(a->task.localPath);
    if (!a->local->open(QIODevice::ReadOnly)) {
        conclude(a, FileOutcome::Failed,
                 tr("Не удалось открыть «%1»: %2")
                     .arg(a->task.localPath, a->local->errorString()));
        return;
    }
    if (a->task.isSegment()) {
        a->destination = a->task.group->remoteDestination;
        a->next = a->task.segmentStart;
        a->rangeEnd = a->task.segmentEnd;
        openRemote(a, OpenWrite);
        return;
    }
    a->destination = a->task.remotePath;
    a->rangeEnd = a->task.size;
    if (a->task.remoteAbsent && !a->task.restarted) {
        planUpload(a, false, 0);
        return;
    }
    a->phase = Phase::Stat;
    const quint32 id = ch.nextId();
    Builder builder(Stat, id);
    builder.string(bytes(a->destination));
    send(builder.finish(), a, Kind::Stat, 0, 0, id);
}

void SftpConnection::Pipeline::planUpload(Active *a, bool exists, qint64 remoteSize)
{
    if (a->task.restarted) {
        if (exists && remoteSize >= a->task.size) {
            a->next = a->task.size;
            report(a, a->next);
            openRemote(a, OpenWrite);
            return;
        }
        a->next = exists ? remoteSize : 0;
        report(a, a->next);
        openRemote(a, OpenWrite | OpenCreate | (a->next == 0 ? quint32(OpenTruncate) : 0u));
        return;
    }

    const auto policy = a->task.overwrite ? OverwritePolicy::Overwrite : host.policy();
    if (exists && policy == OverwritePolicy::Rename) {
        a->phase = Phase::Probe;
        const quint32 id = ch.nextId();
        Builder builder(Stat, id);
        builder.string(bytes(c.numberedVariant(a->task.remotePath, a->probeIndex)));
        send(builder.finish(), a, Kind::Probe, 0, 0, id);
        return;
    }
    TransferPlan::Destination state;
    state.exists = exists;
    state.size = remoteSize;
    const TransferPlan plan = TransferPlan::makeUpload(
        policy, a->task.remotePath, a->task.size, state, a->task.remotePath);
    if (plan.skip) {
        report(a, std::max<qint64>(a->task.size, 0));
        conclude(a, FileOutcome::Skipped);
        return;
    }
    a->destination = plan.destination;
    a->next = plan.startOffset;
    report(a, a->next);
    openRemote(a, OpenWrite | OpenCreate | (a->next == 0 ? quint32(OpenTruncate) : 0u));
}

void SftpConnection::Pipeline::startDownload(Active *a)
{
    FileTask &task = a->task;
    if (task.isSegment()) {
        a->partial = task.group->partialLocal;
        a->destination = task.group->finalLocal;
        a->next = task.segmentStart;
        a->rangeEnd = task.segmentEnd;
        a->local = std::make_unique<QFile>(a->partial);
        if (!a->local->open(QIODevice::ReadWrite)) {
            conclude(a, FileOutcome::Failed,
                     tr("Не удалось открыть «%1»: %2")
                         .arg(a->partial, a->local->errorString()));
            return;
        }
        openRemote(a, OpenRead);
        return;
    }

    QString destination = task.localPath;
    qint64 startOffset = 0;
    bool discardPartial = false;
    if (task.restarted) {
        const QFileInfo partial(task.localPath + c.partialSuffix());
        startOffset = partial.exists() ? partial.size() : 0;
        if (task.size >= 0 && startOffset > task.size)
            startOffset = 0;
    } else {
        const QFileInfo existing(task.localPath);
        const QFileInfo partial(task.localPath + c.partialSuffix());
        TransferPlan::Destination state;
        state.exists = existing.exists();
        state.size = state.exists ? existing.size() : 0;
        state.partialSize = partial.exists() ? partial.size() : -1;
        const TransferPlan plan = TransferPlan::make(
            task.overwrite ? OverwritePolicy::Overwrite : host.policy(), task.localPath, task.size, state, [&task] {
                for (int index = 2; index < 1000; ++index) {
                    const QString candidate =
                        RemoteConnection::numberedVariant(task.localPath, index);
                    if (!QFileInfo::exists(candidate))
                        return candidate;
                }
                return task.localPath;
            });
        if (plan.skip) {
            report(a, std::max<qint64>(task.size, 0));
            conclude(a, FileOutcome::Skipped);
            return;
        }
        destination = plan.destination;
        startOffset = plan.startOffset;
        discardPartial = plan.discardPartial;
    }

    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        conclude(a, FileOutcome::Failed,
                 tr("Не удалось создать папку для «%1».").arg(destination));
        return;
    }
    a->destination = destination;
    a->rangeEnd = task.size;

    const int parallel = host.parallelism();
    if (startOffset == 0 && task.size >= SegmentThreshold && parallel > 1) {
        QFile::remove(destination + c.partialSuffix());
        const int count = static_cast<int>(std::clamp<qint64>(
            task.size / SegmentTarget, 2, std::min(parallel, 8)));
        auto group = std::make_shared<SegmentGroup>();
        group->partialLocal = destination + c.segmentSuffix();
        group->finalLocal = destination;
        group->size = task.size;
        group->modified = task.modified;
        group->permissions = task.permissions;
        group->remaining = count;
        QFile partial(group->partialLocal);
        if (!partial.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            !partial.resize(task.size)) {
            conclude(a, FileOutcome::Failed,
                     tr("Не удалось создать «%1»: %2")
                         .arg(group->partialLocal, partial.errorString()));
            return;
        }
        partial.close();
        const qint64 span = task.size / count;
        QVector<FileTask> segments;
        for (int i = 1; i < count; ++i) {
            FileTask segment = task;
            segment.group = group;
            segment.segmentStart = span * i;
            segment.segmentEnd = i == count - 1 ? task.size : span * (i + 1);
            segment.reported = 0;
            segments.append(segment);
        }
        task.group = group;
        task.segmentStart = 0;
        task.segmentEnd = span;
        host.addUnits(task.jobId, count - 1);
        host.pushFront(segments);
        startDownload(a);
        return;
    }

    a->partial = destination + c.partialSuffix();
    if (discardPartial || startOffset == 0)
        QFile::remove(a->partial);
    a->local = std::make_unique<QFile>(a->partial);
    const QIODevice::OpenMode mode =
        startOffset > 0 ? QIODevice::ReadWrite
                        : (QIODevice::ReadWrite | QIODevice::Truncate);
    if (!a->local->open(mode)) {
        conclude(a, FileOutcome::Failed,
                 tr("Не удалось создать «%1»: %2")
                     .arg(a->partial, a->local->errorString()));
        return;
    }
    a->opened = true;
    a->next = startOffset;
    report(a, startOffset);
    openRemote(a, OpenRead);
}

void SftpConnection::Pipeline::openRemote(Active *a, quint32 flags)
{
    a->phase = Phase::Open;
    const quint32 id = ch.nextId();
    Builder builder(Open, id);
    FileAttrs attrs;
    if (flags & OpenCreate) {
        attrs.flags = AttrPermissions;
        attrs.permissions = a->task.permissions ? (a->task.permissions & 07777) : 0644;
    }
    builder.string(bytes(a->task.upload ? a->destination : a->task.remotePath))
        .u32(flags)
        .attrs(attrs);
    send(builder.finish(), a, Kind::Open, 0, 0, id);
}

void SftpConnection::Pipeline::segmentAfterOpen(Active *a)
{
    FileTask &task = a->task;
    const int parallel = host.parallelism();
    if (task.isSegment() || a->next != 0 || task.size < SegmentThreshold ||
        parallel < 2)
        return;
    const int count = static_cast<int>(
        std::clamp<qint64>(task.size / SegmentTarget, 2, std::min(parallel, 8)));
    auto group = std::make_shared<SegmentGroup>();
    group->remoteDestination = a->destination;
    group->size = task.size;
    group->modified = task.modified;
    group->permissions = task.permissions;
    group->remaining = count;
    const qint64 span = task.size / count;
    QVector<FileTask> segments;
    for (int i = 1; i < count; ++i) {
        FileTask segment = task;
        segment.group = group;
        segment.segmentStart = span * i;
        segment.segmentEnd = i == count - 1 ? task.size : span * (i + 1);
        segment.reported = 0;
        segments.append(segment);
    }
    task.group = group;
    task.segmentStart = 0;
    task.segmentEnd = span;
    a->rangeEnd = span;
    host.addUnits(task.jobId, count - 1);
    host.pushFront(segments);
}

void SftpConnection::Pipeline::pump(Active *a)
{
    if (a->failed || a->canceled)
        return;
    const quint32 chunk = a->task.upload ? ch.maxWrite() : ch.maxRead();
    while (pending.size() < MaxRequests) {
        if (a->task.upload) {
            if (inFlightBytes >= WriteBudget || a->next >= a->rangeEnd)
                break;
            const qint64 length = std::min<qint64>(chunk, a->rangeEnd - a->next);
            if (a->local->pos() != a->next && !a->local->seek(a->next)) {
                a->failed = true;
                a->error = tr("Не удалось прочитать «%1».").arg(a->task.localPath);
                return;
            }
            const quint32 id = ch.nextId();
            Builder builder(Write, id, length + a->handle.size() + 32);
            builder.string(a->handle).u64(static_cast<quint64>(a->next));
            char *target = builder.stringSpace(length);
            const qint64 got = a->local->read(target, length);
            if (got != length) {
                if (got <= 0) {
                    a->rangeEnd = a->next;
                    break;
                }
                a->failed = true;
                a->error = tr("Файл «%1» изменился во время загрузки.")
                               .arg(a->task.localPath);
                return;
            }
            if (!send(builder.finish(), a, Kind::Write, a->next, length, id))
                return;
            a->next += length;
        } else {
            if (inFlightBytes >= ReadBudget)
                break;
            qint64 offset = 0;
            qint64 length = 0;
            if (!a->gaps.isEmpty()) {
                offset = a->gaps.first().first;
                length = std::min<qint64>(chunk, a->gaps.first().second);
                if (length == a->gaps.first().second)
                    a->gaps.removeFirst();
                else
                    a->gaps.first() = {offset + length, a->gaps.first().second - length};
            } else if (a->rangeEnd >= 0) {
                if (a->next >= a->rangeEnd)
                    break;
                offset = a->next;
                length = std::min<qint64>(chunk, a->rangeEnd - a->next);
                a->next += length;
            } else {
                if (a->eof || a->inFlight >= 8)
                    break;
                offset = a->next;
                length = chunk;
                a->next += length;
            }
            const quint32 id = ch.nextId();
            Builder builder(Read, id);
            builder.string(a->handle)
                .u64(static_cast<quint64>(offset))
                .u32(static_cast<quint32>(length));
            if (!send(builder.finish(), a, Kind::Read, offset, length, id))
                return;
        }
    }

    const bool upload = a->task.upload;
    const bool drained =
        a->inFlight == 0 &&
        (upload ? a->next >= a->rangeEnd
                : a->gaps.isEmpty() &&
                      (a->eof || (a->rangeEnd >= 0 && a->next >= a->rangeEnd)));
    if (drained)
        complete(a);
}

void SftpConnection::Pipeline::complete(Active *a)
{
    a->phase = Phase::Finishing;
    const bool segment = a->task.isSegment();
    if (a->task.upload && !segment && a->task.modified > 0) {
        const quint32 id = ch.nextId();
        Builder builder(Fsetstat, id);
        FileAttrs attrs;
        attrs.flags = AttrTimes;
        attrs.atime = attrs.mtime = static_cast<quint32>(a->task.modified);
        builder.string(a->handle).attrs(attrs);
        send(builder.finish(), a, Kind::Fsetstat, 0, 0, id);
    }
    const quint32 id = ch.nextId();
    Builder builder(Close, id);
    builder.string(a->handle);
    send(builder.finish(), a, Kind::Close, 0, 0, id);
}

void SftpConnection::Pipeline::conclude(Active *a, FileOutcome outcome,
                                        const QString &error)
{
    if (a->phase == Phase::Done)
        return;
    a->phase = Phase::Done;
    if (a->inFlight == 0 && !a->handle.isEmpty() && outcome != FileOutcome::Completed) {
        Builder builder(Close, ch.nextId());
        builder.string(a->handle);
        ch.send(builder.finish());
    }
    if (a->local)
        a->local->close();

    if (!a->task.isSegment()) {
        host.finished(a->task, outcome, true, error);
        return;
    }
    SegmentGroup &group = *a->task.group;
    if (outcome != FileOutcome::Completed) {
        group.failed = true;
        QMutexLocker locker(&group.errorMutex);
        if (group.error.isEmpty())
            group.error = error.isEmpty() && outcome == FileOutcome::Canceled
                              ? tr("Отменено.")
                              : error;
    }
    if (group.remaining.fetch_sub(1) != 1) {
        host.finished(a->task, FileOutcome::Completed, false, QString());
        return;
    }
    if (group.failed) {
        QMutexLocker locker(&group.errorMutex);
        host.finished(a->task,
                      outcome == FileOutcome::Canceled ? FileOutcome::Canceled
                                                       : FileOutcome::Failed,
                      true, group.error);
        return;
    }
    if (a->task.upload) {
        if (group.modified > 0) {
            a->phase = Phase::Finishing;
            const quint32 id = ch.nextId();
            Builder builder(Setstat, id);
            FileAttrs attrs;
            attrs.flags = AttrTimes;
            attrs.atime = attrs.mtime = static_cast<quint32>(group.modified);
            builder.string(bytes(group.remoteDestination)).attrs(attrs);
            send(builder.finish(), a, Kind::Setstat, 0, 0, id);
            return;
        }
        host.finished(a->task, FileOutcome::Completed, true, QString());
        return;
    }
    QFile::remove(group.finalLocal);
    if (!QFile::rename(group.partialLocal, group.finalLocal)) {
        host.finished(a->task, FileOutcome::Failed, true,
                      tr("Не удалось сохранить «%1».").arg(group.finalLocal));
        return;
    }
    QFile done(group.finalLocal);
    if (done.open(QIODevice::ReadWrite | QIODevice::Append)) {
        setLocalTime(done, group.modified);
        done.close();
    }
    host.finished(a->task, FileOutcome::Completed, true, QString());
}

void SftpConnection::Pipeline::handle(Packet &reply)
{
    const auto found = pending.find(reply.id);
    if (found == pending.end())
        return;
    const Request request = found.value();
    pending.erase(found);
    Active *a = request.active;
    --a->inFlight;
    if (request.kind == Kind::Read || request.kind == Kind::Write)
        inFlightBytes -= request.length;

    switch (request.kind) {
    case Kind::Stat:
        if (a->canceled)
            break;
        if (reply.type == Attrs) {
            const FileAttrs attrs = reply.attrs();
            planUpload(a, true, static_cast<qint64>(attrs.size));
        } else {
            planUpload(a, false, 0);
        }
        break;
    case Kind::Probe:
        if (a->canceled)
            break;
        if (reply.type == Attrs && a->probeIndex < 1000) {
            ++a->probeIndex;
            const quint32 id = ch.nextId();
            Builder builder(Stat, id);
            builder.string(
                bytes(c.numberedVariant(a->task.remotePath, a->probeIndex)));
            send(builder.finish(), a, Kind::Probe, 0, 0, id);
        } else {
            a->destination = c.numberedVariant(a->task.remotePath, a->probeIndex);
            a->next = 0;
            openRemote(a, OpenWrite | OpenCreate | OpenTruncate);
        }
        break;
    case Kind::Open:
        if (reply.type != Handle) {
            const QString target = a->task.upload ? a->destination : a->task.remotePath;
            a->failed = true;
            a->error = tr("Не удалось открыть «%1»: %2")
                           .arg(target, describeStatus(reply.status(),
                                                       reply.statusMessage()));
            break;
        }
        a->handle = reply.string();
        if (a->task.upload)
            a->opened = true;
        if (a->canceled)
            break;
        a->phase = Phase::Data;
        if (a->task.upload)
            segmentAfterOpen(a);
        pump(a);
        break;
    case Kind::Write: {
        const quint32 status = reply.status();
        if (status != Ok) {
            if (!a->failed) {
                a->failed = true;
                a->error = tr("Сервер не принял данные «%1»: %2")
                               .arg(a->destination,
                                    describeStatus(status, reply.statusMessage()));
            }
            break;
        }
        report(a, request.length);
        if (!a->failed && !a->canceled)
            pump(a);
        break;
    }
    case Kind::Read: {
        if (reply.type == Data) {
            qsizetype size = 0;
            const char *data = reply.stringView(size);
            if (!data && size != 0) {
                a->failed = true;
                a->error = tr("Повреждённый ответ сервера.");
                break;
            }
            if (size > 0) {
                if (a->local->pos() != request.offset)
                    a->local->seek(request.offset);
                if (a->local->write(data, size) != size) {
                    a->failed = true;
                    a->error = tr("Не удалось записать «%1»: %2")
                                   .arg(a->local->fileName(), a->local->errorString());
                    break;
                }
                report(a, size);
            }
            if (size < request.length)
                a->gaps.append({request.offset + size, request.length - size});
        } else if (reply.status() == Eof) {
            a->eof = true;
            a->gaps.erase(std::remove_if(a->gaps.begin(), a->gaps.end(),
                                         [&](const QPair<qint64, qint64> &gap) {
                                             return gap.first >= request.offset;
                                         }),
                          a->gaps.end());
            if (a->rangeEnd < 0 || request.offset < a->rangeEnd)
                a->rangeEnd = std::min(a->rangeEnd < 0 ? request.offset : a->rangeEnd,
                                       request.offset);
        } else {
            a->failed = true;
            a->error = tr("Ошибка чтения «%1»: %2")
                           .arg(a->task.remotePath,
                                describeStatus(reply.status(), reply.statusMessage()));
            break;
        }
        if (!a->failed && !a->canceled)
            pump(a);
        break;
    }
    case Kind::Fsetstat:
        break;
    case Kind::Close: {
        if (a->task.upload) {
            if (reply.status() != Ok && !a->failed) {
                a->failed = true;
                a->error = tr("Сервер не сохранил «%1»: %2")
                               .arg(a->destination,
                                    describeStatus(reply.status(), reply.statusMessage()));
            }
            if (a->inFlight > 0)
                break;
            if (a->failed) {
                conclude(a, FileOutcome::Failed, a->error);
                break;
            }
            conclude(a, FileOutcome::Completed);
            break;
        }
        if (a->task.isSegment()) {
            conclude(a, FileOutcome::Completed);
            break;
        }
        setLocalTime(*a->local, a->task.modified);
        a->local->close();
        QFile::remove(a->destination);
        if (!QFile::rename(a->partial, a->destination)) {
            conclude(a, FileOutcome::Failed,
                     tr("Не удалось сохранить «%1».").arg(a->destination));
            break;
        }
        if (a->task.permissions & 0100) {
            QFile::setPermissions(a->destination,
                                  QFile::permissions(a->destination) |
                                      QFileDevice::ExeOwner | QFileDevice::ExeUser);
        }
        conclude(a, FileOutcome::Completed);
        break;
    }
    case Kind::Setstat:
        a->phase = Phase::Finishing;
        host.finished(a->task, FileOutcome::Completed, true, QString());
        a->phase = Phase::Done;
        break;
    }
}
