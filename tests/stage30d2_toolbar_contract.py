#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")
BUTTON_CPP = Path("src/config/buttonlistview.cpp").read_text(encoding="utf-8")
BUTTON_HDR = Path("src/config/buttonlistview.h").read_text(encoding="utf-8")
VISUALS_CPP = Path("src/config/visualseditor.cpp").read_text(encoding="utf-8")
VISUALS_HDR = Path("src/config/visualseditor.h").read_text(encoding="utf-8")
CONFIG_CPP = Path("src/utils/confighandler.cpp").read_text(encoding="utf-8")
CONFIG_HDR = Path("src/utils/confighandler.h").read_text(encoding="utf-8")
OCR_CPP = Path("src/ocr/ocrrequestcontroller.cpp").read_text(encoding="utf-8")

checks = {
    "FIXED BOTTOM TOOLBAR":
        "createWindowEditorToolbar" in CPP
        and 'setObjectName(QStringLiteral("windowEditorToolbar"))' in CPP
        and "layout->addWidget(toolbar, 0);" in CPP,
    "TOOLBAR OUTSIDE CANVAS":
        "m_windowEditorToolbarButtons" in HDR
        and "button->setParent(toolbarContent);" in CPP
        and "m_buttonHandler->setButtons(vectorButtons);" in CPP,
    "HORIZONTAL OVERFLOW":
        "Qt::ScrollBarAsNeeded" in CPP
        and "Qt::ScrollBarAlwaysOff" in CPP
        and "toolbarScroll->setWidgetResizable(true);" in CPP,
    "PERSISTENT TOOLBAR ORDER":
        "windowEditorToolbarOrder" in CONFIG_HDR
        and '"windowEditorToolbarOrder"' in CONFIG_CPP
        and "setWindowEditorToolbarOrder" in BUTTON_CPP,
    "DRAG SORTING":
        "QAbstractItemView::InternalMove" in BUTTON_CPP
        and "void ButtonListView::dropEvent" in BUTTON_CPP
        and "persistState();" in BUTTON_CPP,
    "RESET DEFAULT ORDER":
        "void resetOrder();" in BUTTON_HDR
        and "void ButtonListView::resetOrder()" in BUTTON_CPP
        and 'tr("Reset Order")' in VISUALS_CPP,
    "VISIBLE BUTTON CONFIG PRESERVED":
        "config.setButtons(enabled);" in BUTTON_CPP
        and "m_listButtons = ConfigHandler().buttons();" in BUTTON_CPP,
    "TOOLBAR ALIGNMENT":
        "windowEditorToolbarAlignment" in CONFIG_HDR
        and '"center"' in CONFIG_CPP
        and 'QStringLiteral("left")' in VISUALS_CPP
        and 'QStringLiteral("center")' in VISUALS_CPP
        and 'QStringLiteral("right")' in VISUALS_CPP
        and "m_toolbarAlignment" in VISUALS_HDR,
    "HELP OVERLAY CLEARED":
        "if (m_windowEditorMode) {\n        OverlayMessage::pop();" in CPP
        and "if (!m_windowEditorMode && !m_selection->isVisible()" in CPP,
    "DUAL SCREEN PIN SOURCE":
        "QScreen* windowEditorSourceScreen = screen();" in CPP
        and "windowEditorSourceScreen->geometry().topLeft()" in CPP
        and "windowEditorSourceGeometry.translate(" in CPP,
    "OCR EMPTY BAR REMOVED":
        "OcrResultDragHandle" not in OCR_CPP
        and "dragHandle" not in OCR_CPP,
    "OCR PLAIN RESULT PRESERVED":
        "QPlainTextEdit" in OCR_CPP
        and 'ocrTr("Save Text...")' in OCR_CPP
        and "Model speed:" in OCR_CPP,
    "STAGE D2 IDENTITY":
        'flameshotWindowEditorStage", "3.0-D.2"' in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<34} PASS")

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

print(f"{'NO MAPPED ROLE MUTATION':<34} PASS")
print("\nSTAGE 3.0-D.2 TOOLBAR CONTRACT: PASS")
