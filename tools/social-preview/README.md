# Social preview generator

Draws the image GitHub shows when a link to this repository is shared: the app icon next to the app name and a short tagline. Run it again whenever the icon or the name changes. The generator is a development helper only; it is never installed and is not part of the app.

## Build

It builds with the rest of the project, so it needs the same dependencies; see [Build and run](../../README.md#build-and-run) in the main README. It is skipped in normal builds and is built on demand when you generate the image.

## Use

```bash
make social-preview           # 640×320, GitHub's minimum size
make social-preview SCALE=2   # 1280×640, sharper on high-resolution screens
```

The image is written to `data/social-preview.png`. To use it, open the repository's **Settings → General → Social preview** on GitHub and upload the file.
