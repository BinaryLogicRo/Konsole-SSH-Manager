#include "sshconfig/sshconfigparser.h"
#include "sshconfig/sshconfigwriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace
{
SshHost makeHost(const QString &alias, const QList<SshOption> &options, const SshMetadata &metadata = {})
{
    SshHost host;
    host.patterns = QStringList{alias};
    host.options = options;
    host.metadata = metadata;
    host.readOnly = false;
    return host;
}

SshHost hostFrom(const SshConfigDocument &document, const QString &alias)
{
    const qsizetype index = document.findHost(alias);
    return index < 0 ? SshHost() : document.hosts().at(index);
}

// Owner, group and other bits of a path's permissions.
int mode(const QString &path)
{
    return int(QFileInfo(path).permissions()) & 0x7077;
}

const QByteArray s_managed = QByteArrayLiteral(
    "# sshmanager: group=\"Production\" color=\"#d33\" future=\"kept\"\n"
    "Host prod-db   # primary\n"
    "  HostName 10.0.0.12\n"
    "  user   admin\n"
    "  # a comment inside the block\n"
    "  UnknownKeyword something\n"
    "  IdentityFile ~/.ssh/a\n"
    "  IdentityFile ~/.ssh/b\n"
    "\n"
    "# comment about the next host\n"
    "Host other\n"
    "    HostName other.example.com\n");
}

