# Flameshot OCR v2.5

Flameshot OCR v2.5 is a Linux-focused, unofficial Flameshot v14.0.0 fork that
connects screenshot OCR to the independent Local AI Runtime.

## Highlights

- PaddleOCR-VL-1.6 screenshot OCR through Local AI Runtime 0.6.0.
- HY-MT2-7B translation support and real SSE streaming.
- Runtime catalog plus editable custom GGUF model entries.
- Guided GGUF import: metadata inspection, context suggestions, capability
  detection and automatic MMProj matching.
- One active `llama-server` at a time with CPU/Vulkan backend selection.
- Simplified Chinese interface and preserved existing screenshot shortcuts.
- AppImage GIO/GVFS isolation for newer Ubuntu hosts.

Flameshot OCR is now a client-only package. Neither package contains
`llama-server`, model files or a CUDA Runtime Manager. Local AI Runtime, its
service and model files are installed and maintained independently in the
current user account.

## Packages

- `Flameshot-OCR-2.5-x86_64.AppImage`: portable Qt 6 build.
- `flameshot-ocr_2.5_amd64.deb`: Ubuntu 24.04 amd64 package using system Qt 6.
- A `.sha256sum` file is supplied for each package.

### AppImage

```bash
chmod +x Flameshot-OCR-2.5-x86_64.AppImage
./Flameshot-OCR-2.5-x86_64.AppImage
```

### deb

```bash
sudo apt install ./flameshot-ocr_2.5_amd64.deb
```

The deb conflicts with and replaces the upstream `flameshot` package because
both provide `/usr/bin/flameshot`.

## Runtime setup

Open **Configuration → OCR** and use the installer/status controls to install
or repair Local AI Runtime. Runtime 0.6.0 manages llama.cpp, PaddleOCR-VL-1.6,
HY-MT2-7B and user-supplied GGUF/MMProj files independently from Flameshot.

## Validation

The release was tested on Ubuntu 26.04, GNOME/Wayland and Ryzen 7 6800H with
Vulkan. Real PaddleOCR, HY-MT2 inference, SSE streaming, model switching,
single-model residency, direct AppImage tray launch and zero swap usage passed.
The deb was built on Ubuntu 24.04 and its dependencies and client-only payload
were verified on the 6800H system.

CUDA integration and NVIDIA RTX 3050 hardware validation are intentionally
deferred to Stage 7 after this release and are not claimed as validated v2.5
features.

---

# Flameshot OCR v2.5 中文说明

Flameshot OCR v2.5 是基于 Flameshot v14.0.0 的非官方 Linux 分支，通过独立
的 Local AI Runtime 0.6.0 提供截图 OCR、模型切换和自定义 GGUF 导入。

主要功能包括：

- PaddleOCR-VL-1.6 截图文字识别；
- HY-MT2-7B 翻译及实时流式输出；
- 自动读取 GGUF 元数据并建议名称、类型、上下文和能力；
- 自动匹配同目录 MMProj，所有建议均可手工修改；
- 简体中文界面，保留原有 Flameshot 快捷键；
- AppImage 与 deb 均为纯客户端包，不内置模型或 `llama-server`。

已在 Ubuntu 26.04、GNOME/Wayland、Ryzen 7 6800H Vulkan 环境完成真实
OCR、翻译、流式输出、双向模型切换、单模型驻留、托盘启动及零 swap 验证。
Ubuntu 24.04 amd64 deb 的依赖与文件清单也已验证。

CUDA Runtime 整合和 NVIDIA RTX 3050 实机验证将在发布后的 Stage 7 完成，
不作为 v2.5 已验证功能。Markdown OCR 与直接保存 `.md` 文件也留待后续版本。
