// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrconf.h"

#include "ocr/ocrmanager.h"
#include "utils/confighandler.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
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

    auto* serviceBox = new QGroupBox(tr("PaddleOCR-VL Service"), content);
    auto* serviceForm = new QFormLayout(serviceBox);

    m_serverUrl = new QLineEdit(serviceBox);
    serviceForm->addRow(tr("Server URL:"), m_serverUrl);

    auto* serverPathRow = new QWidget(serviceBox);
    auto* serverPathLayout = new QHBoxLayout(serverPathRow);
    serverPathLayout->setContentsMargins(0, 0, 0, 0);
    m_serverPath = new QLineEdit(serverPathRow);
    auto* browseServer = new QPushButton(tr("Browse..."), serverPathRow);
    serverPathLayout->addWidget(m_serverPath);
    serverPathLayout->addWidget(browseServer);
    serviceForm->addRow(tr("llama-server:"), serverPathRow);

    m_deviceCombo = new QComboBox(serviceBox);
    serviceForm->addRow(tr("Inference device:"), m_deviceCombo);

    m_autoStart = new QCheckBox(
      tr("Automatically start the local OCR service when OCR is used"),
      serviceBox);
    serviceForm->addRow(QString(), m_autoStart);

    m_runtimeStatus = new QLabel(serviceBox);
    m_runtimeStatus->setWordWrap(true);
    serviceForm->addRow(tr("Service status:"), m_runtimeStatus);

    m_deviceStatus = new QLabel(serviceBox);
    m_deviceStatus->setWordWrap(true);
    serviceForm->addRow(tr("Acceleration:"), m_deviceStatus);

    auto* serviceButtons = new QWidget(serviceBox);
    auto* serviceButtonsLayout = new QHBoxLayout(serviceButtons);
    serviceButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_startButton = new QPushButton(tr("Start"), serviceButtons);
    m_stopButton = new QPushButton(tr("Stop"), serviceButtons);
    m_testButton = new QPushButton(tr("Test connection"), serviceButtons);
    serviceButtonsLayout->addWidget(m_startButton);
    serviceButtonsLayout->addWidget(m_stopButton);
    serviceButtonsLayout->addWidget(m_testButton);
    serviceButtonsLayout->addStretch();
    serviceForm->addRow(QString(), serviceButtons);

    layout->addWidget(serviceBox);

    auto* modelBox = new QGroupBox(tr("OCR Model"), content);
    auto* modelForm = new QFormLayout(modelBox);

    m_modelCombo = new QComboBox(modelBox);
    modelForm->addRow(tr("Model:"), m_modelCombo);

    auto* modelRootRow = new QWidget(modelBox);
    auto* modelRootLayout = new QHBoxLayout(modelRootRow);
    modelRootLayout->setContentsMargins(0, 0, 0, 0);
    m_modelRoot = new QLineEdit(modelRootRow);
    auto* browseModelRoot = new QPushButton(tr("Browse..."), modelRootRow);
    modelRootLayout->addWidget(m_modelRoot);
    modelRootLayout->addWidget(browseModelRoot);
    modelForm->addRow(tr("Model storage:"), modelRootRow);

    m_modelStatus = new QLabel(modelBox);
    m_modelStatus->setWordWrap(true);
    modelForm->addRow(tr("Status:"), m_modelStatus);

    auto* modelButtons = new QWidget(modelBox);
    auto* modelButtonsLayout = new QHBoxLayout(modelButtons);
    modelButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_downloadButton = new QPushButton(tr("Download model"), modelButtons);
    m_cancelDownloadButton =
      new QPushButton(tr("Cancel download"), modelButtons);
    m_deleteButton = new QPushButton(tr("Delete model"), modelButtons);
    modelButtonsLayout->addWidget(m_downloadButton);
    modelButtonsLayout->addWidget(m_cancelDownloadButton);
    modelButtonsLayout->addWidget(m_deleteButton);
    modelButtonsLayout->addStretch();
    modelForm->addRow(QString(), modelButtons);

    m_downloadProgress = new QProgressBar(modelBox);
    m_downloadProgress->setRange(0, 1000);
    m_downloadProgress->setVisible(false);
    modelForm->addRow(tr("Download:"), m_downloadProgress);

    layout->addWidget(modelBox);

    auto* updateBox = new QGroupBox(tr("Model Updates"), content);
    auto* updateForm = new QFormLayout(updateBox);

    m_manifestUrl = new QLineEdit(updateBox);
    m_manifestUrl->setPlaceholderText(
      tr("Optional HTTPS URL to a verified models.json manifest"));
    updateForm->addRow(tr("Remote manifest:"), m_manifestUrl);

    m_autoCheckUpdates =
      new QCheckBox(tr("Automatically check the configured manifest"),
                    updateBox);
    updateForm->addRow(QString(), m_autoCheckUpdates);

    m_latestStatus = new QLabel(updateBox);
    m_latestStatus->setWordWrap(true);
    updateForm->addRow(tr("Latest supported:"), m_latestStatus);

    m_checkUpdatesButton =
      new QPushButton(tr("Check model updates"), updateBox);
    updateForm->addRow(QString(), m_checkUpdatesButton);

    auto* note = new QLabel(
      tr("Flameshot OCR ships with a built-in verified model list. "
         "Models are never bundled inside the AppImage. A remote manifest "
         "can add newer models after compatibility has been verified."),
      updateBox);
    note->setWordWrap(true);
    updateForm->addRow(QString(), note);

    layout->addWidget(updateBox);
    layout->addStretch();

    connect(m_serverUrl, &QLineEdit::editingFinished, this, [this]() {
        ConfigHandler().setOcrServerUrl(m_serverUrl->text().trimmed());
        refreshRuntimeStatus();
    });
    connect(m_serverPath, &QLineEdit::editingFinished, this, [this]() {
        ConfigHandler().setOcrServerPath(m_serverPath->text().trimmed());
        rebuildDeviceList();
        refreshRuntimeStatus();
    });
    connect(browseServer, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
          this, tr("Choose llama-server executable"), m_serverPath->text());
        if (!path.isEmpty()) {
            m_serverPath->setText(path);
            ConfigHandler().setOcrServerPath(path);
            rebuildDeviceList();
            refreshRuntimeStatus();
        }
    });

    connect(m_modelRoot, &QLineEdit::editingFinished, this, [this]() {
        ConfigHandler().setOcrModelRoot(m_modelRoot->text().trimmed());
        refreshModelStatus();
    });
    connect(browseModelRoot, &QPushButton::clicked, this, [this]() {
        const QString path =
          QFileDialog::getExistingDirectory(
            this,
            tr("Choose OCR model storage"),
            OcrManager::instance()->modelRoot());
        if (!path.isEmpty()) {
            m_modelRoot->setText(path);
            ConfigHandler().setOcrModelRoot(path);
            refreshModelStatus();
        }
    });

    connect(m_deviceCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                if (index < 0) {
                    return;
                }

                ConfigHandler().setOcrDeviceId(
                  m_deviceCombo->itemData(index).toString());
                refreshRuntimeStatus();
            });

    connect(m_modelCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                if (index < 0) {
                    return;
                }
                ConfigHandler().setOcrModelId(
                  m_modelCombo->itemData(index).toString());
                refreshModelStatus();
                refreshRuntimeStatus();
            });

    connect(m_autoStart, &QCheckBox::toggled, this, [](bool checked) {
        ConfigHandler().setOcrAutoStartServer(checked);
    });
    connect(m_autoCheckUpdates, &QCheckBox::toggled, this, [](bool checked) {
        ConfigHandler().setOcrAutoCheckModelUpdates(checked);
    });
    connect(m_manifestUrl, &QLineEdit::editingFinished, this, [this]() {
        ConfigHandler().setOcrManifestUrl(m_manifestUrl->text().trimmed());
    });

    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        const QString id = m_modelCombo->currentData().toString();
        if (!id.isEmpty()) {
            m_downloadProgress->setValue(0);
            m_downloadProgress->setVisible(true);
            m_downloadButton->setEnabled(false);
            m_cancelDownloadButton->setEnabled(true);
            OcrManager::instance()->downloadModel(id);
        }
    });
    connect(m_cancelDownloadButton,
            &QPushButton::clicked,
            OcrManager::instance(),
            &OcrManager::cancelDownload);

    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        const QString id = m_modelCombo->currentData().toString();
        if (id.isEmpty()) {
            return;
        }
        if (QMessageBox::question(
              this,
              tr("Delete OCR model"),
              tr("Delete the selected OCR model from disk?"),
              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!OcrManager::instance()->removeModel(id, &error)) {
            QMessageBox::warning(this, tr("OCR"), error);
        }
        refreshModelStatus();
    });

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!OcrManager::instance()->startServer(&error)) {
            QMessageBox::warning(this, tr("OCR"), error);
        }
        QTimer::singleShot(300, this, &OcrConf::refreshRuntimeStatus);
    });
    connect(m_stopButton,
            &QPushButton::clicked,
            OcrManager::instance(),
            &OcrManager::stopServer);
    connect(m_testButton, &QPushButton::clicked, this, [this]() {
        m_runtimeStatus->setText(tr("Testing..."));
        OcrManager::instance()->testConnection(
          this,
          [this](bool ok, const QString& error) {
              m_runtimeStatus->setText(
                ok ? tr("Connected — OCR service is healthy")
                   : tr("Not connected — %1").arg(error));
          });
    });

    connect(m_checkUpdatesButton, &QPushButton::clicked, this, [this]() {
        ConfigHandler().setOcrManifestUrl(m_manifestUrl->text().trimmed());
        m_latestStatus->setText(tr("Checking..."));
        OcrManager::instance()->checkRemoteManifest(
          this,
          [this](bool ok, const QString& message) {
              m_latestStatus->setText(message);
              if (ok) {
                  rebuildModelList();
                  refreshModelStatus();
              }
          });
    });

    connect(OcrManager::instance(),
            &OcrManager::downloadProgress,
            this,
            [this](qint64 done, qint64 total, const QString&) {
                if (total <= 0) {
                    return;
                }
                const int value =
                  static_cast<int>((done * 1000) / total);
                m_downloadProgress->setValue(qBound(0, value, 1000));
            });

    connect(OcrManager::instance(),
            &OcrManager::downloadFinished,
            this,
            [this](bool, const QString& message) {
                m_downloadButton->setEnabled(true);
                m_cancelDownloadButton->setEnabled(false);
                QMessageBox::information(this, tr("OCR"), message);
                refreshModelStatus();
                refreshRuntimeStatus();
            });

    connect(OcrManager::instance(),
            &OcrManager::serverStateChanged,
            this,
            &OcrConf::refreshRuntimeStatus);
    connect(OcrManager::instance(),
            &OcrManager::manifestChanged,
            this,
            [this]() {
                rebuildModelList();
                refreshModelStatus();
            });

    updateComponents();

    if (ConfigHandler().ocrAutoCheckModelUpdates() &&
        !ConfigHandler().ocrManifestUrl().trimmed().isEmpty()) {
        QTimer::singleShot(1000, this, [this]() {
            OcrManager::instance()->checkRemoteManifest(
              this,
              [this](bool, const QString& message) {
                  m_latestStatus->setText(message);
                  rebuildModelList();
                  refreshModelStatus();
              });
        });
    }
}

