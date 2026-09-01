#pragma once

#include "types.h"

#include <QDialog>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QStackedWidget;

class ConnectDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget *parent = nullptr);
    ConnectionProfile profile() const;
    static std::optional<ConnectionProfile> lastProfile();

private slots:
    void browsePrivateKey();
    void validateAndAccept();
    void loadRecentProfile(int index);
    void applyProtocol(int index);

private:
    void saveRecentProfile(const ConnectionProfile &profile);
    Protocol selectedProtocol() const;

    QComboBox *m_recent = nullptr;
    QComboBox *m_protocol = nullptr;
    QLabel *m_title = nullptr;
    QCheckBox *m_tls = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_username = nullptr;
    QComboBox *m_authMethod = nullptr;
    QStackedWidget *m_authStack = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_privateKey = nullptr;
    QLineEdit *m_passphrase = nullptr;
};
