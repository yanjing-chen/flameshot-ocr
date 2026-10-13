#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")
BL = Path("src/config/buttonlistview.cpp").read_text(encoding="utf-8")
CFG_H = Path("src/utils/confighandler.h").read_text(encoding="utf-8")
CFG_CPP = Path("src/utils/confighandler.cpp").read_text(encoding="utf-8")
QRC = Path("data/graphics.qrc").read_text(encoding="utf-8")

checks = {
    "E2 CONFIGURABLE STAGE":
        '3.0-E.2-configurable-settings' in CPP,
    "ICON ONLY":
        "windowEditorToolLabel" not in CPP
        and "sourceButton->tool()->name()" in CPP
        and "sourceButton->toolTip()" in CPP,
    "DEFAULT COMPACT":
        "toolbarFrame->setFixedHeight(52);" in CPP
        and "cell->setFixedSize(36, 36);" in CPP,
    "NARROW EXTRA COMPACT":
        "QSize(30, 32)" in CPP
        and "regularRequiredWidth" in CPP,
    "SETTINGS PSEUDO ITEM":
        "kWindowEditorSettingsToolbarItem = -1000" in BL
        and "kWindowEditorSettingsToolbarItem = -1000" in CPP,
    "SETTINGS SHOW HIDE CONFIG":
        "windowEditorSettingsButtonVisible" in CFG_H
        and "windowEditorSettingsButtonVisible" in CFG_CPP
        and "setWindowEditorSettingsButtonVisible" in BL,
    "SETTINGS SAME BUTTON LIST":
        'item->setText(tr("Tool Settings"));' in BL
        and "type == windowEditorSettingsToolbarType()" in BL
        and "Qt::ItemIsDragEnabled" in BL
        and "setDragDropMode(QAbstractItemView::InternalMove);" in BL,
    "SETTINGS ORDER PERSISTENCE":
        "persistState" in BL
        and "windowEditorToolbarOrder" in BL,
    "SETTINGS ORDER USED BY TOOLBAR":
        "orderedItems" in CPP
        and "kWindowEditorSettingsToolbarItem" in CPP,
    "WRENCH RESOURCE":
        "img/window-editor/dark/wrench.svg" in QRC
        and "img/window-editor/accent/wrench.svg" in QRC,
    "SIDE TAB REMOVED IN EDITOR":
        "!m_windowEditorMode && ConfigHandler().showSidePanelButton()" in CPP,
    "SETTINGS OUTSIDE CANVAS":
        "windowEditorSettingsDock" in CPP
        and "windowEditorWorkspaceHost" in CPP
        and "workspaceLayout->addWidget(scrollArea, 1);" in CPP,
    "SETTINGS WIDTH INDEPENDENT":
        # E.2 ensures a fixed-width sibling dock, not a specific pixel size.
        # G.0+G.1 separately checks the 380px design requirement.
        "settingsDock->setFixedWidth(" in CPP
        and "workspaceLayout->addWidget(settingsDock, 0);" in CPP,
    "WRENCH TOGGLE":
        "windowEditorSettingsButton" in CPP
        and "setWindowEditorToolSettingsVisible" in CPP,
    "RIGHT CLICK OPENS SETTINGS":
        "setWindowEditorToolSettingsVisible(true);" in CPP,
    "RESIZE DRIVEN":
        "updateWindowEditorToolbarCompactMode" in HDR
        and "event->type() == QEvent::Resize" in CPP,
    "NORMAL TOOL VISIBILITY PRESERVED":
        "auto visibleTypes = m_config.buttons();" in CPP,
    "ALIGNMENT PRESERVED":
        "windowEditorToolbarAlignment()" in CPP,
    "SCROLL FALLBACK PRESERVED":
        "Qt::ScrollBarAsNeeded" in CPP,
    "PIN PRESERVED":
        "flameshotWindowEditorPinnedInPlace" in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<46} PASS")

print("\nSTAGE 3.0-E.2 CONFIGURABLE TOOL SETTINGS CONTRACT: PASS")
