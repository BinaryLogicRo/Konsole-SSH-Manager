#include "sshagentclient.h"
#include "logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <KLocalizedString>

#include <memory>

namespace
{
constexpr int QueryTimeoutMs = 10000;
// Removing never asks for anything; loading may wait for a passphrase, so it has no timeout.
constexpr int RemoveTimeoutMs = 10000;
constexpr int NoTimeout = -1;

QString findProgram(const QString &name)
{
    return QStandardPaths::findExecutable(name);
}

bool isUsableKeyPath(const QString &path)
{
    // Absolute paths start with '/', so they can never be read as an option.
    return QDir::isAbsolutePath(path) && !path.contains(QLatin1Char('\n'));
}
}

SshAgentClient::SshAgentClient(QObject *parent)
    : QObject(parent)
{
}

SshAgentClient::~SshAgentClient()
{
    // Stop running programs without reporting them to a client that is going away.
    const QList<QProcess *> processes = findChildren<QProcess *>();
    for (QProcess *process : processes) {
        disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->waitForFinished(1000);
    }
}

bool SshAgentClient::isBusy() const
{
    return m_busy;
}

void SshAgentClient::setAskpassProgram(const QString &program)
{
    m_askpassProgram = program;
}

void SshAgentClient::refresh(const QStringList &files)
{
    const quint64 generation = ++m_generation;

    struct Pending {
        Snapshot snapshot;
        int remaining = 0;
    };
    auto pending = std::make_shared<Pending>();
    const auto finishOne = [this, generation, pending] {
        if (--pending->remaining == 0 && generation == m_generation) {
            Q_EMIT refreshed(pending->snapshot);
        }
    };

    const QString sshAdd = findProgram(QStringLiteral("ssh-add"));
    const QString sshKeygen = findProgram(QStringLiteral("ssh-keygen"));
    QStringList toFingerprint;
    for (const QString &path : files) {
        SshKeyFileStatus status;
        status.exists = QFileInfo(path).isFile();
        pending->snapshot.files.insert(path, status);
        if (status.exists && isUsableKeyPath(path) && !sshKeygen.isEmpty()) {
            toFingerprint.append(path);
        }
    }

    // One extra count keeps the snapshot from being emitted before everything has started.
    pending->remaining = 1 + int(toFingerprint.size());

    for (const QString &path : std::as_const(toFingerprint)) {
        run(sshKeygen,
            {QStringLiteral("-l"), QStringLiteral("-E"), QStringLiteral("sha256"), QStringLiteral("-f"), path},
            QueryTimeoutMs,
            QProcessEnvironment::systemEnvironment(),
            [pending, path, finishOne](const SshCommandResult &result) {
                if (result.succeeded()) {
                    pending->snapshot.files[path].info = SshAgentFormat::parseFingerprint(result.standardOutput);
                }
                finishOne();
            });
    }

    Snapshot &snapshot = pending->snapshot;
    if (qEnvironmentVariableIsEmpty("SSH_AUTH_SOCK")) {
        snapshot.status = Status::NoAgent;
        finishOne();
        return;
    }
    if (sshAdd.isEmpty()) {
        snapshot.status = Status::NoTool;
        finishOne();
        return;
    }
    run(sshAdd, {QStringLiteral("-L")}, QueryTimeoutMs, QProcessEnvironment::systemEnvironment(), [pending, finishOne](const SshCommandResult &result) {
        Snapshot &snapshot = pending->snapshot;
        if (!result.started) {
            snapshot.status = Status::NoTool;
        } else if (result.timedOut) {
            snapshot.status = Status::NotResponding;
        } else if (result.exitCode == 0) {
            snapshot.status = Status::Available;
            snapshot.keys = SshAgentFormat::parsePublicKeys(result.standardOutput);
        } else if (result.exitCode == 1 && result.standardError.trimmed().isEmpty()) {
            snapshot.status = Status::Available; // "The agent has no identities."
        } else {
            snapshot.status = result.exitCode == 2 ? Status::Unreachable : Status::Failed;
            snapshot.message = SshAgentFormat::message(result.standardError);
        }
        qCDebug(KSSHM_AGENT) << "ssh-add -L exited with" << result.exitCode << "and listed" << snapshot.keys.size() << "keys";
        finishOne();
    });
}

