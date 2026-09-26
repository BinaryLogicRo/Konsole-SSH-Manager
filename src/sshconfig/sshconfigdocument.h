#pragma once

#include "sshhost.h"

#include <QByteArray>
#include <QList>
#include <QString>

// One physical line of an ssh_config file.
struct SshConfigLine {
    enum class Kind {
        Blank,
        Comment,
        Directive,
    };

    Kind kind = Kind::Blank;
    QByteArray raw; // exact bytes, including the line terminator (if any)
    QString keyword; // as written (original casing)
    QString argument; // everything after the keyword/separator, trailing whitespace removed
    qsizetype argumentStart = -1; // byte offsets of `argument` inside `raw`
    qsizetype argumentEnd = -1;

    bool isKeyword(QStringView name) const;
    bool isBlockHeader() const; // Host or Match
    bool isMetadata() const;
};

// A parsed ssh_config file. Serializing an unmodified document reproduces the
// original bytes exactly; edits only touch the lines they need to.
class SshConfigDocument
{
public:
    // Line ranges of one Host/Match block. `end` is exclusive and stops before the
    // next block (or its metadata comment).
    struct BlockRange {
        qsizetype metadataLine = -1;
        qsizetype headerLine = -1;
        qsizetype end = -1;
    };

    QList<SshConfigLine> lines;

    QByteArray serialize() const;

    // Host and Match blocks in file order. Include lines are not treated as options.
    QList<SshHost> hosts(const QString &sourceFile = {}, bool readOnly = true) const;
    // Same order and length as hosts().
    QList<BlockRange> blockRanges() const;
    // Unquoted arguments of every Include directive, in file order.
    QStringList includePatterns() const;

    // Index (into hosts()/blockRanges()) of the Host block whose alias matches, or -1.
    qsizetype findHost(QStringView alias) const;
    // True if any Host pattern equals `alias` (case-insensitive, like OpenSSH).
    bool containsPattern(QStringView alias) const;

    // "\r\n" if the file uses CRLF line endings, "\n" otherwise.
    QByteArray lineEnding() const;
};
