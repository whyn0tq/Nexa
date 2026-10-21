#include "i18n.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>

#include <cstdio>

namespace {
int failures = 0;

void fail(const QString &what)
{
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what.toUtf8().constData());
}

QHash<QByteArray, QString> table(const I18n::Pair *pairs, int count, const char *name)
{
    QHash<QByteArray, QString> result;
    for (int i = 0; i < count; ++i) {
        const QByteArray key(pairs[i].ru);
        if (result.contains(key))
            fail(QStringLiteral("%1: duplicate key %2").arg(QLatin1String(name),
                                                            QString::fromUtf8(key)));
        result.insert(key, QString::fromUtf8(pairs[i].text));
    }
    return result;
}

QSet<QString> placeholders(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("%\\d"));
    QSet<QString> result;
    for (const QRegularExpressionMatch &match : pattern.globalMatch(text))
        result.insert(match.captured());
    return result;
}

void compare(const QHash<QByteArray, QString> &reference, const QHash<QByteArray, QString> &other,
             const char *name)
{
    for (auto it = reference.cbegin(); it != reference.cend(); ++it) {
        const QString source = QString::fromUtf8(it.key());
        if (!other.contains(it.key())) {
            fail(QStringLiteral("%1: missing %2").arg(QLatin1String(name), source));
            continue;
        }
        if (placeholders(source) != placeholders(other.value(it.key())))
            fail(QStringLiteral("%1: placeholders differ in %2").arg(QLatin1String(name), source));
    }
    for (auto it = other.cbegin(); it != other.cend(); ++it) {
        if (!reference.contains(it.key()))
            fail(QStringLiteral("%1: unknown key %2")
                     .arg(QLatin1String(name), QString::fromUtf8(it.key())));
    }
}
}

int main()
{
    const auto english = table(I18n::English, I18n::EnglishCount, "en");
    const auto german = table(I18n::German, I18n::GermanCount, "de");
    compare(english, english, "en");
    compare(english, german, "de");
    if (failures == 0)
        std::printf("i18n: %d strings in every language\n", I18n::EnglishCount);
    return failures == 0 ? 0 : 1;
}
