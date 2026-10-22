#pragma once

#include "sshconfig/sshhost.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QIcon>
#include <QList>

// Two-level tree for the sidebar: groups → hosts.
//
// Managed hosts are grouped by their `group` metadata. Hosts from any other
// file are read-only and grouped by the file they came from.
class HostTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        SearchTextRole = Qt::UserRole + 1,
        GroupKeyRole,
    };

    explicit HostTreeModel(QObject *parent = nullptr);

    void setHosts(const QList<SshHost> &hosts, const QString &homeDir);

    // nullptr for group rows or invalid indexes.
    const SshHost *hostForIndex(const QModelIndex &index) const;
    QModelIndex indexForHost(const QString &sourceFile, int lineNumber) const;
    // Names of the groups used by managed hosts, sorted.
    QStringList managedGroupNames() const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    static QIcon colorIcon(const QString &color);

private:
    struct Group {
        QString key; // stable identity, used to restore expansion state
        QString title;
        bool managed = false;
        QList<SshHost> hosts;
    };

    QVariant groupData(const Group &group, int role) const;
    QVariant hostData(const SshHost &host, int role) const;

    QList<Group> m_groups;
    QString m_homeDir;
};
