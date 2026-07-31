#pragma once

#include <QCoreApplication>
#include <QString>

class QDateTime;

class Format {
    Q_DECLARE_TR_FUNCTIONS(Format)

public:
    static QString size(qint64 bytes);
    static QString speed(qint64 bytesPerSecond);
    static QString duration(qint64 seconds);
    static QString timestamp(const QDateTime &value);
};
