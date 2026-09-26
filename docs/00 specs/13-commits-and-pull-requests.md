## Commits and pull requests

- Use Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `build:`).
- One logical change per commit. Include tests with behavior changes.
- Mention in the PR description if a change touches `compat.h`, and confirm it was tested on both releases.
- Don't add new dependencies without noting why in the PR description. A new dependency must be packaged in **both** Debian 12 and Debian 13.

---

**Related specs:**

- [Version handling](04-version-handling.md)
- [Testing and CI](10-testing.md)
- [Code style](12-code-style.md)
- [Packaging](11-packaging.md)
