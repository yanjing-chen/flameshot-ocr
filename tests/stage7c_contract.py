#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
SOURCE_CMAKE = (ROOT / "src/CMakeLists.txt").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
CONFIG = (ROOT / "src/utils/confighandler.cpp").read_text(encoding="utf-8")
CONFIG_HEADER = (ROOT / "src/utils/confighandler.h").read_text(encoding="utf-8")
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
MANAGER = (ROOT / "src/ocr/ocrmanager.cpp").read_text(encoding="utf-8")
CONTROLLER = (ROOT / "src/ocr/ocrrequestcontroller.cpp").read_text(
    encoding="utf-8"
)
CONTROLLER_HEADER = (ROOT / "src/ocr/ocrrequestcontroller.h").read_text(
    encoding="utf-8"
)
CAPTURE = (ROOT / "src/widgets/capture/capturewidget.cpp").read_text(
    encoding="utf-8"
)
CAPTURE_HEADER = (ROOT / "src/widgets/capture/capturewidget.h").read_text(
    encoding="utf-8"
)
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"

assert "set(FLAMESHOT_OCR_VERSION 2.8)" in CMAKE
assert 'OCR_VERSION: "2.8"' in WORKFLOW
assert "stage7c_contract.py" in WORKFLOW
assert "ocr/ocrrequestcontroller.cpp" in SOURCE_CMAKE
assert "ocr/ocrrequestcontroller.h" in SOURCE_CMAKE
print("V2.8 DEVELOPMENT IDENTITY      PASS")

for token in (
    'OPTION("ocrOutputFormat"',
    'String             ( "plain"',
    "ocrOutputFormat",
):
    assert token in CONFIG or token in CONFIG_HEADER, token
for token in (
    'tr("Output format:")',
    'tr("Plain text")',
    'tr("Markdown")',
    'QStringLiteral("markdown")',
):
    assert token in UI, token
for token in (
    "return Markdown only",
    "Preserve ",
    "headings, paragraphs, lists, tables, code blocks",
    "Do not add explanations",
):
    assert token in MANAGER, token
print("PLAIN / MARKDOWN OCR MODE       PASS")

for token in (
    "QElapsedTimer",
    'QStringLiteral("predicted_per_second")',
    "charactersPerSecond",
    'ocrTr("Elapsed: %1 s | %2 characters | %3 characters/s")',
    'ocrTr("Model speed: %1 token/s")',
):
    assert token in CONTROLLER or token in CONTROLLER_HEADER, token
print("OCR ELAPSED TIME AND SPEED      PASS")

for token in (
    "QSaveFile",
    "QFileDialog::getSaveFileName",
    'QStringLiteral("md")',
    'ocrTr("Save Markdown...")',
    'ocrTr("Save Text...")',
):
    assert token in CONTROLLER, token
print("MARKDOWN / TEXT SAVE            PASS")

assert "new OcrRequestController(selected, qApp)" in CAPTURE
assert "m_ocrDetached = true" in CAPTURE
assert "else if (!m_ocrDetached)" in CAPTURE
assert "bool m_ocrDetached" in CAPTURE_HEADER
assert "Qt::NonModal" in CONTROLLER
assert "Qt::WindowModal" not in CONTROLLER
assert "Qt::WindowStaysOnTopHint" not in CONTROLLER
assert "showOcrResult" not in CAPTURE
print("DETACHED NON-MODAL OCR WORKFLOW PASS")

# Runtime and CUDA management remain on the existing HTTP boundary. Stage 7C
# must not add process ownership or embedded backend payloads to the client.
for forbidden in (
    "llama-server-cuda",
    "libcublas",
    "libcudart",
):
    assert forbidden not in CONTROLLER, forbidden
assert "QProcess" not in CONTROLLER
print("RUNTIME 0.8 CLIENT BOUNDARY     PASS")

tree = ET.parse(TRANSLATIONS)
contexts = {
    context.findtext("name"): {
        message.findtext("source"): message.findtext("translation")
        for message in context.findall("message")
    }
    for context in tree.getroot().findall("context")
}
for source in (
    "Output format:",
    "Plain text",
    "Markdown",
):
    assert contexts["OcrConf"].get(source), source
for source in (
    "OCR Markdown Result",
    "Elapsed: %1 s | %2 characters | %3 characters/s",
    "Model speed: %1 token/s",
    "Copy Markdown",
    "Save Markdown...",
    "Save OCR result",
):
    assert contexts["CaptureWidget"].get(source), source
print("SIMPLIFIED CHINESE UI           PASS")

print()
print("STAGE 7C V2.8 CONTRACT: PASS")
