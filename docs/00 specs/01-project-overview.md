## Project overview

A KDE Plasma desktop app that manages SSH host profiles and opens connections as embedded Konsole terminals. The window has a sidebar (host tree with groups) and a tabbed area where each tab is an embedded `konsolepart` running `ssh <alias>`.

The app reads the user's `~/.ssh/config` and owns a separate managed file that it creates, edits, and deletes hosts in. It never rewrites the user's handwritten config.

---

**Related specs:**

- [Supported platforms and tech stack](02-platforms-and-tech-stack.md)
- [Repository layout](05-repository-layout.md)
- [SSH config rules (critical)](06-ssh-config-rules.md)
- [Terminal embedding](08-terminal-embedding.md)
