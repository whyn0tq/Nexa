#pragma once

#include "types.h"

#include <QDateTime>
#include <QObject>
#include <QVector>

enum class HostKind {
    Server,
    Web,
    Game,
    Database,
    Storage,
    Desktop,
};

struct SavedHost {
    QString id;
    QString name;
    ConnectionProfile profile;
    int color = 0;
    HostKind kind = HostKind::Server;
    QDateTime lastUsed;
    QString lastPath;

    QString title() const { return name.isEmpty() ? profile.host : name; }
};

class HostStore final : public QObject {
    Q_OBJECT

public:
    static HostStore &instance();

    QVector<SavedHost> hosts() const { return m_hosts; }
    SavedHost host(const QString &id) const;
    bool contains(const QString &id) const;
    QString save(SavedHost host);
    void remove(const QString &id);
    void touch(const QString &id);
    void setLastPath(const QString &id, const QString &path);

    QStringList openSessions() const;
    void setOpenSessions(const QStringList &ids);

    static QString protectSecret(const QString &value);
    static QString unprotectSecret(const QString &value);
    static ConnectionProfile parseAddress(const QString &text, bool *ok = nullptr);

signals:
    void changed();

private:
    HostStore();
    void load();
    void persist();
    void migrateRecentProfiles();

    QVector<SavedHost> m_hosts;
};
