#include "transferplan.h"

#include <QString>

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

const QString Destination = QStringLiteral("/local/report.txt");
const QString Renamed = QStringLiteral("/local/report (2).txt");

TransferPlan download(OverwritePolicy policy, qint64 sourceSize,
                      const TransferPlan::Destination &state)
{
    return TransferPlan::make(policy, Destination, sourceSize, state,
                              [] { return Renamed; });
}

TransferPlan::Destination state(bool exists, qint64 size, qint64 partial)
{
    TransferPlan::Destination value;
    value.exists = exists;
    value.size = size;
    value.partialSize = partial;
    return value;
}

void testDownloadFreshDestination()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, 1000, state(false, 0, -1));
    check(!plan.skip, "fresh download is not skipped");
    check(plan.startOffset == 0, "fresh download starts at zero");
    check(plan.destination == Destination, "fresh download keeps its name");
    check(!plan.discardPartial, "nothing to discard when no partial exists");
}

void testDownloadResumesPartial()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, 1000, state(false, 0, 400));
    check(plan.startOffset == 400, "resume continues from the partial size");
    check(!plan.discardPartial, "a usable partial is kept");
}

void testDownloadDiscardsOversizedPartial()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, 1000, state(false, 0, 1000));
    check(plan.startOffset == 0, "an oversized partial restarts the transfer");
    check(plan.discardPartial, "an unusable partial is discarded");
}

void testDownloadResumesUnknownSize()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, -1, state(false, 0, 400));
    check(plan.startOffset == 400, "unknown source size still resumes");
}

void testDownloadSkipsCompleteFile()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, 1000, state(true, 1000, -1));
    check(plan.skip, "an identically sized destination is skipped");
}

void testDownloadRetriesTruncatedFile()
{
    const TransferPlan plan =
        download(OverwritePolicy::Resume, 1000, state(true, 700, -1));
    check(!plan.skip, "a differently sized destination is transferred again");
    check(plan.startOffset == 0,
          "without a partial the retry starts from the beginning");
}

void testDownloadSkipPolicy()
{
    check(download(OverwritePolicy::Skip, 1000, state(true, 1, -1)).skip,
          "skip policy skips any existing destination");
    check(!download(OverwritePolicy::Skip, 1000, state(false, 0, -1)).skip,
          "skip policy still writes a missing destination");
}

void testDownloadRenamePolicy()
{
    const TransferPlan plan =
        download(OverwritePolicy::Rename, 1000, state(true, 1000, 400));
    check(plan.destination == Renamed, "rename policy picks a free name");
    check(plan.startOffset == 0, "rename policy starts from the beginning");
    check(plan.discardPartial, "rename policy drops the old partial");
}

void testDownloadOverwritePolicy()
{
    const TransferPlan plan =
        download(OverwritePolicy::Overwrite, 1000, state(true, 1000, 400));
    check(!plan.skip, "overwrite policy never skips");
    check(plan.startOffset == 0, "overwrite policy restarts from zero");
    check(plan.discardPartial, "overwrite policy drops the old partial");
}

void testUploadPolicies()
{
    const QString remote = QStringLiteral("/remote/report.txt");
    const QString renamed = QStringLiteral("/remote/report (2).txt");

    const TransferPlan fresh = TransferPlan::makeUpload(
        OverwritePolicy::Resume, remote, 1000, state(false, 0, -1), renamed);
    check(!fresh.skip && fresh.startOffset == 0, "fresh upload starts at zero");

    const TransferPlan complete = TransferPlan::makeUpload(
        OverwritePolicy::Resume, remote, 1000, state(true, 1000, -1), renamed);
    check(complete.skip, "an upload matching the remote size is skipped");

    const TransferPlan resumed = TransferPlan::makeUpload(
        OverwritePolicy::Resume, remote, 1000, state(true, 250, -1), renamed);
    check(resumed.startOffset == 250, "a short remote file is resumed");

    const TransferPlan larger = TransferPlan::makeUpload(
        OverwritePolicy::Resume, remote, 1000, state(true, 4000, -1), renamed);
    check(larger.startOffset == 0,
          "a remote file larger than the source is rewritten");

    const TransferPlan renamedPlan = TransferPlan::makeUpload(
        OverwritePolicy::Rename, remote, 1000, state(true, 10, -1), renamed);
    check(renamedPlan.destination == renamed, "rename policy retargets uploads");

    const TransferPlan skipped = TransferPlan::makeUpload(
        OverwritePolicy::Skip, remote, 1000, state(true, 10, -1), renamed);
    check(skipped.skip, "skip policy leaves the remote file untouched");
}
}

int main()
{
    testDownloadFreshDestination();
    testDownloadResumesPartial();
    testDownloadDiscardsOversizedPartial();
    testDownloadResumesUnknownSize();
    testDownloadSkipsCompleteFile();
    testDownloadRetriesTruncatedFile();
    testDownloadSkipPolicy();
    testDownloadRenamePolicy();
    testDownloadOverwritePolicy();
    testUploadPolicies();

    if (failures == 0)
        std::fprintf(stderr, "all transfer plan checks passed\n");
    return failures == 0 ? 0 : 1;
}
