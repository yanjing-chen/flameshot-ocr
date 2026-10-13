#!/usr/bin/env python3
# G.2D-3 semantic source safety checks, not a GUI acceptance substitute.
from pathlib import Path
import re,xml.etree.ElementTree as ET
ocr=Path('src/config/ocrconf.cpp').read_text()
v=Path('src/config/visualseditor.cpp').read_text()
t=Path('src/widgets/capture/capturewidget.cpp').read_text()
c=Path('src/config/configwindow.cpp').read_text()
h=Path('src/config/gnomecontrols.h').read_text()
checks={
 'OCR long backend status gets full row': 'backendStatusRow->addWidget(m_backendStatus)' in ocr and 'setMinimumHeight(46)' in ocr,
 'Backend requests/callbacks preserved': all(x in ocr for x in ('refreshAccelerationStatus()', '/v1/runtime/backends', 'm_backendStatus->setText(status)')),
 'Original button list and drag order': 'm_buttonList = new ButtonListView();' in v and 'setMinimumHeight(330)' in v and '&ButtonListView::resetOrder' in v,
 'OCR button no permanent background': 'primaryAction' not in t and 'windowEditorToolButton:checked' in t,
 'Existing toolbar SVG/selection preserved': 'g2d2CrispIcon' in t and 'syncWindowEditorToolbarState()' in t,
 'GNOME real popup delegate': 'class ComboRowDelegate' in h and 'setItemDelegate(new ComboRowDelegate' in h and 'drawRoundedRect(itemRect' in h,
 'Numeric slider and original spin stay linked': '&QSlider::valueChanged' in h and '&QSpinBox::setValue' in h and 'spin->setProperty("g2d3NumericLinked"' in h,
 'Live accent and dark theme': 'GnomeAccent::current' in h and 'ColorUtils::colorIsDark' in h and 'GnomeAccent::watch(this' in c,
 'Visuals page fully scrollable': 'g2d3VisualsScroll' in c and 'visualsScroll->setWidget(m_visuals)' in c,
 'Config page controls wired': 'GnomeControls::attachNumericSliders(m_generalConfig)' in c and 'GnomeControls::apply(this)' in c,
 'OCR Runtime not changed': 'OcrManager::instance()' in ocr and 'sendModelRequest(' in ocr,
 'Original screenshot canvas safe': 'scroll->setWidget(content)' in t,
}
for mode in ('dark','accent'):
    p=Path(f'data/img/window-editor/{mode}/marker.svg')
    root=ET.parse(p).getroot()
    checks[f'{mode} SVG well-formed']=root.tag.rsplit('}',1)[-1]=='svg'
for label,ok in checks.items():
    print(f'{label:<50} {"PASS" if ok else "FAIL"}')
    if not ok: raise SystemExit(f'STOP: {label}')
print('G.2D-3 static checks PASS; C++ compilation and GUI manual tests remain necessary')
