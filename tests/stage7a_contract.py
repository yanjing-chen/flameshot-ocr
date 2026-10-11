#!/usr/bin/env python3

from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
UI_HEADER = (ROOT / "src/config/ocrconf.h").read_text(encoding="utf-8")
CONFIG = (ROOT / "src/utils/confighandler.cpp").read_text(encoding="utf-8")
CONFIG_HEADER = (ROOT / "src/utils/confighandler.h").read_text(encoding="utf-8")
CAPTURE = (ROOT / "src/widgets/capture/capturewidget.cpp").read_text(
    encoding="utf-8"
)
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"


assert re.search(r"set\(FLAMESHOT_OCR_VERSION 2\.[67]\)", CMAKE)
assert re.search(r'OCR_VERSION: "2\.[67]"', WORKFLOW)
assert "stage7a_contract.py" in WORKFLOW
print("V2.6+ RELEASE IDENTITY         PASS")

for token in (
    'QStringLiteral("/v1/runtime/preferences")',
    'QStringLiteral("preload_model")',
    "m_preloadOcrModel",
    "refreshPreloadPreference",
    "setPreloadPreference",
    "Off by default.",
    "first OCR request",
    "next Runtime start",
    'QStringLiteral("/v1/models/remove")',
    'tr("Remove entry")',
):
    assert token in UI or token in UI_HEADER, token
print("OCR STARTUP PRELOAD UI         PASS")

CONFIG_WINDOW = (ROOT / "src/config/configwindow.cpp").read_text(
    encoding="utf-8"
)
assert "preloadSavePending()" in CONFIG_WINDOW
assert "event->ignore()" in CONFIG_WINDOW
print("PRELOAD SAVE COMPLETION GUARD  PASS")

assert 'OPTION("ocrShowUnavailableCatalogModels",Bool' in CONFIG
assert "( false" in re.search(
    r'OPTION\("ocrShowUnavailableCatalogModels".*', CONFIG
).group(0)
assert "ocrShowUnavailableCatalogModels" in CONFIG_HEADER
assert "m_showUnavailableModels->isChecked()" in UI
assert 'source != QStringLiteral("custom")' in UI
assert "id != activeModel" in UI
assert "Showing %1 of %2 Runtime model(s)." in UI
print("MISSING CATALOG FILTER          PASS")

result_body = re.search(
    r"void CaptureWidget::showOcrResult\(.*?\n}\n\nvoid CaptureWidget::drawErrorMessage",
    CAPTURE,
    re.DOTALL,
)
assert result_body, "showOcrResult body"
result_source = result_body.group(0)
assert "new QDialog(nullptr" in result_source
assert "Qt::NonModal" in result_source
assert "Qt::WindowModal" not in result_source
assert "OcrResultDragHandle" in result_source
assert "startSystemMove()" in CAPTURE
print("WAYLAND RESULT WINDOW DRAG     PASS")

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
        "Load this OCR model when Local AI Runtime starts",
        "Show catalog models whose files are not installed",
        "Off by default. The model will load automatically on the first OCR request.",
        "Saved. The Runtime will stay idle until the first model request.",
        "Showing %1 of %2 Runtime model(s). Custom entries are stored by Local AI Runtime.",
        "Remove entry",
        "Delete catalog model files",
        "Catalog model files deleted.",
    ),
    "ConfigWindow": (
        "Saving OCR settings",
        "Please wait until the startup loading preference has been saved, then close the window again.",
    ),
    "CaptureWidget": (
        "OCR Result — drag here to move the window",
        "Drag this bar to move the OCR result window.",
    ),
}.items():
    for source in sources:
        assert contexts[context_name].get(source), (context_name, source)

for source in (
    "Remove the custom model registration “%1”?\n\n"
    "The external GGUF and MMProj files will remain on disk.",
    "Delete all Runtime-managed files for “%1”?\n\n"
    "If this model is running, it will be unloaded first. "
    "You can download it again later.",
):
    assert contexts["OcrConf"].get(source), source
    assert "\\n" not in source
print("SIMPLIFIED CHINESE UI          PASS")

for forbidden in (
    "llama-server-vulkan",
    "llama-server-cpu",
    "HunyuanOCR",
):
    assert forbidden not in UI, forbidden
    assert forbidden not in UI_HEADER, forbidden
print("CLIENT-ONLY BOUNDARY           PASS")

print()
print("STAGE 7A CONTRACT: PASS")
