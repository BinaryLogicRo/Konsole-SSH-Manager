## Repository layout

```
CMakeLists.txt         Build and install configuration
Makefile               Developer commands
src/
  main.cpp             App entry
  compat.h             Qt/KF compatibility
  logging.cpp          Logging categories
  sshconfig/           Config parsing, storage, and writing
  sshagent/            SSH agent and key file handling
  session/             SSH helper and the app's listener for failed sessions
  ui/                  Main window
    hosts/             Host sidebar, tree model, actions, and dialogs
    agent/             SSH agent panel
    terminal/          Embedded terminal tabs
tests/                 Qt tests and SSH config fixtures
data/                  Desktop entry and icon
tools/                 Screenshot and social preview tools
docs/                  Specs and images
```

UI code lives only in `ui/` (and `main.cpp`). Everything else in `src/` must be testable without a display, so it must never use widgets or KParts. The one exception is `compat.h`, which stays at the top of `src/` (see [Version handling](05-version-handling.md)) and may be included only by UI code.

---

**Related specs:**

- [Project overview](01-project-overview.md)
- [Version handling](05-version-handling.md)
- [SSH config rules (critical)](07-ssh-config-rules.md)
- [Testing and CI](11-testing.md)
