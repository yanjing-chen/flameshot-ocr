// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>

class QJsonObject;
class QNetworkReply;
class QTemporaryDir;

class LocalAiRuntimeInstaller : public QObject
{
    Q_OBJECT

public:
    static LocalAiRuntimeInstaller* instance();

    bool busy() const;

    void refreshStatus();
    void installOrRepair();

signals:
    void statusChanged(bool appInstalled,
                       const QString& appVersion,
                       bool serviceRunning,
                       bool endpointConflict,
                       bool llamaInstalled,
                       const QString& llamaVersion,
                       bool paddleInstalled,
                       const QString& paddleVersion,
                       const QString& message);

    void installProgress(int step,
                         int total,
                         const QString& message);

    void installFinished(bool ok,
                         const QString& message);

private:
    using JsonCallback =
      std::function<void(bool, const QJsonObject&, const QString&)>;

    explicit LocalAiRuntimeInstaller(QObject* parent = nullptr);

    static QUrl localBaseUrl();
    static QUrl appManifestUrl();

    QUrl apiUrl(const QString& path) const;

    QString installedAppVersion() const;
    bool serviceFileInstalled() const;

    bool localLlamaInstalled() const;
    QString localLlamaVersion() const;

    bool localPaddleInstalled() const;

    void getJson(const QUrl& url, JsonCallback callback);
    void postJson(const QUrl& url,
                  const QJsonObject& payload,
                  JsonCallback callback);

    void fetchAppManifest();
    bool parseAppManifest(const QJsonObject& manifest,
                          QString* error);

    void downloadAppBundle();
    bool verifyDownloadedBundle(QString* error) const;
    void extractAppBundle();
    void runBundleInstaller();

    void startOrRestartService(bool restart);
    void waitForService();

    void ensureLlamaRuntime();
    void pollLlamaRuntime();

    void ensurePaddleModel();
    void pollPaddleModel();

    bool readPaddleStatus(const QJsonObject& object,
                          bool* installed,
                          QString* version,
                          QString* operationState,
                          QString* operationError) const;

    void runProcess(const QString& program,
                    const QStringList& arguments,
                    const QString& workingDirectory,
                    std::function<void(bool, const QString&)> callback);

    void finishInstall();
    void failInstall(const QString& message);

    void resetTemporaryFiles();

    QNetworkAccessManager m_network;

    bool m_busy{ false };
    bool m_runtimeWasRunning{ false };

    int m_pollAttempts{ 0 };
    int m_consecutiveErrors{ 0 };

    QString m_targetVersion;
    QString m_archiveName;
    QString m_archivePath;
    QString m_expectedSha256;
    qint64 m_expectedArchiveSize{ 0 };
    QUrl m_archiveUrl;

    QString m_installerPath;
    QStringList m_installerArguments;

    QNetworkReply* m_archiveReply{ nullptr };
    QFile* m_archiveFile{ nullptr };
    QTemporaryDir* m_tempDir{ nullptr };
    bool m_archiveWriteFailed{ false };
};
