#include "effectivesettingsdialog.h"
#include "sshconfig/sshresolver.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <KLocalizedString>

EffectiveSettingsDialog::EffectiveSettingsDialog(const QString &alias, QWidget *parent)
    : QDialog(parent)
    , m_output(new QPlainTextEdit(this))
    , m_job(new SshEffectiveConfigJob(this))
{
    setWindowTitle(i18nc("@title:window, %1 is a host alias", "Effective Settings: %1", alias));

    auto *label = new QLabel(i18n("Settings OpenSSH resolves for <b>%1</b> from all configuration files (<tt>ssh -G</tt>):", alias.toHtmlEscaped()), this);
    label->setWordWrap(true);

    m_output->setReadOnly(true);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setPlainText(i18n("Running ssh…"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_output, 1);
    layout->addWidget(buttons);
    resize(640, 560);

    connect(m_job, &SshEffectiveConfigJob::finished, this, &EffectiveSettingsDialog::showResult);
    if (!m_job->start(alias)) {
        m_output->setPlainText(i18n("Could not run ssh. Make sure the OpenSSH client is installed (Debian package \"openssh-client\")."));
    }
}

void EffectiveSettingsDialog::showResult(const SshCommandResult &result)
{
    if (!result.started) {
        m_output->setPlainText(i18n("Could not run ssh. Make sure the OpenSSH client is installed (Debian package \"openssh-client\")."));
        return;
    }
    if (result.exitCode != 0) {
        const QString error = QString::fromUtf8(result.standardError).trimmed();
        m_output->setPlainText(i18n("OpenSSH reported an error:\n\n%1", error.isEmpty() ? i18n("(no error message)") : error));
        return;
    }
    m_output->setPlainText(QString::fromUtf8(result.standardOutput));
}
