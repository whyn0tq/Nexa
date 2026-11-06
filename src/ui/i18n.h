#pragma once

#include <QString>

namespace I18n {

enum class Language { English = 0, Russian, German, French };

struct Pair {
    const char *ru;
    const char *text;
};
extern const Pair English[];
extern const int EnglishCount;
extern const Pair German[];
extern const int GermanCount;
extern const Pair French[];
extern const int FrenchCount;

Language current();
Language active();
void setLanguage(Language language);
Language fromCode(const QString &code);
void install();

void requestRestart();
bool restartRequested();

}
