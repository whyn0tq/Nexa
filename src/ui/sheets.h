#pragma once

#include "glass.h"
#include "hoststore.h"

#include <functional>

class QCheckBox;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class Segmented;

namespace Sheets {

void confirm(QWidget *host, const QString &title, const QString &text,
             const QString &confirmText, bool danger, std::function<void()> onConfirm);
void prompt(QWidget *host, const QString &title, const QString &placeholder,
            const QString &initial, const QString &confirmText, Icon icon,
            std::function<void(const QString &)> onAccept);
void trust(QWidget *host, const QString &server, const QString &fingerprint,
           const QString &keyType, bool changed,
           std::function<void(bool accepted, bool remember)> onAnswer);

}

class HostEditor final : public Sheet {
    Q_OBJECT

public:
    HostEditor(QWidget *host, const SavedHost &value, bool isNew);

    std::function<void(const SavedHost &, bool connect)> onSave;

private:
    void applyProtocol(int index);
    void applyAuth(int index);
    void submit(bool connect);
    SavedHost collect() const;

    SavedHost m_value;
    QLineEdit *m_name = nullptr;
    Segmented *m_protocol = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    Segmented *m_auth = nullptr;
    QStackedWidget *m_authStack = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_key = nullptr;
    QLineEdit *m_passphrase = nullptr;
    QLineEdit *m_path = nullptr;
    QWidget *m_authRow = nullptr;
    QLabel *m_error = nullptr;
    int m_color = 0;
    QVector<QWidget *> m_swatches;
};