void OcrConf::rebuildModelList()
{
    const QString selected = ConfigHandler().ocrModelId();
    const QSignalBlocker blocker(m_modelCombo);
    m_modelCombo->clear();

    const auto models = OcrManager::instance()->availableModels();
    for (const auto& model : models) {
        m_modelCombo->addItem(model.name, model.id);
    }

    int index = m_modelCombo->findData(selected);
    if (index < 0 && m_modelCombo->count() > 0) {
        index = 0;
    }
    if (index >= 0) {
        m_modelCombo->setCurrentIndex(index);
        ConfigHandler().setOcrModelId(m_modelCombo->itemData(index).toString());
    }
}

void OcrConf::rebuildDeviceList()
{
    QString selected = ConfigHandler().ocrDeviceId().trimmed();
    if (selected.isEmpty()) {
        selected = QStringLiteral("auto");
    }

    const QSignalBlocker blocker(m_deviceCombo);
    m_deviceCombo->clear();

    m_deviceCombo->addItem(tr("Automatic (recommended)"),
                           QStringLiteral("auto"));

    const auto devices = OcrManager::instance()->availableDevices();
    for (const auto& device : devices) {
        const QString label =
          tr("%1 — %2 [%3]").arg(device.name, device.backend, device.id);
        m_deviceCombo->addItem(label, device.id);
    }

    m_deviceCombo->addItem(tr("CPU"), QStringLiteral("cpu"));

    int index = m_deviceCombo->findData(selected);
    if (index < 0) {
        m_deviceCombo->addItem(
          tr("%1 (currently unavailable)").arg(selected), selected);
        index = m_deviceCombo->count() - 1;
    }

    m_deviceCombo->setCurrentIndex(index);
}

