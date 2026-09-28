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
- Any new dependency must come from a trustworthy, popular source; see [Security](10-security.md).

---

**Related specs:**

- [Project overview](01-project-overview.md)
- [Setup and build](04-setup-and-build.md)
- [Version handling](05-version-handling.md)
- [Packaging](12-packaging.md)
- [Security](10-security.md)
