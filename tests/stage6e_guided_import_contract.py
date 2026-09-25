#!/usr/bin/env python3

from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
README = (ROOT / "README.md").read_text(encoding="utf-8")
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"


for token in (
    'QStringLiteral("/v1/models/inspect")',
    'QStringLiteral("model_path")',
    'QStringLiteral("mmproj_path")',
    'QStringLiteral("auto_match_mmproj"), true',
    'QStringLiteral("suggested")',
    'QStringLiteral("native_context_size")',
    'QStringLiteral("has_chat_template")',
    'QStringLiteral("warnings")',
):
    assert token in UI, token
print("RUNTIME 0.6 INSPECT API    PASS")

for token in (
    'tr("Import GGUF model")',
    'dialogTr("Import custom GGUF model")',
    'dialogTr("Analyze GGUF")',
    'dialogTr("Automatic analysis:")',
    "inspectModel();",
    "applyInspection(object);",
    "load(suggested);",
):
    assert token in UI, token
print("GUIDED IMPORT UI           PASS")

inspect_body = re.search(
    r"void inspectModel\(\).*?\n    void applyInspection",
    UI,
    re.DOTALL,
)
assert inspect_body, "inspectModel body"
inspect_source = inspect_body.group(0)
assert 'QStringLiteral("/v1/models/custom")' not in inspect_source
assert "m_saveButton->setEnabled(false)" in inspect_source
assert "m_saveButton->setEnabled(true)" in inspect_source
assert "QPointer<CustomModelDialog>" in inspect_source
print("REVIEW-BEFORE-SAVE GUARD   PASS")

add_body = re.search(
    r"void OcrConf::addCustomModel\(\).*?\n}\n\nvoid OcrConf::editSelectedCustomModel",
    UI,
    re.DOTALL,
)
assert add_body, "addCustomModel body"
add_source = add_body.group(0)
assert add_source.index("dialog.exec()") < add_source.index(
    'QStringLiteral("/v1/models/custom")'
)
print("NO AUTOMATIC REGISTRATION PASS")

for field in (
    "m_id",
    "m_name",
    "m_type",
    "m_modelPath",
    "m_mmprojPath",
    "m_contextSize",
    "m_gpuLayers",
    "m_defaultPrompt",
    "m_chat",
    "m_translation",
    "m_ocr",
    "m_vision",
    "m_streaming",
    "m_thinking",
    "m_temperature",
    "m_customPrompt",
    "m_contextCapability",
):
    assert field in UI, field
assert "m_id->setEnabled(!m_editing)" in UI
print("EDITABLE SUGGESTIONS       PASS")

for phrase in (
    "Automatic analysis is unavailable.",
    "enter every setting manually.",
    "Review every suggested setting before saving.",
    "never deletes either file.",
):
    assert phrase in UI, phrase
print("MANUAL FALLBACK            PASS")

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
        "Import GGUF model",
        "Catalog models are read-only. Custom entries may reference any local GGUF and optional MMProj file. Removing a custom entry never deletes either file. Automatic GGUF analysis requires Local AI Runtime 0.6.0 or later; every suggested setting remains editable.",
    ),
    "CustomModelDialog": (
        "Import custom GGUF model",
        "Analyze GGUF",
        "Automatic analysis:",
        "Analyzing GGUF metadata...",
        "Metadata read. Architecture: %1.",
        "MMProj automatically matched: %1.",
        "Review every suggested setting before saving.",
        "Several MMProj candidates are similarly likely; choose one manually.",
    ),
}.items():
    for source in sources:
        assert contexts[context_name].get(source), (context_name, source)
print("SIMPLIFIED CHINESE UI      PASS")

assert "Local AI Runtime stable version: **0.6.0**" in README
assert "自动分析不会加载模型" in README
assert "stage6e_guided_import_contract.py" in WORKFLOW
print("DOCUMENTATION / CI GATE    PASS")

for forbidden in (
    "llama-server-vulkan",
    "llama-server-cpu",
    "HunyuanOCR",
):
    assert forbidden not in UI, forbidden
print("CLIENT-ONLY SCOPE          PASS")

print()
print("STAGE 6E GUIDED IMPORT CONTRACT: PASS")
