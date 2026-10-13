#!/usr/bin/env python3
from pathlib import Path

cpp = Path('src/widgets/capture/capturewidget.cpp').read_text(encoding='utf-8')
up_cpp = Path('src/widgets/panel/utilitypanel.cpp').read_text(encoding='utf-8')
up_h = Path('src/widgets/panel/utilitypanel.h').read_text(encoding='utf-8')

setter_start = cpp.index('void CaptureWidget::setWindowEditorToolSettingsVisible(bool visible)')
setter_end = cpp.index('void CaptureWidget::setState(CaptureToolButton* b)', setter_start)
setter = cpp[setter_start:setter_end]

checks = {
    'UTILITYPANEL DOCK MODE API':
        'void enableWindowEditorDockMode();' in up_h and
        'void UtilityPanel::enableWindowEditorDockMode()' in up_cpp,
    'EDITOR ENABLES DOCK MODE':
        cpp.count('canvas->m_panel->enableWindowEditorDockMode();') == 1,
    'DOCK HAS RESIZABLE INTERNAL SETTINGS':
        'dockLayout->addWidget(m_internalPanel);' in up_cpp and
        'm_internalPanel->setSizePolicy(QSizePolicy::Expanding,' in up_cpp,
    'DOCK OPENS INTERNAL CONTENT':
        'if (m_windowEditorDockMode)' in up_cpp and
        'm_internalPanel->show();' in up_cpp and
        'QWidget::show();' in up_cpp,
    'DOCK CLOSES INTERNAL CONTENT':
        'm_internalPanel->hide();' in up_cpp and
        'QWidget::hide();' in up_cpp,
    'EDITOR USES REAL UTILITYPANEL API':
        'm_panel->show();' in setter and
        'm_panel->hide();' in setter and
        'm_panel->setVisible(visible);' not in setter.split('dock->setVisible(visible);',1)[-1],
    'FULLSCREEN ANIMATION PRESERVED':
        'm_showAnimation->start();' in up_cpp and
        'm_hideAnimation->start();' in up_cpp,
    'EXACT PANEL REPARENT PRESERVED':
        cpp.count('canvas->m_panel->setParent(settingsDock);') == 1,
    'DRAWING SETTINGS WIRED':
        'm_panel->setToolWidget(b->tool()->configurationWidget());' in cpp and
        'm_panel->pushWidget(m_sidePanel);' in cpp,
    'EDITOR PIN PRESERVED':
        'flameshotWindowEditorPinnedInPlace' in cpp,
}
for label, ok in checks.items():
    print(f'{label:<48} {"PASS" if ok else "FAIL"}')
    assert ok, label
print('\nSTAGE 3.0-E.2B DOCKED PANEL CONTRACT: PASS')
