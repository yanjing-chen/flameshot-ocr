// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

class OcrConf : public QWidget
{
    Q_OBJECT

public:
    explicit OcrConf(QWidget* parent = nullptr);

public slots:
    void updateComponents();

private:
    void refreshRuntimeStatus();
    void refreshSharedRuntimeStatus();

    QLineEdit* m_serverUrl;
    QLineEdit* m_modelId;
    QLabel* m_runtimeStatus;

    QLabel* m_sharedAppStatus;
    QLabel* m_sharedLlamaStatus;
    QLabel* m_sharedPaddleStatus;
    QLabel* m_sharedInstallStatus;

    QPushButton* m_testButton;
    QPushButton* m_installSharedRuntimeButton;
    QPushButton* m_refreshSharedRuntimeButton;

    QProgressBar* m_sharedInstallProgress;
};
