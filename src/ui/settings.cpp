#include "settings.h"

#include "controls.h"
#include "i18n.h"
#include "theme.h"
#include "transferprefs.h"

#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
class EditorOption final : public QWidget {
public:
    EditorOption(const Editors::Editor &editor, QWidget *parent = nullptr)
        : QWidget(parent), m_editor(editor)
    {
        setFixedHeight(56);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    Editors::Kind kind() const { return m_editor.kind; }
    void setEditor(const Editors::Editor &editor)
    {
        m_editor = editor;
        update();
    }
    void setSelected(bool selected)
    {
        m_selected = selected;
        update();
    }
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (rect().contains(event->position().toPoint()) && onClick)
            onClick();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_selected) {
            painter.setPen(QPen(QColor(255, 90, 108, 150), 1));
            painter.setBrush(QColor(255, 66, 86, 30));
        } else {
            painter.setPen(QPen(QColor(255, 255, 255, underMouse() ? 40 : 22), 1));
            painter.setBrush(QColor(255, 255, 255, underMouse() ? 16 : 9));
        }
        painter.drawRoundedRect(r, 12, 12);

        const QPointF mark(r.left() + 22, r.center().y());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_selected ? Theme::Accent : Theme::TextMuted, 1.6));
        painter.drawEllipse(mark, 8, 8);
        if (m_selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::Accent);
            painter.drawEllipse(mark, 4.5, 4.5);
        }

        const qreal left = r.left() + 42;
        const qreal chipWidth = 86;
        painter.setFont(Theme::font(13.5, QFont::DemiBold));
        painter.setPen(Theme::Text);
        QString title = m_editor.name;
        if (m_editor.kind == Editors::Kind::Notepad)
            title += QObject::tr(" · по умолчанию");
        painter.drawText(QRectF(left, r.top() + 9, r.width() - left - chipWidth, 20),
                         Qt::AlignLeft | Qt::AlignVCenter, title);
        painter.setFont(Theme::font(12));
        painter.setPen(Theme::TextMuted);
        const QString detail = m_editor.available
                                   ? m_editor.path
                                   : (m_editor.kind == Editors::Kind::Custom
                                          ? QObject::tr("Укажите путь к программе ниже")
                                          : QObject::tr("Не установлен на этом компьютере"));
        const int textWidth = static_cast<int>(r.right() - left - chipWidth - 12);
        painter.drawText(QRectF(left, r.top() + 29, textWidth, 18),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(detail, Qt::ElideMiddle, textWidth));

        const QColor tone = m_editor.available ? Theme::Success : Theme::TextFaint;
        const QString chip = m_editor.available ? QObject::tr("найден") : QObject::tr("нет");
        painter.setFont(Theme::font(11, QFont::DemiBold));
        const qreal w = painter.fontMetrics().horizontalAdvance(chip) + 18;
        const QRectF chipRect(r.right() - w - 14, r.center().y() - 10, w, 20);
        QColor fill = tone;
        fill.setAlpha(30);
        QColor line = tone;
        line.setAlpha(80);
        painter.setPen(QPen(line, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(chipRect, 7, 7);
        painter.setPen(tone);
        painter.drawText(chipRect, Qt::AlignCenter, chip);
    }

private:
    Editors::Editor m_editor;
    bool m_selected = false;
};

class FitScroll final : public QScrollArea {
public:
    FitScroll()
    {
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setStyleSheet(QStringLiteral("QScrollArea, #qt_scrollarea_viewport, #settingsPage "
                                    "{ background: transparent; }"));
        viewport()->setAutoFillBackground(false);
    }

    void setWidget(QWidget *page)
    {
        page->setAutoFillBackground(false);
        page->setObjectName(QStringLiteral("settingsPage"));
        QScrollArea::setWidget(page);
    }

    QSize sizeHint() const override
    {
        QWidget *page = widget();
        if (!page)
            return QScrollArea::sizeHint();
        const int w = width() > 0 ? width() : page->sizeHint().width();
        const int h = page->hasHeightForWidth() ? page->heightForWidth(w)
                                                : page->sizeHint().height();
        return {page->sizeHint().width(), h};
    }

    QSize minimumSizeHint() const override { return {0, 120}; }
};

class PolicyOption final : public QWidget {
public:
    PolicyOption(OverwritePolicy policy, QWidget *parent = nullptr)
        : QWidget(parent), m_policy(policy)
    {
        setFixedHeight(56);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setToolTip(TransferPrefs::policyHint(policy));
    }

