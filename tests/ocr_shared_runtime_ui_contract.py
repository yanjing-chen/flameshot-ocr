#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

ui = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/config/ocrconf.h").read_text(encoding="utf-8")


for text in (
    'tr("Local AI Runtime")',
    'tr("API endpoint:")',
    'tr("Model ID:")',
    'tr("Test AI endpoint")',
    "Download and install",
    "Repair / check for updates",
    "Install missing components",
    "is not removed when Flameshot OCR is uninstalled",
):
    assert text in ui, text
print("SHARED RUNTIME UI           PASS")

for removed in (
    "Legacy llama-server",
    "Legacy fallback device",
    "Start legacy fallback",
    "Stop legacy fallback",
    "Legacy NVIDIA CUDA Runtime",
    "Legacy OCR Model",
    "Download model",
    "Delete model",
    "Check CUDA updates",
):
    assert removed not in ui, removed
print("LEGACY UI REMOVED           PASS")

for removed_member in (
    "m_serverPath",
    "m_modelRoot",
    "m_deviceCombo",
    "m_installCudaButton",
    "m_downloadButton",
):
    assert removed_member not in header, removed_member
print("LEGACY WIDGETS REMOVED      PASS")

print()
print("FLAMESHOT SHARED RUNTIME UI CONTRACT: PASS")
