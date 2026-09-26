# AGENTS.md

Guidance for AI coding agents working on **konsole-ssh-manager**.

## Project overview

A KDE Plasma desktop app that manages SSH host profiles and opens connections as embedded Konsole terminals. The window has a sidebar (host tree with groups) and a tabbed area where each tab is an embedded `konsolepart` running `ssh <alias>`.

The app reads the user's `~/.ssh/config` and owns a separate managed file that it creates, edits, and deletes hosts in. It never rewrites the user's handwritten config.

## Supported platforms

The same source tree must build and run on both targets:

| Target | Desktop | Qt | KDE Frameworks | Konsole | OpenSSH |
|---|---|---|---|---|---|
| Debian 12 (bookworm) | Plasma 5.27 | Qt 5.15 | KF5 (5.103) | 22.12 | 9.2 |
| Debian 13 (trixie) | Plasma 6 | Qt 6 | KF6 | 24.x+ | 10.x |

**The Debian 12 versions are the floor.** Any API, compiler feature, or OpenSSH keyword must work there, or be guarded behind a version check.

Linux only. No Windows or macOS code paths.

## Tech stack

- C++20, limited to what GCC 12 (Debian 12) supports. In particular, **`std::format` is not available**; use `QString::arg()` instead.
- CMake ≥ 3.24, Extra CMake Modules (ECM)
- Qt 5.15 or Qt 6 (Widgets, Test)
- KDE Frameworks 5 or 6: KParts, KCoreAddons, KI18n
- Runtime dependency: `konsole` (provides the `konsolepart` plugin)

## Setup and build

### Debian 12

```bash
sudo apt install build-essential cmake extra-cmake-modules qtbase5-dev \
  libkf5parts-dev libkf5coreaddons-dev libkf5i18n-dev konsole clang-format

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=5
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/bin/konsole-ssh-manager
```

### Debian 13

```bash
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev \
  libkf6parts-dev libkf6coreaddons-dev libkf6i18n-dev konsole clang-format

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=6
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/bin/konsole-ssh-manager
```

If `QT_MAJOR_VERSION` is omitted, CMake auto-detects: it prefers Qt 6 when available and falls back to Qt 5.

Format code before committing:

```bash
cmake --build build --target clang-format
```

### Testing both targets from one machine

Use containers (`podman`/`docker` with `debian:12` and `debian:13` images, or `distrobox`) to build and run the tests on the other release. A change is not done until it builds and passes tests on **both**.

## Version handling

### CMake

- `QT_MAJOR_VERSION` is `5` or `6`. `KF_MAJOR_VERSION` is always set equal to it, since Qt 5 pairs with KF5 and Qt 6 pairs with KF6.
- Always reference targets through the variables: `Qt${QT_MAJOR_VERSION}::Widgets`, `KF${KF_MAJOR_VERSION}::Parts`, and so on. Never hard-code `Qt6::` or `KF6::`.
- Don't create separate CMakeLists for each version.

### C++

- **All version-specific code lives in `src/compat.h`** (and `compat.cpp` if needed). Everywhere else, code must be version-neutral.
- Guard with `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`. Don't use KF version macros for things that are really Qt differences.

Common pitfalls to avoid outside `compat.h`:

- **Removed in Qt 6:** `QRegExp` (use `QRegularExpression`), `QTextCodec`, `QString::splitRef`, `QStringRef` (use `QStringView`).
- **Deprecated forms:** `QString::SkipEmptyParts` (use `Qt::SkipEmptyParts`) and `QLinkedList`.
- **Container sizes:** they are `int` in Qt 5 and `qsizetype` in Qt 6. Use `auto` or `qsizetype`, and never narrow with a C-style cast.
- **High DPI:** Qt 5 needs `Qt::AA_EnableHighDpiScaling` set before `QApplication` is constructed; Qt 6 enables it by default. Keep this in `main.cpp` behind a guard.
- **Qt 6.x-only APIs:** don't use them without a guard, even if they're convenient.

## Repository layout

```
src/
  main.cpp               App entry, KAboutData, KLocalizedString setup
  compat.h               All Qt5/Qt6 and KF5/KF6 differences
  mainwindow.*           Splitter: sidebar + QTabWidget, sidebar side is configurable
  terminaltab.*          Wraps one konsolepart instance and its ssh session
  sshconfig/
    sshconfigparser.*    Lossless parser for ssh_config files
    sshconfigwriter.*    Serializes the managed file, atomic writes
    sshhost.*            Host data model (alias + ordered options + metadata)
  models/
    hosttreemodel.*      QAbstractItemModel for the sidebar (groups → hosts)
  dialogs/
    hosteditdialog.*     Add/edit host form
tests/
  data/                  Sample ssh config fixtures
  tst_sshconfigparser.cpp
  tst_sshconfigwriter.cpp
```

This layout is the target structure. Keep UI code out of `sshconfig/`: that layer must be testable without a display.

## SSH config rules (critical)

These rules protect the user's SSH setup. Treat any violation as a bug.

