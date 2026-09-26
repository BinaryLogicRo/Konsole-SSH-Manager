#pragma once

#include "sshconfig/hoststore.h"

#include <QMainWindow>

class HostOperations;
class HostSidebar;
class QAction;
class QLabel;
class QSplitter;
class QTabWidget;
class TerminalTab;

// Splitter with the host sidebar on one side (configurable) and SSH session
// tabs on the other.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const SshPaths &paths, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupActions();
    void setupMenusAndToolBar();
    void updateActions();
    void onHostsChanged();
    void showContextMenu(const QPoint &globalPos);

    void connectToHost(const SshHost &host);
    void showEffectiveSettings();

    void setSidebarOnRight(bool onRight);
    void closeTab(int index);
    void onSessionFinished(TerminalTab *tab);
    void showAbout();
    void restoreSettings();
    void saveSettings() const;

    HostStore *m_store = nullptr;
    HostSidebar *m_sidebar = nullptr;
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_splitter = nullptr;
    QWidget *m_includeBanner = nullptr;
    HostOperations *m_operations = nullptr;

    QAction *m_addAction = nullptr;
    QAction *m_connectAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_duplicateAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_effectiveAction = nullptr;
    QAction *m_sidebarRightAction = nullptr;
    QAction *m_nextTabAction = nullptr;
    QAction *m_previousTabAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_aboutAction = nullptr;
};
