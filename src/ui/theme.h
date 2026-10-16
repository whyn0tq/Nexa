#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace Theme {

inline const QColor Base{7, 7, 9};
inline const QColor Text{244, 244, 245};
inline const QColor TextSecondary{161, 161, 170};
inline const QColor TextMuted{113, 113, 122};
inline const QColor TextFaint{82, 82, 91};

inline const QColor Accent{255, 66, 86};
inline const QColor AccentHover{255, 99, 115};
inline const QColor AccentDeep{200, 30, 58};
inline const QColor AccentSoft{255, 66, 86, 38};
inline const QColor AccentGlow{255, 66, 86, 90};

inline const QColor Success{61, 220, 151};
inline const QColor Warning{255, 181, 71};
inline const QColor Danger{255, 92, 108};

inline const QColor Glass{255, 255, 255, 10};
inline const QColor GlassHover{255, 255, 255, 17};
inline const QColor GlassActive{255, 255, 255, 24};
inline const QColor GlassSheet{17, 17, 20, 228};
inline const QColor Stroke{255, 255, 255, 20};
inline const QColor StrokeStrong{255, 255, 255, 36};

constexpr int RadiusLarge = 16;
constexpr int Radius = 12;
constexpr int RadiusSmall = 9;

constexpr int TitleBarHeight = 48;
constexpr int RowHeight = 34;

QColor labelColor(int index);
int labelColorCount();
QFont font(qreal pixelSize, QFont::Weight weight = QFont::Normal);
QFont monoFont(qreal pixelSize);
QString styleSheet();
QString rgba(const QColor &color);

}
