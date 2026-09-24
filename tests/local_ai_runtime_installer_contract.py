#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

header = (
    ROOT / "src/ocr/localairuntimeinstaller.h"
).read_text(encoding="utf-8")

source = (
    ROOT / "src/ocr/localairuntimeinstaller.cpp"
).read_text(encoding="utf-8")

ui = (
    ROOT / "src/config/ocrconf.cpp"
).read_text(encoding="utf-8")

cmake = (
    ROOT / "src/CMakeLists.txt"
).read_text(encoding="utf-8")


required_urls = (
    "http://127.0.0.1:8111",
    "releases/download/app-manifest/app-manifest.json",
)

for item in required_urls:
    assert item in source, item

print("FIXED LOCAL ENDPOINT       PASS")
print("FIXED APP MANIFEST         PASS")


for route in (
    "/health",
    "/v1/runtime/llama/status",
    "/v1/runtime/llama/install",
    "/v1/models",
    "/v1/models/install",
):
    assert route in source, route

print("RUNTIME API CONTRACT       PASS")


assert 'QStringLiteral("model")' in source
assert "paddleocr-vl-1.6" in source

print("PADDLE MODEL ROUTING        PASS")


assert "QCryptographicHash::Sha256" in source
assert "m_expectedArchiveSize" in source
assert "m_expectedSha256" in source

print("APP SHA256 VERIFICATION     PASS")


assert 'QStringLiteral("/usr/bin/tar")' in source
assert 'QStringLiteral("/usr/bin/bash")' in source
assert 'QStringLiteral("/usr/bin/systemctl")' in source
assert 'QStringLiteral("--user")' in source

print("USER INSTALL PIPELINE       PASS")


assert "sudo" not in source.lower()

print("NO SUDO                     PASS")


for item in (
    "LD_LIBRARY_PATH",
    "QT_PLUGIN_PATH",
    "PYTHONHOME",
    "PYTHONPATH",
):
    assert item in source

print("APPIMAGE ENV SANITIZE       PASS")


for item in (
    "Runtime application:",
    "llama.cpp runtime:",
    "PaddleOCR-VL:",
    "Download and install",
    "Repair / check for updates",
    "Install missing components",
):
    assert item in ui, item

print("ONE-CLICK INSTALL UI        PASS")


assert "Legacy OCR Model (Compatibility)" in ui
assert "OCR Backend (Advanced / Compatibility)" in ui

print("LEGACY FALLBACK UI          PASS")


assert "ocr/localairuntimeinstaller.cpp" in cmake

print("CMAKE REGISTRATION          PASS")


assert "installOrRepair" in header
assert "refreshStatus" in header
assert "installProgress" in header
assert "installFinished" in header

print("INSTALLER PUBLIC CONTRACT   PASS")


print()
print("LOCAL AI RUNTIME INSTALLER CONTRACT: PASS")
