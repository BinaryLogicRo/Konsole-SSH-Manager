#include "terminaltab.h"
#include "compat.h"
#include "logging.h"
#include "sshsession.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>

TerminalTab *TerminalTab::create(const QString &alias, QWidget *parent, QString *errorMessage)
{
    auto *tab = new TerminalTab(alias, parent);
    if (!tab->startSession(errorMessage)) {
        delete tab;
        return nullptr;
    }
    return tab;
}

TerminalTab::TerminalTab(const QString &alias, QWidget *parent)
    : QWidget(parent)
    , m_alias(alias)
    , m_failureBar(new QWidget(this))
    , m_failureLabel(new QLabel(m_failureBar))
{
    auto *failureIcon = new QLabel(m_failureBar);
    failureIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(22, 22));
    m_failureLabel->setWordWrap(true);
    auto *retryButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "&Retry"), m_failureBar);
    auto *closeButton = new QPushButton(QIcon::fromTheme(QStringLiteral("tab-close")), i18nc("@action:button", "&Close Tab"), m_failureBar);
    auto *barLayout = new QHBoxLayout(m_failureBar);
    barLayout->addWidget(failureIcon);
    barLayout->addWidget(m_failureLabel, 1);
    barLayout->addWidget(retryButton);
    barLayout->addWidget(closeButton);
    m_failureBar->hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_failureBar);

    connect(retryButton, &QPushButton::clicked, this, &TerminalTab::retry);
    connect(closeButton, &QPushButton::clicked, this, [this] {
        Q_EMIT closeRequested(this);
    });
}

TerminalTab::~TerminalTab()
{
    // If ssh already exited the part is gone (QPointer is null); never delete it twice.
    if (m_part) {
        disconnect(m_part, nullptr, this, nullptr);
        delete m_part;
    }
}

QString TerminalTab::alias() const
{
    return m_alias;
}

qint64 TerminalTab::sessionProcessId() const
{
    auto *terminal = qobject_cast<TerminalInterface *>(m_part.data());
    return terminal ? terminal->terminalProcessId() : 0;
}

void TerminalTab::focusTerminal()
{
    if (m_part && m_part->widget()) {
        m_part->widget()->setFocus();
    }
}

void TerminalTab::showFailure()
{
    m_failureLabel->setText(i18n("The session to %1 ended with an error.", m_alias));
    m_failureBar->show();
}

void TerminalTab::retry()
{
    QString error;
    if (!startSession(&error)) {
        m_failureLabel->setText(error);
        return;
    }
    m_failureBar->hide();
    focusTerminal();
}

bool TerminalTab::startSession(QString *errorMessage)
{
    const QString helper = QCoreApplication::applicationDirPath() + QLatin1Char('/') + SshSession::helperName();
    if (!QFileInfo(helper).isExecutable()) {
        *errorMessage = i18n("The session helper %1 is missing. It is built together with the app; please rebuild.", helper);
        return false;
    }

    KParts::ReadOnlyPart *part = nullptr;
    QString lastError;
    const QStringList pluginIds{Compat::konsolePartPluginId(), Compat::konsolePartFallbackPluginId()};
    for (const QString &pluginId : pluginIds) {
        const auto result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(pluginId), this);
        if (result.plugin) {
            part = result.plugin;
            qCDebug(KSSHM_TERMINAL) << "Loaded Konsole part" << pluginId;
            break;
        }
        lastError = result.errorString;
        qCDebug(KSSHM_TERMINAL) << "Could not load" << pluginId << result.errorString;
    }

    if (!part) {
        *errorMessage = i18n("The Konsole terminal component could not be loaded. Please make sure Konsole is installed (Debian package \"konsole\").");
        if (!lastError.isEmpty()) {
            *errorMessage += QLatin1Char('\n') + i18n("Details: %1", lastError);
        }
        return false;
    }

    auto *terminal = qobject_cast<TerminalInterface *>(part);
    if (!terminal || !part->widget()) {
        *errorMessage = i18n("The installed Konsole component does not provide a terminal. Please make sure Konsole is installed correctly.");
        delete part;
        return false;
    }

    if (m_part) {
        disconnect(m_part, nullptr, this, nullptr);
        delete m_part;
    }
    m_part = part;
    layout()->addWidget(m_part->widget());
    setFocusProxy(m_part->widget());
    connect(m_part, &QObject::destroyed, this, &TerminalTab::onPartDestroyed);

    // The helper runs `ssh <alias>` (no shell, no extra options) and keeps the
    // terminal open on failure so the user can read ssh's error.
    terminal->startProgram(helper, {SshSession::helperName(), m_alias});
    qCDebug(KSSHM_TERMINAL) << "Started ssh session for" << m_alias;
    return true;
}

void TerminalTab::onPartDestroyed()
{
    qCDebug(KSSHM_TERMINAL) << "Session ended for" << m_alias;
    Q_EMIT sessionFinished(this);
}
