// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrconf.h"

#include "ocr/localairuntimeinstaller.h"
#include "ocr/ocrmanager.h"
#include "utils/confighandler.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
QString dialogTr(const char* source)
{
    return QCoreApplication::translate("CustomModelDialog", source);
}

using InspectResultCallback =
  std::function<void(bool, const QJsonObject&, const QString&)>;
using InspectFunction =
  std::function<void(const QJsonObject&, InspectResultCallback)>;

QString localizedInspectionWarning(const QString& warning)
{
    if (warning ==
        QStringLiteral("The suggested context is capped at 4096 for a "
                       "conservative memory default.")) {
        return dialogTr(
          "The suggested context is capped at 4096 for a conservative "
          "memory default.");
    }
    if (warning ==
        QStringLiteral("Several MMProj candidates are similarly likely; "
                       "choose one manually.")) {
        return dialogTr(
          "Several MMProj candidates are similarly likely; choose one "
          "manually.");
    }
    if (warning ==
        QStringLiteral("A possible MMProj was found but was not selected "
                       "automatically.")) {
        return dialogTr(
          "A possible MMProj was found but was not selected automatically.");
    }
    if (warning ==
        QStringLiteral("GGUF has no general.architecture metadata; verify "
                       "compatibility manually.")) {
        return dialogTr(
          "GGUF has no general.architecture metadata; verify compatibility "
          "manually.");
    }
    if (warning ==
        QStringLiteral("Only the first 64 same-directory GGUF files were "
                       "checked.")) {
        return dialogTr(
          "Only the first 64 same-directory GGUF files were checked.");
    }
    if (warning ==
        QStringLiteral("The selected MMProj has no recognized projector "
                       "metadata; verify it manually.")) {
        return dialogTr(
          "The selected MMProj has no recognized projector metadata; verify "
          "it manually.");
    }
    return warning;
}