    OverwritePolicy policy() const { return m_policy; }
    void setSelected(bool selected)
    {
        m_selected = selected;
        update();
    }
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (rect().contains(event->position().toPoint()) && onClick)
            onClick();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_selected) {
            painter.setPen(QPen(QColor(255, 90, 108, 150), 1));
            painter.setBrush(QColor(255, 66, 86, 30));
        } else {
            painter.setPen(QPen(QColor(255, 255, 255, underMouse() ? 40 : 22), 1));
            painter.setBrush(QColor(255, 255, 255, underMouse() ? 16 : 9));
        }
        painter.drawRoundedRect(r, 12, 12);

        const QPointF mark(r.left() + 20, r.center().y());
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_selected ? Theme::Accent : Theme::TextMuted, 1.6));
        painter.drawEllipse(mark, 7.5, 7.5);
        if (m_selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::Accent);
            painter.drawEllipse(mark, 4, 4);
        }

        const qreal left = r.left() + 38;
        painter.setFont(Theme::font(13.5, QFont::DemiBold));
        painter.setPen(Theme::Text);
        painter.drawText(QRectF(left, r.top() + 11, r.right() - left - 10, 20),
                         Qt::AlignLeft | Qt::AlignVCenter, TransferPrefs::policyTitle(m_policy));
        painter.setFont(Theme::font(11.5));
        painter.setPen(Theme::TextMuted);
        const int hintWidth = static_cast<int>(r.right() - left - 10);
        painter.drawText(QRectF(left, r.top() + 30, hintWidth, 18), Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(TransferPrefs::policyShortHint(m_policy),
                                                          Qt::ElideRight, hintWidth));
    }

private:
    OverwritePolicy m_policy;
    bool m_selected = false;
};

void paintFlag(QPainter &painter, const QRectF &r, I18n::Language language)
{
    QPainterPath shape;
    shape.addRoundedRect(r, 4, 4);
    painter.save();
    painter.setClipPath(shape);
    painter.setPen(Qt::NoPen);
    if (language == I18n::Language::Russian) {
        const qreal h = r.height() / 3;
        painter.fillRect(QRectF(r.left(), r.top(), r.width(), h), QColor(255, 255, 255));
        painter.fillRect(QRectF(r.left(), r.top() + h, r.width(), h), QColor(0, 57, 166));
        painter.fillRect(QRectF(r.left(), r.top() + 2 * h, r.width(), r.height() - 2 * h),
                         QColor(213, 43, 30));
    } else {
        const qreal stripe = r.height() / 13;
        for (int i = 0; i < 13; ++i)
            painter.fillRect(QRectF(r.left(), r.top() + i * stripe, r.width(), stripe + 0.5),
                             i % 2 ? QColor(255, 255, 255) : QColor(178, 34, 52));
        const QRectF canton(r.left(), r.top(), r.width() * 0.42, stripe * 7);
        painter.fillRect(canton, QColor(60, 59, 110));
        painter.setBrush(QColor(255, 255, 255));
        constexpr int Columns = 5;
        constexpr int Rows = 4;
        for (int row = 0; row < Rows; ++row) {
            for (int column = 0; column < Columns; ++column) {
                const qreal x = canton.left() + canton.width() * (column + 0.5 + (row % 2) * 0.25) /
                                                    (Columns + 0.25);
                const qreal y = canton.top() + canton.height() * (row + 0.5) / Rows;
                painter.drawEllipse(QPointF(x, y), 0.65, 0.65);
            }
        }
    }
    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 50), 1));
    painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
}

class LanguageOption final : public QWidget {
public:
    LanguageOption(I18n::Language language, const QString &title, const QString &native,
                   QWidget *parent = nullptr)
        : QWidget(parent), m_language(language), m_title(title), m_native(native)
    {
        setFixedHeight(56);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    I18n::Language language() const { return m_language; }
    void setSelected(bool selected)
    {
        m_selected = selected;
        update();
    }
    std::function<void()> onClick;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (rect().contains(event->position().toPoint()) && onClick)
            onClick();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (m_selected) {
            painter.setPen(QPen(QColor(255, 90, 108, 150), 1));
            painter.setBrush(QColor(255, 66, 86, 30));
        } else {
            painter.setPen(QPen(QColor(255, 255, 255, underMouse() ? 40 : 22), 1));
            painter.setBrush(QColor(255, 255, 255, underMouse() ? 16 : 9));
        }
        painter.drawRoundedRect(r, 12, 12);

        paintFlag(painter, QRectF(r.left() + 14, r.center().y() - 10, 30, 20), m_language);

        const qreal left = r.left() + 56;
        const qreal checkSpace = 34;
        painter.setFont(Theme::font(13.5, QFont::DemiBold));
        painter.setPen(Theme::Text);
        painter.drawText(QRectF(left, r.top() + 10, r.right() - left - checkSpace, 20),
                         Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.setFont(Theme::font(11.5));
        painter.setPen(Theme::TextMuted);
        painter.drawText(QRectF(left, r.top() + 29, r.right() - left - checkSpace, 18),
                         Qt::AlignLeft | Qt::AlignVCenter, m_native);
        if (m_selected)
            Icons::paint(painter, Icon::Check,
                         QRectF(r.right() - 30, r.center().y() - 8, 16, 16), Theme::Accent, 2.0);
    }

private:
    I18n::Language m_language;
    QString m_title;
    QString m_native;
    bool m_selected = false;
};
}

