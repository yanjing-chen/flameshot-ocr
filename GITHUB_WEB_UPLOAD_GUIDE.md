# GitHub web upload guide

Target repository:

```text
https://github.com/yanjing-chen/flameshot-ocr
```

## Important browser limitation

GitHub's browser upload allows at most 100 files in one upload operation and 25 MiB per file. The Flameshot source tree contains more than 100 files, so uploading the entire project through the web interface will require multiple batches.

For a large source tree, a normal Git push is substantially easier. If you still want browser-only upload, use the batching method below.

## Create the repository

1. Sign in to GitHub.
2. Create a new repository named `flameshot-ocr`.
3. Prefer **Public** if you want Actions artifacts and the project to be visible.
4. Do **not** initialize it with an additional README, license, or `.gitignore`; this prepared folder already contains them.

## Upload

Use **Add file → Upload files**.

Upload the prepared source tree in batches of at most 100 files.

On Ubuntu/Nautilus, press `Ctrl+H` so hidden folders are visible. Be sure the following file reaches GitHub exactly at this path:

```text
.github/workflows/build.yml
```

Also verify these files are present at repository root:

```text
README.md
README_UPSTREAM.md
models.json
THIRD_PARTY_NOTICES.md
```

## Trigger GitHub Actions

After the source and workflow are committed to `main`:

1. Open the repository's **Actions** tab.
2. Select **Build AppImage and deb**.
3. The initial upload to `main` should trigger a build automatically.
4. If necessary, choose **Run workflow**.

When the two jobs finish, download the build artifacts from the workflow run:

```text
Flameshot-OCR-...-AppImage-x86_64
Flameshot-OCR-...-deb-ubuntu-24.04-amd64
```

## Future releases

The workflow also runs for tags matching:

```text
paddleocr-vl-v*
v*
```

Example:

```text
paddleocr-vl-v2.4
```

The package version will be derived from that tag.
