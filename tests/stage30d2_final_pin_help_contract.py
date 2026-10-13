#!/usr/bin/env python3

from pathlib import Path

CAPTURE = Path("src/widgets/capture/capturewidget.cpp").read_text(
    encoding="utf-8"
)
PIN_CPP = Path("src/tools/pin/pinwidget.cpp").read_text(encoding="utf-8")
PIN_HDR = Path("src/tools/pin/pinwidget.h").read_text(encoding="utf-8")

checks = {
    "FULLSCREEN HELP REPUSH BLOCKED":
        '!property("windowEditorPreviewDetached").toBool()' in CAPTURE
        and "OverlayMessage::push(m_helpMessage);" in CAPTURE,
    "EDITOR PIN INTERCEPT":
        "m_windowEditorMode &&" in CAPTURE
        and "(m_context.request.tasks() & CaptureRequest::PIN)" in CAPTURE,
    "PIN EXPORT CANCELLED":
        "m_context.request.removeTask(CaptureRequest::PIN);" in CAPTURE
        and "m_captureDone = false;" in CAPTURE,
    "SAME EDITOR TOPLEVEL PRESERVED":
        "QWidget* editorShell = window();" in CAPTURE
        and "flameshotWindowEditorPinnedInPlace" in CAPTURE,
    "EMBEDDED PIN VIEW":
        "bool embedded = false" in PIN_HDR
        and "m_embedded(embedded)" in PIN_CPP
        and "if (!m_embedded)" in PIN_CPP,
    "NO NEW TOPLEVEL FOR EDITOR PIN":
        "new PinWidget(" in CAPTURE
        and "editorShell," in CAPTURE
        and "true);" in CAPTURE,
    "PIN REMAINS DRAGGABLE":
        "nativeWindow->startSystemMove()" in PIN_CPP
        and "topLevel ? topLevel->windowHandle()" in PIN_CPP,
    "PIN CLOSES EDITOR SHELL":
        "if (m_embedded)" in PIN_CPP
        and "topLevel->close();" in PIN_CPP,
    "LEGACY PIN PATH PRESERVED":
        "setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint |"
        in PIN_CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<36} PASS")

stage_start = CAPTURE.index(
    "if (m_context.fullscreen &&",
    CAPTURE.index("void CaptureWidget::initSelection()"),
)
stage_end = CAPTURE.index(
    "connect(m_selection, &SelectionWidget::visibilityChanged",
    stage_start,
)
stage_block = CAPTURE[stage_start:stage_end]

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

print(f"{'FULLSCREEN ROLE SAFETY':<36} PASS")
print("\nSTAGE 3.0-D.2 FINAL PIN/HELP CONTRACT: PASS")
