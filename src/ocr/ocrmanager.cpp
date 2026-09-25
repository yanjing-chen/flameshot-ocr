// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrmanager.h"

#include "utils/confighandler.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

namespace
{
QString normalizedBaseUrl(QString value)
{
    value = value.trimmed();
    while (value.endsWith('/')) {
        value.chop(1);
    }

    if (value.isEmpty()) {
        value = QStringLiteral("http://127.0.0.1:8111");
    }

    return value;
}
}

OcrManager* OcrManager::instance()
{
    static OcrManager manager;
    return &manager;
}

OcrManager::OcrManager(QObject* parent)
  : QObject(parent)
{
}

QUrl OcrManager::serverBaseUrl() const
{
    return QUrl(normalizedBaseUrl(ConfigHandler().ocrServerUrl()));
}

QUrl OcrManager::healthEndpoint() const
{
    return QUrl(normalizedBaseUrl(ConfigHandler().ocrServerUrl()) +
                QStringLiteral("/health"));
}

QUrl OcrManager::chatEndpoint() const
{
    return QUrl(normalizedBaseUrl(ConfigHandler().ocrServerUrl()) +
                QStringLiteral("/v1/chat/completions"));
}

QString OcrManager::activeModelId() const
{
    const QString configured = ConfigHandler().ocrModelId().trimmed();
    return configured.isEmpty() ? QStringLiteral("paddleocr-vl-1.6")
                                : configured;
}

QString OcrManager::activePrompt() const
{
    return QStringLiteral("OCR:");
}

void OcrManager::testConnection(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    QPointer<QObject> guard(context);
    QNetworkRequest request(healthEndpoint());
    request.setTransferTimeout(5000);

    QNetworkReply* reply = m_network.get(request);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [guard, reply, callback]() {
                const bool ok = reply->error() == QNetworkReply::NoError;
                const QString error = ok ? QString() : reply->errorString();
                reply->deleteLater();

                if (guard) {
                    callback(ok, error);
                }
            });
}

void OcrManager::ensureReady(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    testConnection(
      context,
      [this, callback](bool ok, const QString& error) {
          if (ok) {
              callback(true, {});
              return;
          }

          callback(
            false,
            tr("Local AI Runtime is not available at %1.\n%2\n\n"
               "Install or start it from OCR settings.")
              .arg(serverBaseUrl().toString(), error));
      });
}