class CustomModelDialog : public QDialog
{
public:
    explicit CustomModelDialog(const QJsonObject& model,
                               InspectFunction inspector,
                               QWidget* parent = nullptr)
      : QDialog(parent)
      , m_editing(!model.isEmpty())
      , m_inspector(inspector)
    {
        setWindowTitle(m_editing ? dialogTr("Edit custom model")
                                 : dialogTr("Import custom GGUF model"));
        setMinimumWidth(620);

        auto* layout = new QVBoxLayout(this);
        auto* form = new QFormLayout();
        layout->addLayout(form);

        m_id = new QLineEdit(this);
        m_id->setPlaceholderText(QStringLiteral("my-model"));
        m_id->setEnabled(!m_editing);
        form->addRow(dialogTr("Model ID:"), m_id);

        m_name = new QLineEdit(this);
        form->addRow(dialogTr("Display name:"), m_name);

        m_type = new QComboBox(this);
        m_type->addItem(dialogTr("Chat"), QStringLiteral("chat"));
        m_type->addItem(dialogTr("Translation"),
                        QStringLiteral("translation"));
        m_type->addItem(dialogTr("OCR"), QStringLiteral("ocr"));
        m_type->addItem(dialogTr("Vision"), QStringLiteral("vision"));
        m_type->addItem(dialogTr("Other"), QStringLiteral("other"));
        form->addRow(dialogTr("Model type:"), m_type);

        auto* modelPathRow = new QWidget(this);
        auto* modelPathLayout = new QHBoxLayout(modelPathRow);
        modelPathLayout->setContentsMargins(0, 0, 0, 0);
        m_modelPath = new QLineEdit(modelPathRow);
        auto* browseModel =
          new QPushButton(dialogTr("Browse..."), modelPathRow);
        m_inspectButton =
          new QPushButton(dialogTr("Analyze GGUF"), modelPathRow);
        modelPathLayout->addWidget(m_modelPath, 1);
        modelPathLayout->addWidget(browseModel);
        modelPathLayout->addWidget(m_inspectButton);
        form->addRow(dialogTr("GGUF model:"), modelPathRow);

        m_inspectionStatus = new QLabel(
          dialogTr("Choose a GGUF model. Runtime 0.6.0 will read its metadata "
                   "and suggest editable settings."),
          this);
        m_inspectionStatus->setWordWrap(true);
        form->addRow(dialogTr("Automatic analysis:"), m_inspectionStatus);

        auto* mmprojPathRow = new QWidget(this);
        auto* mmprojPathLayout = new QHBoxLayout(mmprojPathRow);
        mmprojPathLayout->setContentsMargins(0, 0, 0, 0);
        m_mmprojPath = new QLineEdit(mmprojPathRow);
        auto* browseMmproj =
          new QPushButton(dialogTr("Browse..."), mmprojPathRow);
        mmprojPathLayout->addWidget(m_mmprojPath, 1);
        mmprojPathLayout->addWidget(browseMmproj);
        form->addRow(dialogTr("MMProj (optional):"), mmprojPathRow);

        m_contextSize = new QSpinBox(this);
        m_contextSize->setRange(512, 1048576);
        m_contextSize->setSingleStep(512);
        m_contextSize->setValue(4096);
        form->addRow(dialogTr("Context size:"), m_contextSize);

        m_gpuLayers = new QSpinBox(this);
        m_gpuLayers->setRange(0, 999);
        m_gpuLayers->setValue(99);
        form->addRow(dialogTr("GPU layers:"), m_gpuLayers);

        m_defaultPrompt = new QPlainTextEdit(this);
        m_defaultPrompt->setMaximumHeight(90);
        form->addRow(dialogTr("Default prompt:"), m_defaultPrompt);

        auto* capabilitiesBox =
          new QGroupBox(dialogTr("Capabilities"), this);
        auto* capabilitiesLayout = new QGridLayout(capabilitiesBox);
        m_chat = new QCheckBox(dialogTr("Chat"), capabilitiesBox);
        m_translation =
          new QCheckBox(dialogTr("Translation"), capabilitiesBox);
        m_ocr = new QCheckBox(dialogTr("OCR"), capabilitiesBox);
        m_vision = new QCheckBox(dialogTr("Vision"), capabilitiesBox);
        m_streaming =
          new QCheckBox(dialogTr("Streaming"), capabilitiesBox);
        m_thinking =
          new QCheckBox(dialogTr("Thinking"), capabilitiesBox);
        m_temperature =
          new QCheckBox(dialogTr("Supports temperature parameter"),
                        capabilitiesBox);
        m_customPrompt =
          new QCheckBox(dialogTr("Custom prompt"), capabilitiesBox);
        m_contextCapability =
          new QCheckBox(dialogTr("Context size"), capabilitiesBox);

        capabilitiesLayout->addWidget(m_chat, 0, 0);
        capabilitiesLayout->addWidget(m_translation, 0, 1);
        capabilitiesLayout->addWidget(m_ocr, 0, 2);
        capabilitiesLayout->addWidget(m_vision, 1, 0);
        capabilitiesLayout->addWidget(m_streaming, 1, 1);
        capabilitiesLayout->addWidget(m_thinking, 1, 2);
        capabilitiesLayout->addWidget(m_temperature, 2, 0);
        capabilitiesLayout->addWidget(m_customPrompt, 2, 1);
        capabilitiesLayout->addWidget(m_contextCapability, 2, 2);
        layout->addWidget(capabilitiesBox);

        auto* note = new QLabel(
          dialogTr("The Runtime references these files in place. Adding or "
                   "removing an entry never copies or deletes GGUF files."),
          this);
        note->setWordWrap(true);
        layout->addWidget(note);

        auto* buttons = new QDialogButtonBox(
          QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        m_saveButton = buttons->button(QDialogButtonBox::Save);
        layout->addWidget(buttons);

        connect(browseModel, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
              this,
              dialogTr("Choose GGUF model"),
              m_modelPath->text(),
              dialogTr("GGUF models (*.gguf);;All files (*)"));
            if (!path.isEmpty()) {
                m_modelPath->setText(path);
                inspectModel();
            }
        });

        connect(m_inspectButton,
                &QPushButton::clicked,
                this,
                [this]() { inspectModel(); });

