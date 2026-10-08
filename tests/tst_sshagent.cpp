#include "sshagent/sshagentformat.h"
#include "sshagent/sshkeylist.h"

#include <QTest>

namespace
{
// Throwaway test keys; the fingerprints are what `ssh-keygen -l -E sha256` prints for them.
const QByteArray s_ed25519 = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIBo68voHlxosyS+bzYnYAHgHXSzLz8IYmxO2HKNjZGK8 demo ed25519 key";
const QString s_ed25519Fingerprint = QStringLiteral("SHA256:herqzo/Ojfq+/SquVhZGxGbGh1DtHJ3CB56SWsV2u8s");

const QByteArray s_rsa =
    "ssh-rsa "
    "AAAAB3NzaC1yc2EAAAADAQABAAABAQCyqS17MrtuhR4Zf14OwCVa+RaaN3M0YMuBJs4I+0ZuEKwO+JsYaWbTZzhqdyHyEf9iQouuz7hgzkuFD6bPO/ddaWMdefNfN4Utwn3cUKGecdRDKiwYkoD5rKzA/"
    "RfCtYWL3filj3/YjoicmoNFW+2B/eUoSq+Ob/f+qg8uxZkHmsu3LqVti+VhVx9oI3mIs/1b6F4/Z6ZU5G+uIUE4DMQ9WPGY+FgM20UdjGqXiz/pd8nmP3tZcJi4hhiSpYtBqInfgTG/"
    "qBNO7HLYb0hA++uYgYDEgd6EtXT9ho5Tzzlf6X1kCLcWQm1yr0NSnL/aCREH3jMPelduzdFUSs5FfJqx";
const QString s_rsaFingerprint = QStringLiteral("SHA256:KQaqPw68o5iag4JhTW4L+54g2fqWTZFn9SLcpJ5/CH8");

const QByteArray s_ecdsa =
    "ecdsa-sha2-nistp256 "
    "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBDojGgIghMiFUarP40iMXvMyZymTx1PHpF2QMPdpnj4xS0VNL+TFwm7MwBDHXJqYnf1QOd2MxQ4L3kqsyeuuNCU= "
    "ecdsa@example.test";
const QString s_ecdsaFingerprint = QStringLiteral("SHA256:xpEjXDJ0g6WLGfmhCTkev01aXVi6URYxZQpjWOGaaIA");

// A certificate for the ED25519 key above; ssh-keygen reports the key's own fingerprint for it.
const QByteArray s_ed25519Certificate =
    "ssh-ed25519-cert-v01@openssh.com "
    "AAAAIHNzaC1lZDI1NTE5LWNlcnQtdjAxQG9wZW5zc2guY29tAAAAIClV5szOhLhzTAvlwnbJ5JX8PnDUxuYz6JRpqJcDLkn5AAAAIBo68voHlxosyS+"
    "bzYnYAHgHXSzLz8IYmxO2HKNjZGK8AAAAAAAAAAAAAAABAAAABGRlbW8AAAAIAAAABGRlbW8AAAAAAAAAAP//////////"
    "AAAAAAAAAIIAAAAVcGVybWl0LVgxMS1mb3J3YXJkaW5nAAAAAAAAABdwZXJtaXQtYWdlbnQtZm9yd2FyZGluZwAAAAAAAAAWcGVybWl0LXBvcnQtZm9yd2FyZGluZwAAAAAAAAAKcGVybWl0LXB0eQAAAA"
    "AAAAAOcGVybWl0LXVzZXItcmMAAAAAAAAAAAAAADMAAAALc3NoLWVkMjU1MTkAAAAg+St/"
    "Knu9nJY4csAXxjmputkbYp46WTetYJHi9kEl2wAAAABTAAAAC3NzaC1lZDI1NTE5AAAAQHZk067DtfSNiBv3vxnbrzAcKFaq9PFDXQ2eQPMjZSWnJ5UwBo6fqB6Hnwrl3/LkYknvwCulDMbObb5iD+bi/"
    "AI= "
    "demo ed25519 key";

SshAgentKey agentKey(const QByteArray &line)
{
    const QList<SshAgentKey> keys = SshAgentFormat::parsePublicKeys(line);
    return keys.isEmpty() ? SshAgentKey{} : keys.first();
}

SshKeyFileStatus keyFile(const QString &fingerprint, const QString &type = QStringLiteral("ED25519"))
{
    return {true, SshKeyFileInfo{256, fingerprint, QString(), type}};
}
}

