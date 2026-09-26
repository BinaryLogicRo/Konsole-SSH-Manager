# AGENTS.md

Guidance for AI coding agents working on **konsole-ssh-manager**.

## Project overview

A KDE Plasma 6 desktop app that manages SSH host profiles and opens connections as embedded Konsole terminals. The window has a sidebar (host tree with groups) and a tabbed area where each tab is an embedded `konsolepart` running `ssh <alias>`.

The app reads the user's `~/.ssh/config` and owns a separate managed file that it creates, edits, and deletes hosts in. It never rewrites the user's handwritten config.

Target platform: Debian 13 (trixie) with KDE Plasma 6. Linux only.

## Tech stack

- C++20, CMake ≥ 3.24, Extra CMake Modules (ECM)
- Qt 6 (Widgets, Test)
- KDE Frameworks 6: KParts, KCoreAddons, KI18n
- Runtime dependency: `konsole` (provides the `konsolepart` plugin)
- No Qt 5 / KF5 compatibility code. Do not add it.

## Setup and build

```bash
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev \
  libkf6parts-dev libkf6coreaddons-dev libkf6i18n-dev konsole clang-format

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/bin/konsole-ssh-manager
```

Format code before committing:

```bash
cmake --build build --target clang-format
```

## Repository layout

```
src/
  main.cpp               App entry, KAboutData, KLocalizedString setup
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

Keep UI code out of `sshconfig/`. That layer must be testable without a display.

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

- Load the part with `KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(QStringLiteral("kf6/parts/konsolepart")), parent)`.
- Get a `TerminalInterface` with `qobject_cast`, then call `startProgram(QStringLiteral("ssh"), {QStringLiteral("ssh"), alias})`.
- If the plugin fails to load, show a clear error that says Konsole must be installed. Never crash.
- The part destroys itself when the ssh process exits. Connect to `QObject::destroyed` and remove the tab. Don't delete the part twice.
- One part instance per tab. Don't share parts between tabs.

## Security

- **Launch ssh with the alias only.** Never build shell command strings, never pass the command through `sh -c`, and never expand options into the argv.
- Validate aliases before use and on save: no whitespace, no leading `-` (prevents option injection), no control characters.
- Never store passwords in plain text, in config comments, or in app settings. Password storage, if added, must go through KWallet. Prefer documenting key-based auth and ssh-agent (`ksshaskpass`) instead.
- Don't log full config contents or `ssh -G` output at default log levels. Use `QLoggingCategory` with categories that are off by default.

## Testing

- Use Qt Test (`QTEST_GUILESS_MAIN` for non-UI tests).
- **Tests must never touch the real `~/.ssh`.** Use `QTemporaryDir` and pass paths explicitly. Parser and writer classes take file paths as parameters; don't hard-code `QDir::homePath()` inside them.
- Every parser change needs a round-trip test with a fixture in `tests/data/`.
- Run `ctest` and ensure it passes before finishing a task.

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
- Don't add new dependencies without noting why in the PR description.

## Licensing

GPL-3.0-or-later (required because the app loads `konsolepart`, which is GPL). Add SPDX headers to every new source file:

```cpp
// SPDX-FileCopyrightText: 2026 Cristi
// SPDX-License-Identifier: GPL-3.0-or-later
```