#pragma once

#include <QColor>
#include <QFont>
#include <QString>

namespace Theme {
inline const QColor Base{0, 0, 0};
inline const QColor Surface{18, 18, 19};
inline const QColor SurfaceHover{26, 26, 30};
inline const QColor Input{16, 16, 19};
inline const QColor Divider{30, 30, 32};
inline const QColor Text{242, 243, 245};
inline const QColor TextSecondary{181, 186, 193};
inline const QColor TextMuted{148, 155, 164};
inline const QColor TextFaint{128, 131, 140};

inline const QColor Accent{88, 101, 242};
inline const QColor AccentText{139, 149, 247};
inline const QColor AccentHover{71, 82, 196};
inline const QColor AccentDeep{60, 69, 165};
inline const QColor AccentSoft{88, 101, 242, 46};
inline const QColor AccentGlow{88, 101, 242, 90};

inline const QColor Success{35, 165, 90};
inline const QColor Warning{240, 178, 50};
inline const QColor Danger{242, 63, 67};

inline const QColor Glass{255, 255, 255, 10};
inline const QColor GlassHover{255, 255, 255, 17};
inline const QColor GlassActive{255, 255, 255, 24};
inline const QColor GlassSheet{18, 18, 20, 250};
inline const QColor Stroke{255, 255, 255, 38};
inline const QColor StrokeStrong{255, 255, 255, 56};
inline const QColor FieldStroke{255, 255, 255, 90};
inline const QColor FieldStrokeHover{255, 255, 255, 130};

constexpr int RadiusLarge = 12;
constexpr int Radius = 8;
constexpr int RadiusSmall = 6;

constexpr int TitleBarHeight = 40;
constexpr int RowHeight = 34;

QColor withAlpha(QColor color, int alpha);
QColor labelColor(int index);
QColor labelGlyph(int index);
int labelColorCount();
QFont font(qreal pixelSize, QFont::Weight weight = QFont::Normal);
QFont monoFont(qreal pixelSize);
QString styleSheet();
QString rgba(const QColor &color);

}
