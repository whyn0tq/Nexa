#include "connectdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <limits>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#endif

namespace {
ConnectionProfile normalizedProfile(ConnectionProfile value)
{
    QString endpoint = value.host.trimmed();
    if (!endpoint.contains(QStringLiteral("://")))
        return value;

    const QUrl url(endpoint);
    if (!url.isValid() || url.host().isEmpty())
        return value;

    value.host = url.host();
    if (url.port() > 0)
        value.port = static_cast<quint16>(url.port());
    if (value.username.isEmpty() && !url.userName().isEmpty())
        value.username = QUrl::fromPercentEncoding(url.userName().toUtf8());
    return value;
}

QString protectSecret(const QString &value)
{
#ifdef Q_OS_WIN
    if (value.isEmpty())
        return {};
    QByteArray plain = value.toUtf8();
    if (plain.size() >
        static_cast<qsizetype>(std::numeric_limits<DWORD>::max()))
        return {};
    DATA_BLOB input{
        static_cast<DWORD>(plain.size()),
        reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))};
    DATA_BLOB output{};
    const BOOL protectedSuccessfully =
        CryptProtectData(&input, L"Nexa credential", nullptr,
                         nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                         &output);
    plain.fill('\0');
    if (!protectedSuccessfully)
        return {};
    const QByteArray protectedData(
        reinterpret_cast<const char *>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return QString::fromLatin1(protectedData.toBase64());
#else
    Q_UNUSED(value);
    return {};
#endif
}

