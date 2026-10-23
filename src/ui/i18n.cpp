#include "i18n.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace I18n {
namespace {

const auto LanguageKey = QStringLiteral("ui/language");
bool g_restart = false;
Language g_active = Language::English;

class TableTranslator final : public QTranslator {
public:
    TableTranslator(const Pair *pairs, int count)
    {
        m_table.reserve(count);
        for (int i = 0; i < count; ++i)
            m_table.insert(QByteArray(pairs[i].ru), QString::fromUtf8(pairs[i].text));
    }

    QString translate(const char *context, const char *sourceText, const char *disambiguation,
                      int n) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        return sourceText ? m_table.value(QByteArray(sourceText)) : QString();
    }

    bool isEmpty() const override { return m_table.isEmpty(); }

private:
    QHash<QByteArray, QString> m_table;
};

QString codeOf(Language language)
{
    switch (language) {
    case Language::Russian:
        return QStringLiteral("ru");
    case Language::German:
        return QStringLiteral("de");
    case Language::English:
        break;
    }
    return QStringLiteral("en");
}

}

Language fromCode(const QString &code)
{
    if (code == QLatin1String("ru"))
        return Language::Russian;
    if (code == QLatin1String("de"))
        return Language::German;
    return Language::English;
}

Language current()
{
    return fromCode(QSettings().value(LanguageKey, QStringLiteral("en")).toString());
}

void setLanguage(Language language)
{
    QSettings().setValue(LanguageKey, codeOf(language));
}

Language active()
{
    return g_active;
}

void install()
{
    g_active = current();
    if (g_active == Language::Russian) {
        QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
        return;
    }
    static TableTranslator english(English, EnglishCount);
    QCoreApplication::installTranslator(&english);
    if (g_active == Language::German) {
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        static TableTranslator german(German, GermanCount);
        QCoreApplication::installTranslator(&german);
        return;
    }
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
}

void requestRestart()
{
    g_restart = true;
}

bool restartRequested()
{
    return g_restart;
}

}
