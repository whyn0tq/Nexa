#pragma once

#include "types.h"

#include <QString>

struct TransferPlan {
    QString destination;
    qint64 startOffset = 0;
    bool skip = false;
    bool discardPartial = false;

    struct Destination {
        bool exists = false;
        qint64 size = 0;

        qint64 partialSize = -1;
    };

    template <typename UniqueName>
    static TransferPlan make(OverwritePolicy policy, const QString &destination,
                             qint64 sourceSize, const Destination &state,
                             UniqueName uniqueName)
    {
        TransferPlan plan;
        plan.destination = destination;

        if (state.exists) {
            switch (policy) {
            case OverwritePolicy::Skip:
                plan.skip = true;
                return plan;
            case OverwritePolicy::Rename:
                plan.destination = uniqueName();
                plan.discardPartial = true;
                return plan;
            case OverwritePolicy::Resume:

                if (sourceSize >= 0 && state.size == sourceSize) {
                    plan.skip = true;
                    return plan;
                }
                break;
            case OverwritePolicy::Overwrite:
                break;
            }
        }

        if (policy == OverwritePolicy::Resume) {
            if (state.partialSize > 0 &&
                (sourceSize < 0 || state.partialSize < sourceSize))
                plan.startOffset = state.partialSize;
            else
                plan.discardPartial = state.partialSize >= 0;
            return plan;
        }

        plan.discardPartial = state.partialSize >= 0;
        return plan;
    }

    static TransferPlan makeUpload(OverwritePolicy policy,
                                   const QString &destination,
                                   qint64 sourceSize,
                                   const Destination &state,
                                   const QString &renamedDestination)
    {
        TransferPlan plan;
        plan.destination = destination;
        if (!state.exists)
            return plan;

        switch (policy) {
        case OverwritePolicy::Skip:
            plan.skip = true;
            break;
        case OverwritePolicy::Rename:
            plan.destination = renamedDestination;
            break;
        case OverwritePolicy::Resume:
            if (state.size == sourceSize)
                plan.skip = true;
            else if (state.size > 0 && state.size < sourceSize)
                plan.startOffset = state.size;
            break;
        case OverwritePolicy::Overwrite:
            break;
        }
        return plan;
    }
};
