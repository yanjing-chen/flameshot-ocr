// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPixmap>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QWidget;

class OcrRequestController : public QObject
{
    Q_OBJECT

public:
    explicit OcrRequestController(const QPixmap& image,
                                  QObject* parent = nullptr);

    void start();

private:
    void closeProgress();
    void cancel();
    void sendRequest();
    void handleReply();
    void showFailure(const QString& title, const QString& message);
    void showResult(const QString& text, double modelTokensPerSecond);
    void saveResult(const QString& text, bool markdown, QWidget* parent);

    QPixmap m_image;
    QString m_outputFormat;
    QElapsedTimer m_elapsed;
    QPointer<QProgressDialog> m_progress;
    QPointer<QNetworkReply> m_reply;
    QNetworkAccessManager* m_network;
    bool m_canceled = false;
};
