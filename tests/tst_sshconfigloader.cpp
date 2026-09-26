#include "sshconfig/hoststore.h"
#include "sshconfig/sshconfigloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace
{
void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

int mode(const QString &path)
{
    return int(QFileInfo(path).permissions()) & 0x7077;
}

SshHost makeHost(const QString &alias, const QString &hostName)
{
    SshHost host;
    host.patterns = QStringList{alias};
    host.options = {{QStringLiteral("HostName"), hostName}};
    host.readOnly = false;
    return host;
}
}

// Every test uses a fake home inside a QTemporaryDir; the real ~/.ssh is never touched.
class TestSshConfigLoader : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_home;
    SshPaths m_paths;

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_home.isValid());
        QDir(m_home.path()).removeRecursively();
        QDir().mkpath(m_home.path());
        m_paths = SshPaths::forHome(m_home.path(), m_home.filePath(QStringLiteral("backups")));
    }

    void pathsLayout()
    {
        QCOMPARE(m_paths.managedFile, m_home.path() + QStringLiteral("/.ssh/config.d/konsole-ssh-manager.conf"));
        QCOMPARE(m_paths.userConfig, m_home.path() + QStringLiteral("/.ssh/config"));
        QVERIFY(!m_paths.backupDir.startsWith(m_paths.managedDir));
    }

    void emptyHome()
    {
        const SshConfigLoader::Result result = SshConfigLoader(m_paths).load();
        QVERIFY(result.files.isEmpty());
        QVERIFY(!result.managedFileIncluded);
        QVERIFY(result.errors.isEmpty());
    }

    void expandIncludePath()
    {
        const SshConfigLoader loader(m_paths);
        QCOMPARE(loader.expandIncludePath(QStringLiteral("config.d/*")), m_paths.sshDir + QStringLiteral("/config.d/*"));
        QCOMPARE(loader.expandIncludePath(QStringLiteral("~/x.conf")), m_home.path() + QStringLiteral("/x.conf"));
        QCOMPARE(loader.expandIncludePath(QStringLiteral("/etc/ssh/x")), QStringLiteral("/etc/ssh/x"));
    }

    void includesResolveInGlobOrder()
    {
        writeFile(m_paths.userConfig, "Include config.d/*\nInclude ~/other.conf\nHost main\n");
        writeFile(m_paths.managedDir + QStringLiteral("/b.conf"), "Host b\n");
        writeFile(m_paths.managedDir + QStringLiteral("/a.conf"), "Host a\n");
        writeFile(m_paths.managedFile, "Host managed\n");
        writeFile(m_home.filePath(QStringLiteral("other.conf")), "Host other\n");

        const SshConfigLoader::Result result = SshConfigLoader(m_paths).load();
        QStringList names;
        for (const SshConfigFile &file : result.files) {
            names << QFileInfo(file.path).fileName();
            QCOMPARE(file.managed, file.path == m_paths.managedFile);
        }
        QCOMPARE(names,
                 (QStringList{QStringLiteral("config"),
                              QStringLiteral("a.conf"),
                              QStringLiteral("b.conf"),
                              QStringLiteral("konsole-ssh-manager.conf"),
                              QStringLiteral("other.conf")}));
        QVERIFY(result.managedFileIncluded);
    }

    void managedFileShownWhenNotIncluded()
    {
        writeFile(m_paths.userConfig, "Host main\n");
        writeFile(m_paths.managedFile, "Host managed\n");
        const SshConfigLoader::Result result = SshConfigLoader(m_paths).load();
        QCOMPARE(result.files.size(), 2);
        QVERIFY(result.files.at(1).managed);
        QVERIFY(!result.managedFileIncluded);
    }

    void includeCycleIsLoadedOnce()
    {
        writeFile(m_paths.userConfig, "Include config\nInclude loop.conf\nHost main\n");
        writeFile(m_paths.sshDir + QStringLiteral("/loop.conf"), "Include config\nInclude loop.conf\n");
        const SshConfigLoader::Result result = SshConfigLoader(m_paths).load();
        QCOMPARE(result.files.size(), 2);
    }

    void storeAddCreatesPrivateManagedFileOnly()
    {
        HostStore store(m_paths);
        const HostStore::Result result = store.addHost(makeHost(QStringLiteral("prod"), QStringLiteral("10.0.0.1")));
        QVERIFY2(result.ok(), qPrintable(result.detail));

        QCOMPARE(readFile(m_paths.managedFile), QByteArray("Host prod\n    HostName 10.0.0.1\n"));
        QCOMPARE(mode(m_paths.managedFile), int(SshConfigWriter::FilePermissions));
        QCOMPARE(mode(m_paths.managedDir), int(SshConfigWriter::DirectoryPermissions));
        QVERIFY(!QFileInfo::exists(m_paths.userConfig)); // never created without confirmation
        QVERIFY(!store.isManagedFileIncluded());
        QCOMPARE(store.hosts().size(), 1);
        QVERIFY(!store.hosts().first().readOnly);
    }

    void storeEditsNeverTouchUserConfig()
    {
        const QByteArray userConfig = "# mine\nHost mine\n    HostName mine.example.com\n";
        writeFile(m_paths.userConfig, userConfig);
        HostStore store(m_paths);
        QVERIFY(store.addHost(makeHost(QStringLiteral("a"), QStringLiteral("1"))).ok());
        QVERIFY(store.updateHost(QStringLiteral("a"), makeHost(QStringLiteral("a2"), QStringLiteral("2"))).ok());
        QVERIFY(store.removeHost(QStringLiteral("a2")).ok());
        QCOMPARE(readFile(m_paths.userConfig), userConfig);

        // Hosts from the user config are read-only.
        QCOMPARE(store.hosts().size(), 1);
        QVERIFY(store.hosts().first().readOnly);
        QVERIFY(store.isAliasDefinedOutsideManagedFile(QStringLiteral("MINE")));
    }

    void storeRejectsDuplicatesAndMissingHosts()
    {
        HostStore store(m_paths);
        QVERIFY(store.addHost(makeHost(QStringLiteral("a"), QStringLiteral("1"))).ok());
        QVERIFY(store.addHost(makeHost(QStringLiteral("b"), QStringLiteral("2"))).ok());
        QCOMPARE(store.addHost(makeHost(QStringLiteral("A"), QStringLiteral("3"))).error, HostStore::Error::AliasExists);
        QCOMPARE(store.updateHost(QStringLiteral("a"), makeHost(QStringLiteral("b"), QStringLiteral("3"))).error, HostStore::Error::AliasExists);
        QCOMPARE(store.updateHost(QStringLiteral("zzz"), makeHost(QStringLiteral("zzz"), QStringLiteral("3"))).error, HostStore::Error::HostNotFound);
        QCOMPARE(store.removeHost(QStringLiteral("zzz")).error, HostStore::Error::HostNotFound);
        QCOMPARE(store.addHost(makeHost(QStringLiteral("-bad"), QStringLiteral("3"))).error, HostStore::Error::InvalidHost);
        // Changing only the case of the alias is not a collision with itself.
        QVERIFY(store.updateHost(QStringLiteral("a"), makeHost(QStringLiteral("A"), QStringLiteral("1"))).ok());
    }

    void storeEditsStartFromDiskContent()
    {
        HostStore store(m_paths);
        QVERIFY(store.addHost(makeHost(QStringLiteral("a"), QStringLiteral("1"))).ok());
        // Another program appends a host; the store's next edit must keep it.
        writeFile(m_paths.managedFile, readFile(m_paths.managedFile) + "\n# external\nHost external\n    User x\n");
        QVERIFY(store.addHost(makeHost(QStringLiteral("b"), QStringLiteral("2"))).ok());
        QVERIFY(readFile(m_paths.managedFile).contains("# external\nHost external\n    User x\n"));
        QVERIFY(store.readManagedHost(QStringLiteral("external")).has_value());
    }

    void addIncludeLineOnceWithBackup()
    {
        const QByteArray original = "Host mine\n    User me\n";
        writeFile(m_paths.userConfig, original);
        QFile::setPermissions(m_paths.userConfig, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);

        HostStore store(m_paths);
        QVERIFY(!store.isManagedFileIncluded());
        QVERIFY(store.addIncludeLine().ok());
        QCOMPARE(readFile(m_paths.userConfig), QByteArray("Include config.d/*\n") + original);
        QCOMPARE(mode(m_paths.userConfig), int(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup));
        QVERIFY(store.isManagedFileIncluded());

        const QStringList backups = QDir(m_paths.backupDir).entryList(QDir::Files);
        QCOMPARE(backups.size(), 1);
        QCOMPARE(readFile(QDir(m_paths.backupDir).filePath(backups.first())), original);

        // Already included: nothing changes.
        QVERIFY(store.addIncludeLine().ok());
        QCOMPARE(readFile(m_paths.userConfig), QByteArray("Include config.d/*\n") + original);
    }

    void addIncludeLineCreatesMissingConfig()
    {
        HostStore store(m_paths);
        QVERIFY(store.addIncludeLine().ok());
        QCOMPARE(readFile(m_paths.userConfig), QByteArray("Include config.d/*\n"));
        QCOMPARE(mode(m_paths.userConfig), int(SshConfigWriter::FilePermissions));
        QCOMPARE(mode(m_paths.sshDir), int(SshConfigWriter::DirectoryPermissions));
    }

    void externalChangesTriggerReload()
    {
        writeFile(m_paths.userConfig, "Host one\n");
        HostStore store(m_paths);
        QCOMPARE(store.hosts().size(), 1);
        QSignalSpy spy(&store, &HostStore::hostsChanged);
        writeFile(m_paths.userConfig, "Host one\nHost two\n");
        QTRY_COMPARE_WITH_TIMEOUT(store.hosts().size(), 2, 5000);
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_GUILESS_MAIN(TestSshConfigLoader)

#include "tst_sshconfigloader.moc"
