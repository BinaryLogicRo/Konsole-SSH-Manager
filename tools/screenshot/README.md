# Screenshot generator

Renders the screenshot shown in the main README: the app's main window as a first-time user sees it, framed like a desktop window. Run it again whenever the UI changes visibly. The generator is a development helper only; it is never installed and is not part of the app.

The window only ever shows fictional demo hosts from the `demo-ssh` folder next to this file. The generator runs the app in a throwaway home folder, so it never reads or changes your real SSH configuration or settings, and it doesn't open any SSH sessions. It prints the hosts it loaded so you can check that nothing real appears. To change what the screenshot shows, edit the demo hosts and keep every name and address fictional.

## Build

It builds with the rest of the project, so it needs the same dependencies; see [Build and run](../../README.md#build-and-run) in the main README. It is skipped in normal builds and is built on demand when you generate the screenshot.

## Use

```bash
make screenshot
```

The image is written to `docs/images/screenshot.png`, the file the main README shows.

AI agents may run this only when the user explicitly asks for a new screenshot; see [Setup and build](../../docs/00%20specs/04-setup-and-build.md).
