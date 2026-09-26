## Security

- **Launch ssh with the alias only.** Never build shell command strings, never pass the command through `sh -c`, and never expand options into the argv.
- Validate aliases before use and on save: no whitespace, no leading `-` (prevents option injection), no control characters.
- Never store passwords in plain text, in config comments, or in app settings. Password storage, if added, must go through KWallet (`KF${KF_MAJOR_VERSION}::Wallet`). Prefer documenting key-based auth and ssh-agent (`ksshaskpass`) instead.
- Don't log full config contents or `ssh -G` output at default log levels. Use `QLoggingCategory` with categories that are off by default.

---

**Related specs:**

- [SSH config rules (critical)](06-ssh-config-rules.md)
- [Metadata format](07-metadata-format.md)
- [Terminal embedding](08-terminal-embedding.md)
