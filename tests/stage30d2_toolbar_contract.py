#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
OCR_CPP = Path("src/ocr/ocrrequestcontroller.cpp").read_text(encoding="utf-8")
BUTTON_CPP = Path("src/config/buttonlistview.cpp").read_text(encoding="utf-8")
BUTTON_HDR = Path("src/config/buttonlistview.h").read_text(encoding="utf-8")
VISUALS_CPP = Path("src/config/visualseditor.cpp").read_text(encoding="utf-8")
VISUALS_HDR = Path("src/config/visualseditor.h").read_text(encoding="utf-8")
CONFIG_CPP = Path("src/utils/confighandler.cpp").read_text(encoding="utf-8")
CONFIG_HDR = Path("src/utils/confighandler.h").read_text(encoding="utf-8")

checks = {
    "FIXED BOTTOM TOOLBAR":
        "createWindowEditorToolbar" in CPP
        and ("layout->addWidget(scrollArea, 1);" in CPP or "workspaceLayout->addWidget(scrollArea, 1);" in CPP)
        and "layout->addWidget(toolbar, 0);" in CPP,

    "TOOLBAR OUTSIDE CANVAS":
        "canvas->createWindowEditorToolbar(editor);" in CPP
        and "scrollArea->setWidget(canvas);" in CPP
        and "layout->addWidget(toolbar, 0);" in CPP
        and "scrollArea->setWidget(toolbar)" not in CPP,

    "HORIZONTAL TOOLBAR SCROLL":
        "Qt::ScrollBarAsNeeded" in CPP
        and "Qt::ScrollBarAlwaysOff" in CPP,

    "DRAG ORDER PERSISTENCE":
        "windowEditorToolbarOrder" in CONFIG_HDR
        and "windowEditorToolbarOrder" in CONFIG_CPP
        and "windowEditorToolbarOrder" in BUTTON_CPP
        and "InternalMove" in BUTTON_CPP,

    "VISIBLE BUTTON CONFIG PRESERVED":
        "setButtons" in BUTTON_CPP
        and "ConfigHandler().buttons()" in BUTTON_CPP,

    "TOOLBAR ALIGNMENT":
        "windowEditorToolbarAlignment" in CONFIG_HDR
        and "windowEditorToolbarAlignment" in CONFIG_CPP
        and "windowEditorToolbarAlignment" in VISUALS_CPP
        and "m_toolbarAlignment" in VISUALS_HDR,

    "HELP OVERLAY CLEARED":
        "if (m_windowEditorMode) {" in CPP
        and "OverlayMessage::pop();" in CPP
        and '!property("windowEditorPreviewDetached").toBool()' in CPP,

    "WINDOW EDITOR PIN IN PLACE":
        "flameshotWindowEditorPinnedInPlace" in CPP
        and "(m_context.request.tasks() & CaptureRequest::PIN)" in CPP,

    "OCR EMPTY BAR REMOVED":
        "OcrResultDragHandle" not in OCR_CPP
        and "dragHandle" not in OCR_CPP,

    "OCR PLAIN RESULT PRESERVED":
        "QPlainTextEdit" in OCR_CPP
        and 'ocrTr("Save Text...")' in OCR_CPP
        and "Model speed:" in OCR_CPP,

    "STAGE D2 IDENTITY":
        'flameshotWindowEditorStage", "3.0-D.2"' in CPP,

    "E1 MODERN TOOLBAR COMPATIBLE":
        (
            "windowEditorToolbarFrame" in CPP
            and "windowEditorToolButton" in CPP
        )
        or "setWidget(toolbarContent)" in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<36} PASS")

# D.2's real architectural invariant is that the fullscreen Wayland surface
# is never role-mutated into the normal editor window. The editor must still
# be created as a separate ordinary top-level after selection.
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

assert "new QWidget(nullptr, Qt::Window)" in stage_block
assert "scrollArea->setWidget(canvas);" in stage_block
assert "layout->addWidget(toolbar, 0);" in stage_block

print(f"{'NO MAPPED ROLE MUTATION':<36} PASS")
print(f"{'SEPARATE NORMAL EDITOR':<36} PASS")
print("\nSTAGE 3.0-D.2 TOOLBAR CONTRACT: PASS")
