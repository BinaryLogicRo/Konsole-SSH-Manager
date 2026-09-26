#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QStringView>

// One `Keyword value` line inside a Host/Match block. The value is kept exactly
// as written in the file (quotes included), so it can be written back unchanged.
struct SshOption {
    QString keyword;
    QString value;

    bool operator==(const SshOption &other) const = default;
};

// App-only metadata (group, color, note, and any unknown keys), in file order.
using SshMetadata = QList<QPair<QString, QString>>;

// A Host or Match block as found in one ssh_config file.
class SshHost
{
public:
    enum class Kind {
        Host,
        Match,
    };

    Kind kind = Kind::Host;
    QStringList patterns; // Host patterns (unquoted), or Match criteria tokens
    QList<SshOption> options;
    SshMetadata metadata;
    QString sourceFile;
    int lineNumber = 0; // 1-based line of the Host/Match keyword
    bool readOnly = true;

    // First concrete (non-wildcard, non-negated, valid) Host pattern, or empty.
    QString alias() const;
    bool isConnectable() const;
    QString displayName() const;

    // OpenSSH uses the first value obtained for each keyword; so does this.
    QString optionValue(QStringView keyword) const;

    QString metadataValue(QStringView key) const;
    // Sets or replaces a key in place; an empty value removes it.
    void setMetadataValue(const QString &key, const QString &value);

    QString group() const;
    QString color() const;
    QString note() const;

    // Compares what is stored in the file (kind, patterns, options, metadata),
    // ignoring where the block was found.
    bool hasSameContent(const SshHost &other) const;
};

namespace SshValidation
{
// No whitespace, no leading '-', no control characters, no quotes, '#' or '\\'.
bool isValidAlias(QStringView alias);
// A valid alias that is also not a wildcard or negated pattern.
bool isConcretePattern(QStringView pattern);
// ASCII letters and digits only.
bool isValidKeyword(QStringView keyword);
// Non-empty, no control characters (so a value can't inject extra lines).
bool isValidValue(QStringView value);
}

namespace SshMetadataFormat
{
// True for a `# sshmanager: ...` comment line (leading whitespace allowed).
bool isMetadataComment(QStringView line);
SshMetadata parse(QStringView line);
// Formats a full comment line, without a line terminator.
QString format(const SshMetadata &metadata);
}
