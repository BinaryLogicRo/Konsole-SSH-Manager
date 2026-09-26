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

---

**Related specs:**

- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Setup and build](03-setup-and-build.md)
- [Repository layout](05-repository-layout.md)
- [Terminal embedding](08-terminal-embedding.md)
