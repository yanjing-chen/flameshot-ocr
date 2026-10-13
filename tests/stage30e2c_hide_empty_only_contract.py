#!/usr/bin/env python3
from pathlib import Path
import subprocess

up_h = Path('src/widgets/panel/utilitypanel.h').read_text(encoding='utf-8')
up_cpp = Path('src/widgets/panel/utilitypanel.cpp').read_text(encoding='utf-8')
editor = Path('src/widgets/capture/capturewidget.cpp').read_text(encoding='utf-8')

checks = {
    'SENTINEL IS STILL CREATED':
        'm_captureTools->addItem(tr("<Empty>"));' in up_cpp,
    'ROW 0 HIDDEN WHEN REQUESTED':
        'm_captureTools->setRowHidden(0, m_emptyLayerRowHidden);' in up_cpp,
    'INITIAL ROW ALSO UPDATED':
        'm_captureTools->setRowHidden(0, hidden);' in up_cpp,
    'EDITOR ONLY ENABLES HIDING':
        editor.count('canvas->m_panel->setEmptyLayerRowHidden(true);') == 1,
    'API DECLARED':
        'void setEmptyLayerRowHidden(bool hidden);' in up_h,
    'FULLSCREEN DEFAULT STILL VISIBLE':
        'bool m_emptyLayerRowHidden{ false };' in up_h,
    'LAYER INDEXING UNCHANGED':
        'm_captureTools->currentRow() - 1' in up_cpp,
    'E2B DOCK STILL ACTIVE':
        'canvas->m_panel->enableWindowEditorDockMode();' in editor,
    'NO AUTOMATIC WINDOW EXPANSION':
        'windowEditorGeometryBeforeSettingsDock' not in editor,
}

for name, condition in checks.items():
    print(f'{name:<43} {"PASS" if condition else "FAIL"}')
    assert condition, name

modified_files = set(subprocess.check_output(
    ['git', 'diff', '--name-only'], text=True).splitlines())
allowed = {
    'src/widgets/panel/utilitypanel.cpp',
    'src/widgets/panel/utilitypanel.h',
    'src/widgets/capture/capturewidget.cpp',
    '.github/workflows/build.yml',
}
assert modified_files <= allowed, f'Unexpected modified tracked files: {modified_files - allowed}'
print('\nSTAGE 3.0-E.2C HIDE EMPTY ONLY CONTRACT: PASS')
