// SPDX-License-Identifier: GPL-3.0-or-later
// G.2D-3: GNOME-like controls without altering any model or configuration slot.
#pragma once

#include "utils/gnomeaccent.h"
#include "utils/colorutils.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLayout>
#include <QListView>
#include <QPainter>
#include <QPalette>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <QWidget>

namespace GnomeControls {

class ComboRowDelegate final : public QStyledItemDelegate {
public:
    explicit ComboRowDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        QSize result = QStyledItemDelegate::sizeHint(option, index);
        result.setHeight(qMax(result.height(), 39));
        return result;
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const bool selected = (opt.state & QStyle::State_Selected);
        const bool hovered = (opt.state & QStyle::State_MouseOver);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF itemRect = QRectF(opt.rect).adjusted(3, 2, -3, -2);
        if (selected || hovered) {
            QColor accent = opt.palette.color(QPalette::Highlight);
            QColor base = opt.palette.color(QPalette::Base);
            QColor fill((accent.red() * 17 + base.red() * 83) / 100,
                        (accent.green() * 17 + base.green() * 83) / 100,
                        (accent.blue() * 17 + base.blue() * 83) / 100);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(itemRect, 8, 8);
        }
        opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
        // Leave item text, checkmarks and accessibility to Qt's item delegate.
        QStyledItemDelegate::paint(painter, opt, index);
        painter->restore();
    }
};

inline void attachNumericSliders(QWidget* general) {
    if (!general) return;
    // Only GeneralConf: do not add sliders to model dialogs or OCR API inputs.
    const auto spinners = general->findChildren<QSpinBox*>();
    for (auto* spin : spinners) {
        if (!spin || spin->property("g2d3NumericLinked").toBool()) continue;
        if (spin->maximum() <= spin->minimum()) continue;
        QLayout* containerLayout = nullptr;
        // Some controls live in nested QHBoxLayouts, others in QGroupBox layouts.
        for (auto* candidate : general->findChildren<QLayout*>()) {
            if (candidate->indexOf(spin) >= 0) {
                containerLayout = candidate;
                break;
            }
        }
        if (!containerLayout) continue;
        auto* wrapper = new QWidget(spin->parentWidget());
        wrapper->setObjectName(QStringLiteral("g2d3NumericRow"));
        QLayoutItem* oldItem = containerLayout->replaceWidget(
            spin, wrapper, Qt::FindDirectChildrenOnly);
        if (!oldItem) { delete wrapper; continue; }
        delete oldItem;
        auto* row = new QHBoxLayout(wrapper);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(12);
        auto* slider = new QSlider(Qt::Horizontal, wrapper);
        slider->setObjectName(QStringLiteral("g2d3NumericSlider"));
        slider->setRange(spin->minimum(), spin->maximum());
        slider->setSingleStep(spin->singleStep());
        slider->setPageStep(qMax(1, (spin->maximum() - spin->minimum()) / 12));
        slider->setValue(spin->value());
        slider->setAccessibleName(spin->toolTip());
        spin->setMinimumWidth(90);
        spin->setMaximumWidth(130);
        row->addWidget(slider, 1);
        row->addWidget(spin, 0);
        QObject::connect(slider, &QSlider::valueChanged,
                         spin, &QSpinBox::setValue);
        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                         slider, &QSlider::setValue);
        spin->setProperty("g2d3NumericLinked", true);
    }
}

inline void apply(QWidget* root) {
    if (!root) return;
    const QColor accent = GnomeAccent::current(root->palette());
    const bool dark = ColorUtils::colorIsDark(root->palette().window().color());
    const QString surface = dark ? QStringLiteral("#30343b")
                                 : QStringLiteral("#ffffff");
    const QString border = dark ? QStringLiteral("#555b65")
                                : QStringLiteral("#d4d9e1");
    const QString foreground = dark ? QStringLiteral("#f1f2f4")
                                    : QStringLiteral("#242936");
    const QString selection = GnomeAccent::blend(accent, QColor(surface), 17).name();
    const QString listStyle = QStringLiteral(R"CSS(
        QListView { background: %1; color: %2; border: 1px solid %3;
            border-radius: 12px; padding: 5px; outline: 0; }
        QListView::item { border: 0; }
    )CSS").arg(surface, foreground, border);
    for (auto* combo : root->findChildren<QComboBox*>()) {
        if (!combo || combo->property("g2d3Ignore").toBool()) continue;
        combo->setMinimumHeight(40);
        combo->setStyleSheet(QStringLiteral(R"CSS(
            QComboBox { background: %1; color: %2; border: 1px solid %3;
                border-radius: 10px; padding: 7px 30px 7px 12px; }
            QComboBox:hover { border-color: %4; }
            QComboBox:focus { border-color: %4; }
            QComboBox::drop-down { border: none; width: 28px; }
            QComboBox QAbstractItemView { background: %1; color: %2;
                border: 1px solid %3; border-radius: 12px;
                selection-background-color: %5; }
        )CSS").arg(surface, foreground, border, accent.name(), selection));
        auto* view = combo->view();
        if (view) {
            if (!view->property("g2d3DelegateInstalled").toBool()) {
                view->setItemDelegate(new ComboRowDelegate(view));
                view->setProperty("g2d3DelegateInstalled", true);
            }
            view->setStyleSheet(listStyle);
            view->setMouseTracking(true);
            view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        }
    }
    const QString spinStyle = QStringLiteral(R"CSS(
        QSpinBox, QDoubleSpinBox { background: %1; color: %2;
            border: 1px solid %3; border-radius: 10px;
            padding: 6px 23px 6px 10px; min-height: 30px; }
        QSpinBox:focus, QDoubleSpinBox:focus { border-color: %4; }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border; subcontrol-position: top right;
            width: 21px; border-left: 1px solid %3; border-top-right-radius: 8px; }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border; subcontrol-position: bottom right;
            width: 21px; border-left: 1px solid %3; border-bottom-right-radius: 8px; }
    )CSS").arg(surface, foreground, border, accent.name());
    for (auto* spin : root->findChildren<QSpinBox*>()) spin->setStyleSheet(spinStyle);
    const QString sliderStyle = QStringLiteral(R"CSS(
        QSlider::groove:horizontal { height: 6px; border-radius: 3px;
            background: %1; }
        QSlider::sub-page:horizontal { background: %2; border-radius: 3px; }
        QSlider::handle:horizontal { background: %3; border: 3px solid %2;
            width: 16px; height: 16px; margin: -8px 0;
            border-radius: 10px; }
        QSlider::handle:horizontal:disabled { border-color: %1; }
    )CSS").arg(border, accent.name(), surface);
    for (auto* slider : root->findChildren<QSlider*>()) {
        if (slider->orientation() == Qt::Horizontal)
            slider->setStyleSheet(sliderStyle);
    }
}

} // namespace GnomeControls
