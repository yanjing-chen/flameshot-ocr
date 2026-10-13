#!/usr/bin/env python3
from pathlib import Path
import re
import xml.etree.ElementTree as ET

cpp=Path('src/widgets/panel/sidepanelwidget.cpp').read_text(encoding='utf-8')
hdr=Path('src/widgets/panel/sidepanelwidget.h').read_text(encoding='utf-8')
cap=Path('src/widgets/capture/capturewidget.cpp').read_text(encoding='utf-8')
utility=Path('src/widgets/panel/utilitypanel.cpp').read_text(encoding='utf-8')

checks={
  'PORTAL PICKCOLOR API': 'QStringLiteral("PickColor")' in cpp and 'org.freedesktop.portal.Screenshot' in cpp,
  'WAYLAND EDITOR ONLY': 'property("flameshotDesktopWideColorPicker").toBool()' in cpp and 'm_windowEditorMode' in cap,
  'PORTAL ASYNC RESPONSE': 'QDBusPendingCallWatcher' in cpp and 'onPortalColorResponse' in cpp,
  'RGB STRUCTURE PARSING': 'QDBusArgument::StructureType' in cpp and 'QColor::fromRgbF' in cpp,
  'SELECTED COLOR SIGNAL': 'emit colorChanged(picked);' in cpp,
  'CANCEL DOES NOT ALTER COLOR': 'finishPortalColorGrab(response != 1)' in cpp,
  'PORTAL UI RESTORATION': 'emit showPanel();' in cpp,
  'LEGACY PICKER STILL PRESENT': 'm_colorGrabber = new ColorGrabWidget(m_pixmap);' in cpp,
  'NO GLOBAL SCREEN PIXEL READ': 'grabWindow(' not in cpp and 'XGetImage' not in cpp,
  'NO MAPPED WINDOW ROLE CHANGE': 'setWindowFlags(' not in cpp,
  'PERSISTED E2C HIDE EMPTY':
      'm_captureTools->setRowHidden(0, m_emptyLayerRowHidden);' in utility
      and 'canvas->m_panel->setEmptyLayerRowHidden(true);' in cap,
  'QT MOC SLOT': 'onPortalColorResponse(uint response, const QVariantMap& results)' in hdr,
  'VALID TS XML': ET.parse('data/translations/Internationalization_zh_CN.ts') is not None,
}
for label,ok in checks.items():
  print(f'{label:<39} {"PASS" if ok else "FAIL"}')
  assert ok,label
print('STAGE 3.0-E.2D PORTAL COLOR PICKER CONTRACT: PASS')
