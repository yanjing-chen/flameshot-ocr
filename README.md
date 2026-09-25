# Flameshot OCR

![Linux](https://img.shields.io/badge/Linux-x86__64-blue)
![Wayland](https://img.shields.io/badge/Wayland-tested-success)
![OCR](https://img.shields.io/badge/OCR-PaddleOCR--VL--1.6-orange)
![Build](https://github.com/yanjing-chen/flameshot-ocr/actions/workflows/build.yml/badge.svg)

**Flameshot OCR** is an independent Linux fork of
[Flameshot](https://github.com/flameshot-org/flameshot). It adds a screenshot
OCR interface backed by the separately installed
[Local AI Runtime](https://github.com/yanjing-chen/local-ai-runtime).

The v2.5 application is based on Flameshot v14.0.0.

> This repository is not an official Flameshot release.

## v2.5 architecture

Flameshot OCR v2.5 has three responsibilities:

- capture screenshots,
- provide the OCR interface and result window,
- connect to or install Local AI Runtime.

The AppImage and deb package do **not** contain `llama-server`, model files, a
CUDA runtime manager, or a model downloader. Local AI Runtime owns those
components and exposes an OpenAI-compatible API on `127.0.0.1:8111`.

Local AI Runtime is installed independently under the current user's home
directory. Removing or replacing Flameshot OCR does not remove the runtime,
its models, or its systemd user service.

```text
Flameshot OCR
    └── HTTP client: http://127.0.0.1:8111
          └── Local AI Runtime
                ├── one active model at a time
                ├── llama.cpp runtime
                └── CPU / Vulkan / CUDA backends
```

## Current development status

- Flameshot OCR version: **v2.5 development branch**
- Upstream base: **Flameshot v14.0.0**
- Local AI Runtime stable version: **0.4.0**
- Default OCR model: **PaddleOCR-VL-1.6**
- Default model ID: `paddleocr-vl-1.6`
- Default API endpoint: `http://127.0.0.1:8111`

Validated on Ubuntu, GNOME/Wayland and Ryzen 7 6800H with Vulkan. Shared
runtime switching between PaddleOCR-VL-1.6 and HY-MT2-7B has also been tested.

## Using OCR

1. Start Flameshot OCR.
2. Open **Configuration → OCR**.
3. If Local AI Runtime is absent, select **Download and install**.
4. Confirm that the runtime, llama.cpp and PaddleOCR-VL entries are installed.
5. Start a normal Flameshot capture.
6. Select an area containing text and choose the OCR tool.
7. Copy or edit the recognized text in the result window.

Existing Flameshot shortcuts are preserved. Flameshot OCR does not require or
install a fixed OCR shortcut.

## OCR settings

The OCR settings page provides:

- Local AI Runtime installation, repair and status refresh,
- runtime application, llama.cpp and PaddleOCR-VL status,
- API endpoint,
- OpenAI-compatible model ID,
- endpoint connection test,
- combined catalog and custom-model list,
- custom GGUF registration and editing,
- optional MMProj, context size and GPU-layer controls,
- chat, translation, OCR, vision and inference capability declarations.

Flameshot sends these model-management requests to Local AI Runtime 0.4.0 or
later. It does not copy or delete external GGUF/MMProj files. Catalog models
remain read-only, and removing a custom entry only removes its Runtime registry
record.

## Packages

GitHub Actions builds:

- `Flameshot-OCR-<version>-x86_64.AppImage`
- `flameshot-ocr_<version>_amd64.deb`
- SHA-256 checksum files

The workflow rejects an artifact if an embedded `llama-server` or the old
`usr/lib/flameshot-ocr/runtime` directory is found.

## Development

This fork uses CMake and Qt 6.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/src/flameshot gui
```

Stage 6B architecture checks:

```bash
python3 tests/ocr_shared_runtime_contract.py
python3 tests/ocr_shared_runtime_ui_contract.py
python3 tests/local_ai_runtime_installer_contract.py
python3 tests/stage6b_client_only_contract.py
python3 tests/stage6c_model_management_contract.py
```

## Credits and licenses

- Flameshot: upstream project and original source base.
- PaddleOCR-VL-1.6 GGUF: PaddlePaddle.
- Local AI Runtime / llama.cpp: separately installed local inference service.
- OCR icon: Material Design Icons / Pictogrammers Team, Apache 2.0.

See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and `LICENSE`.

---

## 中文说明

Flameshot OCR v2.5 是基于 Flameshot v14.0.0 的非官方 Linux 分支。它负责
截图、OCR 界面和识别结果展示，并作为独立 Local AI Runtime 的客户端和
一键安装器。

AppImage 和 deb 不再内置 `llama-server`，也不再包含旧模型下载器、旧
Runtime Manager 或 CUDA Runtime Manager。推理运行环境、模型与硬件后端
统一由 Local AI Runtime 管理。

Local AI Runtime 安装在当前用户目录并通过 `systemd --user` 运行。卸载
Flameshot OCR 不会删除共享 Runtime、模型或服务。原有 Flameshot 快捷键
保持不变。

OCR 设置页可通过 Local AI Runtime 0.4.0 或更高版本查看目录模型，并添加、
编辑或删除自定义 GGUF 条目；支持可选 MMProj、上下文大小、GPU 层数以及
功能能力设置。删除条目不会删除磁盘上的 GGUF 或 MMProj 文件。
