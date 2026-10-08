#pragma once

#include "sshagent/sshagentclient.h"
#include "sshagent/sshkeylist.h"

#include <QWidget>

class QAction;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

// Sidebar panel for the SSH agent: its status, the keys from the SSH
// configuration and those the user added, and which of them are loaded.
// Errors are shown inline, never in a dialog. Nothing runs while it's hidden.
class AgentPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AgentPanel(QWidget *parent = nullptr);

    void setConfigKeys(const QList<SshIdentityFile> &files);
    // Key files the user added earlier (absolute paths), e.g. from the settings.
    void setAddedKeys(const QStringList &paths);
    QStringList addedKeys() const;

    void refresh();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void scheduleRefresh();
    void onRefreshed(const SshAgentClient::Snapshot &snapshot);
    void onActionFinished(bool succeeded, const QString &message);
    void rebuild();
    void updateStatus();
    void updateActions();
    void showError(const QString &message);
    const SshKeyEntry *currentEntry() const;
    QStringList configKeyPaths() const;

    void loadCurrent();
    void removeCurrent();
    void forgetCurrent();
    void chooseKeyFile();
    void loadKey(const QString &path, const QString &name);
    void showContextMenu(const QPoint &pos);

    SshAgentClient *m_client = nullptr;
    QLabel *m_statusIcon = nullptr;
    QLabel *m_status = nullptr;
    QWidget *m_errorBar = nullptr;
    QLabel *m_errorLabel = nullptr;
    QTreeWidget *m_view = nullptr;

    QAction *m_loadAction = nullptr;
    QAction *m_removeAction = nullptr;
    QAction *m_forgetAction = nullptr;
    QAction *m_addFileAction = nullptr;
    QAction *m_refreshAction = nullptr;

    QList<SshIdentityFile> m_configKeys;
    QStringList m_addedKeys;
    SshAgentClient::Snapshot m_snapshot;
    QList<SshKeyEntry> m_entries;
    QString m_busyText; // what is running, shown instead of the status
    bool m_refreshScheduled = false;
};
