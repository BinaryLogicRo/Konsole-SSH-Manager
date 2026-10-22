#pragma once

#include <QDialog>

class QPlainTextEdit;
class SshEffectiveConfigJob;
struct SshCommandResult;

// Shows what OpenSSH will actually use for an alias (`ssh -G <alias>`).
class EffectiveSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    EffectiveSettingsDialog(const QString &alias, QWidget *parent = nullptr);

private:
    void showResult(const SshCommandResult &result);

    QPlainTextEdit *m_output = nullptr;
    SshEffectiveConfigJob *m_job = nullptr;
};
