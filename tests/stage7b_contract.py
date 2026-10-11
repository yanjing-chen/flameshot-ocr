#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
UI_HEADER = (ROOT / "src/config/ocrconf.h").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
INSTALLER = (ROOT / "src/ocr/localairuntimeinstaller.cpp").read_text(
    encoding="utf-8"
)
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"

assert any(
    f"set(FLAMESHOT_OCR_VERSION {version})" in CMAKE
    for version in ("2.7", "2.8", "2.9")
)
assert any(
    f'OCR_VERSION: "{version}"' in WORKFLOW
    for version in ("2.7", "2.8", "2.9")
)
assert "python3 tests/zh_cn_translation_contract.py" in WORKFLOW
assert "stage7b_contract.py" in WORKFLOW
assert any(
    f"Flameshot-OCR/{version}" in UI for version in ("2.7", "2.8", "2.9")
)
assert any(
    f"Flameshot-OCR/{version}" in INSTALLER
    for version in ("2.7", "2.8", "2.9")
)
print("V2.7 DEVELOPMENT IDENTITY      PASS")

for token in (
    'QStringLiteral("/v1/runtime/backends")',
    'QStringLiteral("/v1/runtime/cuda/status?refresh=1")',
    'QStringLiteral("/v1/runtime/cuda/install")',
    'QStringLiteral("/v1/runtime/cuda/rollback")',
    'QStringLiteral("backend_preference")',
    "m_backendPreference",
    "requestCudaBackendInstall",
    "requestCudaBackendRollback",
):
    assert token in UI or token in UI_HEADER, token
print("ACCELERATION MANAGEMENT UI    PASS")

for token in (
    'QStringLiteral("auto")',
    'QStringLiteral("cuda")',
    'QStringLiteral("vulkan")',
    'QStringLiteral("cpu")',
    "Automatic mode tries CUDA, then Vulkan, then CPU.",
    "fallback is selected.",
):
    assert token in UI, token
print("BACKEND SELECTION UI           PASS")

# Flameshot remains an HTTP client. CUDA downloads, extraction and processes
# belong exclusively to Local AI Runtime.
for forbidden in (
    "QProcess",
    "QNetworkReply::downloadProgress",
    "libcublas",
    "libcudart",
    "llama-server-cuda",
):
    assert forbidden not in UI, forbidden
    assert forbidden not in UI_HEADER, forbidden
print("CLIENT-ONLY CUDA BOUNDARY      PASS")

tree = ET.parse(TRANSLATIONS)
contexts = {
    context.findtext("name"): {
        message.findtext("source"): message.findtext("translation")
        for message in context.findall("message")
    }
    for context in tree.getroot().findall("context")
}
for source in (
    "Inference acceleration",
    "Automatic (recommended)",
    "Preferred backend:",
    "Backend status:",
    "CUDA runtime:",
    "Install CUDA runtime",
    "Update CUDA runtime",
    "Roll back CUDA runtime",
    "Backend selection requires Local AI Runtime 0.8.0 or later. — %1",
    "The requested backend is unavailable; a safe fallback is selected.",
):
    assert contexts["OcrConf"].get(source), source
print("SIMPLIFIED CHINESE UI          PASS")

print()
print("STAGE 7B V2.7 CONTRACT: PASS")
