## Packaging

No `.deb` (or any other) packages are created. The app is built from source; see [Setup and build](03-setup-and-build.md). Don't add packaging files or packaging targets.

The only exceptions are the per-user install: `make install` / `make uninstall` (backed by CMake `install()` rules) copy the executables to `~/.local/bin` and the menu entry, generated from the desktop entry template `data/ro.binarylogic.konsole-ssh-manager.desktop.in`, to `~/.local/share/applications`. See [Installation](15-installation.md).

At runtime the app needs `konsole` and `openssh-client` installed.

If `.deb` packaging is ever introduced, each Debian release needs its own package, because the Qt/KF libraries differ.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Installation](15-installation.md)
