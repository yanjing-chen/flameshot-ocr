// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrtool.h"

OcrTool::OcrTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool OcrTool::closeOnButtonPressed() const
{
    return false;
}

QIcon OcrTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "format-text.svg");
}

QString OcrTool::name() const
{
    return tr("OCR");
}

CaptureTool::Type OcrTool::type() const
{
    return CaptureTool::TYPE_OCR;
}

QString OcrTool::description() const
{
    return tr("Recognize text in selection with PaddleOCR-VL");
}

CaptureTool* OcrTool::copy(QObject* parent)
{
    return new OcrTool(parent);
}

void OcrTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
    emit requestAction(REQ_OCR);
}
