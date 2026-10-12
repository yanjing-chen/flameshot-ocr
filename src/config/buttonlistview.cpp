// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "buttonlistview.h"
#include "tools/toolfactory.h"
#include "utils/confighandler.h"

#include <QDropEvent>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QStringList>

namespace
{
QString encodeButtonOrder(const QList<CaptureTool::Type>& order)
{
    QStringList values;
    values.reserve(order.size());
    for (const auto type : order) {
        values.append(QString::number(static_cast<int>(type)));
    }
    return values.join(QLatin1Char(','));
}
}

ButtonListView::ButtonListView(QWidget* parent)
  : QListWidget(parent)
{
    setMouseTracking(true);
    setFlow(QListWidget::TopToBottom);

    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QAbstractItemView::InternalMove);

    initButtonList();
    updateComponents();

    connect(this,
            &QListWidget::itemChanged,
            this,
            &ButtonListView::updateActiveButtons);
}

QList<CaptureTool::Type> ButtonListView::configuredOrder() const
{
    const auto defaults = CaptureToolButton::getIterableButtonTypes();
    QList<CaptureTool::Type> result;

    const QString encoded = ConfigHandler().windowEditorToolbarOrder();
    const QStringList values =
      encoded.split(QLatin1Char(','), Qt::SkipEmptyParts);

    for (const QString& value : values) {
        bool ok = false;
        const int raw = value.trimmed().toInt(&ok);
        if (!ok) {
            continue;
        }

        const auto type = static_cast<CaptureTool::Type>(raw);
        if (defaults.contains(type) && !result.contains(type)) {
            result.append(type);
        }
    }

    for (const auto type : defaults) {
        if (!result.contains(type)) {
            result.append(type);
        }
    }

    return result;
}

CaptureTool::Type ButtonListView::typeForItem(
  const QListWidgetItem* item) const
{
    return static_cast<CaptureTool::Type>(
      item->data(Qt::UserRole).toInt());
}

void ButtonListView::initButtonList()
{
    ToolFactory factory;

    for (const CaptureTool::Type type : configuredOrder()) {
        CaptureTool* tool = factory.CreateTool(type);
        if (!tool) {
            continue;
        }

        auto* item = new QListWidgetItem(this);

        QColor bgColor = palette().color(QWidget::backgroundRole());
        item->setIcon(tool->icon(bgColor, false));

        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                       Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled |
                       Qt::ItemIsDropEnabled);

        QColor foregroundColor =
          palette().color(QWidget::foregroundRole());
        item->setForeground(foregroundColor);
        item->setText(tool->name());
        item->setToolTip(tool->description());
        item->setData(Qt::UserRole, static_cast<int>(type));

        tool->deleteLater();
    }
}

void ButtonListView::persistState()
{
    QList<CaptureTool::Type> order;
    QList<CaptureTool::Type> enabled;

    for (int row = 0; row < count(); ++row) {
        auto* item = this->item(row);
        const auto type = typeForItem(item);
        order.append(type);

        if (item->checkState() == Qt::Checked) {
            enabled.append(type);
        }
    }

    m_listButtons = enabled;

    ConfigHandler config;
    config.setWindowEditorToolbarOrder(encodeButtonOrder(order));
    config.setButtons(enabled);
}

void ButtonListView::reorderItems(
  const QList<CaptureTool::Type>& order)
{
    for (int targetRow = 0; targetRow < order.size(); ++targetRow) {
        const auto wanted = order.at(targetRow);

        int currentRow = -1;
        for (int row = targetRow; row < count(); ++row) {
            if (typeForItem(item(row)) == wanted) {
                currentRow = row;
                break;
            }
        }

        if (currentRow < 0 || currentRow == targetRow) {
            continue;
        }

        QListWidgetItem* moved = takeItem(currentRow);
        insertItem(targetRow, moved);
    }
}

void ButtonListView::updateActiveButtons(QListWidgetItem* item)
{
    Q_UNUSED(item)
    persistState();
}

void ButtonListView::dropEvent(QDropEvent* event)
{
    QListWidget::dropEvent(event);
    persistState();
}

void ButtonListView::selectAll()
{
    {
        const QSignalBlocker blocker(this);
        for (int row = 0; row < count(); ++row) {
            item(row)->setCheckState(Qt::Checked);
        }
    }

    persistState();
}

void ButtonListView::resetOrder()
{
    ConfigHandler().setWindowEditorToolbarOrder(QString());

    {
        const QSignalBlocker blocker(this);
        reorderItems(CaptureToolButton::getIterableButtonTypes());
    }

    persistState();
}

void ButtonListView::updateComponents()
{
    const QSignalBlocker blocker(this);

    reorderItems(configuredOrder());
    m_listButtons = ConfigHandler().buttons();

    for (int row = 0; row < count(); ++row) {
        auto* item = this->item(row);
        item->setCheckState(
          m_listButtons.contains(typeForItem(item)) ? Qt::Checked
                                                    : Qt::Unchecked);
    }
}
