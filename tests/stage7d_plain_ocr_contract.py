#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

MANAGER = read("src/ocr/ocrmanager.cpp")
MANAGER_HEADER = read("src/ocr/ocrmanager.h")
UI = read("src/config/ocrconf.cpp")
UI_HEADER = read("src/config/ocrconf.h")
CONFIG = read("src/utils/confighandler.cpp")
CONFIG_HEADER = read("src/utils/confighandler.h")
CONTROLLER = read("src/ocr/ocrrequestcontroller.cpp")
CONTROLLER_HEADER = read("src/ocr/ocrrequestcontroller.h")
WORKFLOW = read(".github/workflows/build.yml")
README = read("README.md")
CHANGELOG = read("CHANGELOG_OCR.md")
ZH_CN = read("data/translations/Internationalization_zh_CN.ts")

assert 'return QStringLiteral("OCR:");' in MANAGER
assert "outputFormat" not in MANAGER
assert "outputFormat" not in MANAGER_HEADER

print("NATIVE OCR PROMPT ONLY          PASS")

assert "m_outputFormat" not in UI
assert "m_outputFormat" not in UI_HEADER
assert 'tr("Output format:")' not in UI
assert 'tr("Markdown")' not in UI

print("MARKDOWN SETTINGS UI REMOVED    PASS")

assert CONFIG.count('OPTION("ocrOutputFormat"') == 1
assert "Legacy v2.8 compatibility key" in CONFIG
assert "ocrOutputFormat" not in CONFIG_HEADER

print("LEGACY CONFIG COMPATIBILITY     PASS")

for forbidden in (
    "OCR Markdown Result",
    "Copy Markdown",
    "Save Markdown...",
    "Markdown files (*.md)",
    "bool markdown",
    "m_outputFormat",
):
    assert forbidden not in CONTROLLER, forbidden

assert 'ocrTr("Save Text...")' in CONTROLLER
assert 'QStringLiteral(".txt")' in CONTROLLER

assert "bool markdown" not in CONTROLLER_HEADER
assert "m_outputFormat" not in CONTROLLER_HEADER

print("PLAIN TEXT RESULT ONLY          PASS")

assert "stage7d_plain_ocr_contract.py" in WORKFLOW

print("ACTION CONTRACT GATE            PASS")

for obsolete in (
    "<source>Output format:</source>",
    "<source>Plain text</source>",
    "<source>Markdown</source>",
    "<source>OCR Markdown Result</source>",
    "<source>Copy Markdown</source>",
    "<source>Save Markdown...</source>",
    "<source>Markdown files (*.md);;All files (*)</source>",
):
    assert obsolete not in ZH_CN, obsolete

print("OBSOLETE MARKDOWN UI TEXT       NONE")

development_identity = (
    "development version: **v2.9**" in README
    and "Latest stable Flameshot OCR version: **v2.8**" in README
)

release_identity = (
    "Current stable Flameshot OCR version: **v2.9**" in README
)

assert development_identity or release_identity
assert "plain-text or Markdown OCR output" not in README
assert "Markdown output can be saved directly" not in README
assert "v2.9 已移除 Markdown" in README

print("README CURRENT STATE            PASS")

assert "Markdown OCR output has been removed" in CHANGELOG
assert "## v2.8 — Stage 7C development" in CHANGELOG
assert "- Plain-text and Markdown OCR output modes." in CHANGELOG

print("CHANGELOG HISTORY PRESERVED     PASS")

print()
print("STAGE 7D PLAIN OCR CONTRACT: PASS")
