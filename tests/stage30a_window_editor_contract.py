#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")

required = {
    "NORMAL WINDOW TYPE": 'new QWidget(nullptr, Qt::Window)',
    "SCROLLABLE PREVIEW": "new QScrollArea(editor)",
    "SELECTED PIXMAP": "const QPixmap selected = pixmap();",
    "NO ROLE MUTATION": 'setProperty("windowEditorPreviewDetached", true)',
    "AUTO OPEN ARM": 'setProperty("windowEditorAutoOpenEnabled", true)',
    "QUEUED NORMAL SHOW": "QTimer::singleShot(0, editor",
    "WAYLAND SURFACE CLOSED": "close();",
    "LEGACY OCR DETACH KEPT": "!m_ocrDetached",
}

for label, needle in required.items():
    assert needle in CPP, f"{label}: missing {needle!r}"
    print(f"{label:<28} PASS")

emit_pos = CPP.index("emit m_selection->geometrySettled();")
arm_pos = CPP.index('setProperty("windowEditorAutoOpenEnabled", true)')
assert arm_pos > emit_pos
print(f"{'INITIAL-SELECTION GUARD':<28} PASS")

stage_start = CPP.index("// Stage 3.0-A:")
stage_end = CPP.index("// Historical Flameshot path.", stage_start)
stage_block = CPP[stage_start:stage_end]
for forbidden in (
    "setWindowFlags(",
    "setParent(",
    "showNormal()",
):
    assert forbidden not in stage_block, (
        f"Forbidden mapped-surface mutation in Stage 3.0-A: {forbidden}"
    )
print(f"{'NO MAPPED-SURFACE ROLE CHANGE':<28} PASS")

assert "m_captureDone = true" not in stage_block
print(f"{'NO PREMATURE EXPORT':<28} PASS")

assert '!property("windowEditorPreviewDetached").toBool()' in CPP
print(f"{'HANDOFF DESTRUCTOR GUARD':<28} PASS")

print("\nSTAGE 3.0-A WINDOW EDITOR CONTRACT: PASS")
