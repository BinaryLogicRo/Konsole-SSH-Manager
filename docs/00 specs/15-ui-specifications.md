## UI specifications

Never use popups or dialogs in the app, except Help → About, any future Settings UI, and the standard file chooser for picking a key file (for example, in the [SSH agent panel](19-ssh-agent-panel.md)). The passphrase prompt for loading a key is the app's own askpass prompt (see the SSH agent panel): it is modal to the main window, so clicking the main window never takes the focus away from it, and it never offers to remember the passphrase.