void OcrConf::refreshModelStatus()
{
    const OcrModelInfo model = OcrManager::instance()->activeModel();
    const QString directory = OcrManager::instance()->modelDirectory(model);
    m_modelStatus->setText(
      tr("%1\n%2").arg(OcrManager::instance()->modelStatusText(model),
                       directory));

    const bool installed = OcrManager::instance()->modelInstalled(model);
    m_downloadButton->setEnabled(!installed);
    m_deleteButton->setEnabled(
      QDir(OcrManager::instance()->modelDirectory(model)).exists());
}

void OcrConf::refreshRuntimeStatus()
{
    const QString executable = OcrManager::instance()->serverExecutable();
    const QString preference = ConfigHandler().ocrDeviceId().trimmed();
    const bool automatic =
      preference.isEmpty() ||
      preference.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0;

    m_deviceStatus->setText(
      executable.isEmpty()
        ? tr("llama-server not found")
        : automatic
            ? tr("%1 (auto-selected)")
                .arg(OcrManager::instance()->detectedDevice())
            : tr("%1 (manually selected)")
                .arg(OcrManager::instance()->detectedDevice()));

    if (OcrManager::instance()->managedServerRunning()) {
        m_runtimeStatus->setText(tr("Managed llama-server process is running"));
    } else {
        m_runtimeStatus->setText(
          tr("No managed process (an external server may still be running)"));
    }

    const bool managed = OcrManager::instance()->managedServerRunning();
    m_startButton->setEnabled(!managed);
    m_stopButton->setEnabled(managed);
    m_deviceCombo->setEnabled(!managed);
}

