// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrrequestcontroller.h"

#include "ocr/ocrmanager.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWindow>

namespace
{
QString ocrTr(const char* sourceText)
{
    return QCoreApplication::translate("CaptureWidget", sourceText);
}

}

OcrRequestController::OcrRequestController(const QPixmap& image,
                                           QObject* parent)
  : QObject(parent)
  , m_image(image)
  , m_network(new QNetworkAccessManager(this))
{
}

void OcrRequestController::start()
{
    m_elapsed.start();

    m_progress = new QProgressDialog(
      ocrTr("Preparing PaddleOCR-VL service..."),
      ocrTr("Cancel"),
      0,
      0,
      nullptr);
    m_progress->setAttribute(Qt::WA_DeleteOnClose);
    m_progress->setWindowTitle(ocrTr("OCR"));
    m_progress->setWindowModality(Qt::NonModal);
    m_progress->setMinimumDuration(0);
    m_progress->setAutoClose(false);
    m_progress->setAutoReset(false);
    connect(m_progress, &QProgressDialog::canceled, this, [this]() {
        cancel();
    });
    m_progress->show();

    OcrManager::instance()->ensureReady(
      this,
      [this](bool ready, const QString& error) {
          if (m_canceled) {
              deleteLater();
              return;
          }
          if (!ready) {
              showFailure(
                ocrTr("OCR server unavailable"),
                ocrTr("PaddleOCR-VL is not ready.\n\n%1").arg(error));
              return;
          }
          sendRequest();
      });
}

void OcrRequestController::closeProgress()
{
    if (m_progress) {
        m_progress->close();
        m_progress = nullptr;
    }
}

void OcrRequestController::cancel()
{
    if (m_canceled) {
        return;
    }
    m_canceled = true;
    if (m_reply) {
        m_reply->abort();
    }
    closeProgress();
    deleteLater();
}

void OcrRequestController::sendRequest()
{
    if (m_progress) {
        m_progress->setLabelText(
          ocrTr("Recognizing text with PaddleOCR-VL..."));
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    if (!buffer.open(QIODevice::WriteOnly) || !m_image.save(&buffer, "PNG")) {
        showFailure(ocrTr("OCR"),
                    ocrTr("Failed to prepare the selected image for OCR."));
        return;
    }

    QJsonObject imageUrl;
    imageUrl.insert(QStringLiteral("url"),
                    QStringLiteral("data:image/png;base64,") +
                      QString::fromLatin1(pngData.toBase64()));

    QJsonObject imagePart;
    imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
    imagePart.insert(QStringLiteral("image_url"), imageUrl);

    QJsonObject textPart;
    textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
    textPart.insert(QStringLiteral("text"),
                    OcrManager::instance()->activePrompt());

    QJsonArray content;
    content.append(imagePart);
    content.append(textPart);

    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), content);

    QJsonArray messages;
    messages.append(message);

    QJsonObject payload;
    payload.insert(QStringLiteral("model"),
                   OcrManager::instance()->activeModelId());
    payload.insert(QStringLiteral("messages"), messages);
    payload.insert(QStringLiteral("temperature"), 0);
    payload.insert(QStringLiteral("stream"), false);

    QNetworkRequest request(OcrManager::instance()->chatEndpoint());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    m_reply = m_network->post(
      request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        handleReply();
    });
}

void OcrRequestController::handleReply()
{
    if (m_canceled || !m_reply) {
        return;
    }

    closeProgress();
    const QByteArray response = m_reply->readAll();
    const QNetworkReply::NetworkError networkError = m_reply->error();
    const QString networkErrorText = m_reply->errorString();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (networkError != QNetworkReply::NoError) {
        showFailure(ocrTr("OCR server unavailable"),
                    ocrTr("The OCR request failed.\n\n%1")
                      .arg(networkErrorText));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
      QJsonDocument::fromJson(response, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        showFailure(ocrTr("OCR"),
                    ocrTr("The OCR server returned invalid JSON."));
        return;
    }

    const QJsonObject root = document.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        showFailure(ocrTr("OCR"),
                    ocrTr("The OCR server returned no recognition result."));
        return;
    }

    const QString text = choices.at(0)
                           .toObject()
                           .value(QStringLiteral("message"))
                           .toObject()
                           .value(QStringLiteral("content"))
                           .toString();
    if (text.isEmpty()) {
        closeProgress();
        QMessageBox::information(
          nullptr,
          ocrTr("OCR"),
          ocrTr("No text was recognized in the selected area."));
        deleteLater();
        return;
    }

    const double modelTokensPerSecond =
      root.value(QStringLiteral("timings"))
        .toObject()
        .value(QStringLiteral("predicted_per_second"))
        .toDouble(0.0);
    showResult(text, modelTokensPerSecond);
}

