#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]

ui = (
    ROOT / "src/config/ocrconf.cpp"
).read_text(
    encoding="utf-8"
)

manager = (
    ROOT / "src/ocr/ocrmanager.cpp"
).read_text(
    encoding="utf-8"
)


required = (
    'tr("Local AI Runtime")',
    'tr("API endpoint:")',
    'tr("Legacy llama-server:")',
    'tr("Legacy fallback device:")',
    'tr("Legacy fallback acceleration:")',
    'tr("Start legacy fallback")',
    'tr("Stop legacy fallback")',
    'tr("Test AI endpoint")',
    "shared/local AI runtime is available",
    "legacy v2.4 managed llama-server",
)

for text in required:
    assert text in ui, text

print("SHARED RUNTIME UI          PASS")


# Healthy shared endpoint must block manual legacy startup.
assert re.search(
    r'm_startButton->setEnabled\(\s*!ok\s*&&\s*!currentlyManaged\s*\)',
    ui,
)

print("PORT-CONFLICT GUARD        PASS")


# Legacy functionality must remain available.
assert "startServer(&startError)" in manager
assert "FLAMESHOT_OCR_MANAGED" in manager

print("LEGACY FALLBACK RETAINED   PASS")


# Old CUDA manager remains functional, only visually demoted.
assert "installCudaRuntime()" in ui
assert "Legacy NVIDIA CUDA Runtime (Compatibility)" in ui

print("CUDA FALLBACK RETAINED     PASS")


# This stage must not remove the existing OCR model UI yet.
assert 'tr("Legacy OCR Model (Compatibility)")' in ui

print("OCR MODEL UI RETAINED      PASS")


print()
print("FLAMESHOT SHARED RUNTIME UI CONTRACT: PASS")
