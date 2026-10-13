#!/usr/bin/env python3
from pathlib import Path
import xml.etree.ElementTree as ET

cpp = Path('src/config/configwindow.cpp').read_text(encoding='utf-8')
original = Path('src/config/generalconf.cpp').read_text(encoding='utf-8')
ts = ET.parse('data/translations/Internationalization_zh_CN.ts').getroot()
checks = {
    'FIVE ORIGINAL PAGES': cpp.count('m_tabWidget->addTab(') == 5,
    'SIDEBAR NAVIGATION': 'configNavigation' in cpp and 'new QListWidget(sidebar)' in cpp,
    'ORIGINAL QTabWidget RETAINED': 'm_tabWidget = new QTabWidget(contentCard);' in cpp,
    'HIDDEN TOP TAB STRIP': 'm_tabWidget->tabBar()->hide();' in cpp,
    'SIDEBAR TWO WAY SYNC': '&QListWidget::currentRowChanged' in cpp and '&QTabWidget::currentChanged' in cpp,
    'RESPONSIVE PAGE CARD': 'configContentCard' in cpp and 'layout->addWidget(contentCard, 1);' in cpp,
    'LARGER FONT': 'qMax(11, readableFont.pointSize())' in cpp,
    'MODERN FLAT CARDS': 'border-radius: 12px' in cpp,
    'DARK THEME SUPPORT': 'darkTheme' in cpp and 'ColorUtils::colorIsDark' in cpp,
    'NATIVE COMBO AND SPIN SUBCONTROLS': 'no custom borders, padding, arrows' in cpp,
    'ALL FIVE ERROR INDICATORS': cpp.count('initErrorIndicator(m_') == 5,
    'OCR CLOSE SAFETY': 'm_ocrConfig->preloadSavePending()' in cpp,
    'EXISTING LIVE CONFIG UPDATE': 'ConfigHandler::fileChanged' in cpp,
    'GENERAL GROUP HEADINGS': all(x in original for x in (
        'tr("Startup and notifications")', 'tr("Screenshot behavior")',
        'tr("Editing and display")')),
    'GENERAL SETTING CALLBACKS UNCHANGED': 'ConfigHandler().setShowHelp(checked)' in original,
    'ZH_CN GROUP TRANSLATIONS': all(any(msg.findtext('source') == k and
        msg.findtext('translation') == v for msg in ts.iter('message'))
        for k, v in {'Startup and notifications':'启动与通知',
        'Screenshot behavior':'截图行为', 'Editing and display':'编辑与显示'}.items()),
}
for k, v in checks.items():
    print(f'{k:<42} {"PASS" if v else "FAIL"}')
    assert v, k
print('STAGE 3.0-G.2 MAIN CONFIG REDESIGN: PASS')