void OcrRequestController::showFailure(const QString& title,
                                       const QString& message)
{
    closeProgress();
    QMessageBox::warning(nullptr, title, message);
    deleteLater();
}

void OcrRequestController::showResult(
  const QString& text,
  double modelTokensPerSecond)
{
    const qint64 elapsedMs = qMax<qint64>(1, m_elapsed.elapsed());
    const qsizetype characters = text.toUcs4().size();
    const double seconds = static_cast<double>(elapsedMs) / 1000.0;
    const double charactersPerSecond =
      static_cast<double>(characters) / seconds;

    auto* dialog = new QDialog(nullptr, Qt::Window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(ocrTr("OCR Result"));
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(760, 560);

    auto* layout = new QVBoxLayout(dialog);

    auto* editor = new QPlainTextEdit(dialog);
    editor->setPlainText(text);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(editor);

    QString metrics =
      ocrTr("Elapsed: %1 s | %2 characters | %3 characters/s")
        .arg(QString::number(seconds, 'f', 2))
        .arg(static_cast<qlonglong>(characters))
        .arg(QString::number(charactersPerSecond, 'f', 1));

    if (modelTokensPerSecond > 0.0) {
        metrics += QStringLiteral(" | ") +
                   ocrTr("Model speed: %1 token/s")
                     .arg(QString::number(modelTokensPerSecond, 'f', 1));
    }

    auto* metricsLabel = new QLabel(metrics, dialog);
    metricsLabel->setToolTip(
      ocrTr("Elapsed time includes Runtime readiness and model loading."));
    metricsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(metricsLabel);

    auto* buttons = new QDialogButtonBox(dialog);

    auto* copyButton =
      buttons->addButton(ocrTr("Copy"), QDialogButtonBox::ActionRole);

    auto* saveButton =
      buttons->addButton(ocrTr("Save Text..."),
                         QDialogButtonBox::ActionRole);

    auto* closeButton =
      buttons->addButton(QDialogButtonBox::Close);

    connect(copyButton, &QPushButton::clicked, dialog, [editor]() {
        QApplication::clipboard()->setText(editor->toPlainText());
    });

    connect(saveButton,
            &QPushButton::clicked,
            dialog,
            [this, editor, dialog]() {
                saveResult(editor->toPlainText(), dialog);
            });

    connect(closeButton,
            &QPushButton::clicked,
            dialog,
            &QDialog::close);

    connect(dialog,
            &QObject::destroyed,
            this,
            &QObject::deleteLater);

    layout->addWidget(buttons);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void OcrRequestController::saveResult(
  const QString& text,
  QWidget* parent)
{
    QString directory =
      QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);

    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }

    const QString suggested =
      QDir(directory)
        .filePath(
          QStringLiteral("ocr-result-%1.txt")
            .arg(
              QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd-HHmmss"))));

    QString fileName =
      QFileDialog::getSaveFileName(
        parent,
        ocrTr("Save OCR result"),
        suggested,
        ocrTr("Text files (*.txt);;All files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    if (QFileInfo(fileName).suffix().isEmpty()) {
        fileName += QStringLiteral(".txt");
    }

    QSaveFile file(fileName);

    if (!file.open(QIODevice::WriteOnly) ||
        file.write(text.toUtf8()) < 0 ||
        !file.commit()) {
        QMessageBox::warning(
          parent,
          ocrTr("Save OCR result"),
          ocrTr("Could not save the OCR result to:\n%1")
            .arg(fileName));
    }
}