class TestSshAgent : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsePublicKeys()
    {
        const QByteArray output = s_ed25519 + '\n' + s_rsa + '\n' + s_ecdsa + "\n";
        const QList<SshAgentKey> keys = SshAgentFormat::parsePublicKeys(output);
        QCOMPARE(keys.size(), 3);

        QCOMPARE(keys.at(0).keyType, QStringLiteral("ssh-ed25519"));
        QCOMPARE(keys.at(0).comment, QStringLiteral("demo ed25519 key"));
        QCOMPARE(keys.at(0).fingerprint, s_ed25519Fingerprint);
        QCOMPARE(keys.at(0).publicKeyLine, s_ed25519);
        QVERIFY(!keys.at(0).isCertificate());

        QCOMPARE(keys.at(1).comment, QString());
        QCOMPARE(keys.at(1).fingerprint, s_rsaFingerprint);
        QCOMPARE(keys.at(2).comment, QStringLiteral("ecdsa@example.test"));
        QCOMPARE(keys.at(2).fingerprint, s_ecdsaFingerprint);
    }

    void parsePublicKeysSkipsOtherLines()
    {
        QVERIFY(SshAgentFormat::parsePublicKeys("The agent has no identities.\n").isEmpty());
        QVERIFY(SshAgentFormat::parsePublicKeys("").isEmpty());
        QVERIFY(SshAgentFormat::parsePublicKeys("ssh-ed25519 not-base64!! comment\n").isEmpty());
        // The blob's own type must match the line's.
        QVERIFY(SshAgentFormat::parsePublicKeys("ssh-rsa " + s_ed25519.mid(12)).isEmpty());
        // A truncated certificate is rejected.
        QVERIFY(SshAgentFormat::parsePublicKeys(s_ed25519Certificate.left(97)).isEmpty());
        QCOMPARE(SshAgentFormat::parsePublicKeys("garbage\n" + s_ed25519 + "\r\n").size(), 1);
    }

    void certificateFingerprintIsTheKeys()
    {
        const SshAgentKey certificate = agentKey(s_ed25519Certificate);
        QVERIFY(certificate.isCertificate());
        QCOMPARE(certificate.fingerprint, s_ed25519Fingerprint);
        QCOMPARE(SshAgentFormat::displayType(certificate.keyType), QStringLiteral("ED25519-CERT"));
    }

    void displayType_data()
    {
        QTest::addColumn<QString>("keyType");
        QTest::addColumn<QString>("display");

        QTest::newRow("rsa") << QStringLiteral("ssh-rsa") << QStringLiteral("RSA");
        QTest::newRow("ed25519") << QStringLiteral("ssh-ed25519") << QStringLiteral("ED25519");
        QTest::newRow("ecdsa 384") << QStringLiteral("ecdsa-sha2-nistp384") << QStringLiteral("ECDSA");
        QTest::newRow("ed25519 sk") << QStringLiteral("sk-ssh-ed25519@openssh.com") << QStringLiteral("ED25519-SK");
        QTest::newRow("ecdsa sk") << QStringLiteral("sk-ecdsa-sha2-nistp256@openssh.com") << QStringLiteral("ECDSA-SK");
        QTest::newRow("rsa cert") << QStringLiteral("ssh-rsa-cert-v01@openssh.com") << QStringLiteral("RSA-CERT");
        QTest::newRow("ecdsa sk cert") << QStringLiteral("sk-ecdsa-sha2-nistp256-cert-v01@openssh.com") << QStringLiteral("ECDSA-SK-CERT");
        QTest::newRow("unknown") << QStringLiteral("ssh-future") << QStringLiteral("ssh-future");
    }

    void displayType()
    {
        QFETCH(QString, keyType);
        QFETCH(QString, display);
        QCOMPARE(SshAgentFormat::displayType(keyType), display);
    }

    void parseFingerprint()
    {
        auto info = SshAgentFormat::parseFingerprint("256 SHA256:herqzo/Ojfq+/SquVhZGxGbGh1DtHJ3CB56SWsV2u8s demo (work) key (ED25519)\n");
        QVERIFY(info);
        QCOMPARE(info->bits, 256);
        QCOMPARE(info->fingerprint, s_ed25519Fingerprint);
        QCOMPARE(info->comment, QStringLiteral("demo (work) key"));
        QCOMPARE(info->type, QStringLiteral("ED25519"));

        info = SshAgentFormat::parseFingerprint("2048 SHA256:KQaqPw68o5iag4JhTW4L+54g2fqWTZFn9SLcpJ5/CH8 no comment (RSA)\n");
        QVERIFY(info);
        QCOMPARE(info->comment, QString());
        QCOMPARE(info->type, QStringLiteral("RSA"));

        info = SshAgentFormat::parseFingerprint("256 SHA256:xpEjXDJ0g6WLGfmhCTkev01aXVi6URYxZQpjWOGaaIA ecdsa@example.test (ECDSA)");
        QVERIFY(info);
        QCOMPARE(info->type, QStringLiteral("ECDSA"));

        QVERIFY(!SshAgentFormat::parseFingerprint("ssh-keygen: /x: No such file or directory\n"));
        QVERIFY(!SshAgentFormat::parseFingerprint("256 MD5:aa:bb comment (ED25519)"));
        QVERIFY(!SshAgentFormat::parseFingerprint(""));
    }

    void message()
    {
        const QByteArray warning =
            "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
            "@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @\n"
            "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
            "Permissions 0644 for '/home/u/.ssh/id' are too open.\n"
            "This private key will be ignored.\n";
        QCOMPARE(
            SshAgentFormat::message(warning),
            QStringLiteral("WARNING: UNPROTECTED PRIVATE KEY FILE! Permissions 0644 for '/home/u/.ssh/id' are too open. This private key will be ignored."));
        QCOMPARE(SshAgentFormat::message("  \n"), QString());
    }

    void resolveIdentityFile_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("resolved");

        QTest::newRow("tilde") << QStringLiteral("~/.ssh/id_ed25519") << QStringLiteral("/home/u/.ssh/id_ed25519") << true;
        QTest::newRow("absolute") << QStringLiteral("/keys/./work//id") << QStringLiteral("/keys/work/id") << true;
        QTest::newRow("quoted") << QStringLiteral("\"~/my keys/id\"") << QStringLiteral("/home/u/my keys/id") << true;
        QTest::newRow("home token") << QStringLiteral("%d/.ssh/id") << QStringLiteral("/home/u/.ssh/id") << true;
        QTest::newRow("user token") << QStringLiteral("/keys/%u/id") << QStringLiteral("/keys/u/id") << true;
        QTest::newRow("percent") << QStringLiteral("/keys/100%%/id") << QStringLiteral("/keys/100%/id") << true;
        QTest::newRow("host token") << QStringLiteral("~/.ssh/%h") << QStringLiteral("~/.ssh/%h") << false;
        QTest::newRow("trailing percent") << QStringLiteral("/keys/id%") << QStringLiteral("/keys/id%") << false;
        QTest::newRow("environment") << QStringLiteral("${KEYS}/id") << QStringLiteral("${KEYS}/id") << false;
        QTest::newRow("relative") << QStringLiteral(".ssh/id") << QStringLiteral(".ssh/id") << false;
        QTest::newRow("other user") << QStringLiteral("~other/.ssh/id") << QStringLiteral("~other/.ssh/id") << false;
    }

    void resolveIdentityFile()
    {
        QFETCH(QString, value);
        QFETCH(QString, path);
        QFETCH(bool, resolved);
        const SshIdentityFile file = SshIdentityFiles::resolve(value, QStringLiteral("/home/u"), QStringLiteral("u"));
        QCOMPARE(file.path, path);
        QCOMPARE(file.resolved, resolved);
    }

    void identityFilesFromHosts()
    {
        SshHost web;
        web.patterns = QStringList{QStringLiteral("web")};
        web.options = {{QStringLiteral("IdentityFile"), QStringLiteral("~/.ssh/web")},
                       {QStringLiteral("identityfile"), QStringLiteral("~/.ssh/extra")},
                       {QStringLiteral("User"), QStringLiteral("deploy")}};
        SshHost match;
        match.kind = SshHost::Kind::Match;
        match.patterns = QStringList{QStringLiteral("host"), QStringLiteral("db")};
        match.options = {{QStringLiteral("IDENTITYFILE"), QStringLiteral("/home/u/.ssh/web")}, {QStringLiteral("IdentityFile"), QStringLiteral("~/.ssh/%r")}};

        const QList<SshIdentityFile> files = SshIdentityFiles::fromHosts({web, match}, QStringLiteral("/home/u"), QStringLiteral("u"));
        const QList<SshIdentityFile> expected{{QStringLiteral("/home/u/.ssh/web"), true},
                                              {QStringLiteral("/home/u/.ssh/extra"), true},
                                              {QStringLiteral("~/.ssh/%r"), false}};
        QCOMPARE(files, expected);
    }

    void buildKeyList()
    {
        const QList<SshIdentityFile> config{{QStringLiteral("/k/ed"), true}, {QStringLiteral("/k/gone"), true}, {QStringLiteral("~/.ssh/%h"), false}};
        const QStringList added{QStringLiteral("/k/rsa"), QStringLiteral("/k/ed"), QStringLiteral("/k/unreadable")};
        QHash<QString, SshKeyFileStatus> files;
        files.insert(QStringLiteral("/k/ed"), keyFile(s_ed25519Fingerprint));
        files.insert(QStringLiteral("/k/rsa"), keyFile(s_rsaFingerprint, QStringLiteral("RSA")));
        files.insert(QStringLiteral("/k/gone"), SshKeyFileStatus{});
        files.insert(QStringLiteral("/k/unreadable"), SshKeyFileStatus{true, std::nullopt});
        const QList<SshAgentKey> agent{agentKey(s_ed25519), agentKey(s_ecdsa), agentKey(s_ed25519Certificate)};

        const QList<SshKeyEntry> entries = SshKeyList::build(config, added, files, agent);
        QCOMPARE(entries.size(), 6);

        using Source = SshKeyEntry::Source;
        using State = SshKeyEntry::State;

        QCOMPARE(entries.at(0).source, Source::Configuration);
        QCOMPARE(entries.at(0).path, QStringLiteral("/k/ed"));
        QCOMPARE(entries.at(0).state, State::Loaded);
        QCOMPARE(entries.at(0).type, QStringLiteral("ED25519"));
        // The key and its certificate are both removed with the file's row.
        QCOMPARE(entries.at(0).agentKeys, (QList<QByteArray>{s_ed25519, s_ed25519Certificate}));

        QCOMPARE(entries.at(1).state, State::Missing);
        QCOMPARE(entries.at(2).path, QStringLiteral("~/.ssh/%h"));
        QCOMPARE(entries.at(2).state, State::Missing);

        // /k/ed is listed once, as a configuration key.
        QCOMPARE(entries.at(3).source, Source::Added);
        QCOMPARE(entries.at(3).path, QStringLiteral("/k/rsa"));
        QCOMPARE(entries.at(3).state, State::NotLoaded);
        QVERIFY(entries.at(3).agentKeys.isEmpty());
        QCOMPARE(entries.at(4).path, QStringLiteral("/k/unreadable"));
        QCOMPARE(entries.at(4).state, State::Unknown);

        QCOMPARE(entries.at(5).source, Source::Agent);
        QCOMPARE(entries.at(5).state, State::Loaded);
        QCOMPARE(entries.at(5).name(), QStringLiteral("ecdsa@example.test"));
        QCOMPARE(entries.at(5).type, QStringLiteral("ECDSA"));
        QCOMPARE(entries.at(5).agentKeys, QList<QByteArray>{s_ecdsa});
    }

    void buildKeyListGroupsOtherCertificates()
    {
        const QList<SshAgentKey> agent{agentKey(s_ed25519Certificate), agentKey(s_ed25519), agentKey(s_rsa)};
        const QList<SshKeyEntry> entries = SshKeyList::build({}, {}, {}, agent);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).type, QStringLiteral("ED25519"));
        QCOMPARE(entries.at(0).agentKeys.size(), 2);
        // Without a comment, the fingerprint names the key.
        QCOMPARE(entries.at(1).name(), s_rsaFingerprint);
        QCOMPARE(entries.at(1).id(), QStringLiteral("agent:") + s_rsaFingerprint);
    }
};

QTEST_GUILESS_MAIN(TestSshAgent)
#include "tst_sshagent.moc"
