#pragma once

#include "sshconfig/hoststore.h"

#include <QList>
#include <QMainWindow>

class AgentPanel;
class HostOperations;
class HostSidebar;
class QAction;
class QLabel;
class QSplitter;
class QTabWidget;
class SessionFailureListener;
class TerminalTab;

// Splitter with the sidebar on one side (configurable) and SSH session tabs on
// the other. The sidebar holds the host tree and, below it, the SSH agent panel.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const SshPaths &paths, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupActions();
    void setupMenusAndToolBar();
    void updateActions();
    void onHostsChanged();
    void showContextMenu(const QPoint &globalPos);

    void connectToHost(const SshHost &host);
    void showEffectiveSettings();
    void copyCommandLine();

    void setSidebarOnRight(bool onRight);
    void closeTab(int index);
    void removeTerminalTab(int index);
    void onCurrentTabChanged(int index);
    void updateTabCloseButtons();
    void onSessionFinished(TerminalTab *tab);
    void onSessionFailed(qint64 helperPid);
    void showAbout();
    void restoreSettings();
    void saveSettings() const;

    HostStore *m_store = nullptr;
    HostSidebar *m_sidebar = nullptr;
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_splitter = nullptr;
    QSplitter *m_sidebarSplitter = nullptr; // host sidebar above the agent panel
    AgentPanel *m_agentPanel = nullptr;
    QWidget *m_includeBanner = nullptr;
    QWidget *m_loadErrorBanner = nullptr; // SSH config files that couldn't be read
    QLabel *m_loadErrorLabel = nullptr;
    HostOperations *m_operations = nullptr;
    SessionFailureListener *m_sessionFailures = nullptr;
    QList<QWidget *> m_tabHistory; // tabs, most recently shown first

    QAction *m_addAction = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_duplicateAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_effectiveAction = nullptr;
    QAction *m_copyCommandAction = nullptr;
    QAction *m_sidebarRightAction = nullptr;
    QAction *m_agentPanelAction = nullptr;
    QAction *m_activeTabCloseButtonAction = nullptr;
    QAction *m_nextTabAction = nullptr;
    QAction *m_previousTabAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_aboutAction = nullptr;

    bool m_quitRequested = false; // set by File > Quit, so closing quits instead of minimizing
};
