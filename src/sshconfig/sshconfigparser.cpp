#include "sshconfigparser.h"

#include <QFile>

namespace
{
bool isBlank(char c)
{
    return c == ' ' || c == '\t';
}

// OpenSSH strips trailing " \t\r\n" and form feeds.
bool isTrailingWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

bool isQuote(QChar c)
{
    return c == QLatin1Char('"') || c == QLatin1Char('\'');
}

bool isSpaceOrTab(QChar c)
{
    return c == QLatin1Char(' ') || c == QLatin1Char('\t');
}

// Walks `arguments` like argv_split(). Calls `onToken(start, end, text)` for each
// token and returns the index where a trailing comment starts, or -1.
template<typename Callback>
qsizetype scanArguments(QStringView s, Callback onToken)
{
    const qsizetype n = s.size();
    qsizetype i = 0;
    while (true) {
        while (i < n && isSpaceOrTab(s[i])) {
            ++i;
        }
        if (i >= n) {
            return -1;
        }
        if (s[i] == QLatin1Char('#')) {
            return i;
        }
        const qsizetype start = i;
        QString token;
        QChar quote;
        for (; i < n; ++i) {
            const QChar c = s[i];
            if (quote.isNull() && isSpaceOrTab(c)) {
                break;
            }
            if (c == QLatin1Char('\\') && i + 1 < n) {
                const QChar next = s[i + 1];
                if (isQuote(next) || next == QLatin1Char('\\') || (quote.isNull() && next == QLatin1Char(' '))) {
                    token += next;
                    ++i;
                    continue;
                }
            }
            if (isQuote(c)) {
                if (quote.isNull()) {
                    quote = c;
                    continue;
                }
                if (quote == c) {
                    quote = QChar();
                    continue;
                }
            }
            token += c;
        }
        onToken(start, i, token);
    }
}
}

SshConfigDocument SshConfigParser::parse(const QByteArray &data)
{
    SshConfigDocument document;
    qsizetype start = 0;
    while (start < data.size()) {
        const qsizetype newline = data.indexOf('\n', start);
        if (newline < 0) {
            document.lines.append(parseLine(data.mid(start)));
            break;
        }
        document.lines.append(parseLine(data.mid(start, newline - start + 1)));
        start = newline + 1;
    }
    return document;
}

SshConfigLine SshConfigParser::parseLine(const QByteArray &raw)
{
    SshConfigLine line;
    line.raw = raw;

    qsizetype end = raw.size();
    while (end > 0 && isTrailingWhitespace(raw.at(end - 1))) {
        --end;
    }
    qsizetype pos = 0;
    while (pos < end && isBlank(raw.at(pos))) {
        ++pos;
    }
    if (pos >= end) {
        line.kind = SshConfigLine::Kind::Blank;
        return line;
    }
    if (raw.at(pos) == '#') {
        line.kind = SshConfigLine::Kind::Comment;
        return line;
    }

    line.kind = SshConfigLine::Kind::Directive;
    const qsizetype keywordStart = pos;
    while (pos < end && !isBlank(raw.at(pos)) && raw.at(pos) != '=') {
        ++pos;
    }
    line.keyword = QString::fromUtf8(raw.mid(keywordStart, pos - keywordStart));

    while (pos < end && isBlank(raw.at(pos))) {
        ++pos;
    }
    if (pos < end && raw.at(pos) == '=') {
        ++pos;
        while (pos < end && isBlank(raw.at(pos))) {
            ++pos;
        }
    }
    line.argumentStart = pos;
    line.argumentEnd = end;
    line.argument = QString::fromUtf8(raw.mid(pos, end - pos));
    return line;
}

QStringList SshConfigParser::splitArguments(QStringView arguments)
{
    QStringList result;
    scanArguments(arguments, [&result](qsizetype, qsizetype, const QString &token) {
        result.append(token);
    });
    return result;
}

qsizetype SshConfigParser::trailingCommentStart(QStringView arguments)
{
    qsizetype lastTokenEnd = 0;
    const qsizetype comment = scanArguments(arguments, [&lastTokenEnd](qsizetype, qsizetype end, const QString &) {
        lastTokenEnd = end;
    });
    return comment < 0 ? -1 : lastTokenEnd;
}

bool SshConfigParser::parseFile(const QString &path, SshConfigDocument *document, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    *document = parse(file.readAll());
    return true;
}
