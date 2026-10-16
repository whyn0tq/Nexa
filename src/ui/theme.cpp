#include "theme.h"

#include <QStringList>

namespace Theme {

QColor labelColor(int index)
{
    static const QColor colors[] = {
        Accent,           {255, 138, 61},  {255, 197, 61},  {61, 220, 151},
        {61, 194, 255},   {155, 123, 255}, {255, 107, 193}, {161, 161, 170},
    };
    const int count = static_cast<int>(std::size(colors));
    return colors[((index % count) + count) % count];
}

int labelColorCount()
{
    return 8;
}

QFont font(qreal pixelSize, QFont::Weight weight)
{
    QFont value;
    value.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                       QStringLiteral("Inter")});
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
    background: #141417; color: @text; border: 1px solid @strokeStrong;
    border-radius: 8px; padding: 6px 9px;
}
QLabel { background: transparent; }

QLineEdit, QSpinBox, QComboBox {
    background: rgba(255,255,255,0.045);
    border: 1px solid @stroke;
    border-radius: 10px;
    padding: 0 12px;
    min-height: 36px;
    selection-background-color: @accentSoft;
    selection-color: @text;
}
QLineEdit:hover, QSpinBox:hover, QComboBox:hover { border-color: @strokeStrong; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: rgba(255,66,86,0.75);
    background: rgba(255,255,255,0.06);
}
QLineEdit:disabled { color: @faint; }
QLineEdit[compact="true"] { min-height: 30px; border-radius: 9px; padding: 0 10px; }
QSpinBox::up-button, QSpinBox::down-button { width: 0; border: none; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow { image: none; width: 0; }
QComboBox QAbstractItemView {
    background: #141417; border: 1px solid @strokeStrong; border-radius: 10px;
    padding: 4px; selection-background-color: @accentSoft;
}

QPushButton {
    background: rgba(255,255,255,0.06);
    border: 1px solid @stroke;
    border-radius: 10px;
    padding: 0 16px;
    min-height: 36px;
    color: @text;
    font-weight: 600;
}
QPushButton:hover { background: rgba(255,255,255,0.10); border-color: @strokeStrong; }
QPushButton:pressed { background: rgba(255,255,255,0.05); }
QPushButton:disabled { color: @faint; background: rgba(255,255,255,0.03); }
QPushButton[variant="primary"] {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ff5468, stop:1 #e52a45);
    border: 1px solid rgba(255,140,150,0.45);
    color: white;
}
QPushButton[variant="primary"]:hover {
    background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ff6b7c, stop:1 #ef3550);
}
QPushButton[variant="primary"]:pressed { background: #d42640; }
QPushButton[variant="primary"]:disabled {
    background: rgba(255,66,86,0.25); color: rgba(255,255,255,0.5); border-color: transparent;
}
QPushButton[variant="danger"] {
    background: rgba(255,66,86,0.14); border: 1px solid rgba(255,66,86,0.35); color: #ff7b8a;
}
QPushButton[variant="danger"]:hover { background: rgba(255,66,86,0.22); }
QPushButton[variant="ghost"] { background: transparent; border-color: transparent; color: @text2; }
QPushButton[variant="ghost"]:hover { background: rgba(255,255,255,0.07); color: @text; }

QCheckBox { spacing: 10px; color: @text2; }
QCheckBox::indicator {
    width: 18px; height: 18px; border-radius: 6px;
    border: 1px solid @strokeStrong; background: rgba(255,255,255,0.04);
}
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px 4px 0; }
QScrollBar::handle:vertical {
    background: rgba(255,255,255,0.12); border-radius: 4px; min-height: 36px;
}
QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.22); }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0 4px 2px 4px; }
QScrollBar::handle:horizontal {
    background: rgba(255,255,255,0.12); border-radius: 4px; min-width: 36px;
}
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QAbstractItemView { background: transparent; border: none; }
QTreeView, QListView { background: transparent; border: none; }
QTreeView::item, QListView::item { background: transparent; border: none; }
QHeaderView { background: transparent; border: none; }
QHeaderView::section {
    background: transparent; border: none; color: @muted;
    padding: 0 0 0 14px; font-size: 11px; font-weight: 600; height: 30px;
}
QHeaderView::section:hover { color: @text2; }
QHeaderView::up-arrow, QHeaderView::down-arrow { width: 0; height: 0; }

QMenu {
    background: #141418;
    border: 1px solid @strokeStrong;
    border-radius: 12px;
    padding: 6px;
}
QMenu[glass="true"] { background: transparent; border: none; padding: 7px; }
QMenu::item { padding: 8px 28px 8px 12px; border-radius: 8px; color: @text; background: transparent; }
QMenu::item:selected { background: rgba(255,255,255,0.11); }
QMenu::item:disabled { color: @faint; }
QMenu::separator { height: 1px; background: @stroke; margin: 5px 8px; }
QMenu::icon { padding-left: 10px; }

QSplitter::handle { background: transparent; }
)");
    css.replace(QStringLiteral("@text2"), TextSecondary.name());
    css.replace(QStringLiteral("@text"), Text.name());
    css.replace(QStringLiteral("@muted"), TextMuted.name());
    css.replace(QStringLiteral("@faint"), TextFaint.name());
    css.replace(QStringLiteral("@strokeStrong"), rgba(StrokeStrong));
    css.replace(QStringLiteral("@stroke"), rgba(Stroke));
    css.replace(QStringLiteral("@accentSoft"), rgba(AccentSoft));
    css.replace(QStringLiteral("@accent"), Accent.name());
    return css;
}

}
