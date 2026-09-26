#include "terminaltab.h"
#include "compat.h"
#include "logging.h"

#include <QVBoxLayout>

#include <KLocalizedString>
#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>

TerminalTab *TerminalTab::create(const QString &alias, QWidget *parent, QString *errorMessage)
{
    auto *tab = new TerminalTab(alias, parent);
    if (!tab->loadPart(errorMessage)) {
        delete tab;
        return nullptr;
    }
    return tab;
}

TerminalTab::TerminalTab(const QString &alias, QWidget *parent)
    : QWidget(parent)
    , m_alias(alias)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
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

void TerminalTab::focusTerminal()
{
    if (m_part && m_part->widget()) {
        m_part->widget()->setFocus();
    }
}

bool TerminalTab::loadPart(QString *errorMessage)
{
    QString lastError;
    const QStringList pluginIds{Compat::konsolePartPluginId(), Compat::konsolePartFallbackPluginId()};
    for (const QString &pluginId : pluginIds) {
        const auto result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(pluginId), this);
        if (result.plugin) {
            m_part = result.plugin;
            qCDebug(KSSHM_TERMINAL) << "Loaded Konsole part" << pluginId;
            break;
        }
        lastError = result.errorString;
        qCDebug(KSSHM_TERMINAL) << "Could not load" << pluginId << result.errorString;
    }

    if (!m_part) {
        *errorMessage = i18n("The Konsole terminal component could not be loaded. Please make sure Konsole is installed (Debian package \"konsole\").");
        if (!lastError.isEmpty()) {
            *errorMessage += QLatin1Char('\n') + i18n("Details: %1", lastError);
        }
        return false;
    }

    auto *terminal = qobject_cast<TerminalInterface *>(m_part.data());
    if (!terminal || !m_part->widget()) {
        *errorMessage = i18n("The installed Konsole component does not provide a terminal. Please make sure Konsole is installed correctly.");
        delete m_part;
        return false;
    }

    layout()->addWidget(m_part->widget());
    setFocusProxy(m_part->widget());
    connect(m_part, &QObject::destroyed, this, &TerminalTab::onPartDestroyed);

    // The alias is the only argument: no shell, no extra options.
    terminal->startProgram(QStringLiteral("ssh"), {QStringLiteral("ssh"), m_alias});
    qCDebug(KSSHM_TERMINAL) << "Started ssh session for" << m_alias;
    return true;
}

void TerminalTab::onPartDestroyed()
{
    qCDebug(KSSHM_TERMINAL) << "Session ended for" << m_alias;
    Q_EMIT sessionFinished(this);
}
