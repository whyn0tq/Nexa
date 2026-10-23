#include "theme.h"

#include <QStringList>

namespace Theme {

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

QColor labelColor(int index)
{
    static const QColor colors[] = {
        Accent,           {255, 138, 61},  {255, 197, 61},  {61, 220, 151},
        {61, 194, 255},   {155, 123, 255}, {255, 107, 193}, {161, 161, 170},
    };
    const int count = static_cast<int>(std::size(colors));
    return colors[((index % count) + count) % count];
}

QColor labelGlyph(int index)
{
    const QColor label = labelColor(index);
    const auto mix = [](int from, int to) { return from + (to - from) * 2 / 5; };
    return {mix(label.red(), Text.red()), mix(label.green(), Text.green()),
            mix(label.blue(), Text.blue())};
}

int labelColorCount()
{
    return 8;
}

QFont font(qreal pixelSize, QFont::Weight weight)
{
    QFont value;
    value.setFamilies({QStringLiteral("gg sans"), QStringLiteral("Segoe UI Variable Text"),
                       QStringLiteral("Segoe UI"), QStringLiteral("Inter")});
    value.setPixelSize(qRound(pixelSize));
    value.setWeight(weight);
    value.setHintingPreference(QFont::PreferNoHinting);
    return value;
}

QFont monoFont(qreal pixelSize)
{
    QFont value;
    value.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")});
    value.setPixelSize(qRound(pixelSize));
    return value;
}

QString rgba(const QColor &color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(color.alphaF(), 'f', 3));
}

QString styleSheet()
{
    QString css = QStringLiteral(R"(
* { outline: none; }
QWidget { color: @text; }
QToolTip {
    background: @surface; color: @text; border: 1px solid @strokeStrong;
    border-radius: 8px; padding: 6px 10px; font-weight: 600;
}
QLabel { background: transparent; }

QLineEdit, QSpinBox, QComboBox {
    background: @input;
    border: 1px solid @fieldStroke;
    border-radius: 8px;
    padding: 0 10px;
    min-height: 32px;
    selection-background-color: @accentSoft;
    selection-color: @text;
}
QLineEdit:hover, QSpinBox:hover, QComboBox:hover { border-color: @fieldStrokeHover; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: @accentFocus; }
QLineEdit:disabled { color: @faint; }
QLineEdit[compact="true"] {
    background: @base; min-height: 28px; border-radius: 8px; padding: 0 8px;
}
QSpinBox::up-button, QSpinBox::down-button { width: 0; border: none; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow { image: none; width: 0; }
QComboBox QAbstractItemView {
    background: @surface; border: 1px solid @strokeStrong; border-radius: 8px;
    padding: 6px; selection-background-color: @accent;
}

QPushButton {
    background: @surface;
    border: 1px solid @stroke;
    border-radius: 8px;
    padding: 0 14px;
    min-height: 32px;
    color: @text;
    font-weight: 600;
}
QPushButton:hover { background: @surfaceHover; border-color: @strokeStrong; }
QPushButton:pressed { background: @input; }
QPushButton:disabled { color: @faint; background: @input; }
QPushButton[variant="primary"] {
    background: @accent;
    border: 1px solid @accent;
    color: white;
}
QPushButton[variant="primary"]:hover { background: @accentHover; border-color: @accentHover; }
QPushButton[variant="primary"]:pressed { background: @accentDeep; border-color: @accentDeep; }
QPushButton[variant="primary"]:disabled {
    background: @accentMuted; color: rgba(255,255,255,0.5); border-color: transparent;
}
QPushButton[variant="danger"] { background: @danger; border: 1px solid @danger; color: white; }
QPushButton[variant="danger"]:hover { background: @dangerHover; border-color: @dangerHover; }
QPushButton[variant="ghost"] { background: transparent; border-color: transparent; color: @text2; }
QPushButton[variant="ghost"]:hover { background: rgba(255,255,255,0.06); color: @text; }

QCheckBox { spacing: 10px; color: @text2; }
QCheckBox::indicator {
    width: 18px; height: 18px; border-radius: 5px;
    border: 1px solid @fieldStroke; background: @input;
}
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 2px 4px 0; }
QScrollBar::handle:vertical {
    background: @surfaceHover; border-radius: 3px; min-height: 36px;
}
QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.18); }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0 4px 2px 4px; }
QScrollBar::handle:horizontal {
    background: @surfaceHover; border-radius: 3px; min-width: 36px;
}
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QAbstractItemView { background: transparent; border: none; }
QTreeView, QListView { background: transparent; border: none; }
QTreeView::item, QListView::item { background: transparent; border: none; }
QHeaderView { background: transparent; border: none; }
QHeaderView::section {
    background: transparent; border: none; color: @muted;
    padding: 0 0 0 14px; font-size: 12px; font-weight: 700; height: 30px;
}
QHeaderView::section:hover { color: @text2; }
QHeaderView::up-arrow, QHeaderView::down-arrow { width: 0; height: 0; }

QMenu {
    background: @surface;
    border: 1px solid @stroke;
    border-radius: 8px;
    padding: 6px;
}
QMenu[glass="true"] { background: transparent; border: none; padding: 7px; }
QMenu::item { padding: 7px 28px 7px 10px; border-radius: 4px; color: @text2; background: transparent; }
QMenu::item:selected { background: @accent; color: white; }
QMenu::item:disabled { color: @faint; }
QMenu::separator { height: 1px; background: @divider; margin: 4px 6px; }
QMenu::icon { padding-left: 10px; }

QSplitter::handle { background: transparent; }
)");
    css.replace(QStringLiteral("@text2"), TextSecondary.name());
    css.replace(QStringLiteral("@text"), Text.name());
    css.replace(QStringLiteral("@muted"), TextMuted.name());
    css.replace(QStringLiteral("@faint"), TextFaint.name());
    css.replace(QStringLiteral("@surfaceHover"), SurfaceHover.name());
    css.replace(QStringLiteral("@surface"), Surface.name());
    css.replace(QStringLiteral("@input"), Input.name());
    css.replace(QStringLiteral("@base"), Base.name());
    css.replace(QStringLiteral("@divider"), Divider.name());
    css.replace(QStringLiteral("@dangerHover"), QStringLiteral("#a12829"));
    css.replace(QStringLiteral("@danger"), QStringLiteral("#da373c"));
    css.replace(QStringLiteral("@fieldStrokeHover"), rgba(FieldStrokeHover));
    css.replace(QStringLiteral("@fieldStroke"), rgba(FieldStroke));
    css.replace(QStringLiteral("@strokeStrong"), rgba(StrokeStrong));
    css.replace(QStringLiteral("@stroke"), rgba(Stroke));
    css.replace(QStringLiteral("@accentSoft"), rgba(AccentSoft));
    css.replace(QStringLiteral("@accentHover"), AccentHover.name());
    css.replace(QStringLiteral("@accentDeep"), AccentDeep.name());
    css.replace(QStringLiteral("@accentFocus"), rgba(withAlpha(Accent, 190)));
    css.replace(QStringLiteral("@accentMuted"), rgba(withAlpha(Accent, 64)));
    css.replace(QStringLiteral("@accent"), Accent.name());
    return css;
}

}
