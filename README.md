# Flameshot OCR

![Linux](https://img.shields.io/badge/Linux-x86__64-blue)
![Wayland](https://img.shields.io/badge/Wayland-tested-success)
![OCR](https://img.shields.io/badge/OCR-PaddleOCR--VL--1.6-orange)
![Build](https://github.com/yanjing-chen/flameshot-ocr/actions/workflows/build.yml/badge.svg)

**Flameshot OCR** is an independent Linux fork of
[Flameshot](https://github.com/flameshot-org/flameshot). It adds a screenshot
OCR interface backed by the separately installed
[Local AI Runtime](https://github.com/yanjing-chen/local-ai-runtime).

The v2.9 development application is based on Flameshot v14.0.0.

> This repository is not an official Flameshot release.

## v2.9 architecture

Flameshot OCR v2.9 has three responsibilities:

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

## Current development

- Flameshot OCR development version: **v2.9**
- Latest stable Flameshot OCR version: **v2.8**
- Upstream base: **Flameshot v14.0.0**
- Local AI Runtime stable version: **0.8.0**
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
7. Copy, edit, or save the recognized plain-text result.

Existing Flameshot shortcuts are preserved. Flameshot OCR does not require or
install a fixed OCR shortcut.

## OCR settings

The OCR settings page provides:

- Local AI Runtime installation, repair and status refresh,
- runtime application, llama.cpp and PaddleOCR-VL status,
- API endpoint,
- OpenAI-compatible model ID,
- plain-text OCR output using the model's native `OCR:` task prompt,
- optional startup loading for the selected OCR model (off by default),
- automatic/CUDA/Vulkan/CPU backend selection,
- optional CUDA runtime installation, update and rollback through Local AI Runtime,
- endpoint connection test,
- installed/custom model list with an option to show unavailable catalog entries,
- guided custom GGUF inspection, registration and editing,
- optional MMProj, context size and GPU-layer controls,
- chat, translation, OCR, vision and inference capability declarations.

Flameshot sends model-management requests to Local AI Runtime 0.7.0 or later;
the acceleration controls require Runtime 0.8.0 or later. It does not copy or
delete external GGUF/MMProj files. Removing a catalog model deletes only its
Runtime-managed files; removing a custom entry removes only its Runtime registry
record and keeps its external files. With startup loading disabled, the Runtime service stays available but
does not start llama-server until the first OCR or plugin request. The OCR
result is an independent non-modal window with a Wayland-compatible drag bar.
In v2.8 the fullscreen capture overlay closes as soon as OCR starts, so other
applications remain usable during model loading and recognition. The result
footer reports total elapsed time, character throughput, and the model token
rate when the Runtime supplies it. In v2.9 OCR uses the model's native
`OCR:` task prompt, and results are saved as plain `.txt` files.

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

Stage 6B–6E architecture checks:

```bash
python3 tests/ocr_shared_runtime_contract.py
python3 tests/ocr_shared_runtime_ui_contract.py
python3 tests/local_ai_runtime_installer_contract.py
python3 tests/stage6b_client_only_contract.py
python3 tests/stage6c_model_management_contract.py
python3 tests/stage6e_guided_import_contract.py
python3 tests/stage7a_contract.py
python3 tests/stage7b_contract.py
python3 tests/stage7c_contract.py
```

## Credits and licenses

- Flameshot: upstream project and original source base.
- PaddleOCR-VL-1.6 GGUF: PaddlePaddle.
- Local AI Runtime / llama.cpp: separately installed local inference service.
- OCR icon: Material Design Icons / Pictogrammers Team, Apache 2.0.

See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and `LICENSE`.

---

## 中文说明

Flameshot OCR v2.9 开发版是基于 Flameshot v14.0.0 的非官方 Linux 分支。它负责
截图、OCR 界面和识别结果展示，并作为独立 Local AI Runtime 的客户端和
一键安装器。

AppImage 和 deb 不再内置 `llama-server`，也不再包含旧模型下载器、旧
Runtime Manager 或 CUDA Runtime Manager。推理运行环境、模型与硬件后端
统一由 Local AI Runtime 管理。

Local AI Runtime 安装在当前用户目录并通过 `systemd --user` 运行。卸载
Flameshot OCR 不会删除共享 Runtime、模型或服务。原有 Flameshot 快捷键
保持不变。

OCR 设置页可通过 Local AI Runtime 0.7.0 或更高版本查看目录模型，并以引导
方式导入自定义 GGUF：选择文件后自动读取元数据，建议模型 ID、名称、类型、
上下文大小、功能能力，并在同目录中保守匹配可选 MMProj。所有建议值都可在
保存前修改；自动分析不会加载模型。删除条目不会删除磁盘上的 GGUF 或
MMProj 文件。

v2.6 默认隐藏尚未安装文件的目录模型，可通过开关重新显示；新增“Runtime
启动时加载当前 OCR 模型”选项且默认关闭。关闭后 Runtime 服务仍会启动并
监听端口，llama-server 与模型会在首次 OCR 或插件请求时按需加载。OCR
识别结果改为独立非模态窗口，并提供兼容 Wayland 的拖动栏。

v2.7 新增推理后端选择，可设为自动、NVIDIA CUDA、Vulkan 或 CPU。
自动模式依次尝试 CUDA、Vulkan 与 CPU；指定后端不可用时 Runtime 会安全
回退。CUDA 运行环境仍由 Local AI Runtime 0.8 独立下载、校验、更新和回滚，
不会写入 AppImage 或 deb。

v2.8 曾加入纯文本 / Markdown 输出选择和 `.md` 保存。v2.9 已移除 Markdown
输出模式，OCR 固定使用模型原生 `OCR:` 任务提示，识别结果以普通文本显示、
复制并保存为 `.txt`。点击 OCR 后全屏截图高亮层仍会立即关闭，模型加载与识别
在独立非模态窗口中继续，因此可以同时操作其他软件。结果窗口底部显示总耗时、
字符速度，以及 Runtime 返回时的模型 token 速度。本阶段继续使用 Local AI
Runtime 0.8.0 和已发布的 CUDA r3，不修改 Runtime。
