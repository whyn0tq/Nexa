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
    TableTranslator()
    {
        m_table.reserve(EnglishCount);
        for (int i = 0; i < EnglishCount; ++i)
            m_table.insert(QByteArray(English[i].ru), QString::fromUtf8(English[i].en));
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

}

Language current()
{
    return QSettings().value(LanguageKey, QStringLiteral("en")).toString() == QLatin1String("ru")
               ? Language::Russian
               : Language::English;
}

void setLanguage(Language language)
{
    QSettings().setValue(LanguageKey, language == Language::Russian ? QStringLiteral("ru")
                                                                    : QStringLiteral("en"));
}

Language active()
{
    return g_active;
}

void install()
{
    g_active = current();
    if (current() == Language::Russian) {
        QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
        return;
    }
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    static TableTranslator translator;
    QCoreApplication::installTranslator(&translator);
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
