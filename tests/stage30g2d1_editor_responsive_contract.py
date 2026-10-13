#!/usr/bin/env python3
from pathlib import Path
s=Path('src/config/gnomeswitch.h').read_text()
a=Path('src/utils/gnomeaccent.h').read_text()
u=Path('src/widgets/panel/utilitypanel.cpp').read_text()
c=Path('src/widgets/capture/capturewidget.cpp').read_text()
w=Path('src/config/configwindow.cpp').read_text()
checks={
 'GNOME HOST GSETTINGS ISOLATED': 'hostGsettingsEnv()' in a and 'LD_LIBRARY_PATH' in a,
 'GNOME DYNAMIC ACCENT MONITOR': 'monitor->start(hostGsettings()' in a,
 'KEYBOARD FOCUS INDICATOR': 'm_keyboardFocus' in s and 'Qt::SolidLine' in s,
 'NO MOUSE DASHED FOCUS RING': 'Qt::DashLine' not in s,
 'DOCK VERTICAL SCROLL': 'm_internalPanel->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded)' in u,
 'DOCK LAYER LIST BOUNDED': 'm_captureTools->setMaximumHeight(190)' in u,
 'COLOR WHEEL MINIMUM SIZE': 'windowEditorColorWheel' in u and 'setMinimumSize(112, 112)' in u,
 'EDITOR DOCK GNOME THEME': 'applyEditorDockAppearance' in c and 'GnomeAccent::watch(settingsDock' in c,
 'CONFIG REMAINING GROUPS STYLED': 'QTabWidget#configPageStack QGroupBox' in w,
 'NAV ICONS HIGH DPI': 'pixmap.setDevicePixelRatio(2.0)' in w,
 'NO CHANGE TO OCR RUNTIME': '127.0.0.1:8111' not in ''.join((s,a,u,c,w)),
}
for k,v in checks.items():
 print(f'{k:<43} {"PASS" if v else "FAIL"}')
 if not v:raise SystemExit(1)
