#pragma once

#include "sshagent/sshagentclient.h"
#include "sshagent/sshkeylist.h"

#include <QIcon>
#include <QString>

// User-visible texts and icons of the SSH agent panel.
namespace AgentPanelText
{
QString groupTitle(SshKeyEntry::Source source);
QString stateText(SshKeyEntry::State state);
QIcon stateIcon(SshKeyEntry::State state);
QString toolTip(const SshKeyEntry &entry);
// The agent's status; `icon` gets a theme icon name, or is cleared for none.
QString status(const SshAgentClient::Snapshot &snapshot, QString *icon);
}
