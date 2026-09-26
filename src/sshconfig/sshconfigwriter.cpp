#include "sshconfigwriter.h"
#include "logging.h"
#include "sshconfigparser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>

#include <KLocalizedString>

namespace
{
const QLatin1String s_defaultIndent("    ");

using Kind = SshConfigLine::Kind;

QByteArray terminatorOf(const QByteArray &raw)
{
    if (raw.endsWith("\r\n")) {
        return QByteArrayLiteral("\r\n");
    }
    return raw.endsWith('\n') ? QByteArrayLiteral("\n") : QByteArray();
}

QByteArray indentOf(const QByteArray &raw)
{
    qsizetype i = 0;
    while (i < raw.size() && (raw.at(i) == ' ' || raw.at(i) == '\t')) {
        ++i;
    }
    return raw.left(i);
}

SshConfigLine makeLine(const QByteArray &text, const QByteArray &eol)
{
    return SshConfigParser::parseLine(text + eol);
}

void ensureTerminated(SshConfigLine &line, const QByteArray &eol)
{
    if (!line.raw.endsWith('\n')) {
        line = SshConfigParser::parseLine(line.raw + eol);
    }
}

// Replaces the argument of a directive line and keeps everything around it.
void replaceArgument(SshConfigLine &line, const QString &argument)
{
    const QByteArray separator = line.argument.isEmpty() ? QByteArrayLiteral(" ") : QByteArray();
    const QByteArray raw = line.raw.left(line.argumentStart) + separator + argument.toUtf8() + line.raw.mid(line.argumentEnd);
    line = SshConfigParser::parseLine(raw);
}

bool isOptionLine(const SshConfigLine &line)
{
    return line.kind == Kind::Directive && !line.keyword.isEmpty() && !line.isBlockHeader() && !line.isKeyword(u"Include");
}

// Identifies "the n-th occurrence of keyword" so repeated keywords (e.g. several
// IdentityFile lines) keep their order.
class OccurrenceKeys
{
public:
    QString next(const QString &keyword)
    {
        const QString lower = keyword.toLower();
        return QStringLiteral("%1:%2").arg(lower).arg(m_counts[lower]++);
    }

private:
    QHash<QString, int> m_counts;
};

QByteArray hostLine(const QStringList &patterns)
{
    return QByteArrayLiteral("Host ") + patterns.join(QLatin1Char(' ')).toUtf8();
}

QByteArray optionLine(const QByteArray &indent, const SshOption &option)
{
    return indent + option.keyword.toUtf8() + ' ' + option.value.toUtf8();
}

bool setPrivatePermissions(const QString &path, QFileDevice::Permissions permissions, QString *error)
{
    if (!QFile::setPermissions(path, permissions)) {
        if (error) {
            *error = i18n("Could not set the permissions of %1.", path);
        }
        return false;
    }
    return true;
}
}

SshConfigWriter::SshConfigWriter(const QString &backupDirectory)
    : m_backupDirectory(backupDirectory)
{
}

bool SshConfigWriter::isWritableHost(const SshHost &host)
{
    if (host.kind != SshHost::Kind::Host || host.alias().isEmpty()) {
        return false;
    }
    for (const QString &pattern : host.patterns) {
        if (!SshValidation::isValidAlias(pattern)) {
            return false;
        }
    }
    for (const SshOption &option : host.options) {
        const SshConfigLine probe = SshConfigParser::parseLine(option.keyword.toUtf8());
        if (!SshValidation::isValidKeyword(option.keyword) || !SshValidation::isValidValue(option.value) || probe.isBlockHeader()
            || probe.isKeyword(u"Include")) {
            return false;
        }
    }
    return true;
}

bool SshConfigWriter::addHost(SshConfigDocument &document, const SshHost &host)
{
    if (!isWritableHost(host)) {
        return false;
    }
    const QByteArray eol = document.lineEnding();
    if (!document.lines.isEmpty()) {
        ensureTerminated(document.lines.last(), eol);
        if (document.lines.last().kind != Kind::Blank) {
            document.lines.append(makeLine({}, eol));
        }
    }
    if (!host.metadata.isEmpty()) {
        document.lines.append(makeLine(SshMetadataFormat::format(host.metadata).toUtf8(), eol));
    }
    document.lines.append(makeLine(hostLine(host.patterns), eol));
    for (const SshOption &option : host.options) {
        document.lines.append(makeLine(optionLine(QByteArray(s_defaultIndent.data()), option), eol));
    }
    return true;
}

