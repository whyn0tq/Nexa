#pragma once

#include <QColor>
#include <QString>

struct ThemePalette {
    QColor windowBackground;
    QColor surface;
    QColor surfaceRaised;
    QColor surfaceSunken;
    QColor sidebar;
    QColor border;
    QColor borderStrong;
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor accent;
    QColor accentHover;
    QColor accentSoft;
    QColor accentText;
    QColor success;
    QColor warning;
    QColor danger;
    QColor rowAlternate;
    QColor dropHighlight;
};

class Theme {
public:
    enum class Mode { Dark, Light };

    static Mode mode();
    static void setMode(Mode mode);
    static Mode loadSavedMode();
    static void saveMode(Mode mode);

    static const ThemePalette &palette();
    static const ThemePalette &palette(Mode mode);

    static QString styleSheet();

    static constexpr int SpaceXs = 4;
    static constexpr int SpaceSm = 8;
    static constexpr int SpaceMd = 12;
    static constexpr int SpaceLg = 16;
    static constexpr int SpaceXl = 24;

    static constexpr int RadiusSm = 6;
    static constexpr int RadiusMd = 10;
    static constexpr int RadiusLg = 14;

    static constexpr int ControlHeight = 34;
    static constexpr int RowHeight = 30;

private:
    static Mode s_mode;
};
