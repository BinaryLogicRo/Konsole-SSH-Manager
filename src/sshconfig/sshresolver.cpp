#include "sshresolver.h"
#include "logging.h"
#include "sshconfigwriter.h"

#include <QStandardPaths>
#include <QTemporaryFile>

QString SshResolver::sshProgram()
{
    return QStandardPaths::findExecutable(QStringLiteral("ssh"));
}

SshCommandResult SshResolver::validateHost(const SshHost &host, int timeoutMs)
{
    SshCommandResult result;
    const QString program = sshProgram();
    const QString alias = host.alias();
    if (program.isEmpty() || !SshValidation::isValidAlias(alias)) {
        return result;
    }

    SshHost bare = host;
    bare.metadata.clear();
    SshConfigDocument document;
    if (!SshConfigWriter::addHost(document, bare)) {
        return result;
    }

    QTemporaryFile file; // created 0600
    if (!file.open() || file.write(document.serialize()) < 0 || !file.flush()) {
        return result;
    }

    QProcess process;
    process.start(program, {QStringLiteral("-G"), QStringLiteral("-F"), file.fileName(), alias});
    result.started = process.waitForStarted(timeoutMs);
    if (!result.started) {
        return result;
    }
    if (!process.waitForFinished(timeoutMs)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(1000);
        return result;
    }
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    const QByteArray tempPath = file.fileName().toUtf8();
    result.standardError.replace(tempPath + ": ", QByteArray()).replace(tempPath + ' ', QByteArray()).replace(tempPath, QByteArray());
    qCDebug(KSSHM_CONFIG) << "ssh -G validation for" << alias << "exited with" << result.exitCode;
    return result;
}

SshEffectiveConfigJob::SshEffectiveConfigJob(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus status) {
        SshCommandResult result;
        result.started = true;
        result.exitCode = status == QProcess::NormalExit ? exitCode : -1;
        result.standardOutput = m_process.readAllStandardOutput();
        result.standardError = m_process.readAllStandardError();
        qCDebug(KSSHM_CONFIG) << "ssh -G exited with" << result.exitCode;
        Q_EMIT finished(result);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            Q_EMIT finished(SshCommandResult{});
        }
    });
}

bool SshEffectiveConfigJob::start(const QString &alias)
{
    const QString program = SshResolver::sshProgram();
    if (program.isEmpty() || !SshValidation::isValidAlias(alias) || m_process.state() != QProcess::NotRunning) {
        return false;
    }
    m_process.start(program, {QStringLiteral("-G"), alias});
    return true;
}
