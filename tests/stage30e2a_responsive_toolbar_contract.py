#!/usr/bin/env python3

from pathlib import Path

CPP = Path("src/widgets/capture/capturewidget.cpp").read_text(encoding="utf-8")
HDR = Path("src/widgets/capture/capturewidget.h").read_text(encoding="utf-8")

checks = {
    "E2A TOOLBAR STAGE":
        'flameshotUiStage", "3.0-E.2A"' in CPP,

    "DEFAULT TOOLBAR MORE COMPACT":
        "toolbarFrame->setFixedHeight(70);" in CPP
        and "cell->setFixedSize(46, 50);" in CPP
        and "proxy->setIconSize(QSize(20, 20));" in CPP,

    "AUTO COMPACT API":
        "void updateWindowEditorToolbarCompactMode();" in HDR
        and "void CaptureWidget::updateWindowEditorToolbarCompactMode()" in CPP,

    "VISIBLE-COUNT ADAPTIVE THRESHOLD":
        "const int toolCount = cells.size();" in CPP
        and "regularRequiredWidth" in CPP,

    "SMALL WINDOW COMPACT MODE":
        "toolbarFrame->width() < regularRequiredWidth" in CPP
        and 'toolbarFrame->setProperty("compactMode", compact);' in CPP,

    "COMPACT HIDES LABELS":
        "cell->setFixedSize(32, 34);" in CPP
        and "proxy->setIconSize(QSize(18, 18));" in CPP
        and "label->hide();" in CPP,

    "REGULAR RESTORES LABELS":
        "label->show();" in CPP
        and "label->setFixedWidth(44);" in CPP,

    "RESIZE DRIVEN":
        "bool eventFilter(QObject* watched, QEvent* event) override;" in HDR
        and "event->type() == QEvent::Resize" in CPP
        and "editor->installEventFilter(canvas);" in CPP,

    "INITIAL SYNC":
        "canvas->updateWindowEditorToolbarCompactMode();" in CPP,

    "HORIZONTAL SCROLL FALLBACK":
        "Qt::ScrollBarAsNeeded" in CPP
        and "Qt::ScrollBarAlwaysOff" in CPP,

    "ORDER PRESERVED":
        "windowEditorToolbarOrder()" in CPP
        and "orderedTypes" in CPP,

    "HIDE SHOW PRESERVED":
        "auto visibleTypes = m_config.buttons();" in CPP,

    "ALIGNMENT PRESERVED":
        "windowEditorToolbarAlignment()" in CPP,

    "NO WRAP":
        "auto* row = new QHBoxLayout(content);" in CPP,

    "COMPACT TOOLTIP":
        'sourceButton->tool()->name()' in CPP
        and "sourceButton->toolTip()" in CPP,

    "D2 PIN PRESERVED":
        "flameshotWindowEditorPinnedInPlace" in CPP,
}

for label, ok in checks.items():
    assert ok, label
    print(f"{label:<38} PASS")

print("\nSTAGE 3.0-E.2A RESPONSIVE TOOLBAR CONTRACT: PASS")
