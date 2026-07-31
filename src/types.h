#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

struct RemoteEntry {
    QString name;
    QString path;
    qint64 size = 0;
    QDateTime modified;
    bool directory = false;
    bool symlink = false;
};

using RemoteEntries = QVector<RemoteEntry>;

struct ConnectionProfile {
    QString host;
    quint16 port = 22;
    QString username;
    QString password;
    QString privateKeyPath;
    QString keyPassphrase;
    bool usePrivateKey = false;
};

Q_DECLARE_METATYPE(RemoteEntry)
Q_DECLARE_METATYPE(RemoteEntries)
Q_DECLARE_METATYPE(ConnectionProfile)
