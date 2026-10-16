#pragma once

#include "types.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace ListingCache {

struct Snapshot {
    QHash<QString, RemoteEntries> listings;
    QStringList order;
};

Snapshot load(const QString &hostId);
void save(const QString &hostId, const Snapshot &snapshot);
void forget(const QString &hostId);

}