bool SshConfigWriter::updateHost(SshConfigDocument &document, QStringView alias, const SshHost &host)
{
    const qsizetype index = document.findHost(alias);
    if (index < 0 || !isWritableHost(host)) {
        return false;
    }
    const SshConfigDocument::BlockRange range = document.blockRanges().at(index);
    const SshHost old = document.hosts().at(index);
    const QByteArray eol = document.lineEnding();
    const SshConfigLine &header = document.lines.at(range.headerLine);

    QList<SshConfigLine> region;

    // Metadata comment: untouched unless the metadata changed.
    if (host.metadata == old.metadata) {
        if (range.metadataLine >= 0) {
            region.append(document.lines.at(range.metadataLine));
        }
    } else if (!host.metadata.isEmpty()) {
        const QByteArray terminator = range.metadataLine >= 0 ? terminatorOf(document.lines.at(range.metadataLine).raw) : eol;
        region.append(makeLine(indentOf(header.raw) + SshMetadataFormat::format(host.metadata).toUtf8(), terminator));
    }

    // Host line: only the pattern list changes; a trailing comment is kept.
    SshConfigLine newHeader = header;
    if (host.patterns != old.patterns) {
        QString argument = host.patterns.join(QLatin1Char(' '));
        const qsizetype comment = SshConfigParser::trailingCommentStart(header.argument);
        if (comment >= 0) {
            argument += header.argument.mid(comment);
        }
        replaceArgument(newHeader, argument);
    }
    region.append(newHeader);

    // Options: match old and new by (keyword, occurrence).
    OccurrenceKeys newKeys;
    QHash<QString, QString> wanted;
    QList<QPair<QString, SshOption>> ordered;
    for (const SshOption &option : host.options) {
        const QString key = newKeys.next(option.keyword);
        wanted.insert(key, option.value);
        ordered.append({key, option});
    }

    OccurrenceKeys oldKeys;
    QSet<QString> kept;
    QByteArray indent = QByteArray(s_defaultIndent.data());
    bool indentFound = false;
    qsizetype insertAt = region.size();
    for (qsizetype i = range.headerLine + 1; i < range.end; ++i) {
        SshConfigLine line = document.lines.at(i);
        if (isOptionLine(line)) {
            if (!indentFound) {
                indent = indentOf(line.raw);
                indentFound = true;
            }
            const QString key = oldKeys.next(line.keyword);
            const auto it = wanted.constFind(key);
            if (it == wanted.constEnd()) {
                continue; // option removed
            }
            kept.insert(key);
            if (line.argument != it.value()) {
                replaceArgument(line, it.value());
            }
            region.append(line);
            insertAt = region.size();
        } else {
            region.append(line);
            if (line.kind == Kind::Directive) {
                insertAt = region.size();
            }
        }
    }

    QList<SshConfigLine> added;
    for (const auto &entry : ordered) {
        if (!kept.contains(entry.first)) {
            added.append(makeLine(optionLine(indent, entry.second), eol));
        }
    }
    if (!added.isEmpty()) {
        ensureTerminated(region[insertAt - 1], eol);
        for (qsizetype i = 0; i < added.size(); ++i) {
            region.insert(insertAt + i, added.at(i));
        }
    }

    const qsizetype regionStart = range.metadataLine >= 0 ? range.metadataLine : range.headerLine;
    QList<SshConfigLine> result = document.lines.mid(0, regionStart);
    result += region;
    result += document.lines.mid(range.end);
    document.lines = result;
    return true;
}

bool SshConfigWriter::removeHost(SshConfigDocument &document, QStringView alias)
{
    const qsizetype index = document.findHost(alias);
    if (index < 0) {
        return false;
    }
    const SshConfigDocument::BlockRange range = document.blockRanges().at(index);
    qsizetype removeStart = range.metadataLine >= 0 ? range.metadataLine : range.headerLine;
    qsizetype removeEnd = range.end;

    if (removeEnd < document.lines.size()) {
        // Comment lines directly above the next block describe that block.
        while (removeEnd - 1 > range.headerLine && document.lines.at(removeEnd - 1).kind == Kind::Comment) {
            --removeEnd;
        }
    } else {
        // Removing the last block: don't leave blank lines dangling at the end.
        while (removeStart > 0 && document.lines.at(removeStart - 1).kind == Kind::Blank) {
            --removeStart;
        }
    }
    document.lines.erase(document.lines.begin() + removeStart, document.lines.begin() + removeEnd);
    return true;
}

QByteArray SshConfigWriter::withIncludeLine(const QByteArray &config, const QString &argument)
{
    const QByteArray eol = SshConfigParser::parse(config).lineEnding();
    return QByteArrayLiteral("Include ") + argument.toUtf8() + eol + config;
}

bool SshConfigWriter::writeFile(const QString &path, const QByteArray &data, QFileDevice::Permissions permissions, QString *error)
{
    if (!backupOnce(path, error)) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    // Applies to the temporary file, so the content is never world-readable.
    file.setPermissions(permissions);
    if (file.write(data) != data.size()) {
        if (error) {
            *error = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    qCDebug(KSSHM_CONFIG) << "Wrote" << path << data.size() << "bytes";
    return setPrivatePermissions(path, permissions, error);
}

bool SshConfigWriter::ensurePrivateDirectory(const QString &path, QString *error)
{
    if (!QDir().mkpath(path)) {
        if (error) {
            *error = i18n("Could not create the folder %1.", path);
        }
        return false;
    }
    return setPrivatePermissions(path, DirectoryPermissions, error);
}

QString SshConfigWriter::backupDirectory() const
{
    return m_backupDirectory;
}

bool SshConfigWriter::backupOnce(const QString &path, QString *error)
{
    const QString key = QFileInfo(path).absoluteFilePath();
    if (m_backedUp.contains(key)) {
        return true;
    }
    if (!QFileInfo::exists(path)) {
        m_backedUp.insert(key);
        return true;
    }
    if (!ensurePrivateDirectory(m_backupDirectory, error)) {
        return false;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString base = QStringLiteral("%1/%2.%3").arg(m_backupDirectory, QFileInfo(path).fileName(), stamp);
    QString target = base + QStringLiteral(".bak");
    for (int n = 1; QFileInfo::exists(target); ++n) {
        target = QStringLiteral("%1-%2.bak").arg(base).arg(n);
    }
    if (!QFile::copy(path, target)) {
        if (error) {
            *error = i18n("Could not create the backup file %1.", target);
        }
        return false;
    }
    if (!setPrivatePermissions(target, FilePermissions, error)) {
        return false;
    }
    qCDebug(KSSHM_CONFIG) << "Backed up" << path << "to" << target;
    m_backedUp.insert(key);
    return true;
}
