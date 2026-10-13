#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")

checks = {
    "EXACT LOGICAL IMAGE CANVAS":
        "resize(logicalImageSize);" in CPP,
    "OLD FORCED CANVAS REMOVED":
        "editorCanvasMinimumSize" not in CPP
        and "editorCanvasSize(" not in CPP,
    "SCROLL AREA CENTERS SMALL IMAGE":
        "scrollArea->setAlignment(Qt::AlignCenter);" in CPP,
    "LARGE IMAGE REMAINS SCROLLABLE":
        "scrollArea->setWidgetResizable(false);" in CPP,
    "FIXED BOTTOM TOOLBAR PRESERVED":
        (
            "layout->addWidget(scrollArea, 1);" in CPP
            or (
                "windowEditorWorkspaceHost" in CPP
                and "workspaceLayout->addWidget(scrollArea, 1);" in CPP
                and "layout->addWidget(workspace, 1);" in CPP
                and "windowEditorSettingsDock" in CPP
            )
        )
        and "layout->addWidget(toolbar, 0);" in CPP,
    "D2 PIN-IN-PLACE PRESERVED":
        "flameshotWindowEditorPinnedInPlace" in CPP,
    "D2 HELP HANDOFF PRESERVED":
        '!property("windowEditorPreviewDetached").toBool()' in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<38} PASS")

constructor_start = CPP.index(
    "} else if (m_windowEditorMode && !initialScreenshot.isNull())"
)
constructor_end = CPP.index("QVector<QRect> areas;", constructor_start)
constructor_block = CPP[constructor_start:constructor_end]

assert "resize(logicalImageSize);" in constructor_block
assert "qMax(logicalImageSize.width()" not in constructor_block
assert "qMax(logicalImageSize.height()" not in constructor_block

print(f"{'NO SMALL-IMAGE PADDING CANVAS':<38} PASS")
print("\nSTAGE 3.0-D.2 CANVAS CONTRACT: PASS")