QString unprotectSecret(const QString &value)
{
#ifdef Q_OS_WIN
    const QByteArray protectedData =
        QByteArray::fromBase64(value.toLatin1());
    if (protectedData.isEmpty() ||
        protectedData.size() >
            static_cast<qsizetype>(std::numeric_limits<DWORD>::max()))
        return {};
    DATA_BLOB input{
        static_cast<DWORD>(protectedData.size()),
        reinterpret_cast<BYTE *>(
            const_cast<char *>(protectedData.constData()))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
        return {};
    const QString result = QString::fromUtf8(
        reinterpret_cast<const char *>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(value);
    return {};
#endif
}

ConnectionProfile profileFromObject(const QJsonObject &object)
{
    ConnectionProfile value;
    value.host = object.value(QStringLiteral("host")).toString();
    value.port = static_cast<quint16>(qBound(
        1, object.value(QStringLiteral("port")).toInt(22), 65535));
    value.username =
        object.value(QStringLiteral("username")).toString();
    value.usePrivateKey =
        object.value(QStringLiteral("usePrivateKey")).toBool();
    value.privateKeyPath =
        object.value(QStringLiteral("privateKeyPath")).toString();
    value.password = unprotectSecret(
        object.value(QStringLiteral("passwordProtected")).toString());
    value.keyPassphrase = unprotectSecret(
        object.value(QStringLiteral("keyPassphraseProtected")).toString());
    return normalizedProfile(value);
}
}

ConnectDialog::ConnectDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Новое подключение"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(tr("Подключение к SFTP-серверу"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);

    m_recent = new QComboBox;
    m_recent->addItem(tr("Новое подключение"));

    QSettings settings;
    const auto profiles = QJsonDocument::fromJson(
        settings.value(QStringLiteral("recentProfiles")).toByteArray()).array();
    for (const auto &value : profiles) {
        const auto object = value.toObject();
        m_recent->addItem(
            QStringLiteral("%1@%2:%3")
                .arg(object.value(QStringLiteral("username")).toString(),
                     object.value(QStringLiteral("host")).toString())
                .arg(object.value(QStringLiteral("port")).toInt(22)),
            object);
    }
    connect(m_recent, &QComboBox::currentIndexChanged,
            this, &ConnectDialog::loadRecentProfile);

    m_host = new QLineEdit;
    m_host->setPlaceholderText(QStringLiteral("sftp.example.com или sftp://sftp.example.com:22"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_username = new QLineEdit;
    m_username->setPlaceholderText(tr("Имя пользователя"));

    m_authMethod = new QComboBox;
    m_authMethod->addItem(tr("Пароль"));
    m_authMethod->addItem(tr("Приватный ключ"));

    m_password = new QLineEdit;
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Пароль"));

    auto *passwordPage = new QWidget;
    auto *passwordLayout = new QFormLayout(passwordPage);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->addRow(tr("Пароль"), m_password);

    m_privateKey = new QLineEdit;
    auto *browse = new QPushButton(tr("Обзор…"));
    connect(browse, &QPushButton::clicked, this, &ConnectDialog::browsePrivateKey);
    auto *keyRow = new QWidget;
    auto *keyRowLayout = new QHBoxLayout(keyRow);
    keyRowLayout->setContentsMargins(0, 0, 0, 0);
    keyRowLayout->addWidget(m_privateKey);
    keyRowLayout->addWidget(browse);

    m_passphrase = new QLineEdit;
    m_passphrase->setEchoMode(QLineEdit::Password);
    m_passphrase->setPlaceholderText(tr("Необязательно"));

    auto *keyPage = new QWidget;
    auto *keyLayout = new QFormLayout(keyPage);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addRow(tr("Приватный ключ"), keyRow);
    keyLayout->addRow(tr("Кодовая фраза"), m_passphrase);

    m_authStack = new QStackedWidget;
    m_authStack->addWidget(passwordPage);
    m_authStack->addWidget(keyPage);
    connect(m_authMethod, &QComboBox::currentIndexChanged,
            m_authStack, &QStackedWidget::setCurrentIndex);

    auto *form = new QFormLayout;
    form->addRow(tr("Недавние"), m_recent);
    form->addRow(tr("Сервер"), m_host);
    form->addRow(tr("Порт"), m_port);
    form->addRow(tr("Пользователь"), m_username);
    form->addRow(tr("Аутентификация"), m_authMethod);
    form->addRow(QString(), m_authStack);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Подключиться"));
    connect(buttons, &QDialogButtonBox::accepted,
            this, &ConnectDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

ConnectionProfile ConnectDialog::profile() const
{
    ConnectionProfile result;
    result.host = m_host->text().trimmed();
    result.port = static_cast<quint16>(m_port->value());
    result.username = m_username->text().trimmed();
    result.password = m_password->text();
    result.privateKeyPath = m_privateKey->text().trimmed();
    result.keyPassphrase = m_passphrase->text();
    result.usePrivateKey = m_authMethod->currentIndex() == 1;
    return normalizedProfile(result);
}

std::optional<ConnectionProfile> ConnectDialog::lastProfile()
{
    QSettings settings;
    const QJsonArray profiles = QJsonDocument::fromJson(
        settings.value(QStringLiteral("recentProfiles")).toByteArray())
                                    .array();
    if (profiles.isEmpty())
        return std::nullopt;
    ConnectionProfile value =
        profileFromObject(profiles.first().toObject());
    if (value.host.isEmpty() || value.username.isEmpty() ||
        (!value.usePrivateKey && value.password.isEmpty()))
        return std::nullopt;
    return value;
}

void ConnectDialog::browsePrivateKey()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Выберите приватный ключ"), QString(),
        tr("Ключи SSH (*)"));
    if (!path.isEmpty())
        m_privateKey->setText(path);
}

void ConnectDialog::validateAndAccept()
{
    const auto value = profile();
    if (value.host.isEmpty() || value.username.isEmpty()) {
        QMessageBox::warning(this, tr("Недостаточно данных"),
                             tr("Укажите сервер и имя пользователя."));
        return;
    }
    if (value.usePrivateKey && value.privateKeyPath.isEmpty()) {
        QMessageBox::warning(this, tr("Не выбран ключ"),
                             tr("Выберите файл приватного ключа."));
        return;
    }
    saveRecentProfile(value);
    accept();
}

void ConnectDialog::loadRecentProfile(int index)
{
    if (index <= 0)
        return;
    const auto object = m_recent->itemData(index).toJsonObject();
    const ConnectionProfile value = profileFromObject(object);
    m_host->setText(value.host);
    m_port->setValue(value.port);
    m_username->setText(value.username);
    m_authMethod->setCurrentIndex(value.usePrivateKey ? 1 : 0);
    m_privateKey->setText(value.privateKeyPath);
    m_password->setText(value.password);
    m_passphrase->setText(value.keyPassphrase);
}

void ConnectDialog::saveRecentProfile(const ConnectionProfile &value)
{
    QSettings settings;
    auto profiles = QJsonDocument::fromJson(
        settings.value(QStringLiteral("recentProfiles")).toByteArray()).array();

    QJsonArray updated;
    QJsonObject current{
        {QStringLiteral("host"), value.host},
        {QStringLiteral("port"), value.port},
        {QStringLiteral("username"), value.username},
        {QStringLiteral("usePrivateKey"), value.usePrivateKey},
        {QStringLiteral("privateKeyPath"), value.privateKeyPath},
        {QStringLiteral("passwordProtected"),
         protectSecret(value.password)},
        {QStringLiteral("keyPassphraseProtected"),
         protectSecret(value.keyPassphrase)}
    };
    updated.append(current);
    for (const auto &item : profiles) {
        const auto object = item.toObject();
        const bool duplicate =
            object.value(QStringLiteral("host")).toString() == value.host &&
            object.value(QStringLiteral("port")).toInt() == value.port &&
            object.value(QStringLiteral("username")).toString() == value.username;
        if (!duplicate && updated.size() < 8)
            updated.append(object);
    }
    settings.setValue(QStringLiteral("recentProfiles"),
                      QJsonDocument(updated).toJson(QJsonDocument::Compact));
}
