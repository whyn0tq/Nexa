#include "sheets.h"

#include "controls.h"
#include "theme.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QHBoxLayout *buttonRow(QPushButton *secondary, QPushButton *primary,
                       QPushButton *extra = nullptr)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addStretch();
    if (extra)
        row->addWidget(extra);
    row->addWidget(secondary);
    row->addWidget(primary);
    return row;
}

QWidget *field(const QString &label, QWidget *input)
{
    auto *box = new QWidget;
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(label, 12, Theme::TextSecondary, QFont::DemiBold));
    layout->addWidget(input);
    return box;
}

QLineEdit *secretField(const QString &placeholder)
{
    auto *edit = new QLineEdit;
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(placeholder);
    QAction *reveal = edit->addAction(Icons::icon(Icon::Eye, Theme::TextMuted, 16),
                                      QLineEdit::TrailingPosition);
    reveal->setToolTip(QObject::tr("Показать"));
    QObject::connect(reveal, &QAction::triggered, edit, [edit] {
        edit->setEchoMode(edit->echoMode() == QLineEdit::Password ? QLineEdit::Normal
                                                                  : QLineEdit::Password);
    });
    return edit;
}

class Swatch final : public QAbstractButton {
public:
    Swatch(const QColor &color, QWidget *parent = nullptr) : QAbstractButton(parent), m_color(color)
    {
        setCheckable(true);
        setFixedSize(26, 26);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (isChecked()) {
            painter.setPen(QPen(m_color, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(1, 1, 24, 24), 9, 9);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);
        painter.drawRoundedRect(QRectF(5, 5, 16, 16), 6, 6);
    }

private:
    QColor m_color;
};

class KindButton final : public QAbstractButton {
public:
    KindButton(HostKind kind, QWidget *parent = nullptr) : QAbstractButton(parent), m_kind(kind)
    {
        setCheckable(true);
        setFixedSize(30, 30);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setToolTip(Icons::hostKindName(kind));
        setAccessibleName(Icons::hostKindName(kind));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        const bool hovered = underMouse();
        if (isChecked()) {
            painter.setPen(QPen(Theme::withAlpha(Theme::Accent, 150), 1));
            painter.setBrush(Theme::AccentSoft);
            painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        } else if (hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Theme::GlassHover);
            painter.drawRoundedRect(r, 8, 8);
        }
        if (hasFocus()) {
            painter.setPen(QPen(Theme::Accent, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(r, 8, 8);
        }
        const QRectF glyph(6, 6, 18, 18);
        if (isChecked())
            Icons::paintFilled(painter, Icons::hostIcon(m_kind), glyph, Theme::AccentText);
        else
            Icons::paint(painter, Icons::hostIcon(m_kind), glyph,
                         hovered ? Theme::Text : Theme::TextMuted);
    }

private:
    HostKind m_kind;
};
}

namespace Sheets {

void confirm(QWidget *host, const QString &title, const QString &text,
             const QString &confirmText, bool danger, std::function<void()> onConfirm)
{
    auto *sheet = new Sheet(host, 440);
    sheet->setTitle(title, text, danger ? Icon::Trash : Icon::Info,
                    danger ? Theme::Accent : Theme::TextSecondary);
    auto *cancel = makeButton(QObject::tr("Отмена"));
    auto *ok = makeButton(confirmText, danger ? QStringLiteral("danger")
                                              : QStringLiteral("primary"));
    sheet->body()->addLayout(buttonRow(cancel, ok));
    QObject::connect(cancel, &QPushButton::clicked, sheet, &Sheet::dismiss);
    QObject::connect(ok, &QPushButton::clicked, sheet, [sheet, onConfirm] {
        sheet->dismiss();
        onConfirm();
    });
    sheet->open();
    ok->setFocus();
}

void prompt(QWidget *host, const QString &title, const QString &placeholder,
            const QString &initial, const QString &confirmText, Icon icon,
            std::function<void(const QString &)> onAccept)
{
    auto *sheet = new Sheet(host, 440);
    sheet->setTitle(title, QString(), icon, Theme::Accent);
    auto *edit = new QLineEdit(initial);
    edit->setPlaceholderText(placeholder);
    sheet->body()->addWidget(edit);
    auto *cancel = makeButton(QObject::tr("Отмена"));
    auto *ok = makeButton(confirmText, QStringLiteral("primary"));
    sheet->body()->addSpacing(4);
    sheet->body()->addLayout(buttonRow(cancel, ok));
    const auto accept = [sheet, edit, onAccept] {
        const QString value = edit->text().trimmed();
        if (value.isEmpty() || value.contains(QLatin1Char('/')) ||
            value.contains(QLatin1Char('\\'))) {
            edit->setFocus();
            return;
        }
        sheet->dismiss();
        onAccept(value);
    };
    QObject::connect(cancel, &QPushButton::clicked, sheet, &Sheet::dismiss);
    QObject::connect(ok, &QPushButton::clicked, sheet, accept);
    QObject::connect(edit, &QLineEdit::returnPressed, sheet, accept);
    sheet->open();
    edit->setFocus();
    const qsizetype dot = initial.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        edit->setSelection(0, static_cast<int>(dot));
    else
        edit->selectAll();
}

void trust(QWidget *host, const QString &server, const QString &fingerprint,
           const QString &keyType, bool changed,
           std::function<void(bool accepted, bool remember)> onAnswer)
{
    auto *sheet = new Sheet(host, 500);
    if (changed) {
        sheet->setTitle(QObject::tr("Ключ сервера изменился"),
                        QObject::tr("Ключ %1 отличается от сохранённого. Это бывает после "
                                    "переустановки сервера, но может означать перехват "
                                    "соединения. Продолжайте, только если уверены.")
                            .arg(server),
                        Icon::Alert, Theme::Warning);
    } else {
        sheet->setTitle(QObject::tr("Новый сервер"),
                        QObject::tr("Вы подключаетесь к %1 впервые. Сверьте отпечаток "
                                    "ключа с данными хостинга.")
                            .arg(server),
                        Icon::Shield, Theme::Success);
    }
    auto *box = new GlassPanel(Glass::Tone::Panel, 12);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(14, 12, 14, 12);
    boxLayout->setSpacing(6);
    boxLayout->addWidget(makeLabel(keyType.isEmpty() ? QObject::tr("SHA-256")
                                                     : QObject::tr("%1 · SHA-256").arg(keyType),
                                   12, Theme::TextMuted, QFont::DemiBold));
    QStringList bytes = fingerprint.split(QLatin1Char(':'));
    QStringList rows;
    while (!bytes.isEmpty()) {
        rows.append(bytes.mid(0, 16).join(QLatin1Char(':')));
        bytes = bytes.mid(16);
    }
    auto *print = new QLabel(rows.join(QChar(QChar::LineFeed)));
    print->setFont(Theme::monoFont(12));
    print->setWordWrap(true);
    print->setTextInteractionFlags(Qt::TextSelectableByMouse);
    print->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::Text.name()));
    boxLayout->addWidget(print);
    sheet->body()->addWidget(box);

