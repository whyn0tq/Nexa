#include "theme.h"

#include <QSettings>

namespace {
const ThemePalette &darkPalette()
{
    static const ThemePalette palette{
        QColor(0x0a, 0x0f, 0x19),
        QColor(0x0f, 0x16, 0x23),
        QColor(0x17, 0x22, 0x35),
        QColor(0x0d, 0x14, 0x20),
        QColor(0x0d, 0x13, 0x1f),
        QColor(0x20, 0x2b, 0x3c),
        QColor(0x2e, 0x3c, 0x53),
        QColor(0xf4, 0xf7, 0xfb),
        QColor(0xb4, 0xc0, 0xd4),
        QColor(0x69, 0x77, 0x8d),
        QColor(0x5b, 0x5c, 0xf0),
        QColor(0x6d, 0x6e, 0xf7),
        QColor(0x30, 0x31, 0x7a),
        QColor(0xff, 0xff, 0xff),
        QColor(0x4a, 0xde, 0x80),
        QColor(0xfb, 0xbf, 0x24),
        QColor(0xf8, 0x71, 0x71),
        QColor(0x10, 0x19, 0x27),
        QColor(0x1c, 0x2a, 0x52)
    };
    return palette;
}

const ThemePalette &lightPalette()
{
    static const ThemePalette palette{
        QColor(0xf4, 0xf6, 0xfb),
        QColor(0xff, 0xff, 0xff),
        QColor(0xf1, 0xf4, 0xf9),
        QColor(0xfb, 0xfc, 0xfe),
        QColor(0xff, 0xff, 0xff),
        QColor(0xdf, 0xe4, 0xed),
        QColor(0xc4, 0xcd, 0xdb),
        QColor(0x14, 0x1b, 0x2a),
        QColor(0x46, 0x51, 0x66),
        QColor(0x78, 0x84, 0x99),
        QColor(0x4f, 0x50, 0xe6),
        QColor(0x40, 0x41, 0xd8),
        QColor(0xdd, 0xdf, 0xfb),
        QColor(0xff, 0xff, 0xff),
        QColor(0x17, 0x9c, 0x4d),
        QColor(0xb4, 0x7a, 0x05),
        QColor(0xd4, 0x33, 0x3f),
        QColor(0xf7, 0xf9, 0xfc),
        QColor(0xe4, 0xe6, 0xfd)
    };
    return palette;
}

QString hex(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

QString rgba(const QColor &color, double alpha)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha, 0, 'f', 2);
}
}

Theme::Mode Theme::s_mode = Theme::Mode::Dark;

Theme::Mode Theme::mode()
{
    return s_mode;
}

void Theme::setMode(Mode mode)
{
    s_mode = mode;
}

Theme::Mode Theme::loadSavedMode()
{
    const QString saved = QSettings()
                              .value(QStringLiteral("appearance/theme"),
                                     QStringLiteral("dark"))
                              .toString();
    return saved == QStringLiteral("light") ? Mode::Light : Mode::Dark;
}

void Theme::saveMode(Mode mode)
{
    QSettings().setValue(QStringLiteral("appearance/theme"),
                         mode == Mode::Light ? QStringLiteral("light")
                                             : QStringLiteral("dark"));
}

const ThemePalette &Theme::palette()
{
    return palette(s_mode);
}

const ThemePalette &Theme::palette(Mode mode)
{
    return mode == Mode::Light ? lightPalette() : darkPalette();
}

