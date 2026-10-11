// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "windowededitortool.h"

WindowedEditorTool::WindowedEditorTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool WindowedEditorTool::closeOnButtonPressed() const
{
    return false;
}

QIcon WindowedEditorTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "open_with.svg");
}

QString WindowedEditorTool::name() const
{
    return tr("Continue in Editor");
}

CaptureTool::Type WindowedEditorTool::type() const
{
    return CaptureTool::TYPE_WINDOWED_EDITOR;
}

QString WindowedEditorTool::description() const
{
    return tr("Continue editing in a normal window");
}

CaptureTool* WindowedEditorTool::copy(QObject* parent)
{
    return new WindowedEditorTool(parent);
}

void WindowedEditorTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
    emit requestAction(REQ_WINDOWED_EDITOR);
}
