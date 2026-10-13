#!/usr/bin/env python3

from pathlib import Path
import re

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
OCR_CPP = Path("src/ocr/ocrrequestcontroller.cpp").read_text(encoding="utf-8")

checks = {
    "EDITOR HELP OVERLAY SUPPRESSED":
        "m_config.showHelp() && !m_windowEditorMode" in CPP,
    "IMAGE-SIZED EDITOR CANVAS":
        "resize(logicalImageSize);" in CPP
        and "editorCanvasMinimumSize" not in CPP
        and "editorCanvasSize(" not in CPP,
    "MINIMUM EDITOR WINDOW":
        "editorMinimumWindowSize(760, 520)" in CPP
        and "editor->setMinimumSize(boundedMinimumWindowSize);" in CPP,
    "SCROLLABLE LARGE IMAGE":
        "new QScrollArea(editor)" in CPP
        and "scrollArea->setWidgetResizable(false);" in CPP,
    "STAGE D IDENTITY":
        'flameshotWindowEditorStage", "3.0-D.2"' in CPP,
    "CORE ACTIONS PRESERVED":
        "case CaptureTool::REQ_OCR:" in CPP
        and "saveToClipboardGnomeWorkaround(pixmap(), this);" in CPP
        and "setWindowEditorSourceGeometry" in CPP,
    "OCR HINT BUILD REGRESSION ABSENT":
        "ocrQString" not in OCR_CPP,
    "OCR MOVE HINTS REMOVED":
        "OCR Result — drag here to move the window" not in OCR_CPP
        and "Drag this bar to move the OCR result window." not in OCR_CPP,
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
    if forbidden == "setParent(":
        allowed_child_reparent = "canvas->m_panel->setParent(settingsDock);"
        if allowed_child_reparent in stage_block:
            assert stage_block.count(allowed_child_reparent) == 1, "duplicate child reparent"
            assert stage_block.count("setParent(") == 1, "unexpected reparent"
            continue
    assert forbidden not in stage_block, forbidden

print(f"{'NO MAPPED ROLE MUTATION':<32} PASS")

print("\nSTAGE 3.0-D UI HARDENING CONTRACT: PASS")
