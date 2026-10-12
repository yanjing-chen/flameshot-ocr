#!/usr/bin/env python3

from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
VALIDATED_7DC = "799871ee"

tree = ET.parse(
    ROOT / "data/translations/Internationalization_zh_CN.ts"
)

contexts = {}

for context in tree.getroot().findall("context"):
    name = context.findtext("name")
    entries = {}

    for message in context.findall("message"):
        source = message.findtext("source")
        translation = message.findtext("translation")

        if source is not None:
            entries[source] = translation or ""

    contexts[name] = entries

tray = contexts.get("TrayIcon", {})

for source in (
    "&Take Screenshot",
    "&Open Launcher",
    "&Configuration",
    "&About",
    "&Quit",
):
    value = tray.get(source, "")
    assert value
    assert not re.search(r"\(&[A-Za-z]\)", value)
    assert "&" not in value

print("TRAY MNEMONIC CLEANUP           PASS")

general = contexts.get("GeneralConf", {})

pixelate = general.get("Insecure Pixelate", "")
assert pixelate
assert "不安全" in pixelate
assert "像素" in pixelate

print("GENERAL SETTINGS ZH_CN          PASS")

# Markdown removal may change OCR files, but the failed window-editor
# experiment must still never enter this branch.
for path in (
    "src/widgets/capture/capturewidget.cpp",
    "src/widgets/capture/capturewidget.h",
):
    result = subprocess.run(
        ["git", "diff", "--quiet", VALIDATED_7DC, "--", path],
        cwd=ROOT,
    )
    assert result.returncode == 0, path

print("NO 7D-A WINDOW CODE IMPORTED    PASS")

print()
print("STAGE 7D-C LOCALIZATION CONTRACT: PASS")
