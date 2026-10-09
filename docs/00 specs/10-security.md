## Security

- **Launch ssh with the alias only.** Never build shell command strings, never pass the command through `sh -c`, and never expand options into the argv.
- Validate aliases before use and on save: no whitespace, no leading `-` (prevents option injection), no control characters.
- Never store passwords in plain text, in config comments, or in app settings. Password storage, if added, must go through KWallet (`KF${KF_MAJOR_VERSION}::Wallet`). Prefer key-based auth and ssh-agent instead, and never offer to remember a key's passphrase.
- Don't log full config contents or `ssh -G` output at default log levels. Use `QLoggingCategory` with categories that are off by default.
- **Dependencies must come from trustworthy, popular sources.** Prefer packages from the official Debian 12 and Debian 13 archives (see [Commits and pull requests](14-commits-and-pull-requests.md)). Never pull in, vendor, or copy code from obscure sources, such as GitHub repositories that have only a few stars, little adoption, or no active maintenance. If a source's trustworthiness is in doubt, don't add it; ask the user first.

---

**Related specs:**

- [SSH config rules (critical)](07-ssh-config-rules.md)
- [Metadata format](08-metadata-format.md)
- [Terminal embedding](09-terminal-embedding.md)
- [Supported platforms and tech stack](03-platforms-and-tech-stack.md)
