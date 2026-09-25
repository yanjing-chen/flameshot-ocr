// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrconf.h"

#include "ocr/localairuntimeinstaller.h"
#include "ocr/ocrmanager.h"
#include "utils/confighandler.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

OcrConf::OcrConf(QWidget* parent)
  : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);

    auto* sharedBox = new QGroupBox(tr("Local AI Runtime"), content);
    auto* sharedForm = new QFormLayout(sharedBox);

    m_sharedAppStatus = new QLabel(tr("Checking..."), sharedBox);
    m_sharedAppStatus->setWordWrap(true);
    sharedForm->addRow(tr("Runtime application:"), m_sharedAppStatus);

    m_sharedLlamaStatus = new QLabel(tr("Checking..."), sharedBox);
    m_sharedLlamaStatus->setWordWrap(true);
    sharedForm->addRow(tr("llama.cpp runtime:"), m_sharedLlamaStatus);

    m_sharedPaddleStatus = new QLabel(tr("Checking..."), sharedBox);
    m_sharedPaddleStatus->setWordWrap(true);
    sharedForm->addRow(tr("PaddleOCR-VL:"), m_sharedPaddleStatus);

    m_sharedInstallStatus =
      new QLabel(tr("Checking Local AI Runtime..."), sharedBox);
    m_sharedInstallStatus->setWordWrap(true);
    sharedForm->addRow(tr("Status:"), m_sharedInstallStatus);

    auto* sharedButtons = new QWidget(sharedBox);
    auto* sharedButtonsLayout = new QHBoxLayout(sharedButtons);
    sharedButtonsLayout->setContentsMargins(0, 0, 0, 0);

    m_installSharedRuntimeButton =
      new QPushButton(tr("Download and install"), sharedButtons);
    m_refreshSharedRuntimeButton =
      new QPushButton(tr("Refresh status"), sharedButtons);

    m_installSharedRuntimeButton->setEnabled(false);
    sharedButtonsLayout->addWidget(m_installSharedRuntimeButton);
    sharedButtonsLayout->addWidget(m_refreshSharedRuntimeButton);
    sharedButtonsLayout->addStretch();
    sharedForm->addRow(QString(), sharedButtons);

    m_sharedInstallProgress = new QProgressBar(sharedBox);
    m_sharedInstallProgress->setRange(0, 3);
    m_sharedInstallProgress->setValue(0);
    m_sharedInstallProgress->setFormat(tr("Step %v of %m"));
    m_sharedInstallProgress->setVisible(false);
    sharedForm->addRow(tr("Installation:"), m_sharedInstallProgress);

    auto* sharedNote = new QLabel(
      tr("The Local AI Runtime is installed independently for the current "
         "user and is not removed when Flameshot OCR is uninstalled."),
      sharedBox);
    sharedNote->setWordWrap(true);
    sharedForm->addRow(QString(), sharedNote);

    layout->addWidget(sharedBox);

    auto* connectionBox = new QGroupBox(tr("OCR connection"), content);
    auto* connectionForm = new QFormLayout(connectionBox);

    m_serverUrl = new QLineEdit(connectionBox);
    connectionForm->addRow(tr("API endpoint:"), m_serverUrl);

    m_modelId = new QLineEdit(connectionBox);
    connectionForm->addRow(tr("Model ID:"), m_modelId);

    m_runtimeStatus = new QLabel(connectionBox);
    m_runtimeStatus->setWordWrap(true);
    connectionForm->addRow(tr("Runtime status:"), m_runtimeStatus);

    auto* connectionButtons = new QWidget(connectionBox);
    auto* connectionButtonsLayout = new QHBoxLayout(connectionButtons);
    connectionButtonsLayout->setContentsMargins(0, 0, 0, 0);

    m_testButton =
      new QPushButton(tr("Test AI endpoint"), connectionButtons);
    connectionButtonsLayout->addWidget(m_testButton);
    connectionButtonsLayout->addStretch();
    connectionForm->addRow(QString(), connectionButtons);

    auto* connectionNote = new QLabel(
      tr("Flameshot OCR is a client of the shared Local AI Runtime. It does "
         "not start, stop, download, update, or remove llama.cpp runtimes "
         "and models."),
      connectionBox);
    connectionNote->setWordWrap(true);
    connectionForm->addRow(QString(), connectionNote);

    layout->addWidget(connectionBox);
    layout->addStretch();

    auto* sharedInstaller = LocalAiRuntimeInstaller::instance();

    connect(m_installSharedRuntimeButton,
            &QPushButton::clicked,
            this,
            [this, sharedInstaller]() {
                m_sharedInstallProgress->setVisible(true);
                m_sharedInstallProgress->setValue(0);
                m_installSharedRuntimeButton->setEnabled(false);
                m_refreshSharedRuntimeButton->setEnabled(false);
                sharedInstaller->installOrRepair();
            });

    connect(m_refreshSharedRuntimeButton,
            &QPushButton::clicked,
            this,
            &OcrConf::refreshSharedRuntimeStatus);

    connect(sharedInstaller,
            &LocalAiRuntimeInstaller::statusChanged,
            this,
            [this, sharedInstaller](bool appInstalled,
                                    const QString& appVersion,
                                    bool serviceRunning,
                                    bool endpointConflict,
                                    bool llamaInstalled,
                                    const QString& llamaVersion,
                                    bool paddleInstalled,
                                    const QString& paddleVersion,
                                    const QString& message) {
                const QString version =
                  appVersion.isEmpty() ? tr("unknown version") : appVersion;

                if (serviceRunning) {
                    m_sharedAppStatus->setText(
                      appInstalled
                        ? tr("Installed — %1 — running").arg(version)
                        : tr("Running — %1 "
                             "(external/development installation)")
                            .arg(version));
                } else if (appInstalled) {
                    m_sharedAppStatus->setText(
                      tr("Installed — %1 — service stopped").arg(version));
                } else {
                    m_sharedAppStatus->setText(tr("Not installed"));
                }

                m_sharedLlamaStatus->setText(
                  llamaInstalled
                    ? tr("Installed — %1")
                        .arg(llamaVersion.isEmpty()
                               ? tr("version unknown")
                               : llamaVersion)
                    : tr("Not installed"));

                m_sharedPaddleStatus->setText(
                  paddleInstalled
                    ? tr("Installed — %1")
                        .arg(paddleVersion.isEmpty()
                               ? QStringLiteral("1.6")
                               : paddleVersion)
                    : tr("Not installed"));

                m_sharedInstallStatus->setText(message);

                const bool installerBusy = sharedInstaller->busy();
                m_refreshSharedRuntimeButton->setEnabled(!installerBusy);
                m_installSharedRuntimeButton->setEnabled(
                  !installerBusy && !endpointConflict);

                if (endpointConflict) {
                    m_installSharedRuntimeButton->setText(
                      tr("Port 8111 is in use"));
                } else if (serviceRunning && llamaInstalled &&
                           paddleInstalled) {
                    m_installSharedRuntimeButton->setText(
                      tr("Repair / check for updates"));
                } else if (appInstalled) {
                    m_installSharedRuntimeButton->setText(
                      tr("Install missing components"));
                } else {
                    m_installSharedRuntimeButton->setText(
                      tr("Download and install"));
                }
            });

    connect(sharedInstaller,
            &LocalAiRuntimeInstaller::installProgress,
            this,
            [this](int step, int total, const QString& message) {
                m_sharedInstallProgress->setVisible(true);
                m_sharedInstallProgress->setRange(0, total);
                m_sharedInstallProgress->setValue(step);
                m_sharedInstallStatus->setText(message);
            });

    connect(sharedInstaller,
            &LocalAiRuntimeInstaller::installFinished,
            this,
            [this](bool ok, const QString& message) {
                m_sharedInstallStatus->setText(message);

                if (!ok) {
                    m_sharedInstallProgress->setVisible(false);
                    QMessageBox::warning(
                      this, tr("Local AI Runtime"), message);
                } else {
                    m_sharedInstallProgress->setRange(0, 3);
                    m_sharedInstallProgress->setValue(3);
                    QTimer::singleShot(
                      1500,
                      this,
                      [this]() {
                          m_sharedInstallProgress->setVisible(false);
                      });
                }

                refreshSharedRuntimeStatus();
                refreshRuntimeStatus();
            });

    connect(m_serverUrl, &QLineEdit::editingFinished, this, [this]() {
        ConfigHandler().setOcrServerUrl(m_serverUrl->text().trimmed());
        refreshRuntimeStatus();
    });

    connect(m_modelId, &QLineEdit::editingFinished, this, [this]() {
        QString modelId = m_modelId->text().trimmed();
        if (modelId.isEmpty()) {
            modelId = QStringLiteral("paddleocr-vl-1.6");
            m_modelId->setText(modelId);
        }
        ConfigHandler().setOcrModelId(modelId);
    });

    connect(m_testButton, &QPushButton::clicked, this, [this]() {
        refreshRuntimeStatus();
    });

    updateComponents();
}

