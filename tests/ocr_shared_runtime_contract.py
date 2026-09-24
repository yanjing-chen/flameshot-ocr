#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]

capture = (
    ROOT / "src/widgets/capture/capturewidget.cpp"
).read_text(encoding="utf-8")

manager = (
    ROOT / "src/ocr/ocrmanager.cpp"
).read_text(encoding="utf-8")

config = (
    ROOT / "src/utils/confighandler.cpp"
).read_text(encoding="utf-8")


# Shared public endpoint.
assert '"http://127.0.0.1:8111"' in config
print("DEFAULT 8111 ENDPOINT       PASS")


# Explicit model routing.
assert re.search(
    r'payload\.insert\(\s*'
    r'QStringLiteral\("model"\)\s*,\s*'
    r'OcrManager::instance\(\)->activeModel\(\)\.id\s*\)',
    capture,
    re.S,
)

print("EXPLICIT MODEL FIELD        PASS")


# OpenAI-compatible endpoint.
assert 'QStringLiteral("/v1/chat/completions")' in manager
print("OPENAI CHAT ENDPOINT        PASS")


# Shared endpoint health check occurs before fallback startup.
start = manager.index(
    "void OcrManager::ensureReady("
)

window = manager[start:start + 6000]

health = window.find("testConnection(")
fallback = window.find("startServer(&startError)")

assert health >= 0
assert fallback >= 0
assert health < fallback

print("SHARED-FIRST HEALTH CHECK   PASS")


# Old v2.4 fallback retained.
assert "FLAMESHOT_OCR_MANAGED" in manager
assert "startServer(&startError)" in window

print("V2.4 LEGACY FALLBACK        PASS")


# Old server has API alias equal to requested model id.
assert re.search(
    r'QStringLiteral\("--alias"\)\s*'
    r'<<\s*model\.id',
    manager,
)

print("LEGACY MODEL ALIAS          PASS")


# Existing OCR multimodal request retained.
for needle in (
    'QStringLiteral("image_url")',
    'QStringLiteral("messages")',
    'QStringLiteral("temperature")',
    'QStringLiteral("stream")',
):
    assert needle in capture, needle

print("OCR REQUEST COMPATIBILITY   PASS")


# Backend-neutral health message.
assert (
    "The local AI/OCR service did not become ready"
    in manager
)

print("BACKEND-NEUTRAL HEALTH      PASS")


print()
print("FLAMESHOT SHARED RUNTIME CONTRACT: PASS")
