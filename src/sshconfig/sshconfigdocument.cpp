#include "sshconfigdocument.h"
#include "sshconfigparser.h"

bool SshConfigLine::isKeyword(QStringView name) const
{
    return kind == Kind::Directive && QStringView(keyword).compare(name, Qt::CaseInsensitive) == 0;
}

bool SshConfigLine::isBlockHeader() const
{
    return isKeyword(u"Host") || isKeyword(u"Match");
}

bool SshConfigLine::isMetadata() const
{
    return kind == Kind::Comment && SshMetadataFormat::isMetadataComment(QString::fromUtf8(raw));
}

QByteArray SshConfigDocument::serialize() const
{
    QByteArray result;
    for (const SshConfigLine &line : lines) {
        result += line.raw;
    }
    return result;
}

QList<SshHost> SshConfigDocument::hosts(const QString &sourceFile, bool readOnly) const
{
    QList<SshHost> result;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const SshConfigLine &line = lines.at(i);
        if (line.kind != SshConfigLine::Kind::Directive || line.keyword.isEmpty()) {
            continue;
        }
        if (line.isBlockHeader()) {
            SshHost host;
            host.kind = line.isKeyword(u"Match") ? SshHost::Kind::Match : SshHost::Kind::Host;
            host.patterns = SshConfigParser::splitArguments(line.argument);
            host.sourceFile = sourceFile;
            host.lineNumber = int(i + 1);
            host.readOnly = readOnly;
            if (i > 0 && lines.at(i - 1).isMetadata()) {
                host.metadata = SshMetadataFormat::parse(QString::fromUtf8(lines.at(i - 1).raw));
            }
            result.append(host);
        } else if (!result.isEmpty() && !line.isKeyword(u"Include")) {
            result.last().options.append({line.keyword, line.argument});
        }
    }
    return result;
}

QList<SshConfigDocument::BlockRange> SshConfigDocument::blockRanges() const
{
    QList<BlockRange> result;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).isBlockHeader()) {
            continue;
        }
        BlockRange range;
        range.headerLine = i;
        range.metadataLine = (i > 0 && lines.at(i - 1).isMetadata()) ? i - 1 : -1;
        if (!result.isEmpty()) {
            result.last().end = range.metadataLine >= 0 ? range.metadataLine : i;
        }
        result.append(range);
    }
    if (!result.isEmpty()) {
        result.last().end = lines.size();
    }
    return result;
}

QStringList SshConfigDocument::includePatterns() const
{
    QStringList result;
    for (const SshConfigLine &line : lines) {
        if (line.isKeyword(u"Include")) {
            result += SshConfigParser::splitArguments(line.argument);
        }
    }
    return result;
}

qsizetype SshConfigDocument::findHost(QStringView alias) const
{
    const QList<SshHost> all = hosts();
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (QStringView(all.at(i).alias()).compare(alias, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

bool SshConfigDocument::containsPattern(QStringView alias) const
{
    const QList<SshHost> all = hosts();
    for (const SshHost &host : all) {
        if (host.kind != SshHost::Kind::Host) {
            continue;
        }
        for (const QString &pattern : host.patterns) {
            if (QStringView(pattern).compare(alias, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}

QByteArray SshConfigDocument::lineEnding() const
{
    for (const SshConfigLine &line : lines) {
        if (line.raw.endsWith('\n')) {
            return line.raw.endsWith("\r\n") ? QByteArrayLiteral("\r\n") : QByteArrayLiteral("\n");
        }
    }
    return QByteArrayLiteral("\n");
}
