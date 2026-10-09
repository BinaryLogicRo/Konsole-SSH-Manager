#include "agentpanel.h"
#include "agentpaneltext.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KPasswordDialog>

#include <optional>

namespace
{
constexpr int EntryRole = Qt::UserRole + 1; // index into the entries; -1 for group rows
constexpr int IconSize = 16;

enum Column {
    NameColumn,
    TypeColumn,
    StateColumn,
    ColumnCount,
};

QToolButton *toolButton(QAction *action, QWidget *parent, Qt::ToolButtonStyle style)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(style);
    button->setAutoRaise(true);
    return button;
}
}

AgentPanel::AgentPanel(QWidget *parent)
    : QWidget(parent)
    , m_client(new SshAgentClient(this))
    , m_statusIcon(new QLabel(this))
    , m_status(new QLabel(this))
    , m_errorBar(new QWidget(this))
    , m_errorLabel(new QLabel(m_errorBar))
    , m_view(new QTreeWidget(this))
{
    // The app's own executable asks for passphrases (see sshagent/askpass.h).
    m_client->setAskpassProgram(QCoreApplication::applicationFilePath());

    m_loadAction = new QAction(QIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action", "&Load"), this);
    m_loadAction->setToolTip(i18nc("@info:tooltip", "Load the selected key into the SSH agent"));
    m_removeAction = new QAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action", "&Remove"), this);
    m_removeAction->setToolTip(i18nc("@info:tooltip", "Remove the selected key from the SSH agent"));
    m_forgetAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), i18nc("@action", "Remove from &List"), this);
    m_addFileAction = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action", "&Add Key File…"), this);
    m_refreshAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action", "Re&fresh"), this);

    auto *title = new QLabel(i18nc("@title", "SSH Agent"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto *header = new QHBoxLayout;
    header->addWidget(title, 1);
    header->addWidget(toolButton(m_addFileAction, this, Qt::ToolButtonIconOnly));
    header->addWidget(toolButton(m_refreshAction, this, Qt::ToolButtonIconOnly));

    m_status->setWordWrap(true);
    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_statusIcon, 0, Qt::AlignTop);
    statusRow->addWidget(m_status, 1);

    auto *errorIcon = new QLabel(m_errorBar);
    errorIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-error")).pixmap(IconSize, IconSize));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *dismissButton = new QToolButton(m_errorBar);
    dismissButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    dismissButton->setToolTip(i18nc("@info:tooltip", "Hide this message"));
    dismissButton->setAutoRaise(true);
    auto *errorLayout = new QHBoxLayout(m_errorBar);
    errorLayout->setContentsMargins(0, 0, 0, 0);
    errorLayout->addWidget(errorIcon, 0, Qt::AlignTop);
    errorLayout->addWidget(m_errorLabel, 1);
    errorLayout->addWidget(dismissButton, 0, Qt::AlignTop);
    m_errorBar->hide();

    m_view->setColumnCount(ColumnCount);
    m_view->setHeaderHidden(true);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    QHeaderView *columns = m_view->header();
    columns->setStretchLastSection(false);
    columns->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    columns->setSectionResizeMode(TypeColumn, QHeaderView::ResizeToContents);
    columns->setSectionResizeMode(StateColumn, QHeaderView::ResizeToContents);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(toolButton(m_loadAction, this, Qt::ToolButtonTextBesideIcon));
    buttons->addWidget(toolButton(m_removeAction, this, Qt::ToolButtonTextBesideIcon));
    buttons->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(header);
    layout->addLayout(statusRow);
    layout->addWidget(m_errorBar);
    layout->addWidget(m_view, 1);
    layout->addLayout(buttons);

    connect(m_loadAction, &QAction::triggered, this, &AgentPanel::loadCurrent);
    connect(m_removeAction, &QAction::triggered, this, &AgentPanel::removeCurrent);
    connect(m_forgetAction, &QAction::triggered, this, &AgentPanel::forgetCurrent);
    connect(m_addFileAction, &QAction::triggered, this, &AgentPanel::chooseKeyFile);
    connect(m_refreshAction, &QAction::triggered, this, &AgentPanel::refresh);
    connect(dismissButton, &QToolButton::clicked, m_errorBar, &QWidget::hide);
    connect(m_client, &SshAgentClient::refreshed, this, &AgentPanel::onRefreshed);
    connect(m_client, &SshAgentClient::actionFinished, this, &AgentPanel::onActionFinished);
    connect(m_client, &SshAgentClient::passphraseRequested, this, &AgentPanel::askPassphrase);
    connect(m_view, &QTreeWidget::currentItemChanged, this, &AgentPanel::updateActions);
    connect(m_view, &QTreeWidget::itemDoubleClicked, this, [this] {
        if (m_loadAction->isEnabled()) {
            loadCurrent();
        }
    });
    connect(m_view, &QWidget::customContextMenuRequested, this, &AgentPanel::showContextMenu);

    updateStatus();
    updateActions();
}

