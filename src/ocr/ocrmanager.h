// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QFile>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QUrl>
#include <functional>

struct OcrModelInfo
{
    QString id;
    QString name;
    QString directory;
    QString modelFile;
    QString mmprojFile;
    QUrl modelUrl;
    QUrl mmprojUrl;
    qint64 modelSize{ 0 };
    qint64 mmprojSize{ 0 };
    QString prompt{ QStringLiteral("OCR:") };
    bool verified{ false };
};

struct OcrDeviceInfo
{
    QString id;
    QString name;
    QString backend;
};

class OcrManager : public QObject
{
    Q_OBJECT

public:
    static OcrManager* instance();

    QList<OcrModelInfo> availableModels() const;
    OcrModelInfo activeModel() const;
    QString latestModelId() const;

    QString modelRoot() const;
    QString modelDirectory(const OcrModelInfo& model) const;
    QString modelPath(const OcrModelInfo& model) const;
    QString mmprojPath(const OcrModelInfo& model) const;
    bool modelInstalled(const OcrModelInfo& model) const;
    QString modelStatusText(const OcrModelInfo& model) const;

    QString serverExecutable() const;
    QUrl serverBaseUrl() const;
    QUrl healthEndpoint() const;
    QUrl chatEndpoint() const;
    QString activePrompt() const;
    QList<OcrDeviceInfo> availableDevices() const;
    QString detectedDevice() const;

    bool nvidiaDriverAvailable() const;
    bool cudaRuntimeInstalled() const;
    QString cudaRuntimeVersion() const;
    bool cudaRuntimeBusy() const;

    bool startServer(QString* error = nullptr);
    void stopServer();
    bool managedServerRunning() const;

    void testConnection(
      QObject* context,
      std::function<void(bool, const QString&)> callback);
    void ensureReady(QObject* context,
                     std::function<void(bool, const QString&)> callback);

    void downloadModel(const QString& modelId);
    void cancelDownload();

    void installCudaRuntime();
    void cancelCudaRuntimeInstall();

    bool removeModel(const QString& modelId, QString* error = nullptr);

    void checkRemoteManifest(
      QObject* context,
      std::function<void(bool, const QString&)> callback);

signals:
    void downloadProgress(qint64 done, qint64 total, const QString& fileName);
    void downloadFinished(bool ok, const QString& message);

    void cudaRuntimeProgress(qint64 done, qint64 total);
    void cudaRuntimeFinished(bool ok, const QString& message);
    void cudaRuntimeChanged();

    void serverStateChanged();
    void manifestChanged();

private:
    explicit OcrManager(QObject* parent = nullptr);

    struct DownloadItem
    {
        QString fileName;
        QUrl url;
        qint64 expectedSize{ 0 };
    };

    static QList<OcrModelInfo> builtInModels();
    QList<OcrModelInfo> cachedRemoteModels() const;
    QString manifestCachePath() const;
    OcrModelInfo modelById(const QString& id) const;

    void waitUntilHealthy(
      QObject* context,
      int attemptsLeft,
      std::function<void(bool, const QString&)> callback);

    void startNextDownload();
    void failDownload(const QString& message);

    void startCudaRuntimeExtraction(const QString& archivePath);
    void failCudaRuntime(const QString& message);
    QString cudaRuntimeBaseDir() const;
    QString cudaRuntimeCurrentDir() const;

    QString chooseDevice(const QString& executable) const;

    QNetworkAccessManager m_network;

    QList<DownloadItem> m_downloadQueue;
    int m_downloadIndex{ 0 };
    QFile* m_downloadFile{ nullptr };
    QNetworkReply* m_downloadReply{ nullptr };
    qint64 m_downloadOffset{ 0 };
    qint64 m_downloadTotal{ 0 };
    qint64 m_downloadCompleted{ 0 };
    bool m_downloadCanceled{ false };
    QString m_downloadModelDir;

    QFile* m_cudaDownloadFile{ nullptr };
    QNetworkReply* m_cudaDownloadReply{ nullptr };
    QProcess* m_cudaExtractProcess{ nullptr };
    qint64 m_cudaDownloadOffset{ 0 };
    bool m_cudaCanceled{ false };
    QString m_cudaArchivePath;
    QString m_cudaStagingDir;
};
