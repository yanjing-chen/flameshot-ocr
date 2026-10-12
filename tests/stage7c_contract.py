#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

CMAKE = read("CMakeLists.txt")
WORKFLOW = read(".github/workflows/build.yml")
CONFIG = read("src/utils/confighandler.cpp")
CONFIG_HEADER = read("src/utils/confighandler.h")
UI = read("src/config/ocrconf.cpp")
UI_HEADER = read("src/config/ocrconf.h")
MANAGER = read("src/ocr/ocrmanager.cpp")
MANAGER_HEADER = read("src/ocr/ocrmanager.h")
CONTROLLER = read("src/ocr/ocrrequestcontroller.cpp")
CONTROLLER_HEADER = read("src/ocr/ocrrequestcontroller.h")
CAPTURE = read("src/widgets/capture/capturewidget.cpp")
CAPTURE_HEADER = read("src/widgets/capture/capturewidget.h")

assert "set(FLAMESHOT_OCR_VERSION 2.9)" in CMAKE
assert 'OCR_VERSION: "2.9"' in WORKFLOW

print("V2.9 DEVELOPMENT IDENTITY       PASS")

assert 'return QStringLiteral("OCR:");' in MANAGER
assert "outputFormat()" not in MANAGER
assert "outputFormat() const" not in MANAGER_HEADER

for forbidden in (
    "m_outputFormat",
    'tr("Output format:")',
    'tr("Markdown")',
):
    assert forbidden not in UI, forbidden

assert "m_outputFormat" not in UI_HEADER

assert "ocrOutputFormat" not in CONFIG_HEADER
assert CONFIG.count('OPTION("ocrOutputFormat"') == 1
assert "Legacy v2.8 compatibility key" in CONFIG

print("PLAIN-TEXT-ONLY OCR             PASS")

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
    'ocrTr("Save Text...")',
    'QStringLiteral(".txt")',
):
    assert token in CONTROLLER, token

for forbidden in (
    "OCR Markdown Result",
    "Copy Markdown",
    "Save Markdown...",
    "Markdown files (*.md)",
    "bool markdown",
    "m_outputFormat",
):
    assert forbidden not in CONTROLLER, forbidden

assert "bool markdown" not in CONTROLLER_HEADER
assert "m_outputFormat" not in CONTROLLER_HEADER

print("PLAIN TEXT RESULT AND SAVE      PASS")

assert "new OcrRequestController(selected, qApp)" in CAPTURE
assert "m_ocrDetached = true" in CAPTURE
assert "else if (!m_ocrDetached)" in CAPTURE
assert "bool m_ocrDetached" in CAPTURE_HEADER
assert "Qt::NonModal" in CONTROLLER
assert "Qt::WindowModal" not in CONTROLLER
assert "Qt::WindowStaysOnTopHint" not in CONTROLLER

print("DETACHED NON-MODAL OCR WORKFLOW PASS")

print()
print("STAGE 7C COMPATIBILITY CONTRACT: PASS")