1. **Never write to `~/.ssh/config`** except to insert one line, `Include config.d/*`, and only after explicit user confirmation. This line must go before the first `Host` or `Match` block, because an `Include` inside a block is scoped to that block.
2. The app owns exactly one file: `~/.ssh/config.d/konsole-ssh-manager.conf`. All create/update/delete operations target this file only. Hosts found in other files are shown as read-only (they can be imported by copying them into the managed file).
3. **Round-trip must be lossless.** Parsing a file and serializing it without edits must produce byte-identical output. Preserve comments, blank lines, indentation, keyword casing, ordering, and unknown keywords. Add a fixture test for every new edge case.
4. Keywords are case-insensitive. For a given host, OpenSSH uses the **first** value obtained for each option. Do not "deduplicate" by keeping the last one.
5. Support `Host` patterns (`*`, `?`, `!negation`, multiple patterns per line), `Match` blocks, `Include` (relative paths resolve against `~/.ssh/`, globs allowed), and both `Key Value` and `Key=Value` syntax. Parse `Match` blocks for display, but don't offer editing for them.
6. Write atomically with `QSaveFile`. Set permissions to `0600` on the file and `0700` on `~/.ssh/config.d/`. Create a timestamped backup before the first write in each session.
7. Watch files with `QFileSystemWatcher` and reload on external changes. If the user has unsaved edits in a dialog when this happens, warn instead of silently overwriting.
8. When you need effective settings for display, use `ssh -G <alias>` rather than reimplementing OpenSSH's resolution logic.
9. **OpenSSH version differences:**
   - Any keyword the editor *offers* must be valid in OpenSSH 9.2 (Debian 12).
   - Keywords that only newer versions accept may be preserved when already present, but the UI shouldn't suggest them.
   - Don't offer deprecated or removed algorithms (e.g. DSA keys, which OpenSSH 10 dropped) as choices.
   - If a user-entered keyword is rejected, surface `ssh -G`'s error in the UI instead of failing silently.

## Metadata format

App-only data (group, color, notes) is stored as structured comments immediately above the `Host` line in the managed file:

```
# sshmanager: group="Production" color="#d33" note="Primary DB"
Host prod-db
    HostName 10.0.0.12
    User admin
```

Unknown metadata keys must be preserved on write. Never store secrets in metadata.

## Terminal embedding

- **Plugin ID:** it's `kf6/parts/konsolepart` on KF6 and `kf5/parts/konsolepart` on KF5. Define it once in `compat.h`. If loading fails, retry with the bare ID `konsolepart` before giving up, since older or unusual installs may place it differently.
- **Loading:** use `KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(pluginId), parent)`. This API exists in both KF 5.103 and KF6, so no separate code paths are needed.
- **Starting ssh:** get a `TerminalInterface` via `qobject_cast`, then call `startProgram(QStringLiteral("ssh"), {QStringLiteral("ssh"), alias})`. If the location of the `TerminalInterface` header differs between KF5 and KF6, handle that include in `compat.h`.
- **Missing plugin:** if the plugin fails to load, show a clear error that says Konsole must be installed. Never crash.
- **Process exit:** the part destroys itself when the ssh process exits. Connect to `QObject::destroyed` and remove the tab. Don't delete the part twice.
- **Instances:** use one part instance per tab. Don't share parts between tabs.

## Security

- **Launch ssh with the alias only.** Never build shell command strings, never pass the command through `sh -c`, and never expand options into the argv.
- Validate aliases before use and on save: no whitespace, no leading `-` (prevents option injection), no control characters.
- Never store passwords in plain text, in config comments, or in app settings. Password storage, if added, must go through KWallet (`KF${KF_MAJOR_VERSION}::Wallet`). Prefer documenting key-based auth and ssh-agent (`ksshaskpass`) instead.
- Don't log full config contents or `ssh -G` output at default log levels. Use `QLoggingCategory` with categories that are off by default.

## Testing

- Use Qt Test (`QTEST_GUILESS_MAIN` for non-UI tests). It behaves the same on Qt 5 and Qt 6.
- **Tests must never touch the real `~/.ssh`.** Use `QTemporaryDir` and pass paths explicitly. Parser and writer classes take file paths as parameters; don't hard-code `QDir::homePath()` inside them.
- Every parser change needs a round-trip test with a fixture in `tests/data/`.
- Run `ctest` and ensure it passes on **both** Debian 12 and Debian 13 before finishing a task.

### CI

CI runs a matrix of `debian:12` (`QT_MAJOR_VERSION=5`) and `debian:13` (`QT_MAJOR_VERSION=6`). Both jobs must pass. Don't mark a job as allowed-to-fail to get a change merged.

## Packaging

Build a separate `.deb` for each release; one binary package can't serve both, because the Qt/KF libraries differ.

| Release | Package version | Runtime dependencies |
|---|---|---|
| Debian 12 | `X.Y.Z~deb12` | `konsole`, `openssh-client`, KF5 libraries |
| Debian 13 | `X.Y.Z~deb13` | `konsole`, `openssh-client`, KF6 libraries |

Install a `.desktop` file and an AppStream metainfo file. Their contents are shared between releases.

## Code style

- KDE coding style, enforced by the repo's `.clang-format` via ECM's `KDEClangFormat`.
- Wrap user-visible strings in `i18n()` / `i18nc()`. Never use raw literals in the UI.
- Use `QStringLiteral` for string literals in code.
- Prefer Qt containers and types at API boundaries. Prefer `std::unique_ptr` or Qt parent ownership over raw `new`/`delete` without an owner.
- Use signals and slots with function-pointer syntax, not the `SIGNAL()`/`SLOT()` macros.
- Keep classes small. If a file grows past about 400 lines, consider splitting it.

## Commits and pull requests

- Use Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `build:`).
- One logical change per commit. Include tests with behavior changes.
- Mention in the PR description if a change touches `compat.h`, and confirm it was tested on both releases.
- Don't add new dependencies without noting why in the PR description. A new dependency must be packaged in **both** Debian 12 and Debian 13.
