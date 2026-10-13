// SPDX-License-Identifier: GPL-3.0-or-later
// Window-local GNOME-style switch. Original QCheckBox remains the source of truth.
#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>

class GnomeSettingsSwitch final : public QAbstractButton
{
public:
    explicit GnomeSettingsSwitch(QWidget* parent = nullptr)
      : QAbstractButton(parent)
    {
        setObjectName(QStringLiteral("configGnomeSwitch"));
        setCheckable(true);
        setFixedSize(50, 30);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        m_accent = palette().color(QPalette::Highlight);
    }

    void setAccentColor(const QColor& accent)
    {
        if (accent.isValid()) m_accent = accent;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor track = isChecked() ? m_accent : QColor(QStringLiteral("#adb4be"));
        if (!isEnabled()) track.setAlpha(95);
        painter.setPen(Qt::NoPen);
        painter.setBrush(track);
        painter.drawRoundedRect(QRectF(1, 2, 48, 26), 13, 13);
        painter.setBrush(QColor(QStringLiteral("#ffffff")));
        painter.drawEllipse(QRectF(isChecked() ? 25 : 5, 5, 20, 20));
        if (hasFocus()) {
            QPen focusPen(m_accent, 1.5, Qt::DashLine);
            painter.setPen(focusPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(0.8, 1.8, 48.4, 26.4), 13, 13);
        }
    }

private:
    QColor m_accent;
};