SettingsSheet::SettingsSheet(QWidget *host) : Sheet(host, 560)
{
    setTitle(tr("Настройки"), tr("Изменения применяются сразу."), Icon::Sliders, Theme::Accent);

    auto *page = new QWidget;
    auto *form = new QVBoxLayout(page);
    form->setContentsMargins(0, 0, 4, 0);
    form->setSpacing(12);

    form->addWidget(makeLabel(tr("Язык интерфейса"), 13.5, Theme::Text, QFont::DemiBold));
    auto *languages = new QHBoxLayout;
    languages->setSpacing(8);
    const bool russianNow = I18n::active() == I18n::Language::Russian;
    const QVector<LanguageOption *> options{
        new LanguageOption(I18n::Language::English, tr("Американский"),
                           QStringLiteral("English (US)")),
        new LanguageOption(I18n::Language::Russian, tr("Русский"),
                           russianNow ? QStringLiteral("Russian") : QStringLiteral("Русский"))};
    for (LanguageOption *option : options) {
        const I18n::Language language = option->language();
        option->onClick = [this, language] {
            I18n::setLanguage(language);
            refreshLanguage();
        };
        m_languages.append(option);
        languages->addWidget(option, 1);
    }
    form->addLayout(languages);
    m_restartRow = new QWidget;
    auto *restartRow = new QHBoxLayout(m_restartRow);
    restartRow->setContentsMargins(0, 0, 0, 0);
    restartRow->setSpacing(10);
    auto *restartHint =
        makeLabel(tr("Язык сменится после перезапуска Nexa. Открытые вкладки вернутся сами."), 12,
                  Theme::TextSecondary);
    restartHint->setWordWrap(true);
    restartRow->addWidget(restartHint, 1);
    auto *restart = makeButton(tr("Перезапустить"), QStringLiteral("primary"));
    restart->setStyleSheet(QStringLiteral("min-height:32px;border-radius:9px;"));
    restartRow->addWidget(restart);
    form->addWidget(m_restartRow);
    connect(restart, &QPushButton::clicked, this, [host] {
        I18n::requestRestart();
        host->window()->close();
    });
    refreshLanguage();
    form->addSpacing(6);

    form->addWidget(makeLabel(tr("Редактор для просмотра и правки"), 13.5, Theme::Text,
                                QFont::DemiBold));
    auto *hint = makeLabel(tr("По умолчанию файлы открываются в Блокноте. Если выбранного "
                              "редактора нет, Nexa сама откроет файл в VS Code, а если нет и "
                              "его — в Notepad++."),
                           12.5, Theme::TextSecondary);
    hint->setWordWrap(true);
    form->addWidget(hint);

    for (const Editors::Editor &editor : Editors::detect()) {
        auto *option = new EditorOption(editor);
        const Editors::Kind kind = editor.kind;
        option->onClick = [this, kind] { select(kind); };
        m_options.append(option);
        form->addWidget(option);
    }

    auto *customRow = new QHBoxLayout;
    customRow->setSpacing(8);
    m_customPath = new QLineEdit(Editors::customPath());
    m_customPath->setPlaceholderText(tr("Например, C:\\Program Files\\Sublime Text\\sublime_text.exe"));
    auto *browse = makeButton(tr("Обзор"));
    customRow->addWidget(m_customPath, 1);
    customRow->addWidget(browse);
    form->addLayout(customRow);

    m_effective = makeLabel(QString(), 12.5, Theme::TextSecondary);
    m_effective->setWordWrap(true);
    form->addWidget(m_effective);

    form->addSpacing(6);
    form->addWidget(makeLabel(tr("Если файл уже есть"), 13.5, Theme::Text, QFont::DemiBold));
    auto *policies = new QGridLayout;
    policies->setSpacing(8);
    for (int i = 0; i < 4; ++i) {
        auto *option = new PolicyOption(static_cast<OverwritePolicy>(i));
        option->onClick = [i] {
            TransferPrefs::instance().setPolicy(static_cast<OverwritePolicy>(i));
        };
        m_policies.append(option);
        policies->addWidget(option, i / 2, i % 2);
    }
    form->addLayout(policies);

    form->addSpacing(6);
    auto *connectionsTitle = new QHBoxLayout;
    connectionsTitle->setSpacing(8);
    connectionsTitle->addWidget(makeLabel(tr("Потоков на сервер"), 13.5, Theme::Text,
                                          QFont::DemiBold));
    connectionsTitle->addWidget(makeLabel(tr("больше потоков — быстрее много мелких файлов"),
                                          12, Theme::TextMuted), 1);
    form->addLayout(connectionsTitle);
    QStringList counts;
    for (const int value : TransferPrefs::connectionChoices())
        counts.append(QString::number(value));
    m_connections = new Segmented(counts);
    m_connections->setFixedHeight(34);
    form->addWidget(m_connections);
    connect(m_connections, &Segmented::changed, this, [](int index) {
        TransferPrefs::instance().setConnections(
            TransferPrefs::connectionChoices().value(index, 4));
    });
    connect(&TransferPrefs::instance(), &TransferPrefs::policyChanged, this,
            &SettingsSheet::refreshTransfers);
    connect(&TransferPrefs::instance(), &TransferPrefs::connectionsChanged, this,
            &SettingsSheet::refreshTransfers);
    refreshTransfers();

    auto *scroll = new FitScroll;
    scroll->setWidget(page);
    body()->addWidget(scroll, 1);

    auto *done = makeButton(tr("Готово"), QStringLiteral("primary"));
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(done);
    body()->addSpacing(4);
    body()->addLayout(row);

    connect(done, &QPushButton::clicked, this, &Sheet::dismiss);
    connect(m_customPath, &QLineEdit::editingFinished, this, [this] {
        Editors::setCustomPath(m_customPath->text());
        refresh();
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Программа для просмотра и правки"),
            qEnvironmentVariable("ProgramFiles"), tr("Программы (*.exe)"));
        if (path.isEmpty())
            return;
        m_customPath->setText(QDir::toNativeSeparators(path));
        Editors::setCustomPath(path);
        select(Editors::Kind::Custom);
    });
    refresh();
}

