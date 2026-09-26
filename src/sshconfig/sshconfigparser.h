#pragma once

#include "sshconfigdocument.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// Lossless ssh_config parser. Tokenizing follows OpenSSH's readconf.c:
// `Keyword value`, `Keyword=value` and `Keyword = value` are all accepted,
// keywords are case-insensitive, and arguments may be quoted.
namespace SshConfigParser
{
SshConfigDocument parse(const QByteArray &data);

// Parses one line; `raw` may include its terminator.
SshConfigLine parseLine(const QByteArray &raw);

// Splits an argument string like OpenSSH's argv_split(): whitespace separated,
// single/double quotes, backslash escapes, and an unquoted token starting with
// '#' ends the list (trailing comment).
QStringList splitArguments(QStringView arguments);

// Index in `arguments` where a trailing comment starts (including the whitespace
// before it), or -1 when there is none.
qsizetype trailingCommentStart(QStringView arguments);

// Reads and parses `path`. Returns false and sets `error` if it can't be read.
bool parseFile(const QString &path, SshConfigDocument *document, QString *error);
}
