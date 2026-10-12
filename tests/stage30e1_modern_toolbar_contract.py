#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")
QRC = Path("data/graphics.qrc").read_text(encoding="utf-8")

checks = {
    "E1 UI STAGE":
        'flameshotWindowEditorUiStage", "3.0-E.1"' in CPP,
    "MODERN PROXY TOOLBAR":
        'windowEditorToolButton' in CPP
        and "new QToolButton(cell)" in CPP,
    "LEGACY BUTTONS HIDDEN":
        "sourceButton->hide();" in CPP,
    "CUSTOM MODERN ICONS":
        ":/img/window-editor/dark/%1.svg" in CPP
        and ":/img/window-editor/%1/%2.svg" in CPP,
    "ACTIVE ACCENT ICON":
        'QStringLiteral("accent")' in CPP
        and "syncWindowEditorToolbarState" in CPP,
    "TOOL LABELS":
        'windowEditorToolLabel' in CPP
        and "sourceButton->name()" in CPP,
    "ORDER PERSISTENCE PRESERVED":
        "windowEditorToolbarOrder()" in CPP
        and "orderedTypes" in CPP,
    "ALIGNMENT PRESERVED":
        "windowEditorToolbarAlignment()" in CPP,
    "HORIZONTAL SCROLL PRESERVED":
        "Qt::ScrollBarAsNeeded" in CPP
        and "Qt::ScrollBarAlwaysOff" in CPP,
    "WORKSPACE MODERNIZED":
        'windowEditorWorkspace' in CPP
        and "#e9edf2" in CPP,
    "SYNC API":
        "void syncWindowEditorToolbarState();" in HDR
        and "void CaptureWidget::syncWindowEditorToolbarState()" in CPP,
    "RESOURCE SET":
        "img/window-editor/dark/pencil.svg" in QRC
        and "img/window-editor/accent/ocr.svg" in QRC,
    "D2 PIN PRESERVED":
        "flameshotWindowEditorPinnedInPlace" in CPP,
    "D2 HELP PRESERVED":
        'windowEditorPreviewDetached' in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<36} PASS")

icon_files = list(Path("data/img/window-editor/dark").glob("*.svg"))
accent_files = list(Path("data/img/window-editor/accent").glob("*.svg"))
assert len(icon_files) >= 20
assert len(icon_files) == len(accent_files)
print(f"{'SVG ICON PAIRS':<36} PASS ({len(icon_files)})")

print("\nSTAGE 3.0-E.1 MODERN TOOLBAR CONTRACT: PASS")
