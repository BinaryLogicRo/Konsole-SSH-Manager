#include "sshconfig/sshconfigparser.h"

#include <QDir>
#include <QFile>
#include <QTest>

class TestSshConfigParser : public QObject
{
    Q_OBJECT

private:
    static QByteArray readFixture(const QString &name)
    {
        QFile file(QStringLiteral(TEST_DATA_DIR "/") + name);
        if (!file.open(QIODevice::ReadOnly)) {
            qFatal("Missing fixture");
        }
        return file.readAll();
    }

private Q_SLOTS:
    void roundTripIsLossless_data()
    {
        QTest::addColumn<QString>("fileName");
        const QStringList files = QDir(QStringLiteral(TEST_DATA_DIR)).entryList(QDir::Files, QDir::Name);
        QVERIFY(!files.isEmpty());
        for (const QString &file : files) {
            QTest::newRow(file.toUtf8().constData()) << file;
        }
    }

    void roundTripIsLossless()
    {
        QFETCH(QString, fileName);
        const QByteArray original = readFixture(fileName);
        QCOMPARE(SshConfigParser::parse(original).serialize(), original);
    }

    void parseLine_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("keyword");
        QTest::addColumn<QString>("argument");

        const int blank = int(SshConfigLine::Kind::Blank);
        const int comment = int(SshConfigLine::Kind::Comment);
        const int directive = int(SshConfigLine::Kind::Directive);
        QTest::newRow("space") << QByteArray("User alice\n") << directive << QStringLiteral("User") << QStringLiteral("alice");
        QTest::newRow("equals") << QByteArray("User=alice\n") << directive << QStringLiteral("User") << QStringLiteral("alice");
        QTest::newRow("spaced equals") << QByteArray("  User = alice  \n") << directive << QStringLiteral("User") << QStringLiteral("alice");
        QTest::newRow("tabs + crlf") << QByteArray("\tuser\talice\t\r\n") << directive << QStringLiteral("user") << QStringLiteral("alice");
        QTest::newRow("no terminator") << QByteArray("Port 22") << directive << QStringLiteral("Port") << QStringLiteral("22");
        QTest::newRow("no argument") << QByteArray("Port\n") << directive << QStringLiteral("Port") << QString();
        QTest::newRow("quoted") << QByteArray("IdentityFile \"~/a b\"\n") << directive << QStringLiteral("IdentityFile") << QStringLiteral("\"~/a b\"");
        QTest::newRow("comment") << QByteArray("  # hi\n") << comment << QString() << QString();
        QTest::newRow("blank") << QByteArray("   \t\n") << blank << QString() << QString();
        QTest::newRow("empty") << QByteArray("\n") << blank << QString() << QString();
        QTest::newRow("form feed") << QByteArray("Compression yes\f\n") << directive << QStringLiteral("Compression") << QStringLiteral("yes");
    }

    void parseLine()
    {
        QFETCH(QByteArray, raw);
        QFETCH(int, kind);
        QFETCH(QString, keyword);
        QFETCH(QString, argument);

        const SshConfigLine line = SshConfigParser::parseLine(raw);
        QCOMPARE(int(line.kind), kind);
        QCOMPARE(line.raw, raw);
        if (line.kind == SshConfigLine::Kind::Directive) {
            QCOMPARE(line.keyword, keyword);
            QCOMPARE(line.argument, argument);
            QCOMPARE(QString::fromUtf8(raw.mid(line.argumentStart, line.argumentEnd - line.argumentStart)), argument);
        }
    }

    void splitArguments_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("simple") << QStringLiteral("a b  c") << QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
        QTest::newRow("double quotes") << QStringLiteral("\"a b\" c") << QStringList{QStringLiteral("a b"), QStringLiteral("c")};
        QTest::newRow("single quotes") << QStringLiteral("'a \"b' c") << QStringList{QStringLiteral("a \"b"), QStringLiteral("c")};
        QTest::newRow("escaped space") << QStringLiteral("a\\ b") << QStringList{QStringLiteral("a b")};
        QTest::newRow("escaped quote") << QStringLiteral("a\\\"b") << QStringList{QStringLiteral("a\"b")};
        QTest::newRow("trailing comment") << QStringLiteral("jump bastion  # comment") << QStringList{QStringLiteral("jump"), QStringLiteral("bastion")};
        QTest::newRow("hash inside token") << QStringLiteral("a#b") << QStringList{QStringLiteral("a#b")};
        QTest::newRow("negation") << QStringLiteral("work-* !work-legacy") << QStringList{QStringLiteral("work-*"), QStringLiteral("!work-legacy")};
        QTest::newRow("empty") << QString() << QStringList{};
    }

    void splitArguments()
    {
        QFETCH(QString, input);
        QFETCH(QStringList, expected);
        QCOMPARE(SshConfigParser::splitArguments(input), expected);
    }

    void trailingCommentStart()
    {
        QCOMPARE(SshConfigParser::trailingCommentStart(u"jump bastion  # c"), qsizetype(12));
        QCOMPARE(SshConfigParser::trailingCommentStart(u"jump bastion"), qsizetype(-1));
        QCOMPARE(SshConfigParser::trailingCommentStart(u"\"a # b\""), qsizetype(-1));
    }

    void hostsFromComplexFile()
    {
        const SshConfigDocument document = SshConfigParser::parse(readFixture(QStringLiteral("complex.conf")));
        const QList<SshHost> hosts = document.hosts(QStringLiteral("complex.conf"), true);
        QCOMPARE(hosts.size(), 4);

        const SshHost &work = hosts.at(0);
        QCOMPARE(work.kind, SshHost::Kind::Host);
        QCOMPARE(work.patterns, (QStringList{QStringLiteral("work-*"), QStringLiteral("!work-legacy")}));
        QVERIFY(!work.isConnectable());
        QCOMPARE(work.optionValue(u"hostname"), QStringLiteral("work.example.com"));
        QCOMPARE(work.optionValue(u"Port"), QStringLiteral("2222"));
        QCOMPARE(work.lineNumber, 6);

        const SshHost &jump = hosts.at(1);
        QCOMPARE(jump.patterns, (QStringList{QStringLiteral("jump"), QStringLiteral("bastion")}));
        QCOMPARE(jump.alias(), QStringLiteral("jump"));
        // OpenSSH uses the first value for a keyword; keywords are case-insensitive.
        QCOMPARE(jump.optionValue(u"IDENTITYFILE"), QStringLiteral("~/.ssh/id_ed25519"));
        QCOMPARE(jump.options.size(), 5);
        QCOMPARE(jump.options.at(1), (SshOption{QStringLiteral("SomeFutureKeyword"), QStringLiteral("value")}));

        const SshHost &match = hosts.at(2);
        QCOMPARE(match.kind, SshHost::Kind::Match);
        QVERIFY(!match.isConnectable());
        QCOMPARE(match.displayName(), QStringLiteral("Match host *.internal exec test -f /tmp/x"));

        QCOMPARE(hosts.at(3).patterns, QStringList{QStringLiteral("?ingle")});
        QCOMPARE(hosts.at(3).optionValue(u"Compression"), QStringLiteral("yes"));

        QCOMPARE(document.includePatterns(), (QStringList{QStringLiteral("config.d/*"), QStringLiteral("extra dir/*.conf")}));
    }

    void metadataAttachesToFollowingHost()
    {
        const SshConfigDocument document = SshConfigParser::parse(readFixture(QStringLiteral("managed.conf")));
        const QList<SshHost> hosts = document.hosts();
        QCOMPARE(hosts.size(), 3);
        QCOMPARE(hosts.at(0).group(), QStringLiteral("Production"));
        QCOMPARE(hosts.at(0).color(), QStringLiteral("#d33"));
        QCOMPARE(hosts.at(0).note(), QStringLiteral("Primary \"DB\""));
        QCOMPARE(hosts.at(0).metadataValue(u"future"), QStringLiteral("kept"));
        QCOMPARE(hosts.at(1).group(), QStringLiteral("Staging"));
        QVERIFY(hosts.at(2).metadata.isEmpty());

        const QList<SshConfigDocument::BlockRange> ranges = document.blockRanges();
        QCOMPARE(ranges.size(), 3);
        QCOMPARE(ranges.at(0).metadataLine, qsizetype(0));
        QCOMPARE(ranges.at(0).headerLine, qsizetype(1));
        QCOMPARE(ranges.at(0).end, qsizetype(5));
        QCOMPARE(ranges.at(2).metadataLine, qsizetype(-1));
        QCOMPARE(ranges.at(2).end, document.lines.size());
    }

    void findHostIsCaseInsensitive()
    {
        const SshConfigDocument document = SshConfigParser::parse(readFixture(QStringLiteral("managed.conf")));
        QCOMPARE(document.findHost(u"STAGE-WEB"), qsizetype(1));
        QCOMPARE(document.findHost(u"missing"), qsizetype(-1));
        QVERIFY(document.containsPattern(u"Plain"));
    }

    void lineEndingDetection()
    {
        QCOMPARE(SshConfigParser::parse(readFixture(QStringLiteral("crlf.conf"))).lineEnding(), QByteArray("\r\n"));
        QCOMPARE(SshConfigParser::parse(readFixture(QStringLiteral("basic.conf"))).lineEnding(), QByteArray("\n"));
        QCOMPARE(SshConfigParser::parse(QByteArray()).lineEnding(), QByteArray("\n"));
    }

    void crlfHostsParse()
    {
        const QList<SshHost> hosts = SshConfigParser::parse(readFixture(QStringLiteral("crlf.conf"))).hosts();
        QCOMPARE(hosts.size(), 1);
        QCOMPARE(hosts.at(0).alias(), QStringLiteral("prod-db"));
        QCOMPARE(hosts.at(0).optionValue(u"User"), QStringLiteral("admin"));
        QCOMPARE(hosts.at(0).group(), QStringLiteral("Production"));
    }
};

QTEST_GUILESS_MAIN(TestSshConfigParser)

#include "tst_sshconfigparser.moc"
