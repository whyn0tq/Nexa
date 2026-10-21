#pragma once

#include "editors.h"
#include "glass.h"

class QLineEdit;
class Segmented;

class SettingsSheet final : public Sheet {
    Q_OBJECT

public:
    explicit SettingsSheet(QWidget *host);

private:
    void select(Editors::Kind kind);
    void refresh();
    void refreshTransfers();
    void refreshLanguage();

    QVector<QWidget *> m_options;
    QVector<QWidget *> m_policies;
    QVector<QWidget *> m_languages;
    Segmented *m_connections = nullptr;
    QLineEdit *m_customPath = nullptr;
    QLabel *m_effective = nullptr;
};
