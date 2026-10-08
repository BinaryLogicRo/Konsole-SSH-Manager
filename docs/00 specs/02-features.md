## Features

What the app offers its users. Keep this list current: update it whenever a user-facing feature is added, changed, or removed.

### Host sidebar

- Lists every host from the user's SSH configuration, including hosts from included files.
- Groups the app's own hosts by user-defined group; hosts from other files are grouped by their file and marked read-only.
- Shows each host's color, and its details (host name, user, port, note, and where it is defined) in a tooltip.
- Shows wildcard patterns and `Match` blocks for reference; they can't be opened as sessions.
- Filters hosts by alias, host name, user, note, or group.
- Remembers which groups are collapsed.
- Offers the actions for the selected host in a context menu.
- Can be placed on the left or right side of the window.

### Managing hosts

- Add, edit, duplicate, and delete the app's own hosts.
- Import a read-only host as an editable copy, leaving the original untouched.
- Edit the common settings (alias, host name, user, port, identity file, proxy jump) in a form, and any other OpenSSH option in a list with keyword suggestions.
- Organize hosts with a group, a color, and a note.
- Checks each entry before saving and shows OpenSSH's own message when it rejects one.
- Shows the effective settings OpenSSH will use for a host.

### Protecting the SSH configuration

- Never rewrites the user's handwritten configuration; the only change it may make there is adding one include line, and only after the user confirms.
- Warns when the app's own hosts aren't visible to ssh yet and offers to fix it.
- Preserves comments, formatting, and unknown options when saving.
- Saves safely, keeps the files private to the user, and makes a backup before changing a file.
- Picks up changes made by other programs automatically, and warns instead of overwriting when a host being edited changed meanwhile.

### SSH sessions

- Opens each connection in its own embedded Konsole terminal tab.
- Allows several sessions to the same host, numbered in the tab titles.
- Shows the host's color in its tab and its address in the tab's tooltip.
- Tabs can be reordered and switched from the keyboard; the close button can be limited to the active tab.
- Closes the tab after a normal logout.
- When a connection fails or drops, keeps the tab open with ssh's error and offers Retry and Close Tab buttons.
- Asks for confirmation before quitting with sessions still open.

### SSH agent

- A panel below the host tree shows whether the SSH agent is available, and lists the keys it holds.
- Lists the keys used by the SSH configuration first, then keys the user adds by choosing a file.
- Loads keys into the agent and removes them, showing any error inline.

### Security

- Connects using only the host's alias, never with extra options or through a shell.
- Never stores passwords; relies on SSH keys and the SSH agent.

### Window and desktop

- Main toolbar that can be hidden and shown again.
- Remembers the window's size, layout, and view options.
- Integrates with Plasma: application menu entry, app icon, and the desktop's look.
- Installs for the current user only, without root access.

---

**Related specs:**

- [Project overview](01-project-overview.md)
- [SSH config rules (critical)](07-ssh-config-rules.md)
- [Terminal embedding](09-terminal-embedding.md)
- [Security](10-security.md)
- [UI specifications](15-ui-specifications.md)
- [Installation](16-installation.md)
- [SSH agent panel](19-ssh-agent-panel.md)
