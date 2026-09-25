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
class QTableWidget;

class OcrConf : public QWidget
{
    Q_OBJECT

public:
    explicit OcrConf(QWidget* parent = nullptr);

public slots:
    void updateComponents();

private:
    using JsonCallback =
      std::function<void(bool, const QJsonObject&, const QString&)>;

    void refreshRuntimeStatus();
    void refreshSharedRuntimeStatus();
    void refreshModelList();
    void addCustomModel();
    void editSelectedCustomModel();
    void removeSelectedCustomModel();
    void useSelectedModelForOcr();
    void updateModelButtons();
    QJsonObject selectedModel() const;
    void sendModelRequest(const QString& method,
                          const QString& path,
                          const QJsonObject& payload,
                          JsonCallback callback);

    QLineEdit* m_serverUrl;
    QLineEdit* m_modelId;
    QLabel* m_runtimeStatus;

    QNetworkAccessManager* m_modelNetwork;
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
