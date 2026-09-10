# Flameshot OCR

![Linux](https://img.shields.io/badge/Linux-x86__64-blue)
![Wayland](https://img.shields.io/badge/Wayland-tested-success)
![OCR](https://img.shields.io/badge/OCR-PaddleOCR--VL--1.6-orange)
![Build](https://github.com/yanjing-chen/flameshot-ocr/actions/workflows/build.yml/badge.svg)

**Flameshot OCR** is an experimental Linux fork of [Flameshot](https://github.com/flameshot-org/flameshot) that adds local OCR powered by **PaddleOCR-VL** through **llama.cpp**.

The OCR path is fully local: screenshots are sent only to a local `llama-server` endpoint on `127.0.0.1` unless the user explicitly changes the server URL.

> This repository is an independent fork and is not an official Flameshot release.

## What v2.3 adds

- OCR button directly inside the Flameshot capture toolbar.
- PaddleOCR-VL-1.6 GGUF support.
- One-click model download with progress reporting.
- Download cancel/resume using `.part` files and HTTP Range requests.
- Model integrity checks by expected file size.
- Model deletion and model-path management.
- Persistent local `llama-server` lifecycle:
  - automatic start,
  - reuse between captures,
  - manual start/stop,
  - PID ownership protection,
  - connection testing.
- Runtime selection:
  - dedicated CUDA runtime first when available,
  - Vulkan runtime next,
  - CPU fallback.
- Remote verified `models.json` manifest with local caching.
- Simplified Chinese OCR-settings translation.
- Dedicated OCR toolbar/configuration icon.
- Wayland-compatible capture workflow.

## Tested configuration

v2.3 has been tested successfully on:

- Ubuntu 26.04
- GNOME 50
- Wayland
- AMD Ryzen 7 8845HS
- Radeon 780M (RADV Vulkan)
- 32 GB RAM
- PaddleOCR-VL-1.6 GGUF
- llama.cpp server

The OCR model itself is **not** bundled into the AppImage or `.deb`.

## Screenshot

![OCR settings](docs/images/ocr-settings.png)

## OCR model

The built-in verified model is:

- `PaddleOCR-VL-1.6-GGUF.gguf`
- `PaddleOCR-VL-1.6-GGUF-mmproj.gguf`

The application can download them from the official PaddlePaddle Hugging Face repository.

Default future model storage:

```text
~/.local/share/flameshot-ocr/models/
```

A previously existing development installation under `~/Models/PaddleOCR-VL-1.6/` is also detected.

## Using OCR

1. Start Flameshot.
2. Open **Configuration → OCR**.
3. Confirm the OCR model is installed. If not, click **Download model**.
4. Leave **Automatically start the local OCR service when OCR is used** enabled.
5. Run a Flameshot GUI capture.
6. Select an area containing text.
7. Click the OCR toolbar button, or use `Ctrl+Shift+O`.
8. The recognized text appears in an editable result window and can be copied.

## Hardware acceleration

Flameshot OCR v2.3 separates the application from the inference runtime.

Runtime selection is designed in this order:

```text
explicit user-configured llama-server
        ↓
llama-server-cuda
        ↓
llama-server-vulkan
        ↓
generic llama-server with CUDA/Vulkan
        ↓
llama-server-cpu
        ↓
generic CPU fallback
```

### AMD

The GitHub Actions packages include a Vulkan llama.cpp runtime. Modern AMD GPUs using Mesa/RADV should normally be detected as `Vulkan0`.

### NVIDIA

The standard package can use NVIDIA GPUs through Vulkan when a working NVIDIA Vulkan driver is installed.

The v2.3 code also supports a dedicated CUDA runtime named:

```text
llama-server-cuda
```

A user-provided CUDA runtime can be placed at:

```text
~/.local/share/flameshot-ocr/runtime/llama-server-cuda
```

and will be preferred automatically when an NVIDIA driver is detected. A dedicated CUDA build is intentionally not bundled in the first automated package because CUDA substantially increases build/package complexity and size.

### CPU

A separate CPU runtime is included as a fallback.

## Remote model manifest

This repository contains:

```text
models.json
```

To use the repository-hosted manifest, set **Configuration → OCR → Remote manifest** to:

```text
https://raw.githubusercontent.com/yanjing-chen/flameshot-ocr/main/models.json
```

The application caches a valid manifest locally. If the remote endpoint is temporarily unavailable or returns invalid JSON, the last valid cache remains intact.

## GitHub Actions

`.github/workflows/build.yml` builds Linux packages automatically on pushes to `main`, tags, and manual workflow runs.

Artifacts:

- `Flameshot-OCR-<version>-x86_64.AppImage`
- `flameshot-ocr_<version>_amd64.deb`
- SHA-256 checksum files

The workflow follows the current upstream Flameshot Linux packaging approach for AppImage and Debian packaging, while adding the OCR llama.cpp Vulkan/CPU runtimes.

### AppImage

```bash
chmod +x Flameshot-OCR-*.AppImage
./Flameshot-OCR-*.AppImage
```

### Debian/Ubuntu

```bash
sudo apt install ./flameshot-ocr_*_amd64.deb
```

The `.deb` package conflicts with/replaces the official `flameshot` package because both install the same `flameshot` executable and desktop integration.

## Development

This fork is based on the Flameshot source tree and uses CMake/Qt 6.

A typical local build is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run:

```bash
./build/src/flameshot gui
```

## Project status

Current OCR checkpoint:

```text
paddleocr-vl-v2.3
```

Main v2.3 features have been manually tested, including:

- OCR on Wayland,
- persistent OCR service,
- service reuse,
- service stop/cleanup,
- model download,
- interrupted-download resume,
- downloaded-model OCR,
- model deletion,
- remote manifest update,
- offline manifest cache,
- invalid-manifest protection,
- Vulkan runtime selection.

## Credits and licenses

- Flameshot: upstream project and original source base.
- PaddleOCR-VL-1.6 GGUF: PaddlePaddle.
- llama.cpp: local GGUF inference runtime.
- OCR icon: Material Design Icons / Pictogrammers Team, Apache 2.0.

See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and the existing project `LICENSE`.

---

## 中文说明

这是一个在 Flameshot 基础上加入本地 OCR 的实验性 Linux 分支。OCR 使用 PaddleOCR-VL-1.6 GGUF，并通过 llama.cpp 本地推理。

v2.3 已在 Ubuntu 26.04 + GNOME 50 + Wayland + Radeon 780M 环境测试通过。模型不打包进 AppImage 或 deb，用户在 OCR 设置页单独下载。程序支持模型断点续传、后台 OCR 服务持久化、远程模型清单缓存，以及 CUDA / Vulkan / CPU 多运行时选择。

GitHub Actions 会自动生成 AppImage 和 Ubuntu/Debian 安装包。