void AgentPanel::setConfigKeys(const QList<SshIdentityFile> &files)
{
    m_configKeys = files;
    rebuild();
    scheduleRefresh();
}

void AgentPanel::setAddedKeys(const QStringList &paths)
{
    m_addedKeys.clear();
    for (const QString &path : paths) {
        if (QDir::isAbsolutePath(path) && !m_addedKeys.contains(path)) {
            m_addedKeys.append(path);
        }
    }
    rebuild();
    scheduleRefresh();
}

QStringList AgentPanel::addedKeys() const
{
    return m_addedKeys;
}

void AgentPanel::refresh()
{
    m_refreshScheduled = false;
    QStringList files = configKeyPaths() + m_addedKeys;
    files.removeDuplicates();
    m_client->refresh(files);
}

void AgentPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    scheduleRefresh();
}

void AgentPanel::scheduleRefresh()
{
    // A hidden panel never runs anything; it refreshes when it is shown.
    if (!isVisible() || m_refreshScheduled) {
        return;
    }
    m_refreshScheduled = true;
    QTimer::singleShot(0, this, &AgentPanel::refresh);
}

void AgentPanel::onRefreshed(const SshAgentClient::Snapshot &snapshot)
{
    m_snapshot = snapshot;
    rebuild();
    updateStatus();
}

void AgentPanel::onActionFinished(bool succeeded, const QString &message)
{
    if (m_passphraseDialog) {
        m_passphraseDialog->reject(); // ssh-add no longer waits for it
    }
    m_busyText.clear();
    if (succeeded) {
        m_errorBar->hide();
    } else {
        showError(message);
    }
    updateStatus();
    updateActions();
    refresh();
}

void AgentPanel::askPassphrase(const QString &prompt)
{
    // Window-modal, so it stays in front of the main window, and clicking the
    // main window doesn't take the focus away from it. No ShowKeepPassword
    // flag: passphrases are never remembered.
    auto *dialog = new KPasswordDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(i18nc("@title:window", "SSH Key Passphrase"));
    dialog->setPrompt(prompt); // ssh-add's prompt names the key file
    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            m_client->answerPassphrase(dialog->password());
        } else {
            m_client->declinePassphrase();
        }
    });
    m_passphraseDialog = dialog;
    dialog->open();
}

void AgentPanel::rebuild()
{
    const SshKeyEntry *current = currentEntry();
    const QString currentId = current ? current->id() : QString();

    // Files not checked yet show as unknown until the next refresh, not as missing.
    QHash<QString, SshKeyFileStatus> files = m_snapshot.files;
    for (const QString &path : configKeyPaths() + m_addedKeys) {
        if (!files.contains(path)) {
            files.insert(path, SshKeyFileStatus{QFileInfo(path).isFile(), std::nullopt});
        }
    }
    m_entries = SshKeyList::build(m_configKeys, m_addedKeys, files, m_snapshot.keys);

    m_view->clear();
    QTreeWidgetItem *selected = nullptr;
    std::optional<SshKeyEntry::Source> group;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        const SshKeyEntry &entry = m_entries.at(i);
        if (group != entry.source) {
            group = entry.source;
            auto *groupItem = new QTreeWidgetItem(m_view, {AgentPanelText::groupTitle(entry.source)});
            groupItem->setFlags(Qt::ItemIsEnabled);
            groupItem->setFirstColumnSpanned(true);
            groupItem->setData(NameColumn, EntryRole, -1);
            QFont font = groupItem->font(NameColumn);
            font.setBold(true);
            groupItem->setFont(NameColumn, font);
        }
        auto *item = new QTreeWidgetItem(m_view, {entry.name(), entry.type, AgentPanelText::stateText(entry.state)});
        item->setIcon(StateColumn, AgentPanelText::stateIcon(entry.state));
        item->setData(NameColumn, EntryRole, int(i));
        const QString tip = AgentPanelText::toolTip(entry);
        for (int column = 0; column < ColumnCount; ++column) {
            item->setToolTip(column, tip);
        }
        if (entry.id() == currentId) {
            selected = item;
        }
    }
    if (m_entries.isEmpty()) {
        auto *placeholder = new QTreeWidgetItem(m_view, {i18nc("@info", "No keys yet. Use Add Key File to choose one.")});
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setFirstColumnSpanned(true);
    }
    if (selected) {
        m_view->setCurrentItem(selected);
    }
    updateActions();
}