QString Theme::styleSheet()
{
    const ThemePalette &p = palette();
    const bool light = s_mode == Mode::Light;

    const QString selectionText = light ? hex(p.textPrimary) : hex(p.accentText);

    return QStringLiteral(R"(
        * {
            font-family: "Segoe UI", "Inter", sans-serif;
            font-size: 13px;
        }
        QMainWindow, QDialog { background: %{windowBackground}; }
        QWidget#appRoot, QWidget#workspace, QWidget#workspacePage {
            background: %{windowBackground};
        }
        QToolTip {
            background: %{surfaceRaised};
            color: %{textPrimary};
            border: 1px solid %{borderStrong};
            border-radius: %{radiusSm}px;
            padding: 6px 8px;
        }

        QFrame#sidebar {
            background: %{sidebar};
            border: none;
            border-right: 1px solid %{border};
        }
        QLabel { color: %{textSecondary}; background: transparent; }
        QLabel#brandMark {
            color: %{accentText};
            background: %{accent};
            border-radius: %{radiusMd}px;
            font-size: 19px;
            font-weight: 800;
        }
        QLabel#brandTitle {
            color: %{textPrimary};
            font-size: 17px;
            font-weight: 700;
        }
        QLabel#brandSubtitle { color: %{textMuted}; font-size: 11px; }
        QLabel#sectionCaption, QLabel#eyebrow {
            color: %{textMuted};
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
        }
        QLabel#versionLabel {
            color: %{textMuted};
            font-size: 10px;
            padding-top: 4px;
        }
        QPushButton#navigationButton {
            border: none;
            border-radius: %{radiusSm}px;
            padding: 9px 11px;
            text-align: left;
            font-weight: 600;
            color: %{textSecondary};
            background: transparent;
        }
        QPushButton#navigationButton:hover {
            color: %{textPrimary};
            background: %{surfaceRaised};
        }
        QPushButton#navigationButton[active="true"] {
            color: %{accent};
            background: %{accentSoftFaded};
        }
        QLabel#navigationBadge {
            color: %{accentText};
            background: %{accent};
            border-radius: 8px;
            padding: 1px 6px;
            font-size: 10px;
            font-weight: 700;
        }

        QFrame#connectionCard {
            background: %{surface};
            border: 1px solid %{border};
            border-radius: %{radiusMd}px;
        }
        QLabel#connectionLabel { font-size: 12px; font-weight: 600; }
        QLabel#connectionDetail { color: %{textMuted}; font-size: 11px; }

        QLabel#workspaceTitle {
            color: %{textPrimary};
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#workspaceSubtitle { color: %{textMuted}; font-size: 12px; }

        QFrame#fileCard {
            background: %{surface};
            border: 1px solid %{border};
            border-radius: %{radiusLg}px;
        }
        QFrame#fileCard[dropActive="true"] { border: 1px solid %{accent}; }
        QWidget#panelHeader { background: transparent; border: none; }
        QLabel#panelTitle {
            color: %{textPrimary};
            font-size: 14px;
            font-weight: 650;
        }
        QLabel#panelMeta { color: %{textMuted}; font-size: 11px; }

        QLineEdit, QSpinBox, QComboBox {
            background: %{surfaceRaised};
            color: %{textPrimary};
            border: 1px solid %{border};
            border-radius: %{radiusSm}px;
            padding: 7px 10px;
            selection-background-color: %{accent};
            selection-color: %{accentText};
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: %{accent};
        }
        QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
            color: %{textMuted};
            background: %{surfaceSunken};
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background: %{surface};
            color: %{textPrimary};
            border: 1px solid %{border};
            selection-background-color: %{accentSoft};
            selection-color: %{selectionText};
            outline: none;
        }

        QPushButton {
            background: %{surfaceRaised};
            color: %{textPrimary};
            border: 1px solid %{border};
            border-radius: %{radiusSm}px;
            padding: 7px 13px;
            font-weight: 600;
        }
        QPushButton:hover { border-color: %{borderStrong}; }
        QPushButton:pressed { background: %{surfaceSunken}; }
        QPushButton:disabled {
            color: %{textMuted};
            background: %{surfaceSunken};
            border-color: %{border};
        }
        QPushButton#primaryButton {
            background: %{accent};
            border-color: %{accent};
            color: %{accentText};
            padding-left: 16px;
            padding-right: 16px;
        }
        QPushButton#primaryButton:hover {
            background: %{accentHover};
            border-color: %{accentHover};
        }
        QPushButton#primaryButton:disabled {
            background: %{accentSoftFaded};
            border-color: transparent;
            color: %{textMuted};
        }
        QPushButton#secondaryButton { background: transparent; }
        QPushButton#dangerButton {
            color: %{danger};
            border-color: %{dangerFaded};
            background: transparent;
        }
        QPushButton#dangerButton:hover { background: %{dangerFaded}; }
        QPushButton#iconButton {
            background: transparent;
            color: %{textSecondary};
            border: 1px solid transparent;
            padding: 0;
            font-size: 15px;
        }
        QPushButton#iconButton:hover {
            background: %{surfaceRaised};
            border-color: %{border};
            color: %{textPrimary};
        }
        QPushButton#iconButton:disabled { color: %{textMuted}; }
        QPushButton#crumbButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: %{radiusSm}px;
            color: %{textSecondary};
            font-weight: 600;
            padding: 4px 8px;
        }
        QPushButton#crumbButton:hover {
            background: %{surfaceRaised};
            color: %{textPrimary};
        }
        QPushButton#crumbButton[current="true"] { color: %{textPrimary}; }
        QLabel#crumbSeparator { color: %{textMuted}; }
        QToolButton {
            background: transparent;
            border: none;
            border-radius: %{radiusSm}px;
            color: %{textSecondary};
            padding: 4px;
        }
        QToolButton:hover { background: %{surfaceRaised}; }

        QTreeView, QTableView {
            background: %{surfaceSunken};
            alternate-background-color: %{rowAlternate};
            color: %{textPrimary};
            border: none;
            border-top: 1px solid %{border};
            outline: none;
            selection-background-color: %{accentSoft};
            selection-color: %{selectionText};
            show-decoration-selected: 1;
        }
        QTreeView::item, QTableView::item {
            min-height: %{rowHeight}px;
            padding: 2px 6px;
            border: none;
        }
        QHeaderView { background: transparent; }
        QHeaderView::section {
            background: %{surface};
            color: %{textMuted};
            border: none;
            border-bottom: 1px solid %{border};
            padding: 8px 8px;
            font-size: 11px;
            font-weight: 650;
        }
        QHeaderView::section:hover { color: %{textSecondary}; }
        QHeaderView::down-arrow, QHeaderView::up-arrow { width: 9px; }
        QSplitter#fileSplitter::handle {
            background: transparent;
            width: %{spaceMd}px;
        }

        QFrame#notificationBanner {
            border-radius: %{radiusMd}px;
            border: 1px solid %{border};
            background: %{surface};
        }
        QFrame#notificationBanner[severity="error"] {
            border-color: %{dangerFaded};
            background: %{dangerFaded};
        }
        QFrame#notificationBanner[severity="warning"] {
            border-color: %{warningFaded};
            background: %{warningFaded};
        }
        QFrame#notificationBanner[severity="success"] {
            border-color: %{successFaded};
            background: %{successFaded};
        }
        QLabel#notificationText { color: %{textPrimary}; font-weight: 600; }
        QLabel#notificationDetail { color: %{textSecondary}; font-size: 12px; }

        QLabel#emptyStateIcon { color: %{textMuted}; font-size: 30px; }
        QLabel#emptyStateTitle {
            color: %{textSecondary};
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#emptyStateText { color: %{textMuted}; font-size: 12px; }

        QFrame#transferCard {
            background: %{surface};
            border: 1px solid %{border};
            border-radius: %{radiusLg}px;
        }
        QLabel#statTitle { color: %{textMuted}; font-size: 11px; }
        QLabel#statValue {
            color: %{textPrimary};
            font-size: 20px;
            font-weight: 700;
        }

        QStatusBar {
            background: %{sidebar};
            color: %{textMuted};
            border-top: 1px solid %{border};
            min-height: 26px;
        }
        QStatusBar::item { border: none; }
        QProgressBar {
            background: %{surfaceRaised};
            border: 1px solid %{border};
            border-radius: %{radiusSm}px;
            text-align: center;
            color: %{textPrimary};
            font-size: 11px;
        }
        QProgressBar::chunk {
            background: %{accent};
            border-radius: %{radiusSm}px;
        }
        QDialogButtonBox QPushButton { min-width: 104px; }
        QMenu {
            background: %{surface};
            color: %{textPrimary};
            border: 1px solid %{border};
            border-radius: %{radiusSm}px;
            padding: 5px;
        }
        QMenu::item { padding: 7px 26px 7px 10px; border-radius: 5px; }
        QMenu::item:selected { background: %{accentSoft}; color: %{selectionText}; }
        QMenu::item:disabled { color: %{textMuted}; }
        QMenu::separator {
            height: 1px;
            background: %{border};
            margin: 4px 6px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: %{borderStrong};
            border-radius: 4px;
            min-height: 28px;
        }
        QScrollBar::handle:vertical:hover { background: %{textMuted}; }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background: %{borderStrong};
            border-radius: 4px;
            min-width: 28px;
        }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )")
        .replace(QStringLiteral("%{windowBackground}"), hex(p.windowBackground))
        .replace(QStringLiteral("%{surfaceRaised}"), hex(p.surfaceRaised))
        .replace(QStringLiteral("%{surfaceSunken}"), hex(p.surfaceSunken))
        .replace(QStringLiteral("%{surface}"), hex(p.surface))
        .replace(QStringLiteral("%{sidebar}"), hex(p.sidebar))
        .replace(QStringLiteral("%{borderStrong}"), hex(p.borderStrong))
        .replace(QStringLiteral("%{border}"), hex(p.border))
        .replace(QStringLiteral("%{textPrimary}"), hex(p.textPrimary))
        .replace(QStringLiteral("%{textSecondary}"), hex(p.textSecondary))
        .replace(QStringLiteral("%{textMuted}"), hex(p.textMuted))
        .replace(QStringLiteral("%{accentSoftFaded}"), rgba(p.accent, light ? 0.10 : 0.18))
        .replace(QStringLiteral("%{accentSoft}"), hex(p.accentSoft))
        .replace(QStringLiteral("%{accentHover}"), hex(p.accentHover))
        .replace(QStringLiteral("%{accentText}"), hex(p.accentText))
        .replace(QStringLiteral("%{accent}"), hex(p.accent))
        .replace(QStringLiteral("%{successFaded}"), rgba(p.success, 0.14))
        .replace(QStringLiteral("%{warningFaded}"), rgba(p.warning, 0.14))
        .replace(QStringLiteral("%{dangerFaded}"), rgba(p.danger, 0.14))
        .replace(QStringLiteral("%{success}"), hex(p.success))
        .replace(QStringLiteral("%{warning}"), hex(p.warning))
        .replace(QStringLiteral("%{danger}"), hex(p.danger))
        .replace(QStringLiteral("%{hoverRow}"), rgba(p.accent, light ? 0.07 : 0.13))
        .replace(QStringLiteral("%{rowAlternate}"), hex(p.rowAlternate))
        .replace(QStringLiteral("%{selectionText}"), selectionText)
        .replace(QStringLiteral("%{radiusSm}"), QString::number(RadiusSm))
        .replace(QStringLiteral("%{radiusMd}"), QString::number(RadiusMd))
        .replace(QStringLiteral("%{radiusLg}"), QString::number(RadiusLg))
        .replace(QStringLiteral("%{rowHeight}"), QString::number(RowHeight))
        .replace(QStringLiteral("%{spaceMd}"), QString::number(SpaceMd));
}
