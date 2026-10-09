## SSH agent panel

A panel in the sidebar, below the host tree, that loads private keys into the SSH agent and removes them, without the user having to open a terminal. It moves with the sidebar when the sidebar is placed on the other side of the window.

### Purpose

The app's sessions authenticate with SSH keys through the SSH agent (see [Security](10-security.md)). The panel shows what the agent holds, so the user can load the keys their hosts need before connecting and remove them when they're done.

### What it shows

- **Agent status:** whether an agent is reachable and how many keys it holds, or why it can't be used (for example, no agent in the session, or an agent that doesn't respond).
- **Keys**, in this order:
  1. Keys from the SSH configuration: each identity file used by a host in any file the app reads, listed once.
  2. Keys the user added by choosing a file.
  3. Keys loaded in the agent that match neither of the above, such as keys loaded by another program.
- **Each key's details:** its file name (or, for the third group, the agent's comment), its type, and whether it is loaded. The full path and fingerprint go in a tooltip. A key file that doesn't exist is shown as missing and can't be loaded.
- **Errors:** the message from the last failed action, shown inline in the panel and never in a dialog. Use `ssh-add`'s own message where there is one (for example, a wrong passphrase, a key file whose permissions are too open, or an unreachable agent).

### User interactions

- **Load** a listed key into the agent, or **Remove** a loaded key from it. Any loaded key can be removed, including keys loaded by another program.
- **Add a key file:** choose a private key with the standard file chooser. The key is added to the list and loaded. Added keys are remembered (only their paths) and can be removed from the list again, which doesn't unload them. Keys from the SSH configuration follow the configuration and can't be removed from the list.
- **Refresh:** the panel refreshes the status and keys when asked, after each action, and when the SSH configuration changes.
- **Visibility:** the panel can be shown or hidden from the View menu, and its size relative to the host tree can be changed. Both are remembered.

### Implementation notes

- Use the agent from the app's environment, which belongs to the desktop session. The ssh sessions in the app's tabs use the same agent. Never start, stop, lock, or reconfigure an agent.
- Use `ssh-add` to list, load, and remove keys, and `ssh-keygen` to read a key file's fingerprint. Run them directly with an argument list (never through a shell) and asynchronously, so the UI never blocks while waiting, for example, for a passphrase. Pass key files as absolute paths, so a file name is never read as an option.
- Match loaded keys to key files by fingerprint, never by comment or file name.
- **Passphrases:** the app is `ssh-add`'s askpass program, replacing the desktop's (such as `ksshaskpass`). It only shows a passphrase prompt that never offers to remember the passphrase, and passes the answer to `ssh-add`. The app never stores or logs a passphrase.
- Never read, copy, or log the contents of private keys. Store the paths of added keys in the app settings, never in the SSH configuration or its metadata.
- The agent is shared by the whole desktop session: changes made in the panel affect other programs, and changes made by other programs appear after a refresh.
- Keep reading `ssh-add` and `ssh-keygen` output separate from the UI code, and make it testable without a display and without the user's agent (see [Testing and CI](11-testing.md)).

---

**Related specs:**

- [Features](02-features.md)
- [Security](10-security.md)
- [Testing and CI](11-testing.md)
- [UI specifications](15-ui-specifications.md)
- [SSH config rules (critical)](07-ssh-config-rules.md)
