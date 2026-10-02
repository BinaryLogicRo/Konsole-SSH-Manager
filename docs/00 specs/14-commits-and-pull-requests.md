## Commits and pull requests

- Use Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `build:`).
- One logical change per commit. Include tests with behavior changes.
- Mention in the PR description if a change touches `compat.h`, and confirm it was tested on both releases.
- Don't add new dependencies without noting why in the PR description. A new dependency must be packaged in **both** Debian 12 and Debian 13, and must come from a trustworthy, popular source (see [Security](10-security.md)).

The conventions above apply to commits and pull requests made by the user. An AI agent may suggest a commit message or PR description that follows them, but must not create the commit or PR itself.

Every time an AI agent implements a fix or a new feature, it must end its output with a brief, one-line commit message for that change that follows the conventions above.

### Git: AI agents must never alter history

AI agents must never run a git command that can alter the git history, refs, or remotes. This includes, but is not limited to:

- `commit` (including `--amend`), `push`, `pull`, `fetch`, `merge`, `rebase`, `reset`, `revert`, `cherry-pick`, `tag`
- creating, deleting, renaming, or switching branches, or checking out other commits (`branch`, `checkout`, `switch`)
- `stash`, `gc`, `reflog expire`, `filter-branch` / `filter-repo`, `remote add` / `remove` / `set-url`

Read-only commands such as `status`, `diff`, `log`, `show`, and `blame` are allowed. All commits, branches, pushes, and pulls are done by the user.

---

**Related specs:**

- [Version handling](05-version-handling.md)
- [Testing and CI](11-testing.md)
- [Code style](13-code-style.md)
- [Security](10-security.md)
