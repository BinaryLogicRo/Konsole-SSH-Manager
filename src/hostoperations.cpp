#include "hostoperations.h"
#include "dialogs/hosteditdialog.h"
#include "hostsidebar.h"

#include <QMessageBox>
#include <QPushButton>

#include <KLocalizedString>

HostOperations::HostOperations(HostStore *store, HostSidebar *sidebar, QWidget *window)
    : QObject(window)
    , m_store(store)
    , m_sidebar(sidebar)
    , m_window(window)
{
}

void HostOperations::addHost()
{
    SshHost host;
    host.readOnly = false;
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (current && !current->readOnly) {
        host.setMetadataValue(QStringLiteral("group"), current->group());
    }
    const std::optional<SshHost> edited = runHostDialog(host, i18nc("@title:window", "Add Host"), QString());
    if (edited && reportResult(m_store->addHost(*edited)) && !m_store->isManagedFileIncluded()) {
        offerIncludeLine();
    }
}

void HostOperations::editHost()
{
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (!current || current->readOnly || !current->isConnectable()) {
        return;
    }
    const QString alias = current->alias();
    const std::optional<SshHost> edited = runHostDialog(*current, i18nc("@title:window", "Edit Host"), alias);
    if (!edited) {
        return;
    }

    // Never silently overwrite changes another program made meanwhile.
    const std::optional<SshHost> onDisk = m_store->readManagedHost(alias);
    if (!onDisk) {
        const auto answer =
            QMessageBox::question(m_window,
                                  i18nc("@title:window", "Host Removed"),
                                  i18n("%1 was removed from the file by another program while you were editing it. Save your version as a new host?", alias));
        if (answer == QMessageBox::Yes) {
            reportResult(m_store->addHost(*edited));
        }
        return;
    }
    if (!onDisk->hasSameContent(*current)) {
        const auto answer =
            QMessageBox::warning(m_window,
                                 i18nc("@title:window", "Host Changed on Disk"),
                                 i18n("%1 was changed by another program while you were editing it. Overwrite those changes with yours?", alias),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    reportResult(m_store->updateHost(alias, *edited));
}

void HostOperations::duplicateHost()
{
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (!current || current->readOnly || !current->isConnectable()) {
        return;
    }
    SshHost copy = *current;
    copy.patterns = QStringList{current->alias() + QStringLiteral("-copy")};
    const std::optional<SshHost> edited = runHostDialog(copy, i18nc("@title:window", "Duplicate Host"), QString());
    if (edited) {
        reportResult(m_store->addHost(*edited));
    }
}

void HostOperations::deleteHost()
{
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (!current || current->readOnly || !current->isConnectable()) {
        return;
    }
    const auto answer =
        QMessageBox::question(m_window, i18nc("@title:window", "Delete Host"), i18n("Delete %1 from %2?", current->alias(), m_store->paths().managedFile));
    if (answer == QMessageBox::Yes) {
        reportResult(m_store->removeHost(current->alias()));
    }
}

void HostOperations::importHost()
{
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (!current || !current->readOnly || !current->isConnectable()) {
        return;
    }
    SshHost copy = *current;
    copy.readOnly = false;
    copy.patterns.clear();
    for (const QString &pattern : current->patterns) {
        if (SshValidation::isConcretePattern(pattern)) {
            copy.patterns.append(pattern);
        }
    }
    const std::optional<SshHost> edited = runHostDialog(copy, i18nc("@title:window", "Import Host"), QString());
    if (!edited || !reportResult(m_store->addHost(*edited))) {
        return;
    }
    QMessageBox::information(m_window,
                             i18nc("@title:window", "Host Imported"),
                             i18n("A copy of %1 was added to %2. The original in %3 was left unchanged; ssh uses whichever definition it reads first.",
                                  edited->alias(),
                                  m_store->paths().managedFile,
                                  current->sourceFile));
    if (!m_store->isManagedFileIncluded()) {
        offerIncludeLine();
    }
}

void HostOperations::offerIncludeLine()
{
    const SshPaths &paths = m_store->paths();
    const auto answer =
        QMessageBox::question(m_window,
                              i18nc("@title:window", "Include Managed Hosts"),
                              i18n("ssh can't use hosts stored in %1 until %2 includes it.\n\n"
                                   "Add the line \"Include %3\" at the top of %2? Nothing else in that file is changed, and a backup is made first.",
                                   paths.managedFile,
                                   paths.userConfig,
                                   SshPaths::includeArgument()));
    if (answer == QMessageBox::Yes) {
        reportResult(m_store->addIncludeLine());
    }
}

bool HostOperations::confirmConnect(const SshHost &host)
{
    const QString alias = host.alias();
    if (!host.readOnly && !m_store->isManagedFileIncluded()) {
        QMessageBox box(QMessageBox::Warning,
                        i18nc("@title:window", "Host Not Visible to ssh"),
                        i18n("%1 is stored in %2, which %3 does not include yet, so ssh will not find it.\n\n"
                             "Add the line \"Include %4\" at the top of %3? Nothing else in that file is changed, and a backup is made first.",
                             alias,
                             m_store->paths().managedFile,
                             m_store->paths().userConfig,
                             SshPaths::includeArgument()),
                        QMessageBox::NoButton,
                        m_window);
        QPushButton *addButton = box.addButton(i18nc("@action:button", "Add Include Line and Connect"), QMessageBox::AcceptRole);
        QPushButton *anywayButton = box.addButton(i18nc("@action:button", "Connect Anyway"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == addButton) {
            if (!reportResult(m_store->addIncludeLine())) {
                return false;
            }
        } else if (box.clickedButton() != anywayButton) {
            return false;
        }
    }

    return true;
}

void HostOperations::checkOpenDialog()
{
    if (m_activeDialog && !m_dialogAlias.isEmpty()) {
        const std::optional<SshHost> onDisk = m_store->readManagedHost(m_dialogAlias);
        if (!onDisk || !onDisk->hasSameContent(m_dialogSnapshot)) {
            m_activeDialog->showExternalChangeWarning();
        }
    }
}

std::optional<SshHost> HostOperations::runHostDialog(const SshHost &host, const QString &title, const QString &originalAlias)
{
    const auto validator = [this, originalAlias](const SshHost &edited) -> QString {
        const QString alias = edited.alias();
        const bool renamed = alias.compare(originalAlias, Qt::CaseInsensitive) != 0;
        if (!renamed) {
            return {};
        }
        for (const SshHost &other : m_store->hosts()) {
            if (!other.readOnly && other.kind == SshHost::Kind::Host && other.patterns.contains(alias, Qt::CaseInsensitive)) {
                return i18n("A host named %1 already exists in the managed file.", alias);
            }
        }
        return {};
    };

    HostEditDialog dialog(host, m_sidebar->managedGroupNames(), validator, m_window);
    dialog.setWindowTitle(title);
    m_activeDialog = &dialog;
    m_dialogSnapshot = host;
    m_dialogAlias = originalAlias;
    const bool accepted = dialog.exec() == QDialog::Accepted;
    m_activeDialog = nullptr;
    m_dialogAlias.clear();
    if (!accepted) {
        return std::nullopt;
    }
    return dialog.host();
}

bool HostOperations::reportResult(const HostStore::Result &result)
{
    QString message;
    switch (result.error) {
    case HostStore::Error::None:
        return true;
    case HostStore::Error::InvalidHost:
        message = i18n("The host entry is not valid and was not saved.");
        break;
    case HostStore::Error::AliasExists:
        message = i18n("A host named %1 already exists in the managed file.", result.detail);
        break;
    case HostStore::Error::HostNotFound:
        message = i18n("%1 no longer exists in the managed file.", result.detail);
        break;
    case HostStore::Error::ReadFailed:
        message = i18n("Could not read the configuration file: %1", result.detail);
        break;
    case HostStore::Error::WriteFailed:
        message = i18n("Could not save the configuration file: %1", result.detail);
        break;
    }
    QMessageBox::warning(m_window, i18nc("@title:window", "Konsole SSH Manager"), message);
    return false;
}