void SettingsSheet::refreshLanguage()
{
    const I18n::Language chosen = I18n::current();
    for (QWidget *widget : std::as_const(m_languages)) {
        auto *option = static_cast<LanguageOption *>(widget);
        option->setSelected(option->language() == chosen);
    }
    m_restartRow->setVisible(chosen != I18n::active());
    if (isVisible())
        relayout();
}

void SettingsSheet::refreshTransfers()
{
    const TransferPrefs &prefs = TransferPrefs::instance();
    for (QWidget *widget : std::as_const(m_policies)) {
        auto *option = static_cast<PolicyOption *>(widget);
        option->setSelected(option->policy() == prefs.policy());
    }
    m_connections->setCurrent(
        std::max<int>(0, TransferPrefs::connectionChoices().indexOf(prefs.connections())));
}

void SettingsSheet::select(Editors::Kind kind)
{
    Editors::setPreferred(kind);
    if (kind == Editors::Kind::Custom && m_customPath->text().trimmed().isEmpty())
        m_customPath->setFocus();
    refresh();
}

void SettingsSheet::refresh()
{
    const Editors::Kind current = Editors::preferred();
    const QVector<Editors::Editor> editors = Editors::detect();
    for (QWidget *widget : std::as_const(m_options)) {
        auto *option = static_cast<EditorOption *>(widget);
        for (const Editors::Editor &editor : editors) {
            if (editor.kind == option->kind())
                option->setEditor(editor);
        }
        option->setSelected(option->kind() == current);
    }
    const Editors::Choice choice = Editors::resolve();
    if (!choice.editor.available)
        m_effective->setText(tr("Ни один редактор не найден — файлы откроются программой Windows."));
    else if (choice.fellBack)
        m_effective->setText(tr("%1 не найден — файлы будут открываться в программе «%2».")
                                 .arg(Editors::nameOf(current), choice.editor.name));
    else
        m_effective->setText(tr("Файлы будут открываться в программе «%1».").arg(choice.editor.name));
}
