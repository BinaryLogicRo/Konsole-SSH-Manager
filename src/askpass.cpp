#include "askpass.h"
#include "sshagent/sshagentclient.h"

#include <KLocalizedString>
#include <KPasswordDialog>

#include <cstdio>

bool Askpass::isRequested()
{
    return qEnvironmentVariable(SshAgentClient::AskpassModeVariable) == QLatin1String("1");
}

int Askpass::run(const QString &prompt)
{
    // Loading a key only asks for passphrases; refuse confirmations and notices.
    if (!qEnvironmentVariableIsEmpty("SSH_ASKPASS_PROMPT")) {
        return 1;
    }
    // No ShowKeepPassword flag: passphrases are never remembered.
    KPasswordDialog dialog;
    dialog.setWindowTitle(i18nc("@title:window", "SSH Key Passphrase"));
    dialog.setPrompt(prompt.trimmed()); // ssh-add's prompt names the key file
    if (dialog.exec() != QDialog::Accepted) {
        return 1;
    }
    QByteArray answer = dialog.password().toUtf8() + '\n';
    const bool written = std::fwrite(answer.constData(), 1, size_t(answer.size()), stdout) == size_t(answer.size()) && std::fflush(stdout) == 0;
    answer.fill('\0');
    return written ? 0 : 1;
}
