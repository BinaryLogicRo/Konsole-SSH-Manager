#include "sshhost.h"

namespace
{
const QLatin1String s_metadataPrefix("sshmanager:");

bool isControl(QChar c)
{
    return c.unicode() < 0x20 || c.unicode() == 0x7f || c.category() == QChar::Other_Control;
}

bool isMetadataKeyChar(QChar c)
{
    return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
        || (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || c == QLatin1Char('_') || c == QLatin1Char('-');
}

// Returns the text after "# sshmanager:" or a null view when the line isn't metadata.
QStringView metadataBody(QStringView line)
{
    line = line.trimmed();
    if (!line.startsWith(QLatin1Char('#'))) {
        return {};
    }
    line = line.mid(1).trimmed();
    if (!line.startsWith(s_metadataPrefix)) {
        return {};
    }
    return line.mid(s_metadataPrefix.size());
}
}

QString SshHost::alias() const
{
    if (kind != Kind::Host) {
        return {};
    }
    for (const QString &pattern : patterns) {
        if (SshValidation::isConcretePattern(pattern)) {
            return pattern;
        }
    }
    return {};
}

bool SshHost::isConnectable() const
{
    return !alias().isEmpty();
}

QString SshHost::displayName() const
{
    const QString joined = patterns.join(QLatin1Char(' '));
    return kind == Kind::Match ? QStringLiteral("Match %1").arg(joined) : joined;
}

QString SshHost::optionValue(QStringView keyword) const
{
    for (const SshOption &option : options) {
        if (QStringView(option.keyword).compare(keyword, Qt::CaseInsensitive) == 0) {
            return option.value;
        }
    }
    return {};
}

QString SshHost::metadataValue(QStringView key) const
{
    for (const auto &entry : metadata) {
        if (entry.first == key) {
            return entry.second;
        }
    }
    return {};
}

void SshHost::setMetadataValue(const QString &key, const QString &value)
{
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        if (it->first == key) {
            if (value.isEmpty()) {
                metadata.erase(it);
            } else {
                it->second = value;
            }
            return;
        }
    }
    if (!value.isEmpty()) {
        metadata.append({key, value});
    }
}

QString SshHost::group() const
{
    return metadataValue(u"group");
}

QString SshHost::color() const
{
    return metadataValue(u"color");
}

QString SshHost::note() const
{
    return metadataValue(u"note");
}

bool SshHost::hasSameContent(const SshHost &other) const
{
    return kind == other.kind && patterns == other.patterns && options == other.options && metadata == other.metadata;
}

bool SshValidation::isValidAlias(QStringView alias)
{
    if (alias.isEmpty() || alias.startsWith(QLatin1Char('-'))) {
        return false;
    }
    for (const QChar c : alias) {
        if (c.isSpace() || isControl(c) || c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('\\') || c == QLatin1Char('#')) {
            return false;
        }
    }
    return true;
}

bool SshValidation::isConcretePattern(QStringView pattern)
{
    if (!isValidAlias(pattern) || pattern.startsWith(QLatin1Char('!'))) {
        return false;
    }
    return !pattern.contains(QLatin1Char('*')) && !pattern.contains(QLatin1Char('?'));
}

bool SshValidation::isValidKeyword(QStringView keyword)
{
    if (keyword.isEmpty()) {
        return false;
    }
    for (const QChar c : keyword) {
        const bool ok = (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
            || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool SshValidation::isValidValue(QStringView value)
{
    if (value.trimmed().isEmpty()) {
        return false;
    }
    for (const QChar c : value) {
        if (isControl(c)) {
            return false;
        }
    }
    return true;
}

bool SshMetadataFormat::isMetadataComment(QStringView line)
{
    return !metadataBody(line).isNull();
}

SshMetadata SshMetadataFormat::parse(QStringView line)
{
    SshMetadata result;
    const QStringView body = metadataBody(line);
    qsizetype i = 0;
    const qsizetype n = body.size();
    while (i < n) {
        while (i < n && body[i].isSpace()) {
            ++i;
        }
        const qsizetype keyStart = i;
        while (i < n && isMetadataKeyChar(body[i])) {
            ++i;
        }
        if (i == keyStart || i + 1 >= n || body[i] != QLatin1Char('=') || body[i + 1] != QLatin1Char('"')) {
            break; // malformed: keep what was parsed so far
        }
        const QString key = body.mid(keyStart, i - keyStart).toString();
        i += 2;
        QString value;
        bool closed = false;
        while (i < n) {
            const QChar c = body[i];
            if (c == QLatin1Char('\\') && i + 1 < n) {
                const QChar next = body[i + 1];
                value += next == QLatin1Char('n') ? QChar(QLatin1Char('\n')) : next;
                i += 2;
                continue;
            }
            ++i;
            if (c == QLatin1Char('"')) {
                closed = true;
                break;
            }
            value += c;
        }
        if (!closed) {
            break;
        }
        result.append({key, value});
    }
    return result;
}

QString SshMetadataFormat::format(const SshMetadata &metadata)
{
    QString line = QStringLiteral("# ") + s_metadataPrefix;
    for (const auto &entry : metadata) {
        QString escaped;
        for (const QChar c : entry.second) {
            if (c == QLatin1Char('\\') || c == QLatin1Char('"')) {
                escaped += QLatin1Char('\\');
                escaped += c;
            } else if (c == QLatin1Char('\n')) {
                escaped += QLatin1String("\\n");
            } else if (!isControl(c)) {
                escaped += c;
            }
        }
        line += QStringLiteral(" %1=\"%2\"").arg(entry.first, escaped);
    }
    return line;
}
