## Testing

- Use Qt Test (`QTEST_GUILESS_MAIN` for non-UI tests). It behaves the same on Qt 5 and Qt 6.
- **Tests must never touch the real `~/.ssh`.** Use `QTemporaryDir` and pass paths explicitly. Parser and writer classes take file paths as parameters; don't hard-code `QDir::homePath()` inside them.
- Every parser change needs a round-trip test with a fixture in `tests/data/`.
- Run `ctest` and ensure it passes on **both** Debian 12 and Debian 13 before finishing a task.

### CI

CI runs a matrix of `debian:12` (`QT_MAJOR_VERSION=5`) and `debian:13` (`QT_MAJOR_VERSION=6`). Both jobs must pass. Don't mark a job as allowed-to-fail to get a change merged.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [SSH config rules (critical)](06-ssh-config-rules.md)
- [Commits and pull requests](13-commits-and-pull-requests.md)
