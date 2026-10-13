// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#pragma once

#include "tools/capturetool.h"

#include <QPointer>
#include <QWidget>

class QVBoxLayout;
class QPropertyAnimation;
class QScrollArea;
class QPushButton;
class QListWidget;
class QPushButton;
class CaptureWidget;

class UtilityPanel : public QWidget
{
    Q_OBJECT
public:
    explicit UtilityPanel(CaptureWidget* captureWidget);

    [[nodiscard]] QWidget* toolWidget() const;
    void setToolWidget(QWidget* weight);
    void clearToolWidget();
    void pushWidget(QWidget* widget);
    // Hide only the visual sentinel. Row 0 stays in the data model.
    void setEmptyLayerRowHidden(bool hidden);
    // In the Window Editor the panel occupies a real dock layout, not a
    // fullscreen animated overlay. Keep the old fullscreen behavior intact.
    void enableWindowEditorDockMode();
    void fillCaptureTools(
      const QList<QPointer<CaptureTool>>& captureToolObjectsHistory);
    void setActiveLayer(int index);
    int activeLayerIndex();
    bool isVisible() const;

signals:
    void layerChanged(int layer);
    void moveUpClicked(int currentRow);
    void moveDownClicked(int currentRow);

public slots:
    void toggle();
    void hide();
    void show();
    void slotButtonDelete(bool clicked);
    void onCurrentRowChanged(int currentRow);

private slots:
    void slotUpClicked(bool clicked);
    void slotDownClicked(bool clicked);

private:
    void initInternalPanel();
    bool m_emptyLayerRowHidden{ false };
    bool m_windowEditorDockMode{ false };

    QPointer<QWidget> m_toolWidget;
    QScrollArea* m_internalPanel;
    QVBoxLayout* m_upLayout;
    QVBoxLayout* m_bottomLayout;
    QVBoxLayout* m_layout;
    QPropertyAnimation* m_showAnimation;
    QPropertyAnimation* m_hideAnimation;
    QVBoxLayout* m_layersLayout;
    QListWidget* m_captureTools;
    QPushButton* m_buttonDelete;
    QPushButton* m_buttonMoveUp;
    QPushButton* m_buttonMoveDown;
    CaptureWidget* m_captureWidget;
};
