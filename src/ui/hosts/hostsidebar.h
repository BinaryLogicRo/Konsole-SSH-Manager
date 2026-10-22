#pragma once

#include "sshconfig/sshhost.h"

#include <QSet>
#include <QWidget>

#include <optional>

class HostTreeModel;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

// Filter box + host tree. Keeps expansion and selection across reloads.
class HostSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit HostSidebar(QWidget *parent = nullptr);

    void setHosts(const QList<SshHost> &hosts, const QString &homeDir);
    std::optional<SshHost> currentHost() const;
    QStringList managedGroupNames() const;
    void focusFilter();

Q_SIGNALS:
    // Double-click or Enter on a host.
    void hostActivated(const SshHost &host);
    void currentHostChanged();
    void contextMenuRequested(const QPoint &globalPos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void activateIndex(const QModelIndex &proxyIndex);

    QLineEdit *m_filter = nullptr;
    QTreeView *m_view = nullptr;
    HostTreeModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QSet<QString> m_collapsedGroups;
};
