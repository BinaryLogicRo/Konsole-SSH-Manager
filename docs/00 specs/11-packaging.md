## Packaging

Build a separate `.deb` for each release; one binary package can't serve both, because the Qt/KF libraries differ.

| Release | Package version | Runtime dependencies |
|---|---|---|
| Debian 12 | `X.Y.Z~deb12` | `konsole`, `openssh-client`, KF5 libraries |
| Debian 13 | `X.Y.Z~deb13` | `konsole`, `openssh-client`, KF6 libraries |

Install a `.desktop` file and an AppStream metainfo file. Their contents are shared between releases.

---

**Related specs:**

- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Commits and pull requests](13-commits-and-pull-requests.md)
