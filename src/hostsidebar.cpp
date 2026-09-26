#include "hostsidebar.h"
#include "models/hosttreemodel.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <KLocalizedString>

HostSidebar::HostSidebar(QWidget *parent)
    : QWidget(parent)
    , m_filter(new QLineEdit(this))
    , m_view(new QTreeView(this))
    , m_model(new HostTreeModel(this))
    , m_proxy(new QSortFilterProxyModel(this))
{
    m_filter->setPlaceholderText(i18nc("@info:placeholder", "Filter hosts…"));
    m_filter->setClearButtonEnabled(true);

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterRole(HostTreeModel::SearchTextRole);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setRecursiveFilteringEnabled(true);

    m_view->setModel(m_proxy);
    m_view->setHeaderHidden(true);
    m_view->setUniformRowHeights(true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setExpandsOnDoubleClick(true);
    m_view->installEventFilter(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterFixedString(text);
        if (!text.isEmpty()) {
            m_view->expandAll();
        }
    });
    connect(m_view, &QTreeView::doubleClicked, this, &HostSidebar::activateIndex);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &HostSidebar::currentHostChanged);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex index = m_view->indexAt(pos);
        if (index.isValid()) {
            m_view->setCurrentIndex(index);
        }
        Q_EMIT contextMenuRequested(m_view->viewport()->mapToGlobal(pos));
    });
    connect(m_view, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
        m_collapsedGroups.insert(index.data(HostTreeModel::GroupKeyRole).toString());
    });
    connect(m_view, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        m_collapsedGroups.remove(index.data(HostTreeModel::GroupKeyRole).toString());
    });
}

void HostSidebar::setHosts(const QList<SshHost> &hosts, const QString &homeDir)
{
    const std::optional<SshHost> previous = currentHost();

    // Resetting the model collapses everything; don't record that as user intent.
    const QSet<QString> collapsed = m_collapsedGroups;
    m_model->setHosts(hosts, homeDir);
    m_collapsedGroups = collapsed;

    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex index = m_proxy->index(row, 0);
        const bool collapse = m_filter->text().isEmpty() && m_collapsedGroups.contains(index.data(HostTreeModel::GroupKeyRole).toString());
        m_view->setExpanded(index, !collapse);
    }

    if (previous) {
        const QModelIndex source = m_model->indexForHost(previous->sourceFile, previous->lineNumber);
        if (source.isValid()) {
            m_view->setCurrentIndex(m_proxy->mapFromSource(source));
        }
    }
    Q_EMIT currentHostChanged();
}

std::optional<SshHost> HostSidebar::currentHost() const
{
    const SshHost *host = m_model->hostForIndex(m_proxy->mapToSource(m_view->currentIndex()));
    if (!host) {
        return std::nullopt;
    }
    return *host;
}

QStringList HostSidebar::managedGroupNames() const
{
    return m_model->managedGroupNames();
}

void HostSidebar::focusFilter()
{
    m_filter->setFocus();
    m_filter->selectAll();
}

bool HostSidebar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (m_model->hostForIndex(m_proxy->mapToSource(m_view->currentIndex()))) {
                activateIndex(m_view->currentIndex());
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void HostSidebar::activateIndex(const QModelIndex &proxyIndex)
{
    const SshHost *host = m_model->hostForIndex(m_proxy->mapToSource(proxyIndex));
    if (host) {
        Q_EMIT hostActivated(*host);
    }
}
