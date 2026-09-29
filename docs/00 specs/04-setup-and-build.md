## Setup and build

### Setup

| Command | What it does |
|---|---|
| `make help` | List the available commands. |
| `make deps` | Install build dependencies for the current Debian release. Requires `sudo`. |

### Development

| Command | What it does |
|---|---|
| `make configure` | Configure or reconfigure the build. |
| `make` or `make build` | Build the app and tests. |
| `make run` | Build and start the app. |
| `make format` | Format the source code. |

### Testing

| Command | What it does |
|---|---|
| `make test` | Build and run the tests on the current system. |

### Installation

| Command | What it does |
|---|---|
| `make install` | Build and install the app for the current user. |
| `make uninstall` | Remove the installed app files while keeping settings and SSH configuration. |

### Images

| Command | What it does |
|---|---|
| `make screenshot` | Regenerate the README screenshot using fictional demo hosts. |
| `make social-preview` | Regenerate the GitHub social preview image. |

### Clean up

| Command | What it does |
|---|---|
| `make clean` | Remove build outputs while keeping the build configuration. |
| `make rebuild` | Delete the build directory and build again. |

### AI agents: build yes, run no

- AI agents **are allowed** to use `make configure`, `make build`, `make format`, and `make test`.
- AI agents **are not allowed** to use `make run` or launch the app another way. Running the app and checking it by hand is left to the user.
- AI agents **are not allowed** to run `make screenshot` unless the user explicitly asks for a new screenshot. Screenshots must only ever show fictional demo hosts, never real SSH configuration.
- AI agents **are not allowed** to run `make install` or `make uninstall` for the real home directory, because they write outside the project. See [Installation](16-installation.md) for how to check the install rules safely.

---

**Related specs:**

- [Supported platforms and tech stack](03-platforms-and-tech-stack.md)
- [Version handling](05-version-handling.md)
- [Testing and CI](11-testing.md)
- [Code style](13-code-style.md)
- [Installation](16-installation.md)
