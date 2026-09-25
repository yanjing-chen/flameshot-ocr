#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]

capture = (ROOT / "src/widgets/capture/capturewidget.cpp").read_text(
    encoding="utf-8"
)
manager = (ROOT / "src/ocr/ocrmanager.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/ocr/ocrmanager.h").read_text(encoding="utf-8")
config = (ROOT / "src/utils/confighandler.cpp").read_text(encoding="utf-8")


assert '"http://127.0.0.1:8111"' in config
print("DEFAULT 8111 ENDPOINT       PASS")

assert re.search(
    r'payload\.insert\(\s*'
    r'QStringLiteral\("model"\)\s*,\s*'
    r'OcrManager::instance\(\)->activeModelId\(\)\s*\)',
    capture,
    re.S,
)
print("EXPLICIT MODEL FIELD        PASS")

assert 'QStringLiteral("/health")' in manager
assert 'QStringLiteral("/v1/chat/completions")' in manager
print("LOCAL AI API ENDPOINTS      PASS")

assert "testConnection(" in manager
assert "ensureReady(" in manager
assert "startServer" not in manager
assert "stopServer" not in manager
assert "managedServerRunning" not in manager
assert "FLAMESHOT_OCR_MANAGED" not in manager
print("CLIENT-ONLY READINESS       PASS")

for removed in (
    "downloadModel",
    "removeModel",
    "installCudaRuntime",
    "checkCudaRuntimeUpdates",
    "availableDevices",
    "serverExecutable",
):
    assert removed not in header, removed
print("LEGACY MANAGERS REMOVED     PASS")

for needle in (
    'QStringLiteral("image_url")',
    'QStringLiteral("messages")',
    'QStringLiteral("temperature")',
    'QStringLiteral("stream")',
):
    assert needle in capture, needle
print("OCR REQUEST COMPATIBILITY   PASS")

print()
print("FLAMESHOT SHARED RUNTIME CONTRACT: PASS")
