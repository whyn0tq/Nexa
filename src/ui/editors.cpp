#include "editors.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace Editors {

namespace {
const QString PreferredKey = QStringLiteral("editor/kind");
const QString CustomKey = QStringLiteral("editor/customPath");

QString firstExisting(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isFile())
            return QDir::toNativeSeparators(candidate);
    }
    return {};
}

QString env(const char *name)
{
    return qEnvironmentVariable(name);
}

QString findNotepad()
{
    const QString found = firstExisting({env("SystemRoot") + QStringLiteral("/System32/notepad.exe"),
                                         env("SystemRoot") + QStringLiteral("/notepad.exe")});
    return found.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("notepad")) : found;
}

QString findVsCode()
{
    QString found = firstExisting({
        env("LOCALAPPDATA") + QStringLiteral("/Programs/Microsoft VS Code/Code.exe"),
        env("ProgramFiles") + QStringLiteral("/Microsoft VS Code/Code.exe"),
        env("ProgramFiles(x86)") + QStringLiteral("/Microsoft VS Code/Code.exe"),
    });
    if (!found.isEmpty())
        return found;
    const QString script = QStandardPaths::findExecutable(QStringLiteral("code"));
    if (!script.isEmpty()) {
        found = firstExisting({QFileInfo(script).dir().filePath(QStringLiteral("../Code.exe"))});
        if (!found.isEmpty())
            return QDir::toNativeSeparators(QDir::cleanPath(found));
    }
    return {};
}

QString findNotepadPlusPlus()
{
    QStringList candidates;
    for (const QString &key : {QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Notepad++"),
                               QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Notepad++"),
                               QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Notepad++")}) {
        const QString folder = QSettings(key, QSettings::NativeFormat).value(QStringLiteral("Default")).toString();
        if (!folder.isEmpty())
            candidates.append(QDir(folder).filePath(QStringLiteral("notepad++.exe")));
    }
    candidates.append(env("ProgramFiles") + QStringLiteral("/Notepad++/notepad++.exe"));
    candidates.append(env("ProgramFiles(x86)") + QStringLiteral("/Notepad++/notepad++.exe"));
    const QString found = firstExisting(candidates);
    return found.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("notepad++")) : found;
}

Editor make(Kind kind, const QString &path)
{
    Editor editor;
    editor.kind = kind;
    editor.name = nameOf(kind);
    editor.path = path;
    editor.available = !path.isEmpty() && QFileInfo(path).isFile();
    return editor;
}

Editor editorFor(Kind kind)
{
    switch (kind) {
    case Kind::Notepad:
        return make(kind, findNotepad());
    case Kind::VSCode:
        return make(kind, findVsCode());
    case Kind::NotepadPlusPlus:
        return make(kind, findNotepadPlusPlus());
    case Kind::Custom:
        return make(kind, customPath());
    }
    return {};
}
}

QString nameOf(Kind kind)
{
    switch (kind) {
    case Kind::Notepad:
        return QCoreApplication::translate("Editors", "Блокнот");
    case Kind::VSCode:
        return QStringLiteral("VS Code");
    case Kind::NotepadPlusPlus:
        return QStringLiteral("Notepad++");
    case Kind::Custom:
        return QCoreApplication::translate("Editors", "Свой редактор");
    }
    return {};
}

namespace {
QVector<Editor> &fixedList()
{
    static QVector<Editor> editors;
    return editors;
}
}

void useFixedList(const QVector<Editor> &editors)
{
    fixedList() = editors;
}

QVector<Editor> detect()
{
    if (!fixedList().isEmpty())
        return fixedList();
    return {editorFor(Kind::Notepad), editorFor(Kind::VSCode), editorFor(Kind::NotepadPlusPlus),
            editorFor(Kind::Custom)};
}

Kind preferred()
{
    const int value = QSettings().value(PreferredKey, static_cast<int>(Kind::Notepad)).toInt();
    return value >= 0 && value <= static_cast<int>(Kind::Custom) ? static_cast<Kind>(value)
                                                                 : Kind::Notepad;
}

void setPreferred(Kind kind)
{
    QSettings().setValue(PreferredKey, static_cast<int>(kind));
}

QString customPath()
{
    return QSettings().value(CustomKey).toString();
}

void setCustomPath(const QString &path)
{
    QSettings().setValue(CustomKey, QDir::toNativeSeparators(path.trimmed()));
}

Choice resolve()
{
    Choice choice;
    choice.editor = editorFor(preferred());
    if (choice.editor.available)
        return choice;
    choice.fellBack = true;
    for (const Kind fallback : {Kind::VSCode, Kind::NotepadPlusPlus, Kind::Notepad}) {
        const Editor editor = editorFor(fallback);
        if (editor.available) {
            choice.editor = editor;
            return choice;
        }
    }
    choice.editor = Editor();
    return choice;
}

bool open(const QString &file, Choice *used)
{
    const Choice choice = resolve();
    if (used)
        *used = choice;
    if (!choice.editor.available)
        return false;
    return QProcess::startDetached(choice.editor.path, {QDir::toNativeSeparators(file)});
}

}