void OcrConf::updateComponents()
{
    ConfigHandler config;

    {
        const QSignalBlocker blocker(m_serverUrl);
        m_serverUrl->setText(config.ocrServerUrl());
    }
    {
        const QSignalBlocker blocker(m_serverPath);
        const QString configured = config.ocrServerPath().trimmed();
        const bool configuredUsable =
          !configured.isEmpty() && QFileInfo(configured).isExecutable();
        const QString resolved = OcrManager::instance()->serverExecutable();

        // Do not display a stale remembered path (for example /usr/lib from a
        // previously installed .deb) when the AppImage is actually using its
        // own bundled runtime.
        m_serverPath->setText(configuredUsable ? configured : resolved);
    }
    {
        const QSignalBlocker blocker(m_modelRoot);
        m_modelRoot->setText(OcrManager::instance()->modelRoot());
    }
    {
        const QSignalBlocker blocker(m_manifestUrl);
        m_manifestUrl->setText(config.ocrManifestUrl());
    }
    {
        const QSignalBlocker blocker(m_autoStart);
        m_autoStart->setChecked(config.ocrAutoStartServer());
    }
    {
        const QSignalBlocker blocker(m_autoCheckUpdates);
        m_autoCheckUpdates->setChecked(config.ocrAutoCheckModelUpdates());
    }

    rebuildModelList();
    rebuildDeviceList();
    refreshModelStatus();
    refreshRuntimeStatus();

    const QString latest = OcrManager::instance()->latestModelId();
    const QString active = OcrManager::instance()->activeModel().id;
    if (latest == active) {
        m_latestStatus->setText(tr("%1 — current").arg(latest));
    } else {
        m_latestStatus->setText(tr("%1 — update available").arg(latest));
    }

    m_cancelDownloadButton->setEnabled(false);
}