        connect(browseMmproj, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
              this,
              dialogTr("Choose MMProj GGUF"),
              m_mmprojPath->text(),
              dialogTr("GGUF models (*.gguf);;All files (*)"));
            if (!path.isEmpty()) {
                m_mmprojPath->setText(path);
                m_vision->setChecked(true);
            }
        });

        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            QString error;
            if (!validate(&error)) {
                QMessageBox::warning(
                  this, dialogTr("Custom model"), error);
                return;
            }
            accept();
        });

        m_streaming->setChecked(true);
        m_temperature->setChecked(true);
        m_customPrompt->setChecked(true);
        m_contextCapability->setChecked(true);

        if (m_editing) {
            load(model);
        } else {
            m_chat->setChecked(true);
        }

        connect(m_type,
                &QComboBox::currentIndexChanged,
                this,
                [this](int) { applyTypeDefaults(); });
        connect(m_mmprojPath, &QLineEdit::textChanged, this, [this]() {
            if (!m_mmprojPath->text().trimmed().isEmpty()) {
                m_vision->setChecked(true);
            }
        });
    }

    QJsonObject payload() const
    {
        QJsonObject capabilities;
        capabilities.insert(QStringLiteral("chat"), m_chat->isChecked());
        capabilities.insert(QStringLiteral("translation"),
                            m_translation->isChecked());
        capabilities.insert(QStringLiteral("ocr"), m_ocr->isChecked());
        capabilities.insert(QStringLiteral("vision"), m_vision->isChecked());
        capabilities.insert(QStringLiteral("streaming"),
                            m_streaming->isChecked());
        capabilities.insert(QStringLiteral("thinking"),
                            m_thinking->isChecked());
        capabilities.insert(QStringLiteral("temperature"),
                            m_temperature->isChecked());
        capabilities.insert(QStringLiteral("custom_prompt"),
                            m_customPrompt->isChecked());
        capabilities.insert(QStringLiteral("context_size"),
                            m_contextCapability->isChecked());

        QJsonObject result;
        result.insert(QStringLiteral("id"), m_id->text().trimmed());
        result.insert(QStringLiteral("display_name"),
                      m_name->text().trimmed());
        result.insert(QStringLiteral("type"),
                      m_type->currentData().toString());
        result.insert(QStringLiteral("model_path"),
                      m_modelPath->text().trimmed());
        result.insert(QStringLiteral("mmproj_path"),
                      m_mmprojPath->text().trimmed());
        result.insert(QStringLiteral("context_size"), m_contextSize->value());
        result.insert(QStringLiteral("gpu_layers"), m_gpuLayers->value());
        result.insert(QStringLiteral("default_prompt"),
                      m_defaultPrompt->toPlainText());
        result.insert(QStringLiteral("capabilities"), capabilities);
        return result;
    }