bool SshAgentClient::addKey(const QString &path)
{
    const QString sshAdd = findProgram(QStringLiteral("ssh-add"));
    if (m_busy || sshAdd.isEmpty() || !isUsableKeyPath(path)) {
        return false;
    }

    // ssh-add asks for a passphrase through askpass only, never on a terminal
    // the app may have been started from.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    if (!m_askpassProgram.isEmpty()) {
        environment.insert(QStringLiteral("SSH_ASKPASS"), m_askpassProgram);
        environment.insert(QString::fromLatin1(AskpassModeVariable), QStringLiteral("1"));
    }

    m_busy = true;
    run(sshAdd, {path}, NoTimeout, environment, [this](const SshCommandResult &result) {
        finishAction(result, i18n("The key was not loaded."));
    });
    return true;
}

bool SshAgentClient::removeKeys(const QList<QByteArray> &publicKeyLines)
{
    const QString sshAdd = findProgram(QStringLiteral("ssh-add"));
    if (m_busy || sshAdd.isEmpty() || publicKeyLines.isEmpty()) {
        return false;
    }

    // ssh-add -d takes public key files; these only hold public keys and are
    // removed as soon as ssh-add is done.
    auto directory = std::make_shared<QTemporaryDir>();
    if (!directory->isValid()) {
        return false;
    }
    QStringList arguments{QStringLiteral("-d")};
    for (qsizetype i = 0; i < publicKeyLines.size(); ++i) {
        QFile file(directory->filePath(QStringLiteral("key%1.pub").arg(i)));
        if (!file.open(QIODevice::WriteOnly) || file.write(publicKeyLines.at(i) + '\n') < 0) {
            return false;
        }
        arguments.append(file.fileName());
    }

    m_busy = true;
    run(sshAdd, arguments, RemoveTimeoutMs, QProcessEnvironment::systemEnvironment(), [this, directory](const SshCommandResult &result) {
        finishAction(result, i18n("The key was not removed."));
    });
    return true;
}

void SshAgentClient::finishAction(const SshCommandResult &result, const QString &fallbackMessage)
{
    m_busy = false;
    qCDebug(KSSHM_AGENT) << "ssh-add exited with" << result.exitCode;
    if (result.succeeded()) {
        Q_EMIT actionFinished(true, QString());
        return;
    }
    QString message;
    if (!result.started) {
        message = i18n("ssh-add could not be started.");
    } else if (result.timedOut) {
        message = i18n("The SSH agent doesn't respond.");
    } else {
        message = SshAgentFormat::message(result.standardError);
    }
    if (message.isEmpty()) {
        message = fallbackMessage;
    }
    Q_EMIT actionFinished(false, message);
}

void SshAgentClient::run(const QString &program, const QStringList &arguments, int timeoutMs, const QProcessEnvironment &environment, const Callback &done)
{
    auto *process = new QProcess(this);
    process->setProcessEnvironment(environment);
    process->setStandardInputFile(QProcess::nullDevice());

    auto timedOut = std::make_shared<bool>(false);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [process, timedOut, done](int exitCode, QProcess::ExitStatus status) {
        SshCommandResult result;
        result.started = true;
        result.timedOut = *timedOut;
        result.exitCode = status == QProcess::NormalExit ? exitCode : -1;
        result.standardOutput = process->readAllStandardOutput();
        result.standardError = process->readAllStandardError();
        process->deleteLater();
        done(result);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            done(SshCommandResult{});
        }
    });
    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, process, [process, timedOut] {
            *timedOut = true;
            process->kill();
        });
    }
    process->start(program, arguments);
}
