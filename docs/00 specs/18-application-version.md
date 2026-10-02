## Application version

- The CMake project version is the single source of truth for the app's version. Pass it to the executable at build time; the About dialog must display that same value through the app's About data. Do not maintain a separate version string in the UI.
- A release tag must match the version built from that release. Before tagging changes made after a release, choose a new version and update the CMake project version. Never reuse a release tag for changed contents.
- Use a patch increment for compatible corrections or licensing and documentation updates, and a minor increment for new user-facing functionality. The app is still in the `0.x` series, so these are project conventions rather than promises of a stable API.
- Builds must report a version without requiring Git at runtime, including builds from a source archive.

### Release description

At the end of every change, and whenever asked for a GitHub release message, AI agents must provide a brief Markdown message for GitHub's New release description. It must follow these conventions:

- New features go under a `**New:**` label, and bug fixes under a `**Fixed:**` label. Each label is followed by a blank line and a list with one brief item per change.
- Include only the labels that have items. When both are present, `**New:**` comes first.
- Add no other headings or text.

Example:

```markdown
**New:**

- when an SSH connection fails or drops, the tab now shows Retry and Close Tab buttons.

**Fixed:**

- a brief description of the bug that was fixed.
```

---

**Related specs:**

- [Version handling](05-version-handling.md)
- [Setup and build](04-setup-and-build.md)
- [Commits and pull requests](14-commits-and-pull-requests.md)