private:
    void inspectModel()
    {
        const QString path = m_modelPath->text().trimmed();
        const QFileInfo modelFile(path);
        if (!modelFile.isAbsolute() || !modelFile.isFile() ||
            modelFile.suffix().compare(QStringLiteral("gguf"),
                                       Qt::CaseInsensitive) != 0) {
            QMessageBox::warning(
              this,
              dialogTr("Custom model"),
              dialogTr("Choose an existing GGUF model before analysis."));
            return;
        }

        if (!m_inspector) {
            m_inspectionStatus->setText(
              dialogTr("Automatic analysis is unavailable. You can still "
                       "enter every setting manually."));
            return;
        }

        QJsonObject request;
        request.insert(QStringLiteral("model_path"),
                       modelFile.absoluteFilePath());
        request.insert(QStringLiteral("auto_match_mmproj"), true);
        const QString mmprojPath = m_mmprojPath->text().trimmed();
        if (!mmprojPath.isEmpty()) {
            request.insert(QStringLiteral("mmproj_path"), mmprojPath);
        }

        const int serial = ++m_inspectionSerial;
        m_inspectButton->setEnabled(false);
        m_saveButton->setEnabled(false);
        m_inspectionStatus->setText(
          dialogTr("Analyzing GGUF metadata..."));

        QPointer<CustomModelDialog> self(this);
        m_inspector(
          request,
          [self, serial](bool ok,
                         const QJsonObject& object,
                         const QString& error) {
              if (!self || serial != self->m_inspectionSerial) {
                  return;
              }

              self->m_inspectButton->setEnabled(true);
              self->m_saveButton->setEnabled(true);

              if (!ok) {
                  self->m_inspectionStatus->setText(
                    dialogTr("Automatic analysis failed — %1 You can still "
                             "enter every setting manually.")
                      .arg(error));
                  return;
              }

              self->applyInspection(object);
          });
    }

    void applyInspection(const QJsonObject& inspection)
    {
        QJsonObject suggested =
          inspection.value(QStringLiteral("suggested")).toObject();
        if (suggested.isEmpty()) {
            m_inspectionStatus->setText(
              dialogTr("The Runtime returned no model suggestions. You can "
                       "still enter every setting manually."));
            return;
        }

        const QString retainedId = m_id->text();

        // Some GGUF files expose only a generic general.name such as "7B".
        // In that case the filename is a more useful, still-editable default.
        const QString suggestedName =
          suggested.value(QStringLiteral("display_name"))
            .toString()
            .trimmed();
        static const QRegularExpression genericParameterName(
          QStringLiteral("^[0-9]+(?:\\.[0-9]+)?[bBmM]$"));
        if (genericParameterName.match(suggestedName).hasMatch()) {
            const QString filenameName =
              QFileInfo(m_modelPath->text().trimmed()).completeBaseName();
            if (!filenameName.isEmpty()) {
                suggested.insert(QStringLiteral("display_name"),
                                 filenameName);

                QString filenameId = filenameName.toLower();
                filenameId.replace(QRegularExpression(
                                     QStringLiteral("[^a-z0-9]+")),
                                   QStringLiteral("-"));
                filenameId.remove(QRegularExpression(
                  QStringLiteral("^-+|-+$")));
                if (!filenameId.isEmpty()) {
                    suggested.insert(QStringLiteral("id"), filenameId);
                }
            }
        }

        load(suggested);
        if (m_editing) {
            m_id->setText(retainedId);
        }

        QString architecture =
          inspection.value(QStringLiteral("architecture")).toString();
        if (architecture.isEmpty()) {
            architecture = dialogTr("unknown");
        }

        QStringList details;
        details.append(
          dialogTr("Metadata read. Architecture: %1.").arg(architecture));

        const qint64 nativeContext =
          inspection.value(QStringLiteral("native_context_size"))
            .toVariant()
            .toLongLong();
        if (nativeContext > 0) {
            details.append(
              dialogTr("Native context: %1.").arg(nativeContext));
        }

        details.append(
          inspection.value(QStringLiteral("has_chat_template")).toBool(false)
            ? dialogTr("Chat template detected.")
            : dialogTr("No chat template was detected."));

        const QJsonObject mmproj =
          inspection.value(QStringLiteral("mmproj")).toObject();
        const QString mmprojPath =
          mmproj.value(QStringLiteral("path")).toString();
        if (mmproj.value(QStringLiteral("auto_matched")).toBool(false) &&
            !mmprojPath.isEmpty()) {
            details.append(
              dialogTr("MMProj automatically matched: %1.").arg(mmprojPath));
        } else if (mmprojPath.isEmpty()) {
            details.append(dialogTr("No MMProj was selected automatically."));
        }

        QStringList warnings;
        const QJsonArray warningValues =
          inspection.value(QStringLiteral("warnings")).toArray();
        for (const QJsonValue& value : warningValues) {
            const QString warning = value.toString().trimmed();
            if (!warning.isEmpty()) {
                warnings.append(localizedInspectionWarning(warning));
            }
        }
        if (!warnings.isEmpty()) {
            details.append(
              dialogTr("Warnings: %1").arg(warnings.join(QLatin1Char(' '))));
        }

        details.append(
          dialogTr("Review every suggested setting before saving."));
        m_inspectionStatus->setText(details.join(QLatin1Char(' ')));
    }

    void load(const QJsonObject& model)
    {
        m_id->setText(model.value(QStringLiteral("id")).toString());
        m_name->setText(
          model.value(QStringLiteral("display_name")).toString());

        const int typeIndex = m_type->findData(
          model.value(QStringLiteral("type")).toString());
        if (typeIndex >= 0) {
            m_type->setCurrentIndex(typeIndex);
        }

        m_modelPath->setText(
          model.value(QStringLiteral("model_path")).toString());
        m_mmprojPath->setText(
          model.value(QStringLiteral("mmproj_path")).toString());
        m_contextSize->setValue(
          model.value(QStringLiteral("context_size")).toInt(4096));
        m_gpuLayers->setValue(
          model.value(QStringLiteral("gpu_layers")).toInt(99));
        m_defaultPrompt->setPlainText(
          model.value(QStringLiteral("default_prompt")).toString());

        const QJsonObject capabilities =
          model.value(QStringLiteral("capabilities")).toObject();
        m_chat->setChecked(
          capabilities.value(QStringLiteral("chat")).toBool(true));
        m_translation->setChecked(
          capabilities.value(QStringLiteral("translation")).toBool(false));
        m_ocr->setChecked(
          capabilities.value(QStringLiteral("ocr")).toBool(false));
        m_vision->setChecked(
          capabilities.value(QStringLiteral("vision")).toBool(false));
        m_streaming->setChecked(
          capabilities.value(QStringLiteral("streaming")).toBool(true));
        m_thinking->setChecked(
          capabilities.value(QStringLiteral("thinking")).toBool(false));
        m_temperature->setChecked(
          capabilities.value(QStringLiteral("temperature")).toBool(true));
        m_customPrompt->setChecked(
          capabilities.value(QStringLiteral("custom_prompt")).toBool(true));
        m_contextCapability->setChecked(
          capabilities.value(QStringLiteral("context_size")).toBool(true));
    }

    void applyTypeDefaults()
    {
        const QString type = m_type->currentData().toString();
        m_chat->setChecked(type == QStringLiteral("chat") ||
                           type == QStringLiteral("translation") ||
                           type == QStringLiteral("vision"));
        m_translation->setChecked(type == QStringLiteral("translation"));
        m_ocr->setChecked(type == QStringLiteral("ocr"));
        m_vision->setChecked(type == QStringLiteral("ocr") ||
                             type == QStringLiteral("vision") ||
                             !m_mmprojPath->text().trimmed().isEmpty());
        if (type == QStringLiteral("ocr") &&
            m_defaultPrompt->toPlainText().trimmed().isEmpty()) {
            m_defaultPrompt->setPlainText(QStringLiteral("OCR:"));
        }
    }

    bool validate(QString* error) const
    {
        const QString id = m_id->text().trimmed();
        const QRegularExpression idPattern(
          QStringLiteral("^[a-z0-9][a-z0-9._-]{0,63}$"));

        if (!idPattern.match(id).hasMatch()) {
            *error = dialogTr(
              "Model ID must use 1-64 lowercase letters, numbers, dots, "
              "underscores, or hyphens.");
            return false;
        }

        if (m_name->text().trimmed().isEmpty()) {
            *error = dialogTr("Display name is required.");
            return false;
        }

        const QFileInfo modelFile(m_modelPath->text().trimmed());
        if (!modelFile.isAbsolute() || !modelFile.isFile() ||
            modelFile.suffix().compare(QStringLiteral("gguf"),
                                       Qt::CaseInsensitive) != 0) {
            *error = dialogTr(
              "Choose an existing GGUF model using an absolute path.");
            return false;
        }

        const QString mmprojText = m_mmprojPath->text().trimmed();
        if (!mmprojText.isEmpty()) {
            const QFileInfo mmprojFile(mmprojText);
            if (!mmprojFile.isAbsolute() || !mmprojFile.isFile() ||
                mmprojFile.suffix().compare(QStringLiteral("gguf"),
                                            Qt::CaseInsensitive) != 0) {
                *error = dialogTr(
                  "Choose an existing MMProj GGUF using an absolute path.");
                return false;
            }
        }

        if (m_defaultPrompt->toPlainText().size() > 8192) {
            *error = dialogTr("Default prompt is too long.");
            return false;
        }

        return true;
    }

    bool m_editing;
    InspectFunction m_inspector;
    int m_inspectionSerial{ 0 };
    QLineEdit* m_id;
    QLineEdit* m_name;
    QComboBox* m_type;
    QLineEdit* m_modelPath;
    QLineEdit* m_mmprojPath;
    QSpinBox* m_contextSize;
    QSpinBox* m_gpuLayers;
    QPlainTextEdit* m_defaultPrompt;
    QCheckBox* m_chat;
    QCheckBox* m_translation;
    QCheckBox* m_ocr;
    QCheckBox* m_vision;
    QCheckBox* m_streaming;
    QCheckBox* m_thinking;
    QCheckBox* m_temperature;
    QCheckBox* m_customPrompt;
    QCheckBox* m_contextCapability;
    QLabel* m_inspectionStatus;
    QPushButton* m_inspectButton;
    QPushButton* m_saveButton;
};
}

