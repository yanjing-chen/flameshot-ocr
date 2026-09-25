// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

class OcrManager : public QObject
{
    Q_OBJECT

public:
    static OcrManager* instance();

    QUrl serverBaseUrl() const;
    QUrl healthEndpoint() const;
    QUrl chatEndpoint() const;

    QString activeModelId() const;
    QString activePrompt() const;

    void testConnection(
      QObject* context,
      std::function<void(bool, const QString&)> callback);
    void ensureReady(QObject* context,
                     std::function<void(bool, const QString&)> callback);

private:
    explicit OcrManager(QObject* parent = nullptr);

    QNetworkAccessManager m_network;
};
