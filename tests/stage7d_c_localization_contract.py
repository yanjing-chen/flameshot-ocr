#!/usr/bin/env python3

from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
BASE = "e4b8f92ba3daca979150c364f0516572e2255a47"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


CMAKE = read("CMakeLists.txt")
WORKFLOW = read(".github/workflows/build.yml")
OCRCONF = read("src/config/ocrconf.cpp")
INSTALLER = read("src/ocr/localairuntimeinstaller.cpp")

assert "set(FLAMESHOT_OCR_VERSION 2.9)" in CMAKE
assert 'OCR_VERSION: "2.9"' in WORKFLOW
assert "python3 tests/stage7d_c_localization_contract.py" in WORKFLOW
assert "Flameshot-OCR/2.9" in OCRCONF
assert "Flameshot-OCR/2.9" in INSTALLER

print("V2.9 DEVELOPMENT IDENTITY       PASS")

# ------------------------------------------------------------
# Localization checks
# ------------------------------------------------------------
translation_path = (
    ROOT / "data/translations/Internationalization_zh_CN.ts"
)

tree = ET.parse(translation_path)

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
    assert value, ("missing tray translation", source)
    assert not re.search(r"\(&[A-Za-z]\)", value), (source, value)
    assert "&" not in value, (source, value)

print("TRAY MNEMONIC CLEANUP           PASS")

general = contexts.get("GeneralConf", {})

pixelate = general.get("Insecure Pixelate", "")
assert pixelate
assert "不安全" in pixelate
assert "像素" in pixelate

for source in (
    "Capture active monitor in X11 and Windows (skip monitor selection)",
    "Use native fullscreen for capture overlay",
    "Use legacy X11 screenshot method",
):
    assert general.get(source, ""), source

print("GENERAL SETTINGS ZH_CN          PASS")

for context_name, source in (
    ("OcrConf", "Testing AI endpoint..."),
    ("VisualsEditor", "UI language"),
    ("VisualsEditor", "Configuration"),
):
    assert contexts.get(context_name, {}).get(source, ""), (
        context_name,
        source,
    )

print("EXISTING UI TRANSLATIONS        PASS")

# ------------------------------------------------------------
# Stage 7D-C isolation:
# Only localization, version identity and contract-maintenance
# files may differ from the clean v2.8 release.
# ------------------------------------------------------------
tracked = subprocess.check_output(
    ["git", "diff", "--name-only", BASE],
    cwd=ROOT,
    text=True,
).splitlines()

untracked = subprocess.check_output(
    ["git", "ls-files", "--others", "--exclude-standard"],
    cwd=ROOT,
    text=True,
).splitlines()

changed = set(tracked) | set(untracked)

allowed = {
    ".github/workflows/build.yml",
    "CHANGELOG_OCR.md",
    "CMakeLists.txt",
    "data/translations/Internationalization_zh_CN.ts",
    "src/config/ocrconf.cpp",
    "src/ocr/localairuntimeinstaller.cpp",
    "tests/stage7a_contract.py",
    "tests/stage7b_contract.py",
    "tests/stage7c_contract.py",
    "tests/stage7d_c_localization_contract.py",
}

unexpected = sorted(changed - allowed)
assert not unexpected, unexpected

print("7D-C CHANGE ISOLATION           PASS")

# These are the sensitive implementation areas for the postponed
# Stage 7D-A window editor and Stage 7D-B Markdown work. They must
# remain byte-identical to the clean v2.8 base in Stage 7D-C.
for path in (
    "src/widgets/capture/capturewidget.cpp",
    "src/widgets/capture/capturewidget.h",
    "src/ocr/ocrmanager.cpp",
    "src/utils/confighandler.cpp",
    "src/utils/confighandler.h",
):
    result = subprocess.run(
        ["git", "diff", "--quiet", BASE, "--", path],
        cwd=ROOT,
    )
    assert result.returncode == 0, path

print("NO 7D-A WINDOW CODE IMPORTED    PASS")
print("NO 7D-B MARKDOWN CODE IMPORTED  PASS")

print()
print("STAGE 7D-C LOCALIZATION CONTRACT: PASS")