OcrConf::OcrConf(QWidget* parent)
  : QWidget(parent)
{
    m_modelNetwork = new QNetworkAccessManager(this);

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
         "not bundle llama.cpp or model files. Catalog downloads and "
         "acceleration are managed independently by the Runtime."),
      connectionBox);
    connectionNote->setWordWrap(true);
    connectionForm->addRow(QString(), connectionNote);

    layout->addWidget(connectionBox);

    auto* modelsBox = new QGroupBox(tr("Runtime model management"), content);
    auto* modelsLayout = new QVBoxLayout(modelsBox);

    m_modelTable = new QTableWidget(modelsBox);
    m_modelTable->setColumnCount(8);
    m_modelTable->setHorizontalHeaderLabels(
      { tr("Name"),
        tr("Model ID"),
        tr("Type"),
        tr("Source"),
        tr("State"),
        tr("Context"),
        tr("GPU layers"),
        tr("Capabilities") });
    m_modelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modelTable->verticalHeader()->setVisible(false);
    m_modelTable->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
    m_modelTable->horizontalHeader()->setStretchLastSection(true);
    m_modelTable->setMinimumHeight(220);
    modelsLayout->addWidget(m_modelTable);

    auto* modelButtons = new QWidget(modelsBox);
    auto* modelButtonsLayout = new QHBoxLayout(modelButtons);
    modelButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_refreshModelsButton =
      new QPushButton(tr("Refresh models"), modelButtons);
    m_addModelButton = new QPushButton(tr("Import GGUF model"), modelButtons);
    m_editModelButton = new QPushButton(tr("Edit"), modelButtons);
    m_removeEntryButton =
      new QPushButton(tr("Remove entry"), modelButtons);
    m_useModelButton = new QPushButton(tr("Use for OCR"), modelButtons);
    modelButtonsLayout->addWidget(m_refreshModelsButton);
    modelButtonsLayout->addWidget(m_addModelButton);
    modelButtonsLayout->addWidget(m_editModelButton);
    modelButtonsLayout->addWidget(m_removeEntryButton);
    modelButtonsLayout->addWidget(m_useModelButton);
    modelButtonsLayout->addStretch();
    modelsLayout->addWidget(modelButtons);

    m_modelStatus = new QLabel(
      tr("Connect to Local AI Runtime 0.4.0 or later to manage models."),
      modelsBox);
    m_modelStatus->setWordWrap(true);
    modelsLayout->addWidget(m_modelStatus);

    auto* modelNote = new QLabel(
      tr("Catalog models are read-only. Custom entries may reference any "
         "local GGUF and optional MMProj file. Removing a custom entry "
         "never deletes either file. Automatic GGUF analysis requires "
         "Local AI Runtime 0.6.0 or later; every suggested setting remains "
         "editable."),
      modelsBox);
    modelNote->setWordWrap(true);
    modelsLayout->addWidget(modelNote);

    layout->addWidget(modelsBox);
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

    connect(m_refreshModelsButton,
            &QPushButton::clicked,
            this,
            &OcrConf::refreshModelList);
    connect(m_addModelButton,
            &QPushButton::clicked,
            this,
            &OcrConf::addCustomModel);
    connect(m_editModelButton,
            &QPushButton::clicked,
            this,
            &OcrConf::editSelectedCustomModel);
    connect(m_removeEntryButton,
            &QPushButton::clicked,
            this,
            &OcrConf::removeSelectedCustomModel);
    connect(m_useModelButton,
            &QPushButton::clicked,
            this,
            &OcrConf::useSelectedModelForOcr);
    connect(m_modelTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &OcrConf::updateModelButtons);

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

          if (ok) {
              refreshModelList();
          } else {
              m_modelStatus->setText(
                tr("Model management is unavailable while the Runtime is "
                   "disconnected."));
              m_addModelButton->setEnabled(false);
              updateModelButtons();
          }
      });
}

