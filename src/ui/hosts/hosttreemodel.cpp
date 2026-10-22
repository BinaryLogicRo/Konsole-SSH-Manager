#include "hosttreemodel.h"

#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QMap>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

#include <KLocalizedString>

#include <algorithm>

namespace
{
// internalId of group rows; host rows store their group row + 1.
constexpr quintptr GroupId = 0;

QString prettyPath(const QString &path, const QString &homeDir)
{
    if (!homeDir.isEmpty() && path.startsWith(homeDir + QLatin1Char('/'))) {
        return QLatin1Char('~') + path.mid(homeDir.size());
    }
    return path;
}
}

HostTreeModel::HostTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void HostTreeModel::setHosts(const QList<SshHost> &hosts, const QString &homeDir)
{
    beginResetModel();
    m_homeDir = homeDir;
    m_groups.clear();

    QMap<QString, QList<SshHost>> managed; // sorted by group name
    QList<SshHost> ungrouped;
    QList<Group> fileGroups;
    for (const SshHost &host : hosts) {
        if (!host.readOnly) {
            if (host.group().isEmpty()) {
                ungrouped.append(host);
            } else {
                managed[host.group()].append(host);
            }
            continue;
        }
        if (fileGroups.isEmpty() || fileGroups.last().key != host.sourceFile) {
            Group group;
            group.key = host.sourceFile;
            group.title = i18nc("@item group of read-only hosts, %1 is a file path", "%1 (read-only)", prettyPath(host.sourceFile, homeDir));
            fileGroups.append(group);
        }
        fileGroups.last().hosts.append(host);
    }

    const auto byAlias = [](const SshHost &a, const SshHost &b) {
        return a.displayName().compare(b.displayName(), Qt::CaseInsensitive) < 0;
    };
    for (auto it = managed.begin(); it != managed.end(); ++it) {
        Group group;
        group.key = QStringLiteral("managed:") + it.key();
        group.title = it.key();
        group.managed = true;
        group.hosts = it.value();
        std::sort(group.hosts.begin(), group.hosts.end(), byAlias);
        m_groups.append(group);
    }
    if (!ungrouped.isEmpty()) {
        Group group;
        group.key = QStringLiteral("managed-ungrouped");
        group.title = i18nc("@item group of hosts without a group", "Ungrouped");
        group.managed = true;
        group.hosts = ungrouped;
        std::sort(group.hosts.begin(), group.hosts.end(), byAlias);
        m_groups.append(group);
    }
    m_groups += fileGroups;
    endResetModel();
}

const SshHost *HostTreeModel::hostForIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.internalId() == GroupId) {
        return nullptr;
    }
    const auto groupRow = qsizetype(index.internalId() - 1);
    if (groupRow >= m_groups.size() || index.row() >= m_groups.at(groupRow).hosts.size()) {
        return nullptr;
    }
    return &m_groups.at(groupRow).hosts.at(index.row());
}

QModelIndex HostTreeModel::indexForHost(const QString &sourceFile, int lineNumber) const
{
    for (qsizetype g = 0; g < m_groups.size(); ++g) {
        const QList<SshHost> &hosts = m_groups.at(g).hosts;
        for (qsizetype h = 0; h < hosts.size(); ++h) {
            if (hosts.at(h).sourceFile == sourceFile && hosts.at(h).lineNumber == lineNumber) {
                return createIndex(int(h), 0, quintptr(g + 1));
            }
        }
    }
    return {};
}

QStringList HostTreeModel::managedGroupNames() const
{
    QStringList names;
    for (const Group &group : m_groups) {
        if (group.managed && group.key.startsWith(QLatin1String("managed:"))) {
            names.append(group.title);
        }
    }
    return names;
}

