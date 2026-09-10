# Flameshot OCR changelog

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