void OcrConf::sendModelRequest(const QString& method,
                               const QString& path,
                               const QJsonObject& payload,
                               JsonCallback callback)
{
    QString base = m_serverUrl->text().trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        base = QStringLiteral("http://127.0.0.1:8111");
    }

    QNetworkRequest request(QUrl(base + path));
    request.setRawHeader(QByteArrayLiteral("Accept"),
                         QByteArrayLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         QByteArrayLiteral("Flameshot-OCR/2.5"));
    request.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      static_cast<int>(QNetworkRequest::NoLessSafeRedirectPolicy));

    QNetworkReply* reply = nullptr;
    if (method == QStringLiteral("GET")) {
        reply = m_modelNetwork->get(request);
    } else {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/json"));
        reply = m_modelNetwork->post(
          request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    }

    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(30000);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply, callback]() {
                const QByteArray body = reply->readAll();
                const auto networkError = reply->error();
                const QString networkMessage = reply->errorString();
                const int status =
                  reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();
                reply->deleteLater();

                QJsonParseError parseError;
                const QJsonDocument document =
                  QJsonDocument::fromJson(body, &parseError);
                const QJsonObject object =
                  document.isObject() ? document.object() : QJsonObject();

                if (networkError != QNetworkReply::NoError ||
                    status < 200 || status >= 300) {
                    QString message = networkMessage;
                    const QJsonObject errorObject =
                      object.value(QStringLiteral("error")).toObject();
                    const QString apiMessage =
                      errorObject.value(QStringLiteral("message")).toString();
                    if (!apiMessage.isEmpty()) {
                        message = apiMessage;
                    }
                    callback(false, object, message);
                    return;
                }

                if (parseError.error != QJsonParseError::NoError ||
                    !document.isObject()) {
                    callback(false,
                             {},
                             OcrConf::tr("The Runtime returned invalid JSON."));
                    return;
                }

                callback(true, object, {});
            });
}

