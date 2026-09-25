#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "src/ocr/localairuntimeinstaller.h").read_text(
    encoding="utf-8"
)
source = (ROOT / "src/ocr/localairuntimeinstaller.cpp").read_text(
    encoding="utf-8"
)
ui = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "src/CMakeLists.txt").read_text(encoding="utf-8")


for item in (
    "http://127.0.0.1:8111",
    "releases/download/app-manifest/app-manifest.json",
):
    assert item in source, item
print("INSTALL SOURCE CONTRACT    PASS")

for route in (
    "/health",
    "/v1/runtime/llama/status",
    "/v1/runtime/llama/install",
    "/v1/models",
    "/v1/models/install",
):
    assert route in source, route
print("RUNTIME API CONTRACT       PASS")

assert "QCryptographicHash::Sha256" in source
assert "m_expectedArchiveSize" in source
assert "m_expectedSha256" in source
print("APP SHA256 VERIFICATION    PASS")

for item in (
    'QStringLiteral("/usr/bin/tar")',
    'QStringLiteral("/usr/bin/bash")',
    'QStringLiteral("/usr/bin/systemctl")',
    'QStringLiteral("--user")',
):
    assert item in source, item
assert "sudo" not in source.lower()
print("PER-USER INSTALL PIPELINE  PASS")

for item in (
    "LD_LIBRARY_PATH",
    "QT_PLUGIN_PATH",
    "PYTHONHOME",
    "PYTHONPATH",
):
    assert item in source, item
print("APPIMAGE ENV SANITIZE      PASS")

assert "is not removed when Flameshot OCR is uninstalled" in ui
assert "ocr/localairuntimeinstaller.cpp" in cmake
assert "installOrRepair" in header
assert "refreshStatus" in header
assert "installProgress" in header
assert "installFinished" in header
print("INDEPENDENT INSTALLER      PASS")

print()
print("LOCAL AI RUNTIME INSTALLER CONTRACT: PASS")
