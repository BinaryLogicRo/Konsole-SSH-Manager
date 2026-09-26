#include "mainwindow.h"
#include "dialogs/effectivesettingsdialog.h"
#include "hostoperations.h"
#include "hostsidebar.h"
#include "models/hosttreemodel.h"
#include "terminaltab.h"

#include <QAction>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <KAboutData>
#include <KLocalizedString>

#include <algorithm>

namespace
{
QSettings appSettings()
{
    return QSettings(QStringLiteral("konsole-ssh-manager"), QStringLiteral("konsole-ssh-manager"));
}
}

MainWindow::MainWindow(const SshPaths &paths, QWidget *parent)
    : QMainWindow(parent)
    , m_store(new HostStore(paths, this))
    , m_sidebar(new HostSidebar(this))
    , m_tabs(new QTabWidget(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_operations(new HostOperations(m_store, m_sidebar, this))
{
    setWindowTitle(i18nc("@title:window", "Konsole SSH Manager"));

    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_tabs);
    m_splitter->setChildrenCollapsible(false);

    m_includeBanner = new QWidget(this);
    auto *bannerLabel = new QLabel(
        i18n("Hosts you add here are stored in %1, which your %2 does not include yet, so ssh can't find them.", paths.managedFile, paths.userConfig),
        m_includeBanner);
    bannerLabel->setWordWrap(true);
    auto *bannerButton = new QPushButton(i18nc("@action:button", "Add Include Line…"), m_includeBanner);
    auto *bannerLayout = new QHBoxLayout(m_includeBanner);
    auto *bannerIcon = new QLabel(m_includeBanner);
    bannerIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(22, 22));
    bannerLayout->addWidget(bannerIcon);
    bannerLayout->addWidget(bannerLabel, 1);
    bannerLayout->addWidget(bannerButton);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->addWidget(m_includeBanner);
    centralLayout->addWidget(m_splitter, 1);
    setCentralWidget(central);

    setupActions();
    setupMenusAndToolBar();

    connect(bannerButton, &QPushButton::clicked, m_operations, &HostOperations::offerIncludeLine);
    connect(m_store, &HostStore::hostsChanged, this, &MainWindow::onHostsChanged);
    connect(m_sidebar, &HostSidebar::hostActivated, this, &MainWindow::connectToHost);
    connect(m_sidebar, &HostSidebar::currentHostChanged, this, &MainWindow::updateActions);
    connect(m_sidebar, &HostSidebar::contextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::updateTabCloseButtons);
    connect(m_tabs->tabBar(), &QTabBar::tabMoved, this, &MainWindow::updateTabCloseButtons);

    restoreSettings();
    onHostsChanged();
}