void OcrConf::refreshModelList()
{
    m_modelStatus->setText(tr("Loading Runtime models..."));
    m_refreshModelsButton->setEnabled(false);
    m_addModelButton->setEnabled(false);
    m_editModelButton->setEnabled(false);
    m_removeEntryButton->setEnabled(false);
    m_useModelButton->setEnabled(false);

    sendModelRequest(
      QStringLiteral("GET"),
      QStringLiteral("/v1/models/custom"),
      {},
      [this](bool customOk,
             const QJsonObject&,
             const QString& customError) {
          if (!customOk) {
              m_refreshModelsButton->setEnabled(true);
              m_modelStatus->setText(
                tr("Could not load models. Local AI Runtime 0.4.0 or later "
                   "is required. — %1")
                  .arg(customError));
              m_addModelButton->setEnabled(false);
              return;
          }

          sendModelRequest(
            QStringLiteral("GET"),
            QStringLiteral("/v1/models"),
            {},
            [this](bool ok,
                   const QJsonObject& object,
                   const QString& error) {
          m_refreshModelsButton->setEnabled(true);

          if (!ok) {
              m_modelStatus->setText(
                tr("Could not load models. Local AI Runtime 0.4.0 or later "
                   "is required. — %1")
                  .arg(error));
              m_addModelButton->setEnabled(false);
              return;
          }

          const QJsonArray models =
            object.value(QStringLiteral("data")).toArray();
          m_modelTable->setSortingEnabled(false);
          m_modelTable->setRowCount(0);

          const QString activeModel = m_modelId->text().trimmed();
          int activeRow = -1;

          for (const QJsonValue& value : models) {
              if (!value.isObject()) {
                  continue;
              }

              const QJsonObject model = value.toObject();
              const int row = m_modelTable->rowCount();
              m_modelTable->insertRow(row);

              const QString id =
                model.value(QStringLiteral("id")).toString();
              const QString source =
                model.value(QStringLiteral("source")).toString();
              const bool installed =
                model.value(QStringLiteral("installed")).toBool(false);
              const QJsonObject capabilities =
                model.value(QStringLiteral("capabilities")).toObject();
              const QString type =
                model.value(QStringLiteral("type")).toString();
              QString typeName = type;
              if (type == QStringLiteral("chat")) {
                  typeName = tr("Chat");
              } else if (type == QStringLiteral("translation")) {
                  typeName = tr("Translation");
              } else if (type == QStringLiteral("ocr")) {
                  typeName = tr("OCR");
              } else if (type == QStringLiteral("vision")) {
                  typeName = tr("Vision");
              } else if (type == QStringLiteral("other")) {
                  typeName = tr("Other");
              }

              QStringList capabilityNames;
              const QList<QPair<QString, QString>> capabilityLabels = {
                  { QStringLiteral("chat"), tr("chat") },
                  { QStringLiteral("translation"), tr("translation") },
                  { QStringLiteral("ocr"), tr("OCR") },
                  { QStringLiteral("vision"), tr("vision") },
                  { QStringLiteral("streaming"), tr("streaming") },
                  { QStringLiteral("thinking"), tr("thinking") },
              };
              for (const auto& item : capabilityLabels) {
                  if (capabilities.value(item.first).toBool(false)) {
                      capabilityNames.append(item.second);
                  }
              }

              const QStringList columns = {
                  model.value(QStringLiteral("display_name")).toString(id),
                  id,
                  typeName,
                  source == QStringLiteral("custom") ? tr("Custom")
                                                     : tr("Catalog"),
                  installed ? tr("Ready") : tr("Missing files"),
                  QString::number(
                    model.value(QStringLiteral("context_size")).toInt()),
                  QString::number(
                    model.value(QStringLiteral("gpu_layers")).toInt()),
                  capabilityNames.join(QStringLiteral(", ")),
              };

              for (int column = 0; column < columns.size(); ++column) {
                  auto* tableItem = new QTableWidgetItem(columns.at(column));
                  if (column == 0) {
                      tableItem->setData(
                        Qt::UserRole,
                        QString::fromUtf8(
                          QJsonDocument(model).toJson(QJsonDocument::Compact)));
                  }
                  m_modelTable->setItem(row, column, tableItem);
              }

              if (id == activeModel) {
                  activeRow = row;
              }
          }

          if (activeRow >= 0) {
              m_modelTable->selectRow(activeRow);
          } else if (m_modelTable->rowCount() > 0) {
              m_modelTable->selectRow(0);
          }

          m_modelTable->setSortingEnabled(true);
          m_addModelButton->setEnabled(true);
          m_modelStatus->setText(
            tr("Loaded %1 Runtime model(s). Custom entries are stored by "
               "Local AI Runtime.")
              .arg(m_modelTable->rowCount()));
          updateModelButtons();
            });
      });
}

QJsonObject OcrConf::selectedModel() const
{
    const int row = m_modelTable->currentRow();
    if (row < 0 || !m_modelTable->item(row, 0)) {
        return {};
    }

    const QByteArray json =
      m_modelTable->item(row, 0)->data(Qt::UserRole).toString().toUtf8();
    const QJsonDocument document = QJsonDocument::fromJson(json);
    return document.isObject() ? document.object() : QJsonObject();
}

