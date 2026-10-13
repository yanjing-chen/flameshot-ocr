#!/usr/bin/env python3
from pathlib import Path
import subprocess
c=Path('src/config/configwindow.cpp').read_text()
o=Path('src/config/ocrconf.cpp').read_text()
s=Path('src/config/shortcutswidget.cpp').read_text()
h=Path('src/config/shortcutswidget.h').read_text()
t=Path('src/widgets/capture/capturewidget.cpp').read_text()
import xml.etree.ElementTree as ET
ET.parse('data/translations/Internationalization_zh_CN.ts')
checks={
 'OCR FOUR REAL CARDS':all(x in o for x in ('auto sharedCard = makeFormCard','auto accelerationCard = makeFormCard','auto connectionCard = makeFormCard','ocrSettingsCardTitle','modelsHeading')),
 'NO OLD OCR PAGE GROUP BOX':all(x not in o for x in ('new QGroupBox(tr("Local AI Runtime"), content)','new QGroupBox(tr("Inference acceleration"), content)','new QGroupBox(tr("OCR connection"), content)')),
 'OCR TRANSPARENT FULL SCROLL': all(x in o for x in ('ocrSettingsScroll','ocrSettingsContent','setWidgetResizable(true)')) and 'QScrollArea#ocrSettingsScroll' in c,
 'OCR ORIGINAL API CALLBACKS':all(x in o for x in ('&OcrConf::refreshSharedRuntimeStatus','m_refreshModelsButton','m_preloadOcrModel')),
 'OCR LIVE COLOR SWITCHES':'GnomeAccent::watch(this' in o and 'OcrSwitchObserver' in o and 'original->click()' in o,
 'OCR PROGRAMMATIC SWITCH SYNCHRONIZATION': 'ocrPreloadSwitch' in o and 'ocrCatalogSwitch' in o and 'view->setChecked(selectedIsPreloaded)' in o,
 'SHORTCUT SEARCH REAL': 'shortcutSearchField' in s and 'rebuildPreferenceRows' in s,
 'SHORTCUT REAL ROWS': 'shortcutPreferencesCard' in s and 'shortcutKeyButton' in s and 'shortcutPreferenceRow' in s,
 'SHORTCUT DIALOG UNCHANGED': 'onShortcutCellClicked(i, 1)' in s and 'new SetShortcutDialog(nullptr, shortcutName)' in s,
 'SHORTCUTS ORIGINAL MODEL': 'loadShortcuts();' in s and 'm_table->setRowCount(m_shortcuts.size());' in s and 'if (m_rowsLayout) rebuildPreferenceRows();' in s,
 'SHORTCUTS HEADER MEMBERS':all(x in h for x in ('m_search', 'm_rowsScroll','m_rowsLayout')),
 'HIDPI MULTISCALE SVG': 'QSvgRenderer renderer(resource)' in t and 'pixmap.setDevicePixelRatio(dpr)' in t and 'g2d2CrispIcon' in t and t.count('g2d2CrispIcon(') >= 5,
 'TOOL BUTTONS SIZE PRESERVED': 'proxy->setFixedSize(40, 40);' in t and 'proxy->setIconSize(QSize(28, 28));' in t,
 'TOOLBAR CONFIG ORDER PRESERVED': 'windowEditorToolbarOrder()' in t and 'windowEditorToolbarAlignment()' in t,
 'EDITOR PIN STAYS': 'kWindowEditorSettingsToolbarItem' in t,
}
for k, ok in checks.items():
 print(f'{k:<44} {"PASS" if ok else "FAIL"}')
 if not ok: raise AssertionError(k)
print('G.2D-2 STATIC CONTRACT PASS; GUI ACCEPTANCE STILL REQUIRED')
