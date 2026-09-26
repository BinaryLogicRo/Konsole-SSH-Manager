## Packaging

No `.deb` (or any other) packages are created. The app is built from source and run directly from the build directory (`./build/bin/konsole-ssh-manager`); see [Setup and build](03-setup-and-build.md). Don't add packaging files or packaging targets.

At runtime the app needs `konsole` and `openssh-client` installed.

If `.deb` packaging is ever introduced, each Debian release needs its own package, because the Qt/KF libraries differ.

---

**Related specs:**

- [Setup and build](03-setup-and-build.md)
- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
