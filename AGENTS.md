# AGENTS.md

Guidance for AI coding agents working on **konsole-ssh-manager**.

## Filesystem safety

AI agents must **not** run any command that can alter operating-system files. They may modify only files within this project and, when required by the app's specified behavior, the single SSH configuration file managed by the app: `~/.ssh/config.d/konsole-ssh-manager.conf`.

AI agents must **never use GitHub Actions**, including triggering, enabling, configuring, or otherwise interacting with GitHub Actions workflows.

## Specs: MUST read before any work

The project specs live in [`docs/00 specs/`](docs/00%20specs/).

**Reading every spec file listed below is a MUST before doing any development or making any change** to this repository (code, build files, tests, packaging, or docs). Do not skip any of them, even for changes that look small or unrelated. The per-file "when to refer" notes tell you which spec to go back to while working. They do not replace reading all of them first.

| # | Spec | What it is | When to refer to it |
|---|---|---|---|
| 01 | [Project overview](docs/00%20specs/01-project-overview.md) | What the app is: a KDE Plasma SSH host manager with embedded Konsole tabs. Also how it treats `~/.ssh/config` compared with its own managed file. | To understand the product's purpose and scope before any change. |
| 02 | [Supported platforms and tech stack](docs/00%20specs/02-platforms-and-tech-stack.md) | The Debian 12 / Debian 13 target matrix (Qt, KF, Konsole, OpenSSH versions), the Debian 12 "floor" rule, and the C++/CMake/Qt/KF stack. | When choosing an API, language feature, dependency, or OpenSSH keyword, or when unsure whether something works on Debian 12. |
| 03 | [Setup and build](docs/00%20specs/03-setup-and-build.md) | Packages to install, build/test/run commands for each release, formatting, and testing both targets with containers. Also states that AI agents may build and test the app but must **not** run it, or install it into the real home directory. | When setting up an environment, building, testing, formatting, or checking a change on both releases. |
| 04 | [Version handling](docs/00%20specs/04-version-handling.md) | How Qt5/Qt6 and KF5/KF6 are handled in CMake and C++, the `src/compat.h` rule, and common Qt 6 migration pitfalls. | When editing CMake, writing any Qt/KF-dependent code, or touching `compat.h`. |
| 05 | [Repository layout](docs/00%20specs/05-repository-layout.md) | The target source/test directory structure and the rule that `sshconfig/` stays UI-free. | When adding, moving, or splitting files, or deciding where new code belongs. |
| 06 | [SSH config rules (critical)](docs/00%20specs/06-ssh-config-rules.md) | Rules that protect the user's SSH setup: the single managed file, lossless round-trip, OpenSSH semantics, atomic writes, file watching, and OpenSSH version differences. | Whenever code reads, parses, writes, or displays SSH config, or when the editor offers keywords. Any violation is a bug. |
| 07 | [Metadata format](docs/00%20specs/07-metadata-format.md) | The `# sshmanager:` structured-comment format for app-only data (group, color, notes). | When reading or writing host metadata, or adding new metadata keys. |
| 08 | [Terminal embedding](docs/00%20specs/08-terminal-embedding.md) | How `konsolepart` is located, loaded, and started with ssh, plus error handling, process-exit handling, and one part per tab. | When working on terminal tabs, the Konsole part, or session lifecycle. |
| 09 | [Security](docs/00%20specs/09-security.md) | How ssh is launched, alias validation, password/secret handling (KWallet only), logging restrictions, and the rule that dependencies must come only from trustworthy, popular sources. | When launching processes, handling user input or aliases, storing credentials, adding logging, or adding any dependency. |
| 10 | [Testing and CI](docs/00%20specs/10-testing.md) | Qt Test usage, isolating tests from the real `~/.ssh`, fixture requirements, and the two-release CI matrix. | When writing or changing tests, changing the parser, or before finishing any task. |
| 11 | [Packaging](docs/00%20specs/11-packaging.md) | States that no `.deb` packages are created, the only exception being the per-user `make install`, plus the runtime requirements. | When tempted to add packaging or install steps, or when checking runtime requirements. |
| 12 | [Code style](docs/00%20specs/12-code-style.md) | KDE coding style, clang-format, i18n, string literals, ownership, signal/slot syntax, and file size. | When writing or reviewing any C++ code. |
| 13 | [Commits and pull requests](docs/00%20specs/13-commits-and-pull-requests.md) | Conventional Commits, commit scope, and what PR descriptions must mention (compat changes, new dependencies). Also forbids AI agents from running any git command that alters history (commit, push, pull, branch, etc.). | Before running any git command, and when suggesting commit messages or PR descriptions. |
| 14 | [UI specifications](docs/00%20specs/14-ui-specifications.md) | UI interaction constraints, including the ban on popups and dialogs. | When designing or changing any application UI. |
| 15 | [Installation](docs/00%20specs/15-installation.md) | How users install the app for themselves with `make install` (executables to `~/.local/bin`, menu entry, app icon), how to update and uninstall it, where the app keeps its data, and how agents may check the install rules. | When changing the install rules, the desktop entry, the app icon or the app's desktop file ID, where the app stores data, or the install instructions. |
| 16 | [Documentation](docs/00%20specs/16-documentation.md) | Documentation scope and durability rules. | When creating or changing documentation. |
