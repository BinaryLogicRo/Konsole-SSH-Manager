# AGENTS.md

Guidance for AI coding agents working on **konsole-ssh-manager**.

## Filesystem safety

AI agents must **not** run any command that can alter operating-system files. They may modify only files within this project and the SSH configuration file managed by the app.

## Additional rules

AI agents must **never use GitHub Actions**, including triggering, enabling, configuring, or otherwise interacting with GitHub Actions workflows.

## Specs: MUST read before any work

**Read every listed specification before any development or repository change.** Do not skip a specification because a change appears unrelated. The index is intentionally brief; use its prompts to revisit the relevant specifications while working.

| # | Specification | Revisit when |
|---|---|---|
| 01 | [Project overview](docs/00%20specs/01-project-overview.md) | Starting work. |
| 02 | [Features](docs/00%20specs/02-features.md) | Adding, changing, or removing a user-facing feature. |
| 03 | [Supported platforms and tech stack](docs/00%20specs/03-platforms-and-tech-stack.md) | Making platform or technology decisions. |
| 04 | [Setup and build](docs/00%20specs/04-setup-and-build.md) | Building, testing, or formatting. |
| 05 | [Version handling](docs/00%20specs/05-version-handling.md) | Making compatibility changes. |
| 06 | [Repository layout](docs/00%20specs/06-repository-layout.md) | Organizing source files. |
| 07 | [SSH config rules](docs/00%20specs/07-ssh-config-rules.md) | Changing SSH configuration behavior. |
| 08 | [Metadata format](docs/00%20specs/08-metadata-format.md) | Changing host metadata. |
| 09 | [Terminal embedding](docs/00%20specs/09-terminal-embedding.md) | Changing terminal behavior. |
| 10 | [Security](docs/00%20specs/10-security.md) | Handling input, secrets, processes, or dependencies. |
| 11 | [Testing and CI](docs/00%20specs/11-testing.md) | Writing or running tests. |
| 12 | [Packaging](docs/00%20specs/12-packaging.md) | Considering distribution or runtime requirements. |
| 13 | [Code style](docs/00%20specs/13-code-style.md) | Writing or reviewing code. |
| 14 | [Commits and pull requests](docs/00%20specs/14-commits-and-pull-requests.md) | Using Git or preparing a change for review. |
| 15 | [UI specifications](docs/00%20specs/15-ui-specifications.md) | Designing or changing the UI. |
| 16 | [Installation](docs/00%20specs/16-installation.md) | Changing installation behavior. |
| 17 | [Documentation](docs/00%20specs/17-documentation.md) | Creating or changing documentation. |
