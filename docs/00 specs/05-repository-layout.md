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

---

**Related specs:**

- [Project overview](01-project-overview.md)
- [Version handling](04-version-handling.md)
- [SSH config rules (critical)](06-ssh-config-rules.md)
- [Testing and CI](10-testing.md)
