#pragma once

#include "types.h"

#include <QObject>
#include <QString>
#include <QVector>

class TransferPrefs final : public QObject {
    Q_OBJECT

public:
    static TransferPrefs &instance();

    OverwritePolicy policy() const { return m_policy; }
    void setPolicy(OverwritePolicy policy);
    int connections() const { return m_connections; }
    void setConnections(int connections);

    static QString policyTitle(OverwritePolicy policy);
    static QString policyHint(OverwritePolicy policy);
    static QString policyShortHint(OverwritePolicy policy);
    static const QVector<int> &connectionChoices();

signals:
    void policyChanged(OverwritePolicy policy);
    void connectionsChanged(int connections);

private:
    TransferPrefs();

    OverwritePolicy m_policy = OverwritePolicy::Resume;
    int m_connections = 4;
};
