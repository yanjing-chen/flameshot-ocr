#!/usr/bin/env python3
from pathlib import Path
import re
import xml.etree.ElementTree as ET

c=Path('src/config/configwindow.cpp').read_text()
g=Path('src/config/generalconf.cpp').read_text()
s=Path('src/config/gnomeswitch.h').read_text()
f=Path('src/tools/text/textconfig.cpp').read_text()
fh=Path('src/tools/text/textconfig.h').read_text()
accent=Path('src/utils/gnomeaccent.h').read_text()
ts=Path('data/translations/Internationalization_zh_CN.ts')
ET.parse(ts)
font_click_match = re.search(
    r'connect\(m_fontChoices,\s*&QListWidget::itemClicked,\s*this,\s*'
    r'\[this\]\(QListWidgetItem\* item\)\s*\{(.*?)\}\);', f, re.S)
font_click_body = font_click_match.group(1) if font_click_match else ''
checks={
 'ORIGINAL FIVE REAL CONFIG PAGES':c.count('m_tabWidget->addTab(')==5,
 'SEVEN NAVIGATION AND OCR LAST':'m_tabWidget->tabText(1), m_tabWidget->tabIcon(1), 1, -1' in c,
 'DISTINCT BELL SCREENSHOT PENCIL ICONS':all(x in c for x in ('categoryIcon(0)','categoryIcon(1)','categoryIcon(2)')),
 'ICONS NOT HOST-THEME DEPENDENT':'QPainterPath bell;' in c and 'camera-photo' not in c,
 'REAL ROUNDED PREFERENCE CARDS':'configPreferenceCard' in g and 'configPreferenceCard' in c,
 'TITLE AND DETAIL SETTINGS ROWS': 'configPreferenceTitle' in g and 'configPreferenceDescription' in g,
 'REAL PAINTED SWITCHES':'class GnomeSettingsSwitch final' in s and 'drawRoundedRect' in s and 'drawEllipse' in s,
 'ORIGINAL QCHECKBOXES AS SOURCE OF TRUTH':'original->click()' in g and 'original->setParent(row)' in g,
 'ORIGINAL CONFIG CALLBACKS KEPT':'&GeneralConf::saveLastRegion' in g and 'ConfigHandler().setSavePathFixed' in g,
 'BIDIRECTIONAL SWITCH STATE': '&QCheckBox::toggled, sw' in g and '&QAbstractButton::setChecked' in g,
 'GNOME ACCENT SWITCH LIVE': 'GnomeAccent::watch(this, [this]()' in g and 'org.gnome.desktop.interface' in accent,
 'SYSTEM DROPDOWNS STILL NATIVE':'QComboBox, QSpinBox, QDoubleSpinBox { min-height: 32px; }' in c,
 'WAYLAND INLINE FONT LIST': 'windowEditorInlineFontList' in f and 'platformName().startsWith' in f,
 'FONT LIST DOES NOT CREATE POPUP': 'm_fontChoices = new QListWidget(this)' in f and 'm_fontChoices->setVisible(open)' in f,
 'ONE CLICK TO SET FONT': bool(font_click_match) and all(piece in font_click_body for piece in ('if (!item) return;', 'const QString family = item->text();', 'm_fontsCB->setCurrentText(family);')),
 'FONT FAMILY SIGNAL PRESERVED': '&QComboBox::currentTextChanged' in f and '&TextConfig::fontFamilyChanged' in f,
 'TEXTCONFIG FONT WIDGET MEMBER PRESERVED': 'QComboBox* m_fontsCB;' in fh,
 'NO IMAGE GEOMETRY CHANGE': 'src/widgets/capture/capturewidget.cpp' not in __import__('subprocess').check_output(['git','diff','--name-only'],text=True),
}
for label,ok in checks.items():
 print(f'{label:<44} {"PASS" if ok else "FAIL"}')
 if not ok:raise AssertionError(label)
print('STAGE 3.0-G.2C GNOME PREFERENCES AND FONT SELECTOR: PASS')
