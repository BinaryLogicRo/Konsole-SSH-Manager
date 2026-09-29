## Repository layout

```
CMakeLists.txt         Build and install configuration
Makefile               Developer commands
src/
  main.cpp             App entry
  mainwindow.cpp       Main window
  hostsidebar.cpp      Host sidebar
  terminaltab.cpp      Embedded terminal tabs
  sshsessionmain.cpp   SSH helper entry
  compat.h             Qt/KF compatibility
  sshconfig/           Config parsing, storage, and writing
  models/              Host tree model
  dialogs/             Host editing and effective settings
tests/                 Qt tests and SSH config fixtures
data/                  Desktop entry and icon
tools/                 Screenshot and social preview tools
docs/                  Specs and images
```

Keep UI code out of `sshconfig/`: that layer must be testable without a display.

---

**Related specs:**

- [Project overview](01-project-overview.md)
- [Version handling](05-version-handling.md)
- [SSH config rules (critical)](07-ssh-config-rules.md)
- [Testing and CI](11-testing.md)
