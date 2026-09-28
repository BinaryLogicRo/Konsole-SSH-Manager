## Terminal embedding

- **Plugin ID:** it's `kf6/parts/konsolepart` on KF6 and `kf5/parts/konsolepart` on KF5. Define it once in `compat.h`. If loading fails, retry with the bare ID `konsolepart` before giving up, since older or unusual installs may place it differently.
- **Loading:** use `KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(pluginId), parent)`. This API exists in both KF 5.103 and KF6, so no separate code paths are needed.
- **Starting ssh:** get a `TerminalInterface` via `qobject_cast`, then call `startProgram(helperPath, {QStringLiteral("konsole-ssh-session"), alias})`, where `helperPath` is the `konsole-ssh-session` helper in the app's own directory (`src/sshsessionmain.cpp`, logic in `src/sshsession.*`). The helper runs `ssh <alias>` as a direct child (no shell, no extra options, see [Security](09-security.md)) and waits for it. This is needed because the Konsole part offers no way to keep a finished terminal open or to read its text, so without the helper a failed connection closes the tab before its error can be read. If the location of the `TerminalInterface` header differs between KF5 and KF6, handle that include in `compat.h`.
- **Failed sessions:** if ssh exits with a non-zero status (or can't start, or is killed by a signal), the helper prints a short explanation below ssh's own output and waits for Enter before exiting, so the tab stays open until the user has read the error. On a normal logout (exit status 0) it exits at once. The helper validates the alias again and refuses anything else.
- **Retry or close:** a failed helper also tells the app, which then shows Retry and Close Tab buttons above that tab's terminal (inline, never a dialog). The app listens on a local socket that only the current user can reach and passes its path to helpers in an environment variable, which the helper removes before starting ssh. The app matches the connecting process to the tab by its process id (`SO_PEERCRED` against `terminalProcessId()`), since the Konsole part reports neither the exit status nor the terminal's text. Retry starts a new part in the same tab and replaces the old one only once the new one has loaded. Enter in the terminal still closes the tab.
- **Missing plugin:** if the plugin fails to load, show a clear error that says Konsole must be installed. Never crash.
- **Process exit:** the part destroys itself when the helper process exits. Connect to `QObject::destroyed` and remove the tab. Don't delete the part twice.
- **Instances:** use one part instance per tab. Don't share parts between tabs.

---

**Related specs:**

- [Version handling](04-version-handling.md)
- [Security](09-security.md)
- [Project overview](01-project-overview.md)
