#pragma once

#include <QString>
#include <QVector>

namespace Editors {

enum class Kind { Notepad = 0, VSCode, NotepadPlusPlus, Custom };

struct Editor {
    Kind kind = Kind::Notepad;
    QString name;
    QString path;
    bool available = false;
};

QVector<Editor> detect();
void useFixedList(const QVector<Editor> &editors);
QString nameOf(Kind kind);

Kind preferred();
void setPreferred(Kind kind);
QString customPath();
void setCustomPath(const QString &path);

struct Choice {
    Editor editor;
    bool fellBack = false;
};
Choice resolve();

bool open(const QString &file, Choice *used = nullptr);

}
