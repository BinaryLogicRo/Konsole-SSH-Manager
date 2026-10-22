#pragma once

#include "sshconfig/hoststore.h"

#include <QObject>
#include <QPointer>

#include <optional>

class HostEditDialog;
class HostSidebar;
class QWidget;

// User-facing host operations: the dialogs, confirmations and conflict checks
// around HostStore edits. Acts on the host currently selected in the sidebar.
class HostOperations : public QObject
{
    Q_OBJECT

public:
    HostOperations(HostStore *store, HostSidebar *sidebar, QWidget *window);

    void addHost();
    void editHost();
    void duplicateHost();
    void deleteHost();
    void importHost();
    // Asks before adding `Include config.d/*` to ~/.ssh/config.
    void offerIncludeLine();

    // Before opening a managed host that ssh can't see yet (no Include line),
    // offers to add it. Returns false if the user cancelled.
    bool confirmConnect(const SshHost &host);
    // Call after every reload: warns an open edit dialog if its host changed on disk.
    void checkOpenDialog();

private:
    // Runs the edit dialog modally. `originalAlias` is empty for new hosts.
    std::optional<SshHost> runHostDialog(const SshHost &host, const QString &title, const QString &originalAlias);
    bool reportResult(const HostStore::Result &result);

    HostStore *m_store = nullptr;
    HostSidebar *m_sidebar = nullptr;
    QWidget *m_window = nullptr;

    // State of the edit dialog while it's open, to detect external changes.
    QPointer<HostEditDialog> m_activeDialog;
    SshHost m_dialogSnapshot;
    QString m_dialogAlias;
};
