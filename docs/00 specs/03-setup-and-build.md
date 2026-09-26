## Setup and build

### Debian 12

```bash
sudo apt install build-essential cmake extra-cmake-modules qtbase5-dev \
  libkf5parts-dev libkf5coreaddons-dev libkf5i18n-dev konsole clang-format

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=5
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/bin/konsole-ssh-manager   # humans only: AI agents must not run this
```

### Debian 13

```bash
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev \
  libkf6parts-dev libkf6coreaddons-dev libkf6i18n-dev konsole clang-format

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=6
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/bin/konsole-ssh-manager   # humans only: AI agents must not run this
```

If `QT_MAJOR_VERSION` is omitted, CMake auto-detects: it prefers Qt 6 when available and falls back to Qt 5.

The app is always built from source and run directly from the build directory (`./build/bin/konsole-ssh-manager`). There is no install step and no `.deb` package; see [Packaging](11-packaging.md).

### AI agents: build yes, run no

- AI agents **are allowed** to configure and build the app (`cmake -B build ...`, `cmake --build build ...`), run the formatter, and run the test suite (`ctest`).
- AI agents **are not allowed** to run the app itself (`./build/bin/konsole-ssh-manager` or any other way of launching it). Running the app and checking it by hand is left to the user.

Format code before committing:

```bash
cmake --build build --target clang-format
```

### Testing both targets from one machine

Use containers (`podman`/`docker` with `debian:12` and `debian:13` images, or `distrobox`) to build and run the tests on the other release. A change is not done until it builds and passes tests on **both**.

---

**Related specs:**

- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Version handling](04-version-handling.md)
- [Testing and CI](10-testing.md)
- [Code style](12-code-style.md)