void OcrConf::updateModelButtons()
{
    const QJsonObject model = selectedModel();
    const bool selected = !model.isEmpty();
    const bool editable =
      selected && model.value(QStringLiteral("editable")).toBool(false);
    const bool installed =
      selected && model.value(QStringLiteral("installed")).toBool(false);
    const bool ocrCapable =
      model.value(QStringLiteral("capabilities"))
        .toObject()
        .value(QStringLiteral("ocr"))
        .toBool(false);

    m_editModelButton->setEnabled(editable);
    m_removeEntryButton->setEnabled(editable);
    m_useModelButton->setEnabled(installed && ocrCapable);
}

void OcrConf::addCustomModel()
{
    CustomModelDialog dialog(
      {},
      [this](const QJsonObject& payload, InspectResultCallback callback) {
          sendModelRequest(QStringLiteral("POST"),
                           QStringLiteral("/v1/models/inspect"),
                           payload,
                           callback);
      },
      this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_modelStatus->setText(tr("Adding custom model..."));
    m_addModelButton->setEnabled(false);
    sendModelRequest(
      QStringLiteral("POST"),
      QStringLiteral("/v1/models/custom"),
      dialog.payload(),
      [this](bool ok, const QJsonObject&, const QString& error) {
          if (!ok) {
              m_addModelButton->setEnabled(true);
              m_modelStatus->setText(tr("Could not add custom model — %1")
                                       .arg(error));
              QMessageBox::warning(
                this, tr("Custom model"), m_modelStatus->text());
              return;
          }
          m_modelStatus->setText(tr("Custom model added."));
          refreshModelList();
      });
}

void OcrConf::editSelectedCustomModel()
{
    const QJsonObject model = selectedModel();
    if (model.isEmpty() ||
        !model.value(QStringLiteral("editable")).toBool(false)) {
        return;
    }

    CustomModelDialog dialog(
      model,
      [this](const QJsonObject& payload, InspectResultCallback callback) {
          sendModelRequest(QStringLiteral("POST"),
                           QStringLiteral("/v1/models/inspect"),
                           payload,
                           callback);
      },
      this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_modelStatus->setText(tr("Saving custom model..."));
    sendModelRequest(
      QStringLiteral("POST"),
      QStringLiteral("/v1/models/custom"),
      dialog.payload(),
      [this](bool ok, const QJsonObject&, const QString& error) {
          if (!ok) {
              m_modelStatus->setText(tr("Could not save custom model — %1")
                                       .arg(error));
              QMessageBox::warning(
                this, tr("Custom model"), m_modelStatus->text());
              return;
          }
          m_modelStatus->setText(
            tr("Custom model saved. An active model was safely unloaded if "
               "its settings changed."));
          refreshModelList();
      });
}

void OcrConf::removeSelectedCustomModel()
{
    const QJsonObject model = selectedModel();
    if (model.isEmpty() ||
        !model.value(QStringLiteral("editable")).toBool(false)) {
        return;
    }

    const QString id = model.value(QStringLiteral("id")).toString();
    const auto answer = QMessageBox::question(
      this,
      tr("Remove custom model entry"),
      tr("Remove the custom entry “%1”?\n\nThe GGUF and MMProj files "
         "will remain on disk.")
        .arg(id),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    m_modelStatus->setText(tr("Removing custom model entry..."));
    sendModelRequest(
      QStringLiteral("POST"),
      QStringLiteral("/v1/models/custom/remove"),
      { { QStringLiteral("model"), id } },
      [this, id](bool ok, const QJsonObject&, const QString& error) {
          if (!ok) {
              m_modelStatus->setText(tr("Could not remove custom entry — %1")
                                       .arg(error));
              QMessageBox::warning(
                this, tr("Custom model"), m_modelStatus->text());
              return;
          }

          if (ConfigHandler().ocrModelId().trimmed() == id) {
              const QString fallback = QStringLiteral("paddleocr-vl-1.6");
              ConfigHandler().setOcrModelId(fallback);
              m_modelId->setText(fallback);
          }

          m_modelStatus->setText(
            tr("Custom entry removed. External GGUF files were kept."));
          refreshModelList();
      });
}

void OcrConf::useSelectedModelForOcr()
{
    const QJsonObject model = selectedModel();
    if (model.isEmpty()) {
        return;
    }

    const QString id = model.value(QStringLiteral("id")).toString();
    ConfigHandler().setOcrModelId(id);
    m_modelId->setText(id);
    m_modelStatus->setText(
      tr("%1 is now selected for Flameshot OCR.").arg(id));
    updateModelButtons();
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
