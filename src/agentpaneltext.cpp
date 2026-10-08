#include "agentpaneltext.h"

#include <QPixmap>
#include <QStringList>

#include <KLocalizedString>

namespace
{
QString withDetail(const QString &text, const QString &detail)
{
    return detail.isEmpty() ? text : i18nc("status: %1 problem, %2 ssh-add's message", "%1 %2", text, detail);
}

// A theme icon that keeps its colors in a selected row. Breeze emblems are drawn
// with color scheme classes, which the selection would recolor to its plain
// text color, hiding the check mark or symbol inside them.
QIcon fixedColorIcon(const QString &name)
{
    const QIcon themed = QIcon::fromTheme(name);
    QIcon icon;
    for (const int size : {16, 22, 32}) {
        const QPixmap pixmap = themed.pixmap(size, size);
        icon.addPixmap(pixmap, QIcon::Normal);
        icon.addPixmap(pixmap, QIcon::Selected);
    }
    return icon;
}
}

QString AgentPanelText::groupTitle(SshKeyEntry::Source source)
{
    switch (source) {
    case SshKeyEntry::Source::Configuration:
        return i18nc("@title:group", "From SSH Configuration");
    case SshKeyEntry::Source::Added:
        return i18nc("@title:group", "Added Key Files");
    case SshKeyEntry::Source::Agent:
        return i18nc("@title:group", "Other Loaded Keys");
    }
    return {};
}

QString AgentPanelText::stateText(SshKeyEntry::State state)
{
    switch (state) {
    case SshKeyEntry::State::Loaded:
        return i18nc("@item key state", "Loaded");
    case SshKeyEntry::State::NotLoaded:
        return i18nc("@item key state", "Not loaded");
    case SshKeyEntry::State::Missing:
        return i18nc("@item key state", "Missing");
    case SshKeyEntry::State::Unknown:
        return i18nc("@item key state", "Unknown");
    }
    return {};
}

QIcon AgentPanelText::stateIcon(SshKeyEntry::State state)
{
    switch (state) {
    case SshKeyEntry::State::Loaded:
        return fixedColorIcon(QStringLiteral("emblem-checked"));
    case SshKeyEntry::State::Missing:
        return fixedColorIcon(QStringLiteral("emblem-warning"));
    case SshKeyEntry::State::Unknown:
        return fixedColorIcon(QStringLiteral("emblem-question"));
    case SshKeyEntry::State::NotLoaded:
        break;
    }
    return {};
}

QString AgentPanelText::toolTip(const SshKeyEntry &entry)
{
    QStringList lines;
    if (!entry.path.isEmpty()) {
        lines << i18nc("@info:tooltip", "File: %1", entry.path);
    }
    if (!entry.comment.isEmpty()) {
        lines << i18nc("@info:tooltip", "Comment: %1", entry.comment);
    }
    if (!entry.fingerprint.isEmpty()) {
        lines << i18nc("@info:tooltip", "Fingerprint: %1", entry.fingerprint);
    }
    if (entry.state == SshKeyEntry::State::Missing) {
        lines << i18nc("@info:tooltip", "The key file can't be found.");
    } else if (entry.state == SshKeyEntry::State::Unknown) {
        lines << i18nc("@info:tooltip", "The key file's fingerprint can't be read, so it isn't known whether the key is loaded.");
    }
    return lines.join(QLatin1Char('\n'));
}

QString AgentPanelText::status(const SshAgentClient::Snapshot &snapshot, QString *icon)
{
    icon->clear();
    switch (snapshot.status) {
    case SshAgentClient::Status::Unknown:
        return i18n("Checking the SSH agent…");
    case SshAgentClient::Status::NoAgent:
        *icon = QStringLiteral("dialog-warning");
        return i18n("No SSH agent is running in this session, so keys can't be loaded.");
    case SshAgentClient::Status::NoTool:
        *icon = QStringLiteral("dialog-error");
        return i18n("ssh-add can't be run. Make sure the openssh-client package is installed.");
    case SshAgentClient::Status::Unreachable:
        *icon = QStringLiteral("dialog-warning");
        return withDetail(i18n("The SSH agent can't be reached."), snapshot.message);
    case SshAgentClient::Status::NotResponding:
        *icon = QStringLiteral("dialog-warning");
        return i18n("The SSH agent doesn't respond.");
    case SshAgentClient::Status::Failed:
        *icon = QStringLiteral("dialog-error");
        return withDetail(i18n("The SSH agent reported an error."), snapshot.message);
    case SshAgentClient::Status::Available:
        *icon = QStringLiteral("security-high");
        return snapshot.keys.isEmpty()
            ? i18n("The SSH agent is running and holds no keys.")
            : i18np("The SSH agent is running and holds one key.", "The SSH agent is running and holds %1 keys.", int(snapshot.keys.size()));
    }
    return {};
}
