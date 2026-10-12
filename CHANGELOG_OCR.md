# Flameshot OCR changelog

## v2.9 — Stage 7D localization and OCR simplification development

### Changed

- Removed mnemonic-letter suffixes such as `(T)`, `(O)`, `(C)`, `(A)` and `(Q)` from active Simplified Chinese tray-menu translations.
- Completed the Simplified Chinese translations for visible Linux general-settings items already present in v2.8.
- Improved the Simplified Chinese wording of the insecure pixelation option.
- This stage contains no windowed-editor implementation.
- Markdown OCR output has been removed; OCR now always uses PaddleOCR-VL's native `OCR:` task prompt.
- Local AI Runtime 0.8.0 and the existing OCR/model-management boundaries remain unchanged.

### Removed

- Markdown OCR output mode and the output-format selector.
- Markdown-specific result labels and direct `.md` saving.
- The legacy `ocrOutputFormat` configuration key remains recognized only for
  v2.8 configuration compatibility and is ignored by the v2.9 OCR workflow.

## v2.8 — Stage 7C development

### Added

- Plain-text and Markdown OCR output modes.
- Direct `.txt` and `.md` saving from the editable OCR result window.
- Result footer with total elapsed time, character throughput, and optional
  model token throughput reported by Local AI Runtime.

### Changed

- OCR work now continues in an independent controller after the fullscreen
  capture overlay closes, allowing other applications to be used while model
  loading and recognition are in progress.
- OCR progress and result windows are non-modal and no longer forced always on
  top.

### Scope

- Uses the released Local AI Runtime 0.8.0 and CUDA 12.8 r3 unchanged.
- A general-purpose detached screenshot editor remains planned for Stage 7D.

## v2.7 — 2026-10-11

### Added

- Runtime-owned automatic, NVIDIA CUDA, Vulkan and CPU backend selection.
- Optional independently installed CUDA 12.8 r3 runtime with verification,
  update and safe fallback behavior.

### Verified

- Real PaddleOCR on Ryzen 7 6800H / Vulkan and RTX 3050 / CUDA.
- Client-only AppImage and deb packages with Runtime 0.8.0.

## v2.5 — 2026-09-26

### Added

- Independent Local AI Runtime integration with one-click per-user installation
  and repair.
- Runtime model catalog and editable custom GGUF model entries.
- Guided GGUF import with metadata inspection, conservative context suggestions
  and automatic same-directory MMProj discovery.
- Automatic filename fallback for overly generic GGUF names such as `7B`.
- Model capability declarations for chat, translation, OCR, vision, streaming,
  thinking, temperature parameters, custom prompts and context size.
- HY-MT2 translation support through the shared Runtime.

### Changed

- Migrated all OCR inference to Local AI Runtime 0.6.0.
- Reduced Flameshot OCR to the screenshot UI, HTTP client and Runtime installer.
- Ensured that inspection and registration do not load a model; inference loads
  only the selected model and keeps single-model residency.
- Improved Simplified Chinese UI and clarified “Supports temperature parameter”.
- Isolated AppImage GIO/GVFS modules for compatibility with newer Ubuntu hosts.

### Removed

- Embedded Vulkan and CPU `llama-server` binaries.
- The legacy model downloader, server/PID lifecycle manager and remote model
  manifest.
- Flameshot-managed CUDA Runtime Manager and CUDA build workflow.
- The withdrawn HunyuanOCR integration and its model catalog entry.

### Verified

- Ubuntu 26.04, GNOME/Wayland and Ryzen 7 6800H with Vulkan.
- PaddleOCR-VL-1.6 real OCR and HY-MT2-7B real inference.
- Guided GGUF import, MMProj matching and editable suggestions.
- Real SSE streaming and HY-MT2 ↔ PaddleOCR model switching.
- Exactly one shared `llama-server`, correct process release and zero swap usage.
- AppImage direct tray launch, Simplified Chinese/Qt translations and GIO
  isolation.
- Client-only AppImage and Ubuntu 24.04 amd64 deb package contents.
- Existing screenshot shortcuts and external GGUF/MMProj files remain unchanged.

### Scope

- CUDA integration and NVIDIA RTX 3050 hardware validation are deferred to
  Stage 7 after the v2.5 release.
- Markdown-formatted OCR and direct `.md` saving are planned for a later release.

## v2.3

Verified checkpoint: `paddleocr-vl-v2.3`

### Added

- PaddleOCR-VL OCR action in the capture toolbar.
- Local OpenAI-compatible `llama-server` OCR request path.
- Dedicated OCR settings tab.
- Model download, progress, cancel, resume, validation, deletion.
- Remote `models.json` manifest and cache.
- Persistent managed OCR service with PID ownership verification.
- Automatic runtime/device selection.
- CUDA runtime hook.
- Vulkan runtime support.
- CPU fallback.
- Simplified Chinese translations for OCR UI.
- Dedicated OCR Material Design icon.

### Verified manually

- Ubuntu 26.04 / GNOME 50 / Wayland.
- Radeon 780M / Vulkan0.
- OCR service auto-start.
- Service persistence after capture process exits.
- Service reuse with stable PID.
- Manual start/stop and connection testing.
- Model interrupted-download resume.
- Full model download and OCR.
- Model deletion.
- Manifest refresh, offline cache, invalid JSON rejection.
- Dedicated Vulkan runtime selection.
