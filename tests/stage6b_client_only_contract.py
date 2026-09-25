#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

manager = (ROOT / "src/ocr/ocrmanager.cpp").read_text(encoding="utf-8")
manager_header = (ROOT / "src/ocr/ocrmanager.h").read_text(encoding="utf-8")
ui = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
config_source = (ROOT / "src/utils/confighandler.cpp").read_text(
    encoding="utf-8"
)
config_header = (ROOT / "src/utils/confighandler.h").read_text(
    encoding="utf-8"
)


for path in (
    ROOT / "scripts/build-ocr-runtimes.sh",
    ROOT / ".github/workflows/cuda-runtime-test.yml",
    ROOT / "packaging/cuda-runtime/cuda-runtimes.json",
):
    assert not path.exists(), path
print("LEGACY BUILD FILES REMOVED PASS")

for token in (
    "runtime-build/llama-server-vulkan",
    "runtime-build/llama-server-cpu",
    "Build OCR runtimes",
    "LLAMA_CPP_REF",
    "CUDA Runtime Manager downloads",
):
    assert token not in workflow, token
print("PACKAGE INJECTION REMOVED  PASS")

assert "Embedded llama-server found in client-only AppImage" in workflow
assert "Embedded llama-server found in client-only deb" in workflow
print("ARTIFACT NEGATIVE GATES    PASS")

combined = "\n".join((manager, manager_header, ui))
for token in (
    "QProcess",
    "startServer",
    "stopServer",
    "downloadModel",
    "removeModel",
    "installCudaRuntime",
    "cancelCudaRuntimeInstall",
    "checkRemoteManifest",
    "FLAMESHOT_OCR_MANAGED",
):
    assert token not in combined, token
print("LEGACY EXECUTION REMOVED    PASS")

for token in (
    "ocrServerPath",
    "ocrModelRoot",
    "ocrManifestUrl",
    "ocrDeviceId",
    "ocrAutoStartServer",
    "ocrAutoCheckModelUpdates",
):
    assert token not in config_source, token
    assert token not in config_header, token
print("LEGACY CONFIG API REMOVED   PASS")

shortcut_files = [
    path for path in ROOT.rglob("*")
    if path.is_file() and "shortcut" in path.name.lower()
]
assert shortcut_files
print("SHORTCUT SOURCES PRESENT    PASS")

print()
print("STAGE 6B CLIENT-ONLY CONTRACT: PASS")
