## Code style

- KDE coding style, enforced by the repo's `.clang-format` via ECM's `KDEClangFormat`.
- Wrap user-visible strings in `i18n()` / `i18nc()`. Never use raw literals in the UI.
- Use `QStringLiteral` for string literals in code.
- Prefer Qt containers and types at API boundaries. Prefer `std::unique_ptr` or Qt parent ownership over raw `new`/`delete` without an owner.
- Use signals and slots with function-pointer syntax, not the `SIGNAL()`/`SLOT()` macros.
- Keep classes small. If a file grows past about 400 lines, consider splitting it.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [Repository layout](05-repository-layout.md)
- [Commits and pull requests](13-commits-and-pull-requests.md)
