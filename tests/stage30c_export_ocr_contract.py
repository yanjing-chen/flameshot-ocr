#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")

checks = {
    "SOURCE GEOMETRY API":
        "setWindowEditorSourceGeometry" in CPP
        and "setWindowEditorSourceGeometry" in HDR,
    "ORIGINAL EXPORT GEOMETRY":
        "windowEditorSourceGeometry(m_context.selection)" in CPP
        and "m_context.widgetOffset" in CPP,
    "ORIGINAL LAST REGION":
        "windowEditorLastRegion = m_selection->geometry();" in CPP
        and "windowEditorSourceScale" in CPP,
    "EDITOR GEOMETRY STORED":
        "m_windowEditorSourceGeometry = exportGeometry;" in CPP
        and "m_windowEditorLastRegion = lastRegion;" in CPP,
    "EDITOR EXPORT OVERRIDE":
        "m_windowEditorMode && m_windowEditorSourceGeometryValid" in CPP,
    "SAVE PATH PRESERVED":
        "Flameshot::instance()->exportCapture(" in CPP,
    "GNOME COPY WORKAROUND":
        "saveToClipboardGnomeWorkaround(pixmap(), this);" in CPP,
    "OCR REQUEST PATH":
        "case CaptureTool::REQ_OCR:" in CPP
        and "runOcr();" in CPP
        and "OcrRequestController" in CPP,
    "OCR DETACHED":
        "m_ocrDetached = true;" in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<30} PASS")

filter_start = CPP.index("if (m_windowEditorMode) {")
filter_end = CPP.index("\n    if ((m_context.request.tasks()", filter_start)
filter_block = CPP[filter_start:filter_end]

for enabled in (
    "TYPE_SAVE",
    "TYPE_COPY",
    "TYPE_PIN",
    "TYPE_OCR",
):
    forbidden = f"buttonList->removeOne(CaptureTool::{enabled});"
    assert forbidden not in filter_block, enabled

print(f"{'CORE ACTION TOOLS ENABLED':<30} PASS")

assert "buttonList->removeOne(CaptureTool::TYPE_OPEN_APP);" in filter_block
assert "buttonList->removeOne(CaptureTool::TYPE_IMAGEUPLOADER);" in filter_block
print(f"{'EXTERNAL ACTIONS DEFERRED':<30} PASS")

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

print(f"{'NO MAPPED ROLE MUTATION':<30} PASS")

assert 'flameshotWindowEditorStage", "3.0-C"' in CPP
print(f"{'STAGE IDENTITY':<30} PASS")

print("\nSTAGE 3.0-C EXPORT/OCR CONTRACT: PASS")
