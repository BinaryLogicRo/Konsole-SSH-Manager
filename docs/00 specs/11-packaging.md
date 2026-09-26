## Packaging

No `.deb` (or any other) packages are created. The app is built from source and run directly from the build directory (`./build/bin/konsole-ssh-manager`); see [Setup and build](03-setup-and-build.md). Don't add packaging files or packaging targets.

The one exception is the desktop entry template `data/ro.binarylogic.konsole-ssh-manager.desktop.in`. The build generates a menu entry from it that points at the binary in the build directory; there is still no install step. See [Installation](15-installation.md).

At runtime the app needs `konsole` and `openssh-client` installed.

If `.deb` packaging is ever introduced, each Debian release needs its own package, because the Qt/KF libraries differ.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Installation](15-installation.md)
