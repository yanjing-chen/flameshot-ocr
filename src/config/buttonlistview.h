// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "widgets/capture/capturetoolbutton.h"
#include <QListWidget>

class QDropEvent;
class QListWidgetItem;

class ButtonListView : public QListWidget
{
    Q_OBJECT

public:
    explicit ButtonListView(QWidget* parent = nullptr);

public slots:
    void selectAll();
    void resetOrder();
    void updateComponents();

protected:
    void dropEvent(QDropEvent* event) override;
    void initButtonList();

private slots:
    void updateActiveButtons(QListWidgetItem* item);

private:
    QList<CaptureTool::Type> m_listButtons;

    QList<CaptureTool::Type> configuredOrder() const;
    CaptureTool::Type typeForItem(const QListWidgetItem* item) const;
    void persistState();
    void reorderItems(const QList<CaptureTool::Type>& order);
};
