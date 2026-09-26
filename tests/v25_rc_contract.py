#!/usr/bin/env python3

from pathlib import Path
import re
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "src/config/ocrconf.cpp").read_text(encoding="utf-8")
TRANSLATIONS = ROOT / "data/translations/Internationalization_zh_CN.ts"

assert 'dialogTr("Supports temperature parameter")' in UI
assert 'dialogTr("Temperature")' not in UI
assert 'QStringLiteral("temperature")' in UI
print("TEMPERATURE CAPABILITY LABEL    PASS")

for token in (
    "genericParameterName",
    'QStringLiteral("^[0-9]+(?:\\\\.[0-9]+)?[bBmM]$")',
    "completeBaseName()",
    'QStringLiteral("[^a-z0-9]+")',
    'suggested.insert(QStringLiteral("display_name"),',
    'suggested.insert(QStringLiteral("id"), filenameId)',
):
    assert token in UI, token
assert UI.index("genericParameterName") < UI.index("load(suggested);")
assert 'suggested.value(QStringLiteral("display_name"))' in UI
assert 'suggested.value(QStringLiteral("name"))' not in UI
print("GENERIC DISPLAY NAME FALLBACK  PASS")

tree = ET.parse(TRANSLATIONS)
contexts = {
    context.findtext("name"): {
        message.findtext("source"): message.findtext("translation")
        for message in context.findall("message")
    }
    for context in tree.getroot().findall("context")
}
assert contexts["CustomModelDialog"]["Supports temperature parameter"] == "支持温度参数"
assert "Temperature" not in contexts["CustomModelDialog"]
print("SIMPLIFIED CHINESE LABEL       PASS")

inspect_body = re.search(
    r"void inspectModel\(\).*?\n    void applyInspection",
    UI,
    re.DOTALL,
)
assert inspect_body
assert 'QStringLiteral("/v1/models/custom")' not in inspect_body.group(0)
assert "m_saveButton->setEnabled(false)" in inspect_body.group(0)
print("READ-ONLY INSPECTION GUARD     PASS")

print()
print("V2.5 RC CONTRACT: PASS")


WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
GIO_HOOK = ROOT / "packaging/appimage/flameshot-gio-isolation.sh"
hook = GIO_HOOK.read_text(encoding="utf-8")
assert "unset GIO_EXTRA_MODULES" in hook
assert 'GIO_MODULE_DIR="$APPDIR/usr/lib/gio/modules"' in hook
assert "flameshot-gio-isolation.sh" in WORKFLOW
assert 'test -d "$APPROOT/usr/lib/gio/modules"' in WORKFLOW
assert 'apprun-hooks/20-flameshot-gio-isolation.sh' in WORKFLOW
print("APPIMAGE HOST GIO ISOLATION    PASS")
