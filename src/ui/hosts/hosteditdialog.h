#pragma once

#include "sshconfig/sshhost.h"

#include <QDialog>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// Add/edit form for one managed Host block.
//
// Common keywords get their own fields; everything else in the block is shown
// in an "Additional options" table, in file order, so nothing is lost.
class HostEditDialog : public QDialog
{
    Q_OBJECT

public:
    // Extra check run on accept (e.g. duplicate aliases). Returns an error
    // message, or an empty string if the host is acceptable.
    using Validator = std::function<QString(const SshHost &)>;

    HostEditDialog(const SshHost &host, const QStringList &groups, Validator validator, QWidget *parent = nullptr);

    SshHost host() const;
    // Shows a warning that the file changed on disk while the dialog is open.
    void showExternalChangeWarning();

public Q_SLOTS:
    void accept() override;

private:
    void loadHost(const SshHost &host);
    void addOptionRow(const QString &keyword, const QString &value);
    void chooseIdentityFile();
    void chooseColor();
    void setColor(const QString &color);
    QString validationError(const SshHost &host) const;

    SshHost m_original;
    Validator m_validator;
    QString m_color;

    QLabel *m_externalChangeLabel = nullptr;
    QLineEdit *m_alias = nullptr;
    QLineEdit *m_hostName = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_port = nullptr;
    QLineEdit *m_identityFile = nullptr;
    QLineEdit *m_proxyJump = nullptr;
    QComboBox *m_group = nullptr;
    QPushButton *m_colorButton = nullptr;
    QLineEdit *m_note = nullptr;
    QTableWidget *m_options = nullptr;
};
