#!/usr/bin/env python3

from pathlib import Path
import re

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")

checks = {
    "EDITOR HELP OVERLAY SUPPRESSED":
        "m_config.showHelp() && !m_windowEditorMode" in CPP,
    "MINIMUM EDITOR CANVAS":
        "editorCanvasMinimumSize(720, 420)" in CPP
        and "editorCanvasSize(" in CPP,
    "MINIMUM EDITOR WINDOW":
        "editorMinimumWindowSize(760, 460)" in CPP
        and "editor->setMinimumSize(boundedMinimumWindowSize);" in CPP,
    "SCROLLABLE LARGE IMAGE":
        "new QScrollArea(editor)" in CPP
        and "scrollArea->setWidgetResizable(false);" in CPP,
    "STAGE D IDENTITY":
        'flameshotWindowEditorStage", "3.0-D"' in CPP,
    "CORE ACTIONS PRESERVED":
        "case CaptureTool::REQ_OCR:" in CPP
        and "saveToClipboardGnomeWorkaround(pixmap(), this);" in CPP
        and "setWindowEditorSourceGeometry" in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<32} PASS")

# No mapped fullscreen Wayland surface role mutation may reappear.
stage_start = CPP.index(
    "if (m_context.fullscreen &&",
    CPP.index("void CaptureWidget::initSelection()"),
)
stage_end = CPP.index(
    "connect(m_selection, &SelectionWidget::visibilityChanged",
    stage_start,
)
stage_block = CPP[stage_start:stage_end]

for forbidden in (
    "setWindowFlags(",
    "showNormal(",
    "setParent(",
    "setWindowState(",
):
    assert forbidden not in stage_block, forbidden

print(f"{'NO MAPPED ROLE MUTATION':<32} PASS")

print("\nSTAGE 3.0-D UI HARDENING CONTRACT: PASS")
