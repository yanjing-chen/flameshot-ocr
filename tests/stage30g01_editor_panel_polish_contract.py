#!/usr/bin/env python3
from pathlib import Path
import xml.etree.ElementTree as ET

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
SIDE = Path("src/widgets/panel/sidepanelwidget.cpp").read_text(encoding="utf-8")
UTILITY = Path("src/widgets/panel/utilitypanel.cpp").read_text(encoding="utf-8")
HEADER = Path("src/widgets/panel/sidepanelwidget.h").read_text(encoding="utf-8")
TS_PATH = Path("data/translations/Internationalization_zh_CN.ts")

tree = ET.parse(TS_PATH)
translations = {}
for msg in tree.getroot().iter("message"):
    src = msg.findtext("source")
    tr = msg.findtext("translation")
    if src and tr:
        translations.setdefault(src, set()).add(tr)

checks = {
    "ARROW STYLE TRANSLATED":
        ("箭头样式：" in translations.get("Arrow style:", set())
         or "箭头样式" in translations.get("Arrow style", set())),
    "DEFAULT TRANSLATED":
        "默认" in translations.get("Default", set()),
    "CURVED TRANSLATED":
        "弯曲" in translations.get("Curved", set()),
    "EDITOR DOCK ONLY STYLE":
        "QFrame#windowEditorSettingsDock QWidget" in CPP,
    "EDITOR FONT ENLARGED":
        '"font-size: 14px; }"' in CPP,
    "EDITOR PANEL WIDER":
        "settingsDock->setFixedWidth(380);" in CPP,
    "EDITOR PANEL GENEROUS MARGINS":
        "settingsLayout->setContentsMargins(14, 14, 14, 14);" in CPP,
    "MODERN CONTROLS":
        "border-radius: 8px" in CPP,
    "MODERN LAYER LIST":
        # Adjacent C++ string literals are joined at compile time. Verify
        # both halves of the Qt selector and the real widget object name.
        '"QFrame#windowEditorSettingsDock QListWidget"' in CPP
        and '"#windowEditorLayersList { "' in CPP
        and 'windowEditorLayersList' in UTILITY,
    "SIDE PANEL OBJECT NAMES":
        "windowEditorGrabColorButton" in SIDE
        and "windowEditorColorPreview" in SIDE
        and "windowEditorColorHex" in SIDE,
    "LAYER ACTION OBJECT NAMES":
        "windowEditorLayerActionButton" in UTILITY
        and "windowEditorPanelCloseButton" in UTILITY,
    "DOCK LAYOUT SPACING":
        "WINDOW_EDITOR_G1_SPACING" in UTILITY,
    "FULLSCREEN STYLE NOT GLOBAL":
        'setStyleSheet(QStringLiteral(' not in SIDE,
    "NO IMAGE SCALING CHANGE":
        "selected.scaled(" not in CPP,
    "NO WINDOW GEOMETRY EXPANSION":
        "windowEditorGeometryBeforeSettingsDock" not in CPP,
    "E2C EMPTY HIDE PRESERVED":
        "setEmptyLayerRowHidden" in CPP,
    "E2D PORTAL PICKER PRESERVED":
        "PickColor" in SIDE or "PickColor" in HEADER,
}

for label, ok in checks.items():
    print(f"{label:<38} {'PASS' if ok else 'FAIL'}")
    assert ok, label

print("\nSTAGE 3.0-G.0+G.1 EDITOR PANEL POLISH CONTRACT: PASS")
