#include "transferprefs.h"

#include <QSettings>

#include <algorithm>

TransferPrefs &TransferPrefs::instance()
{
    static TransferPrefs prefs;
    return prefs;
}

TransferPrefs::TransferPrefs()
{
    QSettings settings;
    const int policy = settings.value(QStringLiteral("transfers/policy"), 0).toInt();
    m_policy = static_cast<OverwritePolicy>(std::clamp(policy, 0, 3));
    m_connections =
        std::clamp(settings.value(QStringLiteral("transfers/connections"), 4).toInt(), 1, 16);
}

void TransferPrefs::setPolicy(OverwritePolicy policy)
{
    if (policy == m_policy)
        return;
    m_policy = policy;
    QSettings().setValue(QStringLiteral("transfers/policy"), static_cast<int>(policy));
    emit policyChanged(policy);
}

void TransferPrefs::setConnections(int connections)
{
    connections = std::clamp(connections, 1, 16);
    if (connections == m_connections)
        return;
    m_connections = connections;
    QSettings().setValue(QStringLiteral("transfers/connections"), connections);
    emit connectionsChanged(connections);
}

QString TransferPrefs::policyTitle(OverwritePolicy policy)
{
    switch (policy) {
    case OverwritePolicy::Resume:
        return tr("Докачать");
    case OverwritePolicy::Overwrite:
        return tr("Заменить");
    case OverwritePolicy::Skip:
        return tr("Пропустить");
    case OverwritePolicy::Rename:
        return tr("Переименовать");
    }
    return {};
}

QString TransferPrefs::policyHint(OverwritePolicy policy)
{
    switch (policy) {
    case OverwritePolicy::Resume:
        return tr("Оборванный файл продолжится с места остановки, готовый — пропустится");
    case OverwritePolicy::Overwrite:
        return tr("Файл с тем же именем всегда перезаписывается");
    case OverwritePolicy::Skip:
        return tr("Файлы, которые уже есть, не трогаются");
    case OverwritePolicy::Rename:
        return tr("Новый файл сохраняется под другим именем, старый остаётся");
    }
    return {};
}

const QVector<int> &TransferPrefs::connectionChoices()
{
    static const QVector<int> choices{1, 2, 3, 4, 6, 8, 12, 16};
    return choices;
}

QString TransferPrefs::policyShortHint(OverwritePolicy policy)
{
    switch (policy) {
    case OverwritePolicy::Resume:
        return tr("Продолжит с места обрыва");
    case OverwritePolicy::Overwrite:
        return tr("Перезапишет старый файл");
    case OverwritePolicy::Skip:
        return tr("Не тронет существующий");
    case OverwritePolicy::Rename:
        return tr("Сохранит под новым именем");
    }
    return {};
}
