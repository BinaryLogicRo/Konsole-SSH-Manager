# AGENTS.md

Guidance for AI coding agents working on **konsole-ssh-manager**.

## Filesystem safety

AI agents must **not** run any command that can alter operating-system files. They may modify only files within this project and the SSH configuration file managed by the app.

AI agents must **never use GitHub Actions**, including triggering, enabling, configuring, or otherwise interacting with GitHub Actions workflows.

## Specs: MUST read before any work

**Read every listed specification before any development or repository change.** Do not skip a specification because a change appears unrelated. The index is intentionally brief; use its prompts to revisit the relevant specifications while working.

| # | Specification | Revisit when |
|---|---|---|
| 01 | [Project overview](docs/00%20specs/01-project-overview.md) | Starting work. |
| 02 | [Supported platforms and tech stack](docs/00%20specs/02-platforms-and-tech-stack.md) | Making platform or technology decisions. |
| 03 | [Setup and build](docs/00%20specs/03-setup-and-build.md) | Building, testing, or formatting. |
| 04 | [Version handling](docs/00%20specs/04-version-handling.md) | Making compatibility changes. |
| 05 | [Repository layout](docs/00%20specs/05-repository-layout.md) | Organizing source files. |
| 06 | [SSH config rules](docs/00%20specs/06-ssh-config-rules.md) | Changing SSH configuration behavior. |
| 07 | [Metadata format](docs/00%20specs/07-metadata-format.md) | Changing host metadata. |
| 08 | [Terminal embedding](docs/00%20specs/08-terminal-embedding.md) | Changing terminal behavior. |
| 09 | [Security](docs/00%20specs/09-security.md) | Handling input, secrets, processes, or dependencies. |
| 10 | [Testing and CI](docs/00%20specs/10-testing.md) | Writing or running tests. |
| 11 | [Packaging](docs/00%20specs/11-packaging.md) | Considering distribution or runtime requirements. |
| 12 | [Code style](docs/00%20specs/12-code-style.md) | Writing or reviewing code. |
| 13 | [Commits and pull requests](docs/00%20specs/13-commits-and-pull-requests.md) | Using Git or preparing a change for review. |
| 14 | [UI specifications](docs/00%20specs/14-ui-specifications.md) | Designing or changing the UI. |
| 15 | [Installation](docs/00%20specs/15-installation.md) | Changing installation behavior. |
| 16 | [Documentation](docs/00%20specs/16-documentation.md) | Creating or changing documentation. |
