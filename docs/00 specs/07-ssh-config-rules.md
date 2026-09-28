## SSH config rules (critical)

These rules protect the user's SSH setup. Treat any violation as a bug.

1. **Never write to `~/.ssh/config`** except to insert one line, `Include config.d/*`, and only after explicit user confirmation. This line must go before the first `Host` or `Match` block, because an `Include` inside a block is scoped to that block.
2. The app owns exactly one file: `~/.ssh/config.d/konsole-ssh-manager.conf`. All create/update/delete operations target this file only. Hosts found in other files are shown as read-only (they can be imported by copying them into the managed file).
3. **Round-trip must be lossless.** Parsing a file and serializing it without edits must produce byte-identical output. Preserve comments, blank lines, indentation, keyword casing, ordering, and unknown keywords. Add a fixture test for every new edge case.
4. Keywords are case-insensitive. For a given host, OpenSSH uses the **first** value obtained for each option. Do not "deduplicate" by keeping the last one.
5. Support `Host` patterns (`*`, `?`, `!negation`, multiple patterns per line), `Match` blocks, `Include` (relative paths resolve against `~/.ssh/`, globs allowed), and both `Key Value` and `Key=Value` syntax. Parse `Match` blocks for display, but don't offer editing for them.
6. Write atomically with `QSaveFile`. Set permissions to `0600` on the file and `0700` on `~/.ssh/config.d/`. Create a timestamped backup before the first write in each session.
7. Watch files with `QFileSystemWatcher` and reload on external changes. If the user has unsaved edits in a dialog when this happens, warn instead of silently overwriting.
8. When you need effective settings for display, use `ssh -G <alias>` rather than reimplementing OpenSSH's resolution logic.
9. **OpenSSH version differences:**
   - Any keyword the editor *offers* must be valid in OpenSSH 9.2 (Debian 12).
   - Keywords that only newer versions accept may be preserved when already present, but the UI shouldn't suggest them.
   - Don't offer deprecated or removed algorithms (e.g. DSA keys, which OpenSSH 10 dropped) as choices.
   - If a user-entered keyword is rejected, surface `ssh -G`'s error in the UI instead of failing silently.

---

**Related specs:**

- [Metadata format](08-metadata-format.md)
- [Security](10-security.md)
- [Testing and CI](11-testing.md)
- [Supported platforms and tech stack](03-platforms-and-tech-stack.md)