QModelIndex HostTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0) {
        return {};
    }
    if (!parent.isValid()) {
        return row < m_groups.size() ? createIndex(row, 0, GroupId) : QModelIndex();
    }
    if (parent.internalId() != GroupId || parent.row() >= m_groups.size()) {
        return {};
    }
    if (row >= m_groups.at(parent.row()).hosts.size()) {
        return {};
    }
    return createIndex(row, 0, quintptr(parent.row() + 1));
}

QModelIndex HostTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.internalId() == GroupId) {
        return {};
    }
    return createIndex(int(child.internalId() - 1), 0, GroupId);
}

int HostTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return int(m_groups.size());
    }
    if (parent.internalId() == GroupId && parent.row() < m_groups.size()) {
        return int(m_groups.at(parent.row()).hosts.size());
    }
    return 0;
}

int HostTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant HostTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    if (index.internalId() == GroupId) {
        return index.row() < m_groups.size() ? groupData(m_groups.at(index.row()), role) : QVariant();
    }
    const SshHost *host = hostForIndex(index);
    return host ? hostData(*host, role) : QVariant();
}

QVariant HostTreeModel::groupData(const Group &group, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        return group.title;
    case Qt::DecorationRole:
        return QIcon::fromTheme(group.managed ? QStringLiteral("folder") : QStringLiteral("document-properties"));
    case Qt::ToolTipRole:
        return group.managed ? i18n("Hosts managed by this app") : i18n("Hosts from %1. They can't be edited here, but can be imported.", group.key);
    case SearchTextRole:
        return group.title;
    case GroupKeyRole:
        return group.key;
    default:
        return {};
    }
}

QVariant HostTreeModel::hostData(const SshHost &host, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        return host.displayName();
    case Qt::DecorationRole: {
        const QIcon swatch = colorIcon(host.color());
        if (!swatch.isNull()) {
            return swatch;
        }
        return QIcon::fromTheme(host.isConnectable() ? QStringLiteral("network-server") : QStringLiteral("view-filter"));
    }
    case Qt::ForegroundRole:
        if (!host.isConnectable()) {
            return QGuiApplication::palette().color(QPalette::Disabled, QPalette::Text);
        }
        return {};
    case Qt::FontRole:
        if (!host.isConnectable()) {
            QFont font;
            font.setItalic(true);
            return font;
        }
        return {};
    case Qt::ToolTipRole: {
        QStringList lines;
        lines << host.displayName();
        const QString hostName = host.optionValue(u"HostName");
        const QString user = host.optionValue(u"User");
        const QString port = host.optionValue(u"Port");
        if (!hostName.isEmpty()) {
            lines << i18n("Host name: %1", hostName);
        }
        if (!user.isEmpty()) {
            lines << i18n("User: %1", user);
        }
        if (!port.isEmpty()) {
            lines << i18n("Port: %1", port);
        }
        if (!host.note().isEmpty()) {
            lines << i18n("Note: %1", host.note());
        }
        if (!host.isConnectable()) {
            lines << i18n("Pattern or Match block: applies to other hosts, can't be opened directly.");
        }
        lines << i18nc("%1 file path, %2 line number", "Defined in %1, line %2", prettyPath(host.sourceFile, m_homeDir), host.lineNumber);
        return lines.join(QLatin1Char('\n'));
    }
    case SearchTextRole:
        return QStringList{host.displayName(), host.optionValue(u"HostName"), host.optionValue(u"User"), host.note(), host.group()}.join(QLatin1Char(' '));
    default:
        return {};
    }
}

QIcon HostTreeModel::colorIcon(const QString &color)
{
    const QColor value(color);
    if (color.isEmpty() || !value.isValid()) {
        return {};
    }
    static QHash<QString, QIcon> cache;
    const auto it = cache.constFind(color);
    if (it != cache.constEnd()) {
        return it.value();
    }
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(value.darker(140));
    painter.setBrush(value);
    painter.drawRoundedRect(QRectF(1.5, 1.5, 13, 13), 3, 3);
    painter.end();
    const QIcon icon(pixmap);
    cache.insert(color, icon);
    return icon;
}
