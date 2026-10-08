#pragma once

#include <QLoggingCategory>

// Categories are off below warning level by default. Enable with e.g.
// QT_LOGGING_RULES="konsole-ssh-manager.*.debug=true". Never log full config
// contents or `ssh -G` output, even at debug level.
Q_DECLARE_LOGGING_CATEGORY(KSSHM_CONFIG)
Q_DECLARE_LOGGING_CATEGORY(KSSHM_TERMINAL)
Q_DECLARE_LOGGING_CATEGORY(KSSHM_AGENT)
