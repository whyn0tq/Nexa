#include "format.h"

#include <QDateTime>
#include <QLocale>
#include <QStringList>

QString Format::size(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("—");
    static const QStringList units{tr("Б"), tr("КБ"), tr("МБ"), tr("ГБ"),
                                   tr("ТБ"), tr("ПБ")};
    if (bytes < 1024)
        return QStringLiteral("%1 %2").arg(QString::number(bytes), units.at(0));

    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', value < 10.0 ? 1 : 0), units.at(unit));
}

QString Format::speed(qint64 bytesPerSecond)
{
    if (bytesPerSecond <= 0)
        return QStringLiteral("—");
    return tr("%1/с").arg(size(bytesPerSecond));
}

QString Format::duration(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("—");
    if (seconds < 60)
        return tr("%1 с").arg(seconds);
    if (seconds < 3600)
        return tr("%1 мин %2 с").arg(seconds / 60).arg(seconds % 60);
    return tr("%1 ч %2 мин").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

QString Format::timestamp(const QDateTime &value)
{
    if (!value.isValid())
        return QStringLiteral("—");

    const QLocale locale;
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 days = value.daysTo(now);
    if (days == 0)
        return tr("сегодня %1").arg(locale.toString(value, QStringLiteral("HH:mm")));
    if (days == 1)
        return tr("вчера %1").arg(locale.toString(value, QStringLiteral("HH:mm")));
    if (days > 1 && days < 365)
        return locale.toString(value, tr("d MMM HH:mm"));
    return locale.toString(value, tr("dd.MM.yyyy"));
}