MainWindow::~MainWindow()
{
    const QList<TerminalTab *> tabs = findChildren<TerminalTab *>();
    for (TerminalTab *tab : tabs) {
        disconnect(tab, nullptr, this, nullptr);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tabs->count() > 0) {
        const auto answer = QMessageBox::question(
            this,
            i18nc("@title:window", "Quit"),
            i18np("There is one open SSH session. Close it and quit?", "There are %1 open SSH sessions. Close them and quit?", m_tabs->count()));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    saveSettings();
    event->accept();
}

void MainWindow::setupActions()
{
    const auto make = [this](const QString &icon, const QString &text, void (HostOperations::*slot)()) {
        auto *action = new QAction(QIcon::fromTheme(icon), text, this);
        connect(action, &QAction::triggered, m_operations, slot);
        return action;
    };
    // Shortcuts avoid plain Ctrl+<key> combinations, which the terminal needs.
    m_addAction = make(QStringLiteral("list-add"), i18nc("@action", "&Add Host…"), &HostOperations::addHost);
    m_addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    m_connectAction = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), i18nc("@action", "&Connect"), this);
    connect(m_connectAction, &QAction::triggered, this, [this] {
        if (const auto host = m_sidebar->currentHost()) {
            connectToHost(*host);
        }
    });
    m_editAction = make(QStringLiteral("document-edit"), i18nc("@action", "&Edit Host…"), &HostOperations::editHost);
    m_duplicateAction = make(QStringLiteral("edit-copy"), i18nc("@action", "D&uplicate Host…"), &HostOperations::duplicateHost);
    m_deleteAction = make(QStringLiteral("edit-delete"), i18nc("@action", "&Delete Host"), &HostOperations::deleteHost);
    m_importAction = make(QStringLiteral("document-import"), i18nc("@action", "&Import to Managed File…"), &HostOperations::importHost);
    m_effectiveAction = new QAction(QIcon::fromTheme(QStringLiteral("dialog-information")), i18nc("@action", "Show Effective &Settings"), this);
    connect(m_effectiveAction, &QAction::triggered, this, &MainWindow::showEffectiveSettings);

    // Editing shortcuts only apply while the sidebar has focus, never in a terminal.
    m_editAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_deleteAction->setShortcut(QKeySequence(QKeySequence::Delete));
    for (QAction *action : {m_editAction, m_deleteAction}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_sidebar->addAction(action);
    }

    m_sidebarRightAction = new QAction(i18nc("@action", "Sidebar on &Right"), this);
    m_sidebarRightAction->setCheckable(true);
    connect(m_sidebarRightAction, &QAction::toggled, this, &MainWindow::setSidebarOnRight);

    m_activeTabCloseButtonAction = new QAction(i18nc("@action", "Close Button on &Active Tab Only"), this);
    m_activeTabCloseButtonAction->setCheckable(true);
    connect(m_activeTabCloseButtonAction, &QAction::toggled, this, &MainWindow::updateTabCloseButtons);

    m_nextTabAction = new QAction(QIcon::fromTheme(QStringLiteral("go-next")), i18nc("@action", "&Next Tab"), this);
    m_nextTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+PgDown")));
    connect(m_nextTabAction, &QAction::triggered, this, [this] {
        if (m_tabs->count() > 0) {
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
        }
    });
    m_previousTabAction = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")), i18nc("@action", "&Previous Tab"), this);
    m_previousTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+PgUp")));
    connect(m_previousTabAction, &QAction::triggered, this, [this] {
        if (m_tabs->count() > 0) {
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count());
        }
    });

    m_quitAction = new QAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18nc("@action", "&Quit"), this);
    m_quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Q")));
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_aboutAction = new QAction(QIcon::fromTheme(QStringLiteral("help-about")), i18nc("@action", "&About Konsole SSH Manager"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupMenusAndToolBar()
{
    QMenu *fileMenu = menuBar()->addMenu(i18nc("@title:menu", "&File"));
    fileMenu->addAction(m_addAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *hostMenu = menuBar()->addMenu(i18nc("@title:menu", "&Host"));
    for (QAction *action : {m_connectAction, m_editAction, m_duplicateAction, m_deleteAction, m_importAction, m_effectiveAction}) {
        hostMenu->addAction(action);
    }

    QMenu *viewMenu = menuBar()->addMenu(i18nc("@title:menu", "&View"));
    viewMenu->addAction(m_sidebarRightAction);
    viewMenu->addAction(m_activeTabCloseButtonAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_nextTabAction);
    viewMenu->addAction(m_previousTabAction);

    QMenu *helpMenu = menuBar()->addMenu(i18nc("@title:menu", "&Help"));
    helpMenu->addAction(m_aboutAction);

    QToolBar *toolBar = addToolBar(i18nc("@title:window", "Main Toolbar"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    for (QAction *action : {m_connectAction, m_addAction, m_editAction, m_deleteAction}) {
        toolBar->addAction(action);
    }
}

void MainWindow::updateActions()
{
    const std::optional<SshHost> host = m_sidebar->currentHost();
    const bool connectable = host && host->isConnectable();
    const bool editable = connectable && !host->readOnly;
    m_connectAction->setEnabled(connectable);
    m_editAction->setEnabled(editable);
    m_duplicateAction->setEnabled(editable);
    m_deleteAction->setEnabled(editable);
    m_importAction->setEnabled(connectable && host->readOnly);
    m_effectiveAction->setEnabled(connectable);
}

void MainWindow::onHostsChanged()
{
    m_sidebar->setHosts(m_store->hosts(), m_store->paths().homeDir);

    const bool hasManagedHosts = std::any_of(m_store->hosts().cbegin(), m_store->hosts().cend(), [](const SshHost &host) {
        return !host.readOnly;
    });
    m_includeBanner->setVisible(hasManagedHosts && !m_store->isManagedFileIncluded());
    if (!m_store->loadErrors().isEmpty()) {
        statusBar()->showMessage(m_store->loadErrors().join(QLatin1Char(' ')));
    }

    m_operations->checkOpenDialog();
    updateActions();
}

void MainWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    for (QAction *action : {m_connectAction, m_editAction, m_duplicateAction, m_deleteAction, m_importAction, m_effectiveAction}) {
        if (action->isEnabled()) {
            menu.addAction(action);
        }
    }
    menu.addSeparator();
    menu.addAction(m_addAction);
    menu.exec(globalPos);
}

void MainWindow::connectToHost(const SshHost &host)
{
    const QString alias = host.alias();
    if (!host.isConnectable() || !SshValidation::isValidAlias(alias)) {
        return;
    }
    if (!m_operations->confirmConnect(host)) {
        return;
    }

    QString error;
    TerminalTab *tab = TerminalTab::create(alias, m_tabs, &error);
    if (!tab) {
        QMessageBox::critical(this, i18nc("@title:window", "Konsole Not Available"), error);
        return;
    }
    connect(tab, &TerminalTab::sessionFinished, this, &MainWindow::onSessionFinished);

    int sameAlias = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        const auto *other = qobject_cast<TerminalTab *>(m_tabs->widget(i));
        if (other && other->alias() == alias) {
            ++sameAlias;
        }
    }
    const QString title = sameAlias == 0 ? alias : i18nc("tab title: %1 host alias, %2 session number", "%1 (%2)", alias, sameAlias + 1);
    const int index = m_tabs->addTab(tab, HostTreeModel::colorIcon(host.color()), title);
    m_tabs->setTabToolTip(index, host.optionValue(u"HostName").isEmpty() ? alias : host.optionValue(u"HostName"));
    m_tabs->setCurrentIndex(index);
    updateTabCloseButtons();
    tab->focusTerminal();
}

void MainWindow::showEffectiveSettings()
{
    const std::optional<SshHost> current = m_sidebar->currentHost();
    if (!current || !current->isConnectable()) {
        return;
    }
    auto *dialog = new EffectiveSettingsDialog(current->alias(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::setSidebarOnRight(bool onRight)
{
    const int sidebarIndex = m_splitter->indexOf(m_sidebar);
    if ((sidebarIndex == 1) == onRight) {
        return;
    }
    QList<int> sizes = m_splitter->sizes();
    std::reverse(sizes.begin(), sizes.end());
    m_splitter->insertWidget(onRight ? 1 : 0, m_sidebar);
    m_splitter->setStretchFactor(m_splitter->indexOf(m_sidebar), 0);
    m_splitter->setStretchFactor(m_splitter->indexOf(m_tabs), 1);
    m_splitter->setSizes(sizes);
}

void MainWindow::closeTab(int index)
{
    auto *tab = qobject_cast<TerminalTab *>(m_tabs->widget(index));
    m_tabs->removeTab(index);
    if (tab) {
        disconnect(tab, nullptr, this, nullptr);
        tab->deleteLater();
    }
}

// Either every tab or only the visible one shows a close button (View menu).
void MainWindow::updateTabCloseButtons()
{
    QTabBar *tabBar = m_tabs->tabBar();
    const bool activeOnly = m_activeTabCloseButtonAction->isChecked();
    const auto side = static_cast<QTabBar::ButtonPosition>(tabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, tabBar));
    for (int i = 0; i < tabBar->count(); ++i) {
        if (QWidget *button = tabBar->tabButton(i, side)) {
            button->setVisible(!activeOnly || i == tabBar->currentIndex());
        }
    }
}

void MainWindow::onSessionFinished(TerminalTab *tab)
{
    const int index = m_tabs->indexOf(tab);
    if (index >= 0) {
        m_tabs->removeTab(index);
    }
    statusBar()->showMessage(i18n("Session to %1 ended.", tab->alias()), 5000);
    tab->deleteLater();
}

void MainWindow::showAbout()
{
    const KAboutData about = KAboutData::applicationData();
    QMessageBox::about(this,
                       i18nc("@title:window", "About %1", about.displayName()),
                       i18n("<h3>%1 %2</h3><p>%3</p>", about.displayName(), about.version(), about.shortDescription()));
}

void MainWindow::restoreSettings()
{
    QSettings settings = appSettings();
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({260, 900});
    m_sidebarRightAction->setChecked(settings.value(QStringLiteral("sidebar/onRight"), false).toBool());
    m_activeTabCloseButtonAction->setChecked(settings.value(QStringLiteral("tabs/closeButtonOnActiveTabOnly"), false).toBool());
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    const QByteArray splitterState = settings.value(QStringLiteral("window/splitter")).toByteArray();
    if (!splitterState.isEmpty()) {
        m_splitter->restoreState(splitterState);
    }
    if (settings.value(QStringLiteral("window/geometry")).isNull()) {
        resize(1200, 760);
    }
}

void MainWindow::saveSettings() const
{
    QSettings settings = appSettings();
    settings.setValue(QStringLiteral("sidebar/onRight"), m_sidebarRightAction->isChecked());
    settings.setValue(QStringLiteral("tabs/closeButtonOnActiveTabOnly"), m_activeTabCloseButtonAction->isChecked());
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("window/splitter"), m_splitter->saveState());
}