    auto *answered = new bool(false);
    auto *cancel = makeButton(QObject::tr("Отмена"));
    auto *once = makeButton(QObject::tr("Только сейчас"));
    auto *remember = makeButton(QObject::tr("Доверять"), changed ? QStringLiteral("danger")
                                                                   : QStringLiteral("primary"));
    sheet->body()->addSpacing(4);
    sheet->body()->addLayout(buttonRow(once, remember, cancel));
    const auto answer = [sheet, answered, onAnswer](bool accepted, bool keep) {
        if (*answered)
            return;
        *answered = true;
        sheet->dismiss();
        onAnswer(accepted, keep);
    };
    QObject::connect(cancel, &QPushButton::clicked, sheet, [answer] { answer(false, false); });
    QObject::connect(once, &QPushButton::clicked, sheet, [answer] { answer(true, false); });
    QObject::connect(remember, &QPushButton::clicked, sheet, [answer] { answer(true, true); });
    QObject::connect(sheet, &Sheet::dismissed, sheet, [answered, onAnswer] {
        if (!*answered) {
            *answered = true;
            onAnswer(false, false);
        }
    });
    QObject::connect(sheet, &QObject::destroyed, [answered] { delete answered; });
    sheet->open();
    remember->setFocus();
}

}

HostEditor::HostEditor(QWidget *host, const SavedHost &value, bool isNew)
    : Sheet(host, 560), m_value(value), m_color(value.color), m_kind(value.kind)
{
    setTitle(isNew ? tr("Новый сервер") : tr("Сервер «%1»").arg(value.title()),
             tr("Пароли хранятся зашифрованными средствами Windows."),
             Icons::hostIcon(value.kind),
             Theme::Accent);
    const ConnectionProfile &profile = value.profile;

    m_protocol = new Segmented({QStringLiteral("SFTP"), QStringLiteral("FTP"),
                                QStringLiteral("FTPS")});
    m_protocol->setCurrent(profile.protocol == Protocol::Sftp ? 0 : profile.useTls ? 2 : 1);
    body()->addWidget(field(tr("Протокол"), m_protocol));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    m_host = new QLineEdit(profile.host);
    m_host->setPlaceholderText(tr("example.com или 192.168.1.10"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(profile.port ? profile.port : 22);
    m_port->setFixedWidth(110);
    m_port->setButtonSymbols(QAbstractSpinBox::NoButtons);
    grid->addWidget(field(tr("Адрес"), m_host), 0, 0);
    grid->addWidget(field(tr("Порт"), m_port), 0, 1);
    m_user = new QLineEdit(profile.username);
    m_user->setPlaceholderText(tr("root"));
    m_name = new QLineEdit(value.name);
    m_name->setPlaceholderText(tr("Как показывать во вкладке"));
    grid->addWidget(field(tr("Пользователь"), m_user), 1, 0);
    grid->setColumnStretch(0, 1);
    body()->addLayout(grid);

    m_auth = new Segmented({tr("Пароль"), tr("Ключ"), tr("Агент")});
    m_auth->setCurrent(static_cast<int>(profile.auth));
    m_authRow = field(tr("Вход"), m_auth);
    body()->addWidget(m_authRow);

    m_authStack = new QStackedWidget;
    m_password = secretField(tr("Пароль"));
    m_password->setText(profile.password);
    auto *passwordPage = new QWidget;
    auto *passwordLayout = new QVBoxLayout(passwordPage);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->addWidget(field(tr("Пароль"), m_password));
    m_authStack->addWidget(passwordPage);

    auto *keyPage = new QWidget;
    auto *keyLayout = new QVBoxLayout(keyPage);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(12);
    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(8);
    m_key = new QLineEdit(profile.privateKeyPath);
    m_key->setPlaceholderText(tr("C:\\Users\\…\\.ssh\\id_ed25519"));
    auto *browse = makeButton(tr("Обзор"));
    keyRow->addWidget(m_key, 1);
    keyRow->addWidget(browse);
    auto *keyBox = new QWidget;
    keyBox->setLayout(keyRow);
    keyRow->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(field(tr("Файл ключа"), keyBox));
    m_passphrase = secretField(tr("Если ключ защищён"));
    m_passphrase->setText(profile.keyPassphrase);
    keyLayout->addWidget(field(tr("Кодовая фраза"), m_passphrase));
    m_authStack->addWidget(keyPage);

    auto *agentPage = new QWidget;
    auto *agentLayout = new QVBoxLayout(agentPage);
    agentLayout->setContentsMargins(0, 0, 0, 0);
    agentLayout->addWidget(makeLabel(tr("Ключи возьмутся из запущенного SSH-агента."), 13,
                                     Theme::TextSecondary));
    m_authStack->addWidget(agentPage);
    body()->addWidget(m_authStack);

    auto *more = new QGridLayout;
    more->setHorizontalSpacing(12);
    m_path = new QLineEdit(profile.initialPath);
    m_path->setPlaceholderText(tr("Домашняя"));
    more->addWidget(field(tr("Название"), m_name), 0, 0);
    more->addWidget(field(tr("Начальная папка"), m_path), 0, 1);
    body()->addLayout(more);

    auto *looks = new QGridLayout;
    looks->setHorizontalSpacing(12);
    looks->setVerticalSpacing(8);
    auto *kinds = new QHBoxLayout;
    kinds->setSpacing(4);
    for (const HostKind kind : {HostKind::Server, HostKind::Web, HostKind::Game,
                                HostKind::Database, HostKind::Storage, HostKind::Desktop}) {
        auto *button = new KindButton(kind);
        button->setChecked(kind == m_kind);
        connect(button, &QAbstractButton::clicked, this, [this, kind, button] {
            m_kind = kind;
            for (QWidget *other : std::as_const(m_kinds))
                static_cast<QAbstractButton *>(other)->setChecked(other == button);
        });
        m_kinds.append(button);
        kinds->addWidget(button);
    }
    kinds->addStretch();
    looks->addWidget(makeLabel(tr("Тип"), 12, Theme::TextSecondary, QFont::DemiBold), 0, 0);
    looks->addLayout(kinds, 0, 1);

    auto *colors = new QHBoxLayout;
    colors->setSpacing(4);
    for (int i = 0; i < Theme::labelColorCount(); ++i) {
        auto *swatch = new Swatch(Theme::labelColor(i));
        swatch->setChecked(i == m_color);
        connect(swatch, &QAbstractButton::clicked, this, [this, i] {
            m_color = i;
            for (int j = 0; j < m_swatches.size(); ++j)
                static_cast<QAbstractButton *>(m_swatches.at(j))->setChecked(j == i);
        });
        m_swatches.append(swatch);
        colors->addWidget(swatch);
    }
    colors->addStretch();
    looks->addWidget(makeLabel(tr("Метка"), 12, Theme::TextSecondary, QFont::DemiBold), 1, 0);
    looks->addLayout(colors, 1, 1);
    looks->setColumnStretch(1, 1);
    body()->addLayout(looks);

    m_error = makeLabel(QString(), 12, Theme::Danger);
    m_error->hide();
    body()->addWidget(m_error);

    auto *cancel = makeButton(tr("Отмена"));
    auto *save = makeButton(tr("Сохранить"));
    auto *connectButton = makeButton(tr("Подключиться"), QStringLiteral("primary"));
    body()->addSpacing(2);
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(cancel);
    row->addStretch();
    row->addWidget(save);
    row->addWidget(connectButton);
    body()->addLayout(row);

    connect(m_protocol, &Segmented::changed, this, &HostEditor::applyProtocol);
    connect(m_auth, &Segmented::changed, this, &HostEditor::applyAuth);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Выберите приватный ключ"),
            QDir::homePath() + QStringLiteral("/.ssh"));
        if (!path.isEmpty())
            m_key->setText(QDir::toNativeSeparators(path));
    });
    connect(cancel, &QPushButton::clicked, this, &Sheet::dismiss);
    connect(save, &QPushButton::clicked, this, [this] { submit(false); });
    connect(connectButton, &QPushButton::clicked, this, [this] { submit(true); });
    for (QLineEdit *edit : {m_host, m_user, m_password, m_name, m_path})
        connect(edit, &QLineEdit::returnPressed, this, [this] { submit(true); });

    applyProtocol(m_protocol->current());
    applyAuth(m_auth->current());
    QTimer::singleShot(0, this, [this] {
        if (m_host->text().isEmpty())
            m_host->setFocus();
        else if (m_user->text().isEmpty())
            m_user->setFocus();
        else
            m_password->setFocus();
    });
}