void OcrConf::refreshSharedRuntimeStatus()
{
    auto* installer = LocalAiRuntimeInstaller::instance();

    if (!installer->busy()) {
        m_sharedAppStatus->setText(tr("Checking..."));
        m_sharedLlamaStatus->setText(tr("Checking..."));
        m_sharedPaddleStatus->setText(tr("Checking..."));
        m_sharedInstallStatus->setText(
          tr("Checking Local AI Runtime..."));
        m_installSharedRuntimeButton->setEnabled(false);
        m_refreshSharedRuntimeButton->setEnabled(false);
    }

    installer->refreshStatus();
}

void OcrConf::refreshRuntimeStatus()
{
    m_runtimeStatus->setText(tr("Testing AI endpoint..."));
    m_testButton->setEnabled(false);

    OcrManager::instance()->testConnection(
      this,
      [this](bool ok, const QString& error) {
          m_runtimeStatus->setText(
            ok ? tr("Connected — AI endpoint is healthy")
               : tr("Not connected — %1").arg(error));
          m_testButton->setEnabled(true);
      });
}

void OcrConf::updateComponents()
{
    ConfigHandler config;

    {
        const QSignalBlocker blocker(m_serverUrl);
        m_serverUrl->setText(config.ocrServerUrl());
    }

    {
        const QSignalBlocker blocker(m_modelId);
        QString modelId = config.ocrModelId().trimmed();
        if (modelId.isEmpty()) {
            modelId = QStringLiteral("paddleocr-vl-1.6");
        }
        m_modelId->setText(modelId);
    }

    refreshRuntimeStatus();
    refreshSharedRuntimeStatus();
}
