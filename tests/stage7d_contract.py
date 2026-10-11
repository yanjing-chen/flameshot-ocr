#!/usr/bin/env python3

from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
SOURCE_CMAKE = (ROOT / "src/CMakeLists.txt").read_text(encoding="utf-8")
TOOLS_CMAKE = (ROOT / "src/tools/CMakeLists.txt").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
CONFIG = (ROOT / "src/utils/confighandler.cpp").read_text(encoding="utf-8")
CONFIG_HEADER = (ROOT / "src/utils/confighandler.h").read_text(encoding="utf-8")
OCR_UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
OCR_MANAGER = (ROOT / "src/ocr/ocrmanager.cpp").read_text(encoding="utf-8")
CAPTURE = (ROOT / "src/widgets/capture/capturewidget.cpp").read_text(
    encoding="utf-8"
)
CAPTURE_HEADER = (ROOT / "src/widgets/capture/capturewidget.h").read_text(
    encoding="utf-8"
)
TOOL_API = (ROOT / "src/tools/capturetool.h").read_text(encoding="utf-8")
TOOL_FACTORY = (ROOT / "src/tools/toolfactory.cpp").read_text(encoding="utf-8")
BUTTONS = (ROOT / "src/widgets/capture/capturetoolbutton.cpp").read_text(
    encoding="utf-8"
)
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"

assert "set(FLAMESHOT_OCR_VERSION 2.9)" in CMAKE
assert 'OCR_VERSION: "2.9"' in WORKFLOW
assert "stage7d_contract.py" in WORKFLOW
print("V2.9 DEVELOPMENT IDENTITY          PASS")

for token in (
    "TYPE_WINDOWED_EDITOR = 26",
    "REQ_WINDOWED_EDITOR",
):
    assert token in TOOL_API, token
assert "WindowedEditorTool" in TOOL_FACTORY
assert "windowededitor/windowededitortool.cpp" in TOOLS_CMAKE
assert "CaptureTool::TYPE_WINDOWED_EDITOR" in BUTTONS
assert 'SHORTCUT("TYPE_WINDOWED_EDITOR"' in CONFIG
assert 'OPTION("stage7dWindowedEditorMigrationDone"' in CONFIG
assert "stage7dWindowedEditorMigrationDone" in CONFIG_HEADER
assert "m_config.setButtons(visibleButtonTypes)" in CAPTURE
print("WINDOWED EDITOR ACTION             PASS")

for token in (
    "continueInWindowedEditor",
    "m_windowedEditor",
    "showMaximized()",
    "Qt::WindowMinMaxButtonsHint",
    "m_buttonHandler->updatePosition(m_selection->geometry())",
):
    assert token in CAPTURE or token in CAPTURE_HEADER, token

# The same CaptureWidget and QUndoStack stay alive. Stage 7D must not flatten
# the screenshot, replace the object list, or create a second capture widget.
windowed_body = CAPTURE.split(
    "void CaptureWidget::continueInWindowedEditor()", 1
)[1].split("void CaptureWidget::", 1)[0]
for forbidden in (
    "new CaptureWidget",
    "m_captureToolObjects.clear",
    "m_undoStack.clear",
    "m_context.screenshot = pixmap()",
):
    assert forbidden not in windowed_body, forbidden
assert "m_undoStack.count() > 0" in CAPTURE
assert 'tr("Discard screenshot edits?")' in CAPTURE
print("FULL EDIT SESSION PRESERVATION     PASS")

for token in (
    'OPTION("ocrMarkdownExtraPromptEnabled"',
    'OPTION("ocrMarkdownExtraPrompt"',
    "ocrMarkdownExtraPromptEnabled",
    "ocrMarkdownExtraPrompt",
):
    assert token in CONFIG or token in CONFIG_HEADER, token
for token in (
    'tr("Markdown advanced settings")',
    'tr("Enable additional Markdown instructions")',
    'tr("Restore default")',
    "updateMarkdownPromptControls",
):
    assert token in OCR_UI, token
for token in (
    '"OCR:\\nRecognize all visible text and return Markdown only.',
    "clearly bold text with **text**",
    "clearly italic text with ",
    '"*text*.',
    "Additional formatting instructions",
    "ocrMarkdownExtraPromptEnabled()",
    ".left(4000)",
):
    assert token in OCR_MANAGER, token
print("MARKDOWN PROMPT OVERRIDES          PASS")

# Runtime and model ownership remain unchanged.
for forbidden in (
    "QProcess",
    "llama-server-cuda",
    "libcublas",
    "libcudart",
):
    assert forbidden not in OCR_MANAGER, forbidden
assert "editable" in OCR_UI
print("RUNTIME 0.8 CLIENT BOUNDARY        PASS")

tree = ET.parse(TRANSLATIONS)
contexts = {
    context.findtext("name"): {
        message.findtext("source"): message.findtext("translation")
        for message in context.findall("message")
    }
    for context in tree.getroot().findall("context")
}
for source in (
    "Screenshot Editor",
    "Select a screenshot area before continuing in the editor.",
    "Flameshot OCR — Screenshot Editor",
    "Discard screenshot edits?",
    "This screenshot has unsaved edits. Close the editor and discard them?",
):
    assert contexts["CaptureWidget"].get(source), source
for source in (
    "Markdown advanced settings",
    "Built-in recognition rules (always active)",
    "Enable additional Markdown instructions",
    "Restore default",
):
    assert contexts["OcrConf"].get(source), source
for source in (
    "Continue in Editor",
    "Continue editing in a normal window",
):
    assert contexts["WindowedEditorTool"].get(source), source
for source in (
    "&Take Screenshot",
    "&Open Launcher",
    "&Configuration",
    "&About",
    "&Quit",
    "&Latest Uploads",
    "&Open Save Path",
):
    translation = contexts["TrayIcon"].get(source)
    assert translation and "&" not in translation, (source, translation)
for source in (
    "Insecure Pixelate",
    "Capture active monitor in X11 and Windows (skip monitor selection)",
    "Use legacy X11 screenshot method",
):
    assert contexts["GeneralConf"].get(source), source
for source in (
    "UI language",
    "Configuration",
    "Flameshot must be restarted to apply these changes!",
):
    assert contexts["VisualsEditor"].get(source), source
print("SIMPLIFIED CHINESE CLEANUP         PASS")

print()
print("STAGE 7D V2.9 CONTRACT: PASS")