void HostEditor::applyProtocol(int index)
{
    const bool ftp = index > 0;
    const int previousDefault = ftp ? 22 : 21;
    if (m_port->value() == previousDefault || m_port->value() == 0)
        m_port->setValue(ftp ? 21 : 22);
    m_authRow->setVisible(!ftp);
    if (ftp) {
        m_auth->setCurrent(0);
        applyAuth(0);
    }
    QTimer::singleShot(0, this, [this] { relayout(); });
}

void HostEditor::applyAuth(int index)
{
    for (int i = 0; i < m_authStack->count(); ++i)
        m_authStack->widget(i)->setSizePolicy(
            QSizePolicy::Preferred, i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    m_authStack->setCurrentIndex(index);
    m_authStack->adjustSize();
    QTimer::singleShot(0, this, [this] { relayout(); });
}

SavedHost HostEditor::collect() const
{
    SavedHost result = m_value;
    result.name = m_name->text().trimmed();
    result.color = m_color;
    result.kind = m_kind;
    ConnectionProfile &p = result.profile;
    bool parsed = false;
    const ConnectionProfile address = HostStore::parseAddress(m_host->text(), &parsed);
    p.host = parsed ? address.host : m_host->text().trimmed();
    p.port = static_cast<quint16>(m_port->value());
    if (parsed && m_host->text().contains(QStringLiteral(":")) && address.port)
        p.port = address.port;
    p.username = m_user->text().trimmed();
    if (p.username.isEmpty() && parsed)
        p.username = address.username;
    p.protocol = m_protocol->current() == 0 ? Protocol::Sftp : Protocol::Ftp;
    p.useTls = m_protocol->current() == 2;
    p.auth = p.protocol == Protocol::Ftp ? AuthMethod::Password
                                         : static_cast<AuthMethod>(m_auth->current());
    p.password = m_password->text();
    p.privateKeyPath = m_key->text().trimmed();
    p.keyPassphrase = m_passphrase->text();
    p.initialPath = m_path->text().trimmed();
    return result;
}

void HostEditor::submit(bool connect)
{
    const SavedHost value = collect();
    QString problem;
    if (value.profile.host.isEmpty())
        problem = tr("Укажите адрес сервера.");
    else if (value.profile.protocol == Protocol::Sftp && value.profile.username.isEmpty())
        problem = tr("Укажите пользователя.");
    else if (value.profile.auth == AuthMethod::PrivateKey &&
             value.profile.privateKeyPath.isEmpty())
        problem = tr("Выберите файл ключа.");
    if (!problem.isEmpty()) {
        m_error->setText(problem);
        m_error->show();
        relayout();
        return;
    }
    dismiss();
    if (onSave)
        onSave(value, connect);
}
