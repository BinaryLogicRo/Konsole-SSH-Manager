#pragma once

#include <QStringList>

namespace SshKeywords
{
// Client keywords the editor suggests. Every entry is accepted by OpenSSH 9.2
// (Debian 12). Keywords only newer versions understand (e.g. Tag,
// ChannelTimeout, ObscureKeystrokeTiming) are deliberately left out; they are
// still preserved when already present in a file.
const QStringList &offered();
}
