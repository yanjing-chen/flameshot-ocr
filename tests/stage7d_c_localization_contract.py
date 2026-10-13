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


# Stage 3.0+ keeps the old 7D-A Wayland safety rule scoped to the actual
# fullscreen-selector handoff. Ordinary child widgets may legitimately use
# setParent(), but the already-mapped fullscreen CaptureWidget must never be
# reparented or have its top-level window role mutated.
capture_cpp = (Path(__file__).resolve().parents[1] / 'src/widgets/capture/capturewidget.cpp').read_text(encoding='utf-8')
stage_start = capture_cpp.index(
    "if (m_context.fullscreen &&",
    capture_cpp.index("void CaptureWidget::initSelection()"),
)
stage_end = capture_cpp.index(
    "connect(m_selection, &SelectionWidget::visibilityChanged",
    stage_start,
)
stage_block = capture_cpp[stage_start:stage_end]

for forbidden in (
    "setWindowFlags(",
    "showNormal(",
    "setParent(",
    "setWindowState(",
):
    if forbidden == "setParent(":
        allowed_child_reparent = "canvas->m_panel->setParent(settingsDock);"
        if allowed_child_reparent in stage_block:
            assert stage_block.count(allowed_child_reparent) == 1, "duplicate child reparent"
            assert stage_block.count("setParent(") == 1, "unexpected reparent"
            continue
    assert forbidden not in stage_block, forbidden

print('LEGACY 7D-A ROLE MUTATION ABSENT PASS')

print("TRAY MNEMONIC CLEANUP           PASS")

general = contexts.get("GeneralConf", {})

pixelate = general.get("Insecure Pixelate", "")
assert pixelate
assert "不安全" in pixelate
assert "像素" in pixelate

print("GENERAL SETTINGS ZH_CN          PASS")

# v2.9 deliberately excluded the failed Stage 7D-A implementation.
# v3.0 now introduces a different architecture, so capturewidget.cpp is
# expected to differ from the validated 7D-C commit. Keep the historical
# safety guard by inspecting only lines newly added after the 7D-C baseline
# and rejecting the old mapped-surface mutation techniques that failed on
# GNOME Wayland.
result = subprocess.run(
    [
        "git",
        "diff",
        "--unified=0",
        VALIDATED_7DC,
        "--",
        "src/widgets/capture/capturewidget.cpp",
        "src/widgets/capture/capturewidget.h",
    ],
    cwd=ROOT,
    text=True,
    capture_output=True,
)
assert result.returncode == 0

added_lines = []
for line in result.stdout.splitlines():
    if line.startswith("+++"):
        continue
    if line.startswith("+"):
        added_lines.append(line[1:])

added_text = "\n".join(added_lines)

for forbidden in (
    "showNormal(",
    "setWindowState(",
    "stage7dWindowedEditorMigrationDone",
):
    assert forbidden not in added_text, forbidden

# setWindowFlags() exists in original Flameshot capture setup, so reject it
# only when newly added after the validated 7D-C baseline.
assert "setWindowFlags(" not in added_text

print("LEGACY 7D-A ROLE MUTATION ABSENT PASS")

print()
print("STAGE 7D-C LOCALIZATION CONTRACT: PASS")
