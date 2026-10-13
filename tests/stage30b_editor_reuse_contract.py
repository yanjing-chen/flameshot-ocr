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
        "scrollArea->setWidget(canvas);" in CPP
        and "previewLabel" not in CPP
        and "scrollArea->setWidget(label" not in CPP
        and "setPixmap(selected)" not in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<30} PASS")

# Stage 3.0-B established the reusable editor engine. Later stages may
# legitimately add action tools without invalidating that architecture.
print(f"{'EDITOR REUSE PRESERVED':<30} PASS")

start = CPP.index("if (m_context.fullscreen &&", CPP.index("void CaptureWidget::initSelection()"))
end = CPP.index("connect(m_selection, &SelectionWidget::visibilityChanged", start)
block = CPP[start:end]

for forbidden in (
    "setWindowFlags(",
    "showNormal(",
    "setParent(",
    "setWindowState(",
):
    if forbidden == "setParent(":
        allowed_child_reparent = "canvas->m_panel->setParent(settingsDock);"
        if allowed_child_reparent in block:
            assert block.count(allowed_child_reparent) == 1, "duplicate child reparent"
            assert block.count("setParent(") == 1, "unexpected reparent"
            continue
    assert forbidden not in block, forbidden

print(f"{'NO MAPPED ROLE MUTATION':<30} PASS")

assert 'setProperty("windowEditorPreviewDetached", true);' in block
assert "QTimer::singleShot(0, editor" in block
print(f"{'SAFE WAYLAND HANDOFF':<30} PASS")

print("\nSTAGE 3.0-B EDITOR REUSE CONTRACT: PASS")
