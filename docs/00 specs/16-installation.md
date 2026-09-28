## Installation

The app is installed for the current user only; no root access is needed and nothing outside the user's home is touched. There are no `.deb` packages (see [Packaging](12-packaging.md)). During development it can still be run straight from the build directory with `make run` (see [Setup and build](04-setup-and-build.md)).

### What gets installed

`make install` builds the app, then copies:

| File | Installed to |
|---|---|
| `konsole-ssh-manager` (the app) | `~/.local/bin/` |
| `konsole-ssh-session` (the helper each terminal tab runs) | `~/.local/bin/` |
| `ro.binarylogic.konsole-ssh-manager.desktop` (application menu entry) | `~/.local/share/applications/` |
| `ro.binarylogic.konsole-ssh-manager.svg` (app icon, from `data/icons/`) | `~/.local/share/icons/hicolor/scalable/apps/` |

Both executables must stay in the same directory: the app starts the helper from its own directory. The menu entry is generated at build time from the template `data/ro.binarylogic.konsole-ssh-manager.desktop.in`, with the absolute path of the installed app filled in, so it doesn't depend on the build directory.

The icon is used by the menu entry and for the app's windows. The app loads it from the icon theme, where `make install` puts it, and falls back to a copy compiled into the binary (`src/resources.qrc`), so the window icon also shows when running from the build directory.

### Install

```bash
make deps      # build dependencies for your Debian release (sudo apt)
make install
```

`make install` also refreshes Plasma's menu cache (`kbuildsycoca5` on Debian 12, `kbuildsycoca6` on Debian 13) and tells running KDE programs to reload their icons (the `iconChanged` D-Bus signal on `/KIconLoader`, the same one Plasma sends when you change the icon theme). Without that, Plasma and KWin, which only scan icon folders when they start, would show a blank icon until the next login if the install created a new icon folder. "Konsole SSH Manager" then appears in the application launcher (category Internet, or search for "ssh"). To start it from a terminal as `konsole-ssh-manager`, `~/.local/bin` must be in `PATH`; Debian's default `~/.profile` adds it at login when the directory exists.

Keep the menu entry's file name unchanged. The app identifies its windows as `ro.binarylogic.konsole-ssh-manager`, which is how the taskbar (especially on Wayland) matches a window to this entry's name and icon.

If the menu or taskbar still shows a blank icon, log out and back in.

### Updating

Pull the changes and run `make install` again. The installed files are replaced; settings, backups and SSH configuration are kept.

### Uninstalling

```bash
make uninstall
```

This removes the two executables, the menu entry and the icon, and refreshes the menu cache. It keeps:

- `~/.config/konsole-ssh-manager/`: window layout and View menu settings.
- `~/.local/share/konsole-ssh-manager/backups/`: backups of SSH configuration files, made before the app changed them.
- Your SSH setup: hosts created with the app stay in `~/.ssh/config.d/konsole-ssh-manager.conf`, and `~/.ssh/config` keeps its `Include config.d/*` line if you added it, so `ssh` keeps using those hosts.

Delete these by hand only if you no longer want them (see [SSH config rules](07-ssh-config-rules.md)).

### Options

- `PREFIX` (default `~/.local`) changes where `make install` and `make uninstall` put the files, e.g. `make install PREFIX=~/apps`. Use the same `PREFIX` for both. The install is meant to be per-user; don't run it with `sudo`.
- `REFRESH_MENU=0` skips refreshing the menu cache and the icon reload.
- `BUILD_DIR` and `QT_MAJOR_VERSION` work as for `make build`.

### AI agents

AI agents must not run `make install`, `make uninstall`, the menu cache tools, or the icon reload signal against the user's real home directory or session: they write outside the project (see `AGENTS.md`). To check the install rules, install into a directory inside the project's ignored build area, e.g. `make install BUILD_DIR=build-installtest PREFIX=$PWD/build-installtest/prefix REFRESH_MENU=0`, then delete it.

If the desktop file ID changes, it must change in all of these together: the template's file name and its `Icon=` line, the icon's file name in `data/icons/` and in `src/resources.qrc`, `KSSHM_DESKTOP_ID` in the top-level `CMakeLists.txt`, `DESKTOP_ID` in the `Makefile`, and `setDesktopFileName()` and the themed icon name in `src/main.cpp`.

---

**Related specs:**

- [Setup and build](04-setup-and-build.md)
- [Packaging](12-packaging.md)
- [SSH config rules (critical)](07-ssh-config-rules.md)
