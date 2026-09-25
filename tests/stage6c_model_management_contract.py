#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/config/ocrconf.h").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"


for route in (
    'QStringLiteral("/v1/models")',
    'QStringLiteral("/v1/models/custom")',
    'QStringLiteral("/v1/models/custom/remove")',
):
    assert route in UI, route
print("RUNTIME MODEL ROUTES       PASS")

for field in (
    'QStringLiteral("model_path")',
    'QStringLiteral("mmproj_path")',
    'QStringLiteral("context_size")',
    'QStringLiteral("gpu_layers")',
    'QStringLiteral("capabilities")',
    'QStringLiteral("default_prompt")',
):
    assert field in UI, field
print("CUSTOM MODEL FIELDS        PASS")

for capability in (
    "chat",
    "translation",
    "ocr",
    "vision",
    "streaming",
    "thinking",
    "temperature",
    "custom_prompt",
    "context_size",
):
    assert f'QStringLiteral("{capability}")' in UI, capability
print("CAPABILITY EDITOR          PASS")

for label in (
    'tr("Runtime model management")',
    'tr("Add custom model")',
    'tr("Remove entry")',
    'tr("Use for OCR")',
    "never deletes either file.",
):
    assert label in UI, label
print("MODEL MANAGEMENT UI        PASS")

assert "setOcrModelId(id)" in UI
assert 'QStringLiteral("paddleocr-vl-1.6")' in UI
print("OCR MODEL SELECTION        PASS")

for forbidden in (
    "QProcess",
    "llama-server-vulkan",
    "llama-server-cpu",
    "installCudaRuntime",
    "downloadModel",
    "removeModel",
):
    assert forbidden not in UI, forbidden
    assert forbidden not in HEADER, forbidden
print("CLIENT-ONLY BOUNDARY       PASS")

assert "stage6c_model_management_contract.py" in WORKFLOW
print("ACTION CONTRACT GATE       PASS")

tree = ET.parse(TRANSLATIONS)
contexts = {
    context.findtext("name"): {
        message.findtext("source"): message.findtext("translation")
        for message in context.findall("message")
    }
    for context in tree.getroot().findall("context")
}
for context_name, sources in {
    "OcrConf": (
        "Runtime model management",
        "Add custom model",
        "Remove entry",
        "Use for OCR",
        "Custom entry removed. External GGUF files were kept.",
    ),
    "CustomModelDialog": (
        "Edit custom model",
        "GGUF model:",
        "MMProj (optional):",
        "Context size:",
        "GPU layers:",
        "Capabilities",
    ),
}.items():
    for source in sources:
        assert contexts[context_name].get(source), (context_name, source)
print("SIMPLIFIED CHINESE UI      PASS")

print()
print("STAGE 6C MODEL MANAGEMENT CONTRACT: PASS")
