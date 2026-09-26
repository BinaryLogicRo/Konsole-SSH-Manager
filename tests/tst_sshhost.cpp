#include "sshconfig/sshhost.h"

#include <QTest>

class TestSshHost : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aliasValidation_data()
    {
        QTest::addColumn<QString>("alias");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<bool>("concrete");

        QTest::newRow("plain") << QStringLiteral("prod-db") << true << true;
        QTest::newRow("dots and at") << QStringLiteral("user@host.example.com") << true << true;
        QTest::newRow("unicode") << QStringLiteral("café") << true << true;
        QTest::newRow("empty") << QString() << false << false;
        QTest::newRow("leading dash") << QStringLiteral("-oProxyCommand=x") << false << false;
        QTest::newRow("space") << QStringLiteral("a b") << false << false;
        QTest::newRow("tab") << QStringLiteral("a\tb") << false << false;
        QTest::newRow("newline") << QStringLiteral("a\nb") << false << false;
        QTest::newRow("control") << QStringLiteral("a\x01") << false << false;
        QTest::newRow("quote") << QStringLiteral("a\"b") << false << false;
        QTest::newRow("hash") << QStringLiteral("a#b") << false << false;
        QTest::newRow("wildcard") << QStringLiteral("web-*") << true << false;
        QTest::newRow("question") << QStringLiteral("web?") << true << false;
        QTest::newRow("negated") << QStringLiteral("!web") << true << false;
    }

    void aliasValidation()
    {
        QFETCH(QString, alias);
        QFETCH(bool, valid);
        QFETCH(bool, concrete);
        QCOMPARE(SshValidation::isValidAlias(alias), valid);
        QCOMPARE(SshValidation::isConcretePattern(alias), concrete);
    }

    void keywordAndValueValidation()
    {
        QVERIFY(SshValidation::isValidKeyword(u"HostName"));
        QVERIFY(SshValidation::isValidKeyword(u"PKCS11Provider"));
        QVERIFY(!SshValidation::isValidKeyword(u""));
        QVERIFY(!SshValidation::isValidKeyword(u"Host Name"));
        QVERIFY(!SshValidation::isValidKeyword(u"Port=22"));

        QVERIFY(SshValidation::isValidValue(u"\"~/my key\""));
        QVERIFY(!SshValidation::isValidValue(u""));
        QVERIFY(!SshValidation::isValidValue(u"   "));
        QVERIFY(!SshValidation::isValidValue(u"a\nHost evil"));
        QVERIFY(!SshValidation::isValidValue(u"a\rb"));
    }

    void aliasSkipsPatterns()
    {
        SshHost host;
        host.patterns = QStringList{QStringLiteral("*.example.com"), QStringLiteral("!bad"), QStringLiteral("good"), QStringLiteral("other")};
        QCOMPARE(host.alias(), QStringLiteral("good"));
        host.kind = SshHost::Kind::Match;
        QCOMPARE(host.alias(), QString());
        QVERIFY(!host.isConnectable());
    }

    void metadataValues()
    {
        SshHost host;
        host.setMetadataValue(QStringLiteral("group"), QStringLiteral("A"));
        host.setMetadataValue(QStringLiteral("future"), QStringLiteral("x"));
        host.setMetadataValue(QStringLiteral("group"), QStringLiteral("B"));
        QCOMPARE(host.metadata, (SshMetadata{{QStringLiteral("group"), QStringLiteral("B")}, {QStringLiteral("future"), QStringLiteral("x")}}));
        host.setMetadataValue(QStringLiteral("group"), QString());
        QCOMPARE(host.metadata, (SshMetadata{{QStringLiteral("future"), QStringLiteral("x")}}));
    }

    void metadataParse()
    {
        const SshMetadata parsed = SshMetadataFormat::parse(u"# sshmanager: group=\"Production\" color=\"#d33\" note=\"Primary DB\"");
        QCOMPARE(parsed,
                 (SshMetadata{{QStringLiteral("group"), QStringLiteral("Production")},
                              {QStringLiteral("color"), QStringLiteral("#d33")},
                              {QStringLiteral("note"), QStringLiteral("Primary DB")}}));
        QVERIFY(SshMetadataFormat::isMetadataComment(u"  #sshmanager: x=\"1\""));
        QVERIFY(!SshMetadataFormat::isMetadataComment(u"# just a comment"));
        QVERIFY(!SshMetadataFormat::isMetadataComment(u"Host sshmanager:"));
        // Malformed tail: keep what parsed cleanly.
        QCOMPARE(SshMetadataFormat::parse(u"# sshmanager: a=\"1\" broken b=\"2\""), (SshMetadata{{QStringLiteral("a"), QStringLiteral("1")}}));
    }

    void metadataFormatRoundTrip()
    {
        const SshMetadata metadata{
            {QStringLiteral("group"), QStringLiteral("Prod \"EU\"")},
            {QStringLiteral("note"), QStringLiteral("line1\nline2 \\ end")},
            {QStringLiteral("unknown-key"), QStringLiteral("v")},
        };
        const QString line = SshMetadataFormat::format(metadata);
        QVERIFY(!line.contains(QLatin1Char('\n')));
        QCOMPARE(line, QStringLiteral("# sshmanager: group=\"Prod \\\"EU\\\"\" note=\"line1\\nline2 \\\\ end\" unknown-key=\"v\""));
        QCOMPARE(SshMetadataFormat::parse(line), metadata);
    }

    void sameContentIgnoresLocation()
    {
        SshHost a;
        a.patterns = QStringList{QStringLiteral("x")};
        a.options = {{QStringLiteral("User"), QStringLiteral("u")}};
        SshHost b = a;
        b.sourceFile = QStringLiteral("/elsewhere");
        b.lineNumber = 42;
        QVERIFY(a.hasSameContent(b));
        b.options.append({QStringLiteral("Port"), QStringLiteral("2")});
        QVERIFY(!a.hasSameContent(b));
    }
};

QTEST_GUILESS_MAIN(TestSshHost)

#include "tst_sshhost.moc"