void AgentPanel::updateStatus()
{
    QString icon;
    QString text;
    if (!m_busyText.isEmpty()) {
        text = m_busyText;
    } else {
        text = AgentPanelText::status(m_snapshot, &icon);
    }
    m_statusIcon->setPixmap(icon.isEmpty() ? QPixmap() : QIcon::fromTheme(icon).pixmap(IconSize, IconSize));
    m_statusIcon->setVisible(!icon.isEmpty());
    m_status->setText(text);
}

void AgentPanel::updateActions()
{
    const SshKeyEntry *entry = currentEntry();
    const bool idle = !m_client->isBusy();
    const bool agentAvailable = m_snapshot.status == SshAgentClient::Status::Available;
    const bool loadable = entry && !entry->path.isEmpty() && (entry->state == SshKeyEntry::State::NotLoaded || entry->state == SshKeyEntry::State::Unknown);
    m_loadAction->setEnabled(idle && agentAvailable && loadable);
    m_removeAction->setEnabled(idle && agentAvailable && entry && !entry->agentKeys.isEmpty());
    m_forgetAction->setEnabled(entry && entry->source == SshKeyEntry::Source::Added);
    m_addFileAction->setEnabled(idle);
}

void AgentPanel::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorBar->show();
}

const SshKeyEntry *AgentPanel::currentEntry() const
{
    const QTreeWidgetItem *item = m_view->currentItem();
    if (!item) {
        return nullptr;
    }
    bool ok = false;
    const int index = item->data(NameColumn, EntryRole).toInt(&ok);
    return ok && index >= 0 && index < m_entries.size() ? &m_entries.at(index) : nullptr;
}

QStringList AgentPanel::configKeyPaths() const
{
    QStringList paths;
    for (const SshIdentityFile &file : m_configKeys) {
        if (file.resolved) {
            paths.append(file.path);
        }
    }
    return paths;
}

void AgentPanel::loadCurrent()
{
    if (const SshKeyEntry *entry = currentEntry()) {
        loadKey(entry->path, entry->name());
    }
}

void AgentPanel::loadKey(const QString &path, const QString &name)
{
    if (!m_client->addKey(path)) {
        showError(i18n("%1 can't be loaded right now.", name));
        return;
    }
    m_busyText = i18n("Loading %1… If it asks for a passphrase, enter it in the window that opens.", name);
    updateStatus();
    updateActions();
}

void AgentPanel::removeCurrent()
{
    const SshKeyEntry *entry = currentEntry();
    if (!entry) {
        return;
    }
    const QString name = entry->name();
    if (!m_client->removeKeys(entry->agentKeys)) {
        showError(i18n("%1 can't be removed right now.", name));
        return;
    }
    m_busyText = i18n("Removing %1…", name);
    updateStatus();
    updateActions();
}

void AgentPanel::forgetCurrent()
{
    const SshKeyEntry *entry = currentEntry();
    if (entry && entry->source == SshKeyEntry::Source::Added) {
        m_addedKeys.removeAll(entry->path);
        rebuild();
    }
}

void AgentPanel::chooseKeyFile()
{
    const QString sshDir = QDir::homePath() + QStringLiteral("/.ssh");
    QString path = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Add Private Key"), sshDir);
    if (path.isEmpty()) {
        return;
    }
    path = QFileInfo(path).absoluteFilePath();
    // A public key was picked: use the private key next to it.
    if (path.endsWith(QLatin1String(".pub")) && QFileInfo(path.chopped(4)).isFile()) {
        path.chop(4);
    }
    if (!configKeyPaths().contains(path) && !m_addedKeys.contains(path)) {
        m_addedKeys.append(path);
    }
    rebuild();
    for (int row = 0; row < m_view->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_view->topLevelItem(row);
        bool ok = false;
        const int index = item->data(NameColumn, EntryRole).toInt(&ok);
        if (ok && index >= 0 && m_entries.at(index).path == path) {
            m_view->setCurrentItem(item);
            break;
        }
    }

    if (m_snapshot.status == SshAgentClient::Status::Available) {
        loadKey(path, QFileInfo(path).fileName());
    } else {
        refresh();
    }
}

void AgentPanel::showContextMenu(const QPoint &pos)
{
    if (QTreeWidgetItem *item = m_view->itemAt(pos)) {
        m_view->setCurrentItem(item);
    }
    QMenu menu(this);
    for (QAction *action : {m_loadAction, m_removeAction, m_forgetAction}) {
        if (action->isEnabled()) {
            menu.addAction(action);
        }
    }
    menu.addSeparator();
    menu.addAction(m_addFileAction);
    menu.addAction(m_refreshAction);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}
