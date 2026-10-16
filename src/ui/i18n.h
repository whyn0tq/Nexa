#pragma once

#include <QString>

namespace I18n {

enum class Language { English = 0, Russian };

struct Pair {
    const char *ru;
    const char *en;
};
extern const Pair English[];
extern const int EnglishCount;

Language current();
Language active();
void setLanguage(Language language);
void install();

void requestRestart();
bool restartRequested();

}