class TestSshConfigWriter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addToEmptyDocument()
    {
        SshConfigDocument document;
        const SshHost host = makeHost(QStringLiteral("prod-db"),
                                      {{QStringLiteral("HostName"), QStringLiteral("10.0.0.12")}, {QStringLiteral("User"), QStringLiteral("admin")}},
                                      {{QStringLiteral("group"), QStringLiteral("Production")}, {QStringLiteral("color"), QStringLiteral("#d33")}});
        QVERIFY(SshConfigWriter::addHost(document, host));
        QCOMPARE(document.serialize(),
                 QByteArray("# sshmanager: group=\"Production\" color=\"#d33\"\n"
                            "Host prod-db\n"
                            "    HostName 10.0.0.12\n"
                            "    User admin\n"));
        // What we write reads back as the same host.
        QVERIFY(hostFrom(SshConfigParser::parse(document.serialize()), QStringLiteral("prod-db")).hasSameContent(host));
    }

    void addTerminatesLastLineAndSeparatesBlocks()
    {
        SshConfigDocument document = SshConfigParser::parse("Host last\n    HostName last.example.com");
        QVERIFY(SshConfigWriter::addHost(document, makeHost(QStringLiteral("new"), {{QStringLiteral("User"), QStringLiteral("u")}})));
        QCOMPARE(document.serialize(), QByteArray("Host last\n    HostName last.example.com\n\nHost new\n    User u\n"));
    }

    void addKeepsCrlf()
    {
        SshConfigDocument document = SshConfigParser::parse("Host a\r\n    User x\r\n");
        QVERIFY(SshConfigWriter::addHost(document, makeHost(QStringLiteral("b"), {{QStringLiteral("User"), QStringLiteral("y")}})));
        QCOMPARE(document.serialize(), QByteArray("Host a\r\n    User x\r\n\r\nHost b\r\n    User y\r\n"));
    }

    void rejectsInvalidHosts_data()
    {
        QTest::addColumn<QString>("alias");
        QTest::addColumn<QString>("keyword");
        QTest::addColumn<QString>("value");

        QTest::newRow("option injection") << QStringLiteral("-oProxyCommand=evil") << QStringLiteral("User") << QStringLiteral("u");
        QTest::newRow("wildcard only") << QStringLiteral("*") << QStringLiteral("User") << QStringLiteral("u");
        QTest::newRow("newline in value") << QStringLiteral("ok") << QStringLiteral("User") << QStringLiteral("u\nHost evil");
        QTest::newRow("bad keyword") << QStringLiteral("ok") << QStringLiteral("User Name") << QStringLiteral("u");
        QTest::newRow("nested host") << QStringLiteral("ok") << QStringLiteral("Host") << QStringLiteral("evil");
        QTest::newRow("include") << QStringLiteral("ok") << QStringLiteral("include") << QStringLiteral("/etc/passwd");
        QTest::newRow("empty value") << QStringLiteral("ok") << QStringLiteral("User") << QString();
    }

    void rejectsInvalidHosts()
    {
        QFETCH(QString, alias);
        QFETCH(QString, keyword);
        QFETCH(QString, value);
        SshConfigDocument document;
        QVERIFY(!SshConfigWriter::addHost(document, makeHost(alias, {{keyword, value}})));
        QVERIFY(document.lines.isEmpty());
    }

    void updateWithoutChangesIsNoOp()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        const SshHost host = hostFrom(document, QStringLiteral("prod-db"));
        QVERIFY(SshConfigWriter::updateHost(document, u"prod-db", host));
        QCOMPARE(document.serialize(), s_managed);
    }

    void updateChangesOnlyTouchedLines()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        SshHost host = hostFrom(document, QStringLiteral("prod-db"));
        // Change HostName, drop the second IdentityFile, add Port.
        host.options[0].value = QStringLiteral("10.0.0.13");
        host.options.removeAt(4);
        host.options.append({QStringLiteral("Port"), QStringLiteral("2222")});
        QVERIFY(SshConfigWriter::updateHost(document, u"prod-db", host));
        QCOMPARE(document.serialize(),
                 QByteArray("# sshmanager: group=\"Production\" color=\"#d33\" future=\"kept\"\n"
                            "Host prod-db   # primary\n"
                            "  HostName 10.0.0.13\n"
                            "  user   admin\n"
                            "  # a comment inside the block\n"
                            "  UnknownKeyword something\n"
                            "  IdentityFile ~/.ssh/a\n"
                            "  Port 2222\n"
                            "\n"
                            "# comment about the next host\n"
                            "Host other\n"
                            "    HostName other.example.com\n"));
    }

    void updateRenameKeepsTrailingComment()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        SshHost host = hostFrom(document, QStringLiteral("prod-db"));
        host.patterns = QStringList{QStringLiteral("prod-db-1")};
        QVERIFY(SshConfigWriter::updateHost(document, u"PROD-DB", host));
        QVERIFY(document.serialize().contains("\nHost prod-db-1   # primary\n"));
        QCOMPARE(document.findHost(u"prod-db"), qsizetype(-1));
        QCOMPARE(document.findHost(u"prod-db-1"), qsizetype(0));
    }

    void updateMetadataPreservesUnknownKeys()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        SshHost host = hostFrom(document, QStringLiteral("prod-db"));
        host.setMetadataValue(QStringLiteral("group"), QStringLiteral("Staging"));
        host.setMetadataValue(QStringLiteral("note"), QStringLiteral("hi"));
        QVERIFY(SshConfigWriter::updateHost(document, u"prod-db", host));
        QVERIFY(document.serialize().startsWith("# sshmanager: group=\"Staging\" color=\"#d33\" future=\"kept\" note=\"hi\"\nHost prod-db"));
    }

    void updateAddsAndRemovesMetadataLine()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        SshHost other = hostFrom(document, QStringLiteral("other"));
        other.setMetadataValue(QStringLiteral("group"), QStringLiteral("G"));
        QVERIFY(SshConfigWriter::updateHost(document, u"other", other));
        QVERIFY(document.serialize().contains("# comment about the next host\n# sshmanager: group=\"G\"\nHost other\n"));

        other.metadata.clear();
        QVERIFY(SshConfigWriter::updateHost(document, u"other", other));
        QCOMPARE(document.serialize(), s_managed);
    }

    void updateAppendsToFileWithoutTrailingNewline()
    {
        SshConfigDocument document = SshConfigParser::parse("Host last\n    HostName last.example.com");
        SshHost host = hostFrom(document, QStringLiteral("last"));
        host.options.append({QStringLiteral("User"), QStringLiteral("u")});
        QVERIFY(SshConfigWriter::updateHost(document, u"last", host));
        QCOMPARE(document.serialize(), QByteArray("Host last\n    HostName last.example.com\n    User u\n"));
    }

    void updateMissingHostFails()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        QVERIFY(!SshConfigWriter::updateHost(document, u"nope", makeHost(QStringLiteral("nope"), {})));
        QCOMPARE(document.serialize(), s_managed);
    }

    void removeFirstKeepsNextHostsComment()
    {
        SshConfigDocument document = SshConfigParser::parse(s_managed);
        QVERIFY(SshConfigWriter::removeHost(document, u"prod-db"));
        QCOMPARE(document.serialize(), QByteArray("# comment about the next host\nHost other\n    HostName other.example.com\n"));
    }

    void removeLastDropsDanglingBlankLines()
    {
        SshConfigDocument document = SshConfigParser::parse("Host a\n    User x\n\nHost b\n    User y\n");
        QVERIFY(SshConfigWriter::removeHost(document, u"b"));
        QCOMPARE(document.serialize(), QByteArray("Host a\n    User x\n"));
    }

    void removeMiddle()
    {
        SshConfigDocument document = SshConfigParser::parse("Host a\n  User x\n\n# sshmanager: group=\"G\"\nHost b\n  User y\n\nHost c\n  User z\n");
        QVERIFY(SshConfigWriter::removeHost(document, u"b"));
        QCOMPARE(document.serialize(), QByteArray("Host a\n  User x\n\nHost c\n  User z\n"));
        QVERIFY(!SshConfigWriter::removeHost(document, u"b"));
    }

    void includeLineGoesFirst()
    {
        QCOMPARE(SshConfigWriter::withIncludeLine("Host a\n  User x\n", QStringLiteral("config.d/*")), QByteArray("Include config.d/*\nHost a\n  User x\n"));
        QCOMPARE(SshConfigWriter::withIncludeLine("# c\r\nHost a\r\n", QStringLiteral("config.d/*")), QByteArray("Include config.d/*\r\n# c\r\nHost a\r\n"));
        QCOMPARE(SshConfigWriter::withIncludeLine(QByteArray(), QStringLiteral("config.d/*")), QByteArray("Include config.d/*\n"));
    }

    void writeFileSetsPermissionsAndBacksUpOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString configDir = dir.filePath(QStringLiteral("config.d"));
        const QString path = configDir + QStringLiteral("/managed.conf");
        const QString backups = dir.filePath(QStringLiteral("backups"));

        QString error;
        QVERIFY(SshConfigWriter::ensurePrivateDirectory(configDir, &error));
        QCOMPARE(mode(configDir), int(SshConfigWriter::DirectoryPermissions));

        SshConfigWriter writer(backups);
        // First write of a new file: nothing to back up.
        QVERIFY2(writer.writeFile(path, "one\n", SshConfigWriter::FilePermissions, &error), qPrintable(error));
        QCOMPARE(mode(path), int(SshConfigWriter::FilePermissions));
        QVERIFY(!QFileInfo::exists(backups));

        // A new session backs up the existing file once, before its first write.
        SshConfigWriter session(backups);
        QVERIFY(session.writeFile(path, "two\n", SshConfigWriter::FilePermissions, &error));
        QVERIFY(session.writeFile(path, "three\n", SshConfigWriter::FilePermissions, &error));
        const QStringList backupFiles = QDir(backups).entryList(QDir::Files);
        QCOMPARE(backupFiles.size(), 1);
        QVERIFY(backupFiles.first().startsWith(QLatin1String("managed.conf.")));
        QFile backup(QDir(backups).filePath(backupFiles.first()));
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(backup.readAll(), QByteArray("one\n"));
        QCOMPARE(mode(backup.fileName()), int(SshConfigWriter::FilePermissions));
        QCOMPARE(mode(backups), int(SshConfigWriter::DirectoryPermissions));

        QFile written(path);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(written.readAll(), QByteArray("three\n"));
    }
};

QTEST_GUILESS_MAIN(TestSshConfigWriter)

#include "tst_sshconfigwriter.moc"
