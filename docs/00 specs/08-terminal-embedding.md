## Terminal embedding

- **Plugin ID:** it's `kf6/parts/konsolepart` on KF6 and `kf5/parts/konsolepart` on KF5. Define it once in `compat.h`. If loading fails, retry with the bare ID `konsolepart` before giving up, since older or unusual installs may place it differently.
- **Loading:** use `KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(pluginId), parent)`. This API exists in both KF 5.103 and KF6, so no separate code paths are needed.
- **Starting ssh:** get a `TerminalInterface` via `qobject_cast`, then call `startProgram(QStringLiteral("ssh"), {QStringLiteral("ssh"), alias})`. If the location of the `TerminalInterface` header differs between KF5 and KF6, handle that include in `compat.h`.
- **Missing plugin:** if the plugin fails to load, show a clear error that says Konsole must be installed. Never crash.
- **Process exit:** the part destroys itself when the ssh process exits. Connect to `QObject::destroyed` and remove the tab. Don't delete the part twice.
- **Instances:** use one part instance per tab. Don't share parts between tabs.

---

**Related specs:**

- [Version handling](04-version-handling.md)
- [Security](09-security.md)
- [Project overview](01-project-overview.md)
