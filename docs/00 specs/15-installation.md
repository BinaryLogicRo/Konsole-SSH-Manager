## Installation

The app is not installed system-wide and there is no `make install` step: it always runs from the build directory (see [Packaging](11-packaging.md)). "Installing" means adding an entry to the current user's application menu that starts the built binary.

### 1. Build

```bash
make deps     # build dependencies for your Debian release (sudo apt)
make build
```

Keep the project where it is. The menu entry points at the absolute path of `build/bin/konsole-ssh-manager`, and the app starts the `konsole-ssh-session` helper from the same directory.

### 2. Add the menu entry

Every build generates `build/ro.binarylogic.konsole-ssh-manager.desktop` from the template `data/ro.binarylogic.konsole-ssh-manager.desktop.in`, with the absolute path of the binary filled in. Copy it to your user's applications folder:

```bash
mkdir -p ~/.local/share/applications
cp build/ro.binarylogic.konsole-ssh-manager.desktop ~/.local/share/applications/
```

"Konsole SSH Manager" then appears in the application launcher (category Internet, or search for "ssh"). Plasma usually picks up new entries within a few seconds; if it doesn't, refresh the menu cache:

```bash
kbuildsycoca5   # Debian 12 (Plasma 5)
kbuildsycoca6   # Debian 13 (Plasma 6)
```

Keep the file name unchanged. The app identifies its windows as `ro.binarylogic.konsole-ssh-manager`, which is how the taskbar (especially on Wayland) matches a window to this entry's name and icon.

To check the file, run `desktop-file-validate build/ro.binarylogic.konsole-ssh-manager.desktop` (package `desktop-file-utils`).

### Updating

- After pulling changes: run `make build`. The menu entry already points at the rebuilt binary.
- After moving the project or building in another directory (`BUILD_DIR=...`): build, then copy the newly generated `.desktop` file again, because the path inside it is absolute.

### Uninstalling

```bash
rm ~/.local/share/applications/ro.binarylogic.konsole-ssh-manager.desktop
```

Optionally delete the app's own data:

- `~/.config/konsole-ssh-manager/`: window layout and View menu settings.
- `~/.local/share/konsole-ssh-manager/backups/`: backups of SSH configuration files, made before the app changed them.

Your SSH setup is left as it is. Hosts created with the app stay in `~/.ssh/config.d/konsole-ssh-manager.conf`, and `~/.ssh/config` keeps its `Include config.d/*` line if you added it, so `ssh` keeps using those hosts. Remove them by hand only if you no longer want them (see [SSH config rules](06-ssh-config-rules.md)).

### AI agents

AI agents must not perform these steps themselves: they write outside the project (see `AGENTS.md`). They may change the template and this document. If the desktop file ID changes, it must change in three places together: the template's file name, `KSSHM_DESKTOP_ID` in the top-level `CMakeLists.txt`, and `setDesktopFileName()` in `src/main.cpp`.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [Packaging](11-packaging.md)
- [SSH config rules (critical)](06-ssh-config-rules.md)
