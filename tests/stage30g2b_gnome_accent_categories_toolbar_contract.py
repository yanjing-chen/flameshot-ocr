#!/usr/bin/env python3
from pathlib import Path
import xml.etree.ElementTree as ET
c=Path('src/config/configwindow.cpp').read_text()
g=Path('src/config/generalconf.cpp').read_text()
h=Path('src/config/generalconf.h').read_text()
t=Path('src/widgets/capture/capturewidget.cpp').read_text()
a=Path('src/utils/gnomeaccent.h').read_text()
checks={
 'ORIGINAL FIVE PAGES': c.count('m_tabWidget->addTab(')==5,
 'SEVEN NAVIGATIONS OCR LAST': 'SEVEN' not in c and 'm_tabWidget->tabText(1), m_tabWidget->tabIcon(1), 1, -1' in c,
 'GENERAL THREE CATEGORIES': 'm_categoryPages[3]' in h and 'setSettingsCategory(int index)' in g,
 'ONE SCROLL FOR ALL GENERAL OPTIONS': 'while (m_layout->count() > 1)' in g and 'm_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding)' in g,
 'NO FAKE SETTINGS': 'GeneralConf::setSettingsCategory' in g and 'm_generalConfig->setSettingsCategory' in c,
 'ORIGINAL CONFIG BEHAVIOR': 'ConfigHandler::fileChanged' in c and 'm_ocrConfig->preloadSavePending()' in c,
 'NATIVE DROPDOWN SPIN': 'QComboBox, QSpinBox, QDoubleSpinBox { min-height: 32px; }' in c,
 'GNOME ACCENT SOURCE': 'org.gnome.desktop.interface' in a and 'accent-color' in a,
 'GNOME ACCENT LIVE': 'GnomeAccent::watch(this, applyGnomeAppearance);' in c and 'GnomeAccent::watch(toolbarFrame, applyToolbarAccent);' in t,
 'GNOME FALLBACK': 'QPalette::Highlight' in a,
 'TOOLBAR LARGE ICONS': 'proxy->setIconSize(QSize(28, 28));' in t,
 'COMPACT FALLBACK': 'compact ? QSize(23, 23) : QSize(28, 28)' in t,
 'HORIZONTAL SCROLL PRESERVED': 'Qt::ScrollBarAsNeeded' in t,
 'PIN OCR COLOR LOGIC UNTOUCHED': 'PickColor' in Path('src/widgets/panel/sidepanelwidget.cpp').read_text(),
 'WINDOW EDITOR DOCK PRESERVED': 'windowEditorSettingsDock' in t,
}
for k,v in checks.items():
 print(f'{k:<44} {"PASS" if v else "FAIL"}')
 assert v,k
for x in Path('.').glob('**/window-editor/*/marker.svg'):
 ET.parse(x)
print('STAGE 3.0-G.2B CONTRACT: PASS')
