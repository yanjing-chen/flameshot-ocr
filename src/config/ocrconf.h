// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QJsonObject>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QComboBox;
class QTableWidget;

class OcrConf : public QWidget
{
    Q_OBJECT

public:
    explicit OcrConf(QWidget* parent = nullptr);
    bool preloadSavePending() const;

public slots:
    void updateComponents();

private:
    using JsonCallback =
      std::function<void(bool, const QJsonObject&, const QString&)>;

    void refreshRuntimeStatus();
    void refreshSharedRuntimeStatus();
    void refreshModelList();
    void refreshPreloadPreference();
    void setPreloadPreference(bool enabled);
    void refreshAccelerationStatus();
    void refreshCudaBackendStatus(bool refreshManifest);
    void setBackendPreference(int index);
    void requestCudaBackendInstall();
    void requestCudaBackendRollback();
    void addCustomModel();
    void editSelectedCustomModel();
    void removeSelectedModel();
    void useSelectedModelForOcr();
    void updateModelButtons();
    QJsonObject selectedModel() const;
    void sendModelRequest(const QString& method,
                          const QString& path,
                          const QJsonObject& payload,
                          JsonCallback callback);

    QLineEdit* m_serverUrl;
    QLineEdit* m_modelId;
    QComboBox* m_outputFormat;
    QLabel* m_runtimeStatus;
    QCheckBox* m_preloadOcrModel;
    QLabel* m_preloadStatus;
    bool m_preloadSavePending = false;

    QComboBox* m_backendPreference;
    QLabel* m_backendStatus;
    QLabel* m_cudaBackendStatus;
    QPushButton* m_refreshBackendsButton;
    QPushButton* m_cudaInstallButton;
    QPushButton* m_cudaRollbackButton;
    bool m_backendSavePending = false;
    bool m_cudaOperationRunning = false;

    QNetworkAccessManager* m_modelNetwork;
    QCheckBox* m_showUnavailableModels;
    QTableWidget* m_modelTable;
    QLabel* m_modelStatus;
    QPushButton* m_refreshModelsButton;
    QPushButton* m_addModelButton;
    QPushButton* m_editModelButton;
    QPushButton* m_removeEntryButton;
    QPushButton* m_useModelButton;

    QLabel* m_sharedAppStatus;
    QLabel* m_sharedLlamaStatus;
    QLabel* m_sharedPaddleStatus;
    QLabel* m_sharedInstallStatus;

    QPushButton* m_testButton;
    QPushButton* m_installSharedRuntimeButton;
    QPushButton* m_refreshSharedRuntimeButton;

    QProgressBar* m_sharedInstallProgress;
};
