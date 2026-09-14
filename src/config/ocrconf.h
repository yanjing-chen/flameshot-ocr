// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
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
    void rebuildModelList();
    void rebuildDeviceList();
    void refreshModelStatus();
    void refreshCudaRuntimeStatus();
    void refreshRuntimeStatus();

    QLineEdit* m_serverUrl;
    QLineEdit* m_serverPath;
    QLineEdit* m_modelRoot;
    QLineEdit* m_manifestUrl;

    QComboBox* m_modelCombo;
    QComboBox* m_deviceCombo;
    QLabel* m_modelStatus;
    QLabel* m_runtimeStatus;
    QLabel* m_deviceStatus;
    QLabel* m_latestStatus;
    QLabel* m_cudaRuntimeStatus;
    QLabel* m_cudaUpdateStatus;
    QLabel* m_cudaDownloadLabel;

    QCheckBox* m_autoStart;
    QCheckBox* m_autoCheckUpdates;

    QPushButton* m_downloadButton;
    QPushButton* m_cancelDownloadButton;
    QPushButton* m_deleteButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_testButton;
    QPushButton* m_checkUpdatesButton;

    QPushButton* m_installCudaButton;
    QPushButton* m_checkCudaUpdatesButton;
    QPushButton* m_cancelCudaButton;

    QProgressBar* m_downloadProgress;
    QProgressBar* m_cudaDownloadProgress;
};
