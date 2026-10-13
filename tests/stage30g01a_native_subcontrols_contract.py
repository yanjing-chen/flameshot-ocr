#!/usr/bin/env python3
from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")

checks = {
    "BUTTON AND LINEEDIT MODERN STYLE":
        '"QFrame#windowEditorSettingsDock QPushButton, "' in CPP
        and '"QFrame#windowEditorSettingsDock QLineEdit { "' in CPP,
    "SPINBOX NATIVE CHROME":
        '"QFrame#windowEditorSettingsDock QSpinBox, "' in CPP
        and '"min-height: 34px; }"' in CPP
        and '"QFrame#windowEditorSettingsDock QSpinBox:disabled { "' not in CPP,
    "COMBOBOX NATIVE CHROME":
        '"QFrame#windowEditorSettingsDock QComboBox { "' in CPP
        and '"min-height: 34px; }"' in CPP,
    "NO CUSTOM SPIN COMBO BORDER BLOCK":
        '"QFrame#windowEditorSettingsDock QSpinBox, "'
        '"QFrame#windowEditorSettingsDock QComboBox { "'
        '"min-height: 34px; "'
        '"border: 1px solid #cfd5dd; "' not in CPP,
    "G01 FONT STILL ENLARGED":
        '"font-size: 14px; }"' in CPP,
    "G01 PANEL WIDTH PRESERVED":
        "settingsDock->setFixedWidth(380);" in CPP,
    "NO IMAGE SCALE CHANGE":
        "selected.scaled(" not in CPP,
    "NO WINDOW GEOMETRY EXPANSION":
        "windowEditorGeometryBeforeSettingsDock" not in CPP,
}

for label, ok in checks.items():
    print(f"{label:<40} {'PASS' if ok else 'FAIL'}")
    assert ok, label

print("\nSTAGE 3.0-G.0+G.1A NATIVE SUBCONTROLS CONTRACT: PASS")
