#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")

checks = {
    "INITIAL SCREENSHOT ARG":
        "const QPixmap& initialScreenshot" in CPP
        and "const QPixmap& initialScreenshot" in HDR,
    "WINDOW EDITOR MODE":
        "bool windowEditorMode" in CPP
        and "bool m_windowEditorMode" in HDR,
    "EDITOR SCREENSHOT CONTEXT":
        "m_context.screenshot = initialScreenshot;" in CPP
        and "m_context.origScreenshot = initialScreenshot;" in CPP,
    "REUSE CAPTUREWIDGET":
        "auto* canvas = new CaptureWidget(editorRequest," in CPP,
    "SCROLLABLE CANVAS":
        "scrollArea->setWidget(canvas);" in CPP,
    "FULL IMAGE SELECTION":
        "editorRequest.setInitialSelection(selected.rect());" in CPP,
    "FULLSCREEN HANDOFF ONLY":
        "if (m_context.fullscreen &&" in CPP,
    "NO QLABEL PREVIEW":
        "new QLabel" not in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<30} PASS")

for tool in (
    "TYPE_COPY",
    "TYPE_SAVE",
    "TYPE_OPEN_APP",
    "TYPE_PIN",
    "TYPE_OCR",
    "TYPE_ACCEPT",
):
    needle = f"buttonList->removeOne(CaptureTool::{tool});"
    assert needle in CPP, needle

print(f"{'ACTION TOOLS DEFERRED':<30} PASS")

start = CPP.index("if (m_context.fullscreen &&", CPP.index("void CaptureWidget::initSelection()"))
end = CPP.index("connect(m_selection, &SelectionWidget::visibilityChanged", start)
block = CPP[start:end]

for forbidden in (
    "setWindowFlags(",
    "showNormal(",
    "setParent(",
    "setWindowState(",
):
    assert forbidden not in block, forbidden

print(f"{'NO MAPPED ROLE MUTATION':<30} PASS")

assert 'setProperty("windowEditorPreviewDetached", true);' in block
assert "QTimer::singleShot(0, editor" in block
print(f"{'SAFE WAYLAND HANDOFF':<30} PASS")

print("\nSTAGE 3.0-B EDITOR REUSE CONTRACT: PASS")
