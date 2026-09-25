# Flameshot OCR changelog

## v2.5 (development)

### Changed

- Migrated OCR inference to the independent Local AI Runtime service.
- Added one-click per-user Local AI Runtime installation and repair.
- Reduced Flameshot's OCR layer to screenshot UI, HTTP client and installer.
- Removed the embedded Vulkan and CPU `llama-server` binaries.
- Removed the Flameshot-managed server lifecycle and PID manager.
- Removed the legacy Flameshot model downloader and remote model manifest.
- Removed the Flameshot CUDA Runtime Manager and CUDA build workflow.
- Kept Local AI Runtime, its service and its models independent from Flameshot
  package installation and removal.
- Preserved existing screenshot and OCR shortcut behavior.

### Verified

- Chinese application and Qt translations in AppImage.
- Product version and upstream base version output.
- PaddleOCR-VL-1.6 through Local AI Runtime 0.3.2.
- Exactly one shared `llama-server` process during OCR.
- Vulkan inference and zero swap usage on Ryzen 7 6800H.

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
