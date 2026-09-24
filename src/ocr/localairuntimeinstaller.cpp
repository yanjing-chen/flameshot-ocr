// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "localairuntimeinstaller.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

namespace
{
constexpr int kJsonTimeoutMs = 30000;
constexpr int kServicePollLimit = 120;
constexpr int kLlamaPollLimit = 1800;
constexpr int kModelPollLimit = 7200;

const QString kPaddleModelId =
  QStringLiteral("paddleocr-vl-1.6");

const QString kLocalRuntimeUrl =
  QStringLiteral("http://127.0.0.1:8111");

const QString kAppManifestUrl =
  QStringLiteral(
    "https://github.com/yanjing-chen/local-ai-runtime/"
    "releases/download/app-manifest/app-manifest.json");

QProcessEnvironment cleanProcessEnvironment()
{
    QProcessEnvironment env =
      QProcessEnvironment::systemEnvironment();

    const QStringList unsafeVariables = {
        QStringLiteral("LD_LIBRARY_PATH"),
        QStringLiteral("LD_PRELOAD"),
        QStringLiteral("QT_PLUGIN_PATH"),
        QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"),
        QStringLiteral("QML2_IMPORT_PATH"),
        QStringLiteral("QML_IMPORT_PATH"),
        QStringLiteral("PYTHONHOME"),
        QStringLiteral("PYTHONPATH"),
    };

    for (const QString& name : unsafeVariables) {
        env.remove(name);
    }

    env.insert(
      QStringLiteral("PATH"),
      QStringLiteral(
        "/usr/local/sbin:/usr/local/bin:"
        "/usr/sbin:/usr/bin:/sbin:/bin"));

    return env;
}
}

LocalAiRuntimeInstaller* LocalAiRuntimeInstaller::instance()
{
    static LocalAiRuntimeInstaller* installer =
      new LocalAiRuntimeInstaller();
    return installer;
}

LocalAiRuntimeInstaller::LocalAiRuntimeInstaller(QObject* parent)
  : QObject(parent)
{
}

bool LocalAiRuntimeInstaller::busy() const
{
    return m_busy;
}

QUrl LocalAiRuntimeInstaller::localBaseUrl()
{
    return QUrl(kLocalRuntimeUrl);
}

QUrl LocalAiRuntimeInstaller::appManifestUrl()
{
    return QUrl(kAppManifestUrl);
}

QUrl LocalAiRuntimeInstaller::apiUrl(const QString& path) const
{
    return QUrl(kLocalRuntimeUrl + path);
}

QString LocalAiRuntimeInstaller::installedAppVersion() const
{
    const QString sourcePath =
      QDir::home().filePath(
        QStringLiteral(
          ".local/lib/local-ai-runtime/local_ai_runtime.py"));

    QFile file(sourcePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QString source =
      QString::fromUtf8(file.readAll());

    const QRegularExpression expression(
      QStringLiteral(
        R"regex(VERSION\s*=\s*"([^"]+)")regex"));

    const QRegularExpressionMatch match =
      expression.match(source);

    if (!match.hasMatch()) {
        return {};
    }

    return match.captured(1);
}

bool LocalAiRuntimeInstaller::serviceFileInstalled() const
{
    return QFileInfo::exists(
      QDir::home().filePath(
        QStringLiteral(
          ".config/systemd/user/local-ai-runtime.service")));
}

bool LocalAiRuntimeInstaller::localLlamaInstalled() const
{
    const QString current =
      QDir::home().filePath(
        QStringLiteral(
          ".local/share/local-ai-runtime/"
          "runtime/llama/current"));

    const QDir directory(current);

    if (!directory.exists()) {
        return false;
    }

    const QFileInfo vulkan(
      directory.filePath(
        QStringLiteral("bin/llama-server-vulkan")));

    const QFileInfo cpu(
      directory.filePath(
        QStringLiteral("bin/llama-server-cpu")));

    return vulkan.isExecutable() ||
           cpu.isExecutable();
}

QString LocalAiRuntimeInstaller::localLlamaVersion() const
{
    const QFileInfo current(
      QDir::home().filePath(
        QStringLiteral(
          ".local/share/local-ai-runtime/"
          "runtime/llama/current")));

    if (!current.exists()) {
        return {};
    }

    const QString target =
      current.symLinkTarget();

    if (target.isEmpty()) {
        return QStringLiteral("installed");
    }

    return QFileInfo(target).fileName();
}

bool LocalAiRuntimeInstaller::localPaddleInstalled() const
{
    return QFileInfo::exists(
      QDir::home().filePath(
        QStringLiteral(
          ".local/share/local-ai-runtime/models/"
          "paddleocr-vl-1.6/1.6/.installed.json")));
}

void LocalAiRuntimeInstaller::getJson(
  const QUrl& url,
  JsonCallback callback)
{
    QNetworkRequest request(url);

    request.setRawHeader(
      QByteArrayLiteral("Accept"),
      QByteArrayLiteral("application/json"));

    request.setRawHeader(
      QByteArrayLiteral("User-Agent"),
      QByteArrayLiteral("Flameshot-OCR/2.5"));

    request.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      static_cast<int>(
        QNetworkRequest::NoLessSafeRedirectPolicy));

    QNetworkReply* reply =
      m_network.get(request);

    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);

    connect(
      timer,
      &QTimer::timeout,
      reply,
      &QNetworkReply::abort);

    timer->start(kJsonTimeoutMs);

    connect(
      reply,
      &QNetworkReply::finished,
      this,
      [reply, callback]() {
          const QByteArray body =
            reply->readAll();

          const QNetworkReply::NetworkError networkError =
            reply->error();

          const QString networkMessage =
            reply->errorString();

          reply->deleteLater();

          if (networkError != QNetworkReply::NoError) {
              callback(
                false,
                {},
                networkMessage);
              return;
          }

          QJsonParseError parseError;
          const QJsonDocument document =
            QJsonDocument::fromJson(
              body,
              &parseError);

          if (parseError.error !=
                QJsonParseError::NoError ||
              !document.isObject()) {
              callback(
                false,
                {},
                QObject::tr(
                  "The service returned invalid JSON."));
              return;
          }

          callback(
            true,
            document.object(),
            {});
      });
}

void LocalAiRuntimeInstaller::postJson(
  const QUrl& url,
  const QJsonObject& payload,
  JsonCallback callback)
{
    QNetworkRequest request(url);

    request.setHeader(
      QNetworkRequest::ContentTypeHeader,
      QStringLiteral("application/json"));

    request.setRawHeader(
      QByteArrayLiteral("Accept"),
      QByteArrayLiteral("application/json"));

    request.setRawHeader(
      QByteArrayLiteral("User-Agent"),
      QByteArrayLiteral("Flameshot-OCR/2.5"));

    request.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      static_cast<int>(
        QNetworkRequest::NoLessSafeRedirectPolicy));

    QNetworkReply* reply =
      m_network.post(
        request,
        QJsonDocument(payload)
          .toJson(QJsonDocument::Compact));

    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);

    connect(
      timer,
      &QTimer::timeout,
      reply,
      &QNetworkReply::abort);

    timer->start(kJsonTimeoutMs);

    connect(
      reply,
      &QNetworkReply::finished,
      this,
      [reply, callback]() {
          const QByteArray body =
            reply->readAll();

          const QNetworkReply::NetworkError networkError =
            reply->error();

          const QString networkMessage =
            reply->errorString();

          reply->deleteLater();

          if (networkError != QNetworkReply::NoError) {
              callback(
                false,
                {},
                networkMessage);
              return;
          }

          QJsonParseError parseError;
          const QJsonDocument document =
            QJsonDocument::fromJson(
              body,
              &parseError);

          if (parseError.error !=
                QJsonParseError::NoError ||
              !document.isObject()) {
              callback(
                false,
                {},
                QObject::tr(
                  "The service returned invalid JSON."));
              return;
          }

          callback(
            true,
            document.object(),
            {});
      });
}

void LocalAiRuntimeInstaller::refreshStatus()
{
    const QString localVersion =
      installedAppVersion();

    const bool appInstalled =
      !localVersion.isEmpty();

    const bool llamaLocal =
      localLlamaInstalled();

    const QString llamaLocalVersion =
      localLlamaVersion();

    const bool paddleLocal =
      localPaddleInstalled();

    getJson(
      apiUrl(QStringLiteral("/health")),
      [this,
       appInstalled,
       localVersion,
       llamaLocal,
       llamaLocalVersion,
       paddleLocal](
        bool ok,
        const QJsonObject& health,
        const QString& error) {
          if (!ok) {
              emit statusChanged(
                appInstalled,
                localVersion,
                false,
                false,
                llamaLocal,
                llamaLocalVersion,
                paddleLocal,
                paddleLocal
                  ? QStringLiteral("1.6")
                  : QString(),
                appInstalled
                  ? tr("Local AI Runtime is installed, "
                       "but the service is not running.")
                  : tr("Local AI Runtime is not installed."));
              return;
          }

          const QString service =
            health.value(
                    QStringLiteral("service"))
              .toString();

          if (service !=
              QStringLiteral("local-ai-runtime")) {
              emit statusChanged(
                appInstalled,
                localVersion,
                false,
                true,
                llamaLocal,
                llamaLocalVersion,
                paddleLocal,
                paddleLocal
                  ? QStringLiteral("1.6")
                  : QString(),
                tr("Port 8111 is occupied by another "
                   "service."));
              return;
          }

          const QString runningVersion =
            health.value(
                    QStringLiteral("runtime_version"))
              .toString(localVersion);

          getJson(
            apiUrl(
              QStringLiteral(
                "/v1/runtime/llama/status")),
            [this,
             appInstalled,
             runningVersion,
             llamaLocal,
             llamaLocalVersion,
             paddleLocal](
              bool llamaOk,
              const QJsonObject& llama,
              const QString&) {
                const bool llamaInstalled =
                  llamaOk
                    ? llama.value(
                             QStringLiteral("installed"))
                        .toBool(llamaLocal)
                    : llamaLocal;

                const QString llamaVersion =
                  llamaOk
                    ? llama.value(
                             QStringLiteral("current"))
                        .toString(
                          llamaLocalVersion)
                    : llamaLocalVersion;

                getJson(
                  apiUrl(
                    QStringLiteral("/v1/models")),
                  [this,
                   appInstalled,
                   runningVersion,
                   llamaInstalled,
                   llamaVersion,
                   paddleLocal](
                    bool modelsOk,
                    const QJsonObject& models,
                    const QString& modelError) {
                      bool paddleInstalled =
                        paddleLocal;

                      QString paddleVersion =
                        paddleLocal
                          ? QStringLiteral("1.6")
                          : QString();

                      QString state;
                      QString operationError;

                      if (modelsOk) {
                          readPaddleStatus(
                            models,
                            &paddleInstalled,
                            &paddleVersion,
                            &state,
                            &operationError);
                      }

                      const QString message =
                        modelsOk
                          ? tr("Shared Local AI Runtime "
                               "is running.")
                          : tr("Runtime is running, but "
                               "model status could not "
                               "be read: %1")
                              .arg(modelError);

                      emit statusChanged(
                        appInstalled,
                        runningVersion,
                        true,
                        false,
                        llamaInstalled,
                        llamaVersion,
                        paddleInstalled,
                        paddleVersion,
                        message);
                  });
            });
      });
}

void LocalAiRuntimeInstaller::installOrRepair()
{
    if (m_busy) {
        return;
    }

    m_busy = true;
    m_runtimeWasRunning = false;
    m_pollAttempts = 0;
    m_consecutiveErrors = 0;

    emit installProgress(
      0,
      3,
      tr("Checking Local AI Runtime..."));

    getJson(
      apiUrl(QStringLiteral("/health")),
      [this](
        bool ok,
        const QJsonObject& health,
        const QString&) {
          if (ok) {
              const QString service =
                health.value(
                        QStringLiteral("service"))
                  .toString();

              if (service !=
                  QStringLiteral("local-ai-runtime")) {
                  failInstall(
                    tr("Port 8111 is already occupied "
                       "by another service. Stop that "
                       "service before installing the "
                       "shared Local AI Runtime."));
                  return;
              }

              m_runtimeWasRunning = true;
          }

          fetchAppManifest();
      });
}

void LocalAiRuntimeInstaller::fetchAppManifest()
{
    emit installProgress(
      0,
      3,
      tr("Checking the Local AI Runtime release..."));

    getJson(
      appManifestUrl(),
      [this](
        bool ok,
        const QJsonObject& manifest,
        const QString& error) {
          if (!ok) {
              failInstall(
                tr("Could not download the Local AI "
                   "Runtime manifest: %1")
                  .arg(error));
              return;
          }

          QString parseError;

          if (!parseAppManifest(
                manifest,
                &parseError)) {
              failInstall(parseError);
              return;
          }

          const QString installed =
            installedAppVersion();

          if (installed == m_targetVersion &&
              serviceFileInstalled()) {
              if (m_runtimeWasRunning) {
                  emit installProgress(
                    1,
                    3,
                    tr("Local AI Runtime %1 is "
                       "installed and running.")
                      .arg(m_targetVersion));

                  ensureLlamaRuntime();
              } else {
                  startOrRestartService(false);
              }

              return;
          }

          downloadAppBundle();
      });
}

bool LocalAiRuntimeInstaller::parseAppManifest(
  const QJsonObject& manifest,
  QString* error)
{
    if (manifest.value(
          QStringLiteral("schema_version"))
          .toInt() != 1) {
        if (error) {
            *error =
              tr("Unsupported Local AI Runtime "
                 "manifest schema.");
        }
        return false;
    }

    const QString latest =
      manifest.value(
              QStringLiteral("latest"))
        .toString()
        .trimmed();

    if (latest.isEmpty()) {
        if (error) {
            *error =
              tr("The Local AI Runtime manifest "
                 "has no latest version.");
        }
        return false;
    }

    QJsonObject selected;

    const QJsonArray packages =
      manifest.value(
              QStringLiteral("packages"))
        .toArray();

    for (const QJsonValue& value : packages) {
        const QJsonObject package =
          value.toObject();

        if (package.value(
                     QStringLiteral("version"))
                    .toString() == latest &&
            package.value(
                     QStringLiteral("platform"))
                    .toString() ==
              QStringLiteral("linux-x86_64")) {
            selected = package;
            break;
        }
    }

    if (selected.isEmpty()) {
        if (error) {
            *error =
              tr("No compatible Linux x86_64 "
                 "Local AI Runtime package was found.");
        }
        return false;
    }

    const QJsonObject archive =
      selected.value(
                QStringLiteral("archive"))
        .toObject();

    const QString name =
      archive.value(
               QStringLiteral("name"))
        .toString();

    const QString sha =
      archive.value(
               QStringLiteral("sha256"))
        .toString()
        .toLower();

    const qint64 size =
      archive.value(
               QStringLiteral("size"))
        .toVariant()
        .toLongLong();

    const QUrl url(
      archive.value(
               QStringLiteral("url"))
        .toString());

    const QRegularExpression shaPattern(
      QStringLiteral(
        "^[0-9a-f]{64}$"));

    if (name.isEmpty() ||
        QFileInfo(name).fileName() != name ||
        size <= 0 ||
        !shaPattern.match(sha).hasMatch() ||
        !url.isValid() ||
        url.scheme().compare(
          QStringLiteral("https"),
          Qt::CaseInsensitive) != 0) {
        if (error) {
            *error =
              tr("The Local AI Runtime archive "
                 "metadata is invalid.");
        }
        return false;
    }

    const QJsonObject installer =
      selected.value(
                QStringLiteral("installer"))
        .toObject();

    const QString installerPath =
      QDir::cleanPath(
        installer.value(
                   QStringLiteral("path"))
          .toString());

    if (installer.value(
                   QStringLiteral("requires_root"))
          .toBool(true)) {
        if (error) {
            *error =
              tr("The selected Local AI Runtime "
                 "package unexpectedly requires root.");
        }
        return false;
    }

    if (installerPath !=
        QStringLiteral("install-user.sh")) {
        if (error) {
            *error =
              tr("The Local AI Runtime installer "
                 "path is not permitted.");
        }
        return false;
    }

    QStringList installerArguments;

    const QJsonArray arguments =
      installer.value(
                 QStringLiteral("arguments"))
        .toArray();

    for (const QJsonValue& value : arguments) {
        installerArguments.append(
          value.toString());
    }

    if (!installerArguments.contains(
          QStringLiteral("--start"))) {
        if (error) {
            *error =
              tr("The Local AI Runtime installer "
                 "does not request user-service startup.");
        }
        return false;
    }

    m_targetVersion = latest;
    m_archiveName = name;
    m_expectedSha256 = sha;
    m_expectedArchiveSize = size;
    m_archiveUrl = url;
    m_installerPath = installerPath;
    m_installerArguments =
      installerArguments;

    return true;
}

void LocalAiRuntimeInstaller::downloadAppBundle()
{
    resetTemporaryFiles();

    m_tempDir =
      new QTemporaryDir(
        QDir::tempPath() +
        QStringLiteral(
          "/flameshot-local-ai-runtime-XXXXXX"));

    if (!m_tempDir->isValid()) {
        failInstall(
          tr("Could not create a temporary "
             "installation directory."));
        return;
    }

    m_archivePath =
      QDir(m_tempDir->path())
        .filePath(m_archiveName);

    m_archiveFile =
      new QFile(m_archivePath);

    if (!m_archiveFile->open(
          QIODevice::WriteOnly)) {
        failInstall(
          tr("Could not create the temporary "
             "runtime archive."));
        return;
    }

    m_archiveWriteFailed = false;

    emit installProgress(
      0,
      3,
      tr("Downloading Local AI Runtime %1...")
        .arg(m_targetVersion));

    QNetworkRequest request(m_archiveUrl);

    request.setRawHeader(
      QByteArrayLiteral("User-Agent"),
      QByteArrayLiteral("Flameshot-OCR/2.5"));

    request.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      static_cast<int>(
        QNetworkRequest::NoLessSafeRedirectPolicy));

    m_archiveReply =
      m_network.get(request);

    auto* timer =
      new QTimer(m_archiveReply);

    timer->setSingleShot(true);

    connect(
      timer,
      &QTimer::timeout,
      m_archiveReply,
      &QNetworkReply::abort);

    timer->start(120000);

    connect(
      m_archiveReply,
      &QNetworkReply::downloadProgress,
      this,
      [this, timer](
        qint64 received,
        qint64 total) {
          timer->start(120000);

          const qint64 usefulTotal =
            total > 0
              ? total
              : m_expectedArchiveSize;

          emit installProgress(
            0,
            3,
            tr("Downloading Local AI Runtime "
               "%1 — %2 / %3 bytes")
              .arg(m_targetVersion)
              .arg(received)
              .arg(usefulTotal));
      });

    connect(
      m_archiveReply,
      &QIODevice::readyRead,
      this,
      [this]() {
          if (!m_archiveReply ||
              !m_archiveFile) {
              return;
          }

          const QByteArray data =
            m_archiveReply->readAll();

          if (m_archiveFile->write(data) !=
              data.size()) {
              m_archiveWriteFailed = true;
          }
      });

    connect(
      m_archiveReply,
      &QNetworkReply::finished,
      this,
      [this]() {
          QNetworkReply* reply =
            m_archiveReply;

          m_archiveReply = nullptr;

          if (m_archiveFile && reply) {
              const QByteArray tail =
                reply->readAll();

              if (!tail.isEmpty() &&
                  m_archiveFile->write(tail) !=
                    tail.size()) {
                  m_archiveWriteFailed = true;
              }

              m_archiveFile->flush();
              m_archiveFile->close();
          }

          const bool networkOk =
            reply &&
            reply->error() ==
              QNetworkReply::NoError;

          const QString networkError =
            reply
              ? reply->errorString()
              : tr("Unknown download error");

          if (reply) {
              reply->deleteLater();
          }

          if (!networkOk) {
              failInstall(
                tr("Local AI Runtime download "
                   "failed: %1")
                  .arg(networkError));
              return;
          }

          if (m_archiveWriteFailed) {
              failInstall(
                tr("Could not write the downloaded "
                   "runtime archive."));
              return;
          }

          QString verifyError;

          if (!verifyDownloadedBundle(
                &verifyError)) {
              failInstall(verifyError);
              return;
          }

          extractAppBundle();
      });
}

bool LocalAiRuntimeInstaller::verifyDownloadedBundle(
  QString* error) const
{
    const QFileInfo info(m_archivePath);

    if (!info.isFile() ||
        info.size() !=
          m_expectedArchiveSize) {
        if (error) {
            *error =
              tr("Local AI Runtime archive size "
                 "verification failed.");
        }
        return false;
    }

    QFile file(m_archivePath);

    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error =
              tr("Could not open the downloaded "
                 "runtime archive for verification.");
        }
        return false;
    }

    QCryptographicHash hash(
      QCryptographicHash::Sha256);

    while (!file.atEnd()) {
        hash.addData(
          file.read(1024 * 1024));
    }

    const QString actual =
      QString::fromLatin1(
        hash.result().toHex());

    if (actual.compare(
          m_expectedSha256,
          Qt::CaseInsensitive) != 0) {
        if (error) {
            *error =
              tr("Local AI Runtime SHA256 "
                 "verification failed.");
        }
        return false;
    }

    return true;
}

void LocalAiRuntimeInstaller::extractAppBundle()
{
    if (!m_tempDir ||
        !m_tempDir->isValid()) {
        failInstall(
          tr("The temporary installation "
             "directory is unavailable."));
        return;
    }

    const QString extractDirectory =
      QDir(m_tempDir->path())
        .filePath(
          QStringLiteral("extract"));

    if (!QDir().mkpath(
          extractDirectory)) {
        failInstall(
          tr("Could not create the runtime "
             "extraction directory."));
        return;
    }

    emit installProgress(
      0,
      3,
      tr("Verifying and extracting "
         "Local AI Runtime..."));

    runProcess(
      QStringLiteral("/usr/bin/tar"),
      {
          QStringLiteral("-xzf"),
          m_archivePath,
          QStringLiteral("-C"),
          extractDirectory,
      },
      {},
      [this, extractDirectory](
        bool ok,
        const QString& output) {
          if (!ok) {
              failInstall(
                tr("Could not extract Local AI "
                   "Runtime: %1")
                  .arg(output));
              return;
          }

          const QFileInfoList directories =
            QDir(extractDirectory)
              .entryInfoList(
                QDir::Dirs |
                  QDir::NoDotAndDotDot,
                QDir::Name);

          if (directories.size() != 1) {
              failInstall(
                tr("The Local AI Runtime archive "
                   "has an unexpected layout."));
              return;
          }

          const QString root =
            directories.first()
              .absoluteFilePath();

          const QString installer =
            QDir(root)
              .filePath(m_installerPath);

          if (!QFileInfo(installer).isFile()) {
              failInstall(
                tr("The Local AI Runtime installer "
                   "is missing from the archive."));
              return;
          }

          m_installerPath = installer;

          runBundleInstaller();
      });
}

void LocalAiRuntimeInstaller::runBundleInstaller()
{
    emit installProgress(
      0,
      3,
      tr("Installing Local AI Runtime "
         "for the current user..."));

    QStringList arguments;
    arguments.append(m_installerPath);
    arguments.append(m_installerArguments);

    runProcess(
      QStringLiteral("/usr/bin/bash"),
      arguments,
      QFileInfo(m_installerPath)
        .absolutePath(),
      [this](
        bool ok,
        const QString& output) {
          if (!ok) {
              failInstall(
                tr("Local AI Runtime installation "
                   "failed: %1")
                  .arg(output));
              return;
          }

          // Always restart after installation. This is
          // required when an older runtime was already
          // active: systemctl enable --now alone does not
          // replace the already-running process.
          startOrRestartService(true);
      });
}

void LocalAiRuntimeInstaller::startOrRestartService(
  bool restart)
{
    emit installProgress(
      0,
      3,
      restart
        ? tr("Restarting the Local AI Runtime "
             "user service...")
        : tr("Starting the Local AI Runtime "
             "user service..."));

    runProcess(
      QStringLiteral("/usr/bin/systemctl"),
      {
          QStringLiteral("--user"),
          restart
            ? QStringLiteral("restart")
            : QStringLiteral("start"),
          QStringLiteral(
            "local-ai-runtime.service"),
      },
      {},
      [this](
        bool ok,
        const QString& output) {
          if (!ok) {
              failInstall(
                tr("Could not start the Local AI "
                   "Runtime user service: %1")
                  .arg(output));
              return;
          }

          m_pollAttempts = 0;
          m_consecutiveErrors = 0;

          waitForService();
      });
}

void LocalAiRuntimeInstaller::waitForService()
{
    ++m_pollAttempts;

    getJson(
      apiUrl(QStringLiteral("/health")),
      [this](
        bool ok,
        const QJsonObject& health,
        const QString&) {
          if (ok) {
              const QString service =
                health.value(
                        QStringLiteral("service"))
                  .toString();

              if (service !=
                  QStringLiteral("local-ai-runtime")) {
                  failInstall(
                    tr("Port 8111 became occupied "
                       "by another service."));
                  return;
              }

              emit installProgress(
                1,
                3,
                tr("Local AI Runtime %1 is "
                   "installed and running.")
                  .arg(
                    health.value(
                            QStringLiteral(
                              "runtime_version"))
                      .toString(
                        m_targetVersion)));

              ensureLlamaRuntime();
              return;
          }

          if (m_pollAttempts >=
              kServicePollLimit) {
              failInstall(
                tr("Local AI Runtime did not "
                   "become ready in time."));
              return;
          }

          QTimer::singleShot(
            500,
            this,
            &LocalAiRuntimeInstaller::
              waitForService);
      });
}

void LocalAiRuntimeInstaller::ensureLlamaRuntime()
{
    emit installProgress(
      1,
      3,
      tr("Checking the shared llama.cpp runtime..."));

    getJson(
      apiUrl(
        QStringLiteral(
          "/v1/runtime/llama/status?refresh=1")),
      [this](
        bool ok,
        const QJsonObject& status,
        const QString& error) {
          if (!ok) {
              failInstall(
                tr("Could not query the llama.cpp "
                   "runtime: %1")
                  .arg(error));
              return;
          }

          const bool installed =
            status.value(
                    QStringLiteral("installed"))
              .toBool();

          const QJsonObject operation =
            status.value(
                    QStringLiteral("operation"))
              .toObject();

          const QString state =
            operation.value(
                       QStringLiteral("state"))
              .toString();

          if (installed &&
              state != QStringLiteral("running")) {
              emit installProgress(
                2,
                3,
                tr("Shared llama.cpp runtime "
                   "is installed."));

              ensurePaddleModel();
              return;
          }

          if (state ==
              QStringLiteral("running")) {
              m_pollAttempts = 0;
              m_consecutiveErrors = 0;
              pollLlamaRuntime();
              return;
          }

          postJson(
            apiUrl(
              QStringLiteral(
                "/v1/runtime/llama/install")),
            {},
            [this](
              bool postOk,
              const QJsonObject&,
              const QString& postError) {
                if (!postOk) {
                    failInstall(
                      tr("Could not start the "
                         "llama.cpp runtime "
                         "installation: %1")
                        .arg(postError));
                    return;
                }

                m_pollAttempts = 0;
                m_consecutiveErrors = 0;

                pollLlamaRuntime();
            });
      });
}

void LocalAiRuntimeInstaller::pollLlamaRuntime()
{
    ++m_pollAttempts;

    emit installProgress(
      1,
      3,
      tr("Downloading and installing the "
         "shared llama.cpp runtime..."));

    getJson(
      apiUrl(
        QStringLiteral(
          "/v1/runtime/llama/status")),
      [this](
        bool ok,
        const QJsonObject& status,
        const QString& error) {
          if (!ok) {
              ++m_consecutiveErrors;

              if (m_consecutiveErrors >= 15) {
                  failInstall(
                    tr("Lost contact with Local AI "
                       "Runtime while installing "
                       "llama.cpp: %1")
                      .arg(error));
                  return;
              }
          } else {
              m_consecutiveErrors = 0;

              const bool installed =
                status.value(
                        QStringLiteral("installed"))
                  .toBool();

              const QJsonObject operation =
                status.value(
                        QStringLiteral("operation"))
                  .toObject();

              const QString state =
                operation.value(
                           QStringLiteral("state"))
                  .toString();

              if (installed &&
                  state !=
                    QStringLiteral("running")) {
                  emit installProgress(
                    2,
                    3,
                    tr("Shared llama.cpp runtime "
                       "is installed."));

                  ensurePaddleModel();
                  return;
              }

              if (state ==
                  QStringLiteral("error")) {
                  failInstall(
                    tr("llama.cpp runtime "
                       "installation failed: %1")
                      .arg(
                        operation.value(
                                   QStringLiteral(
                                     "error"))
                          .toString()));
                  return;
              }

              if (state ==
                    QStringLiteral("success") &&
                  !installed) {
                  failInstall(
                    tr("The llama.cpp runtime "
                       "reported success but is "
                       "not installed."));
                  return;
              }
          }

          if (m_pollAttempts >=
              kLlamaPollLimit) {
              failInstall(
                tr("Timed out while installing "
                   "the shared llama.cpp runtime."));
              return;
          }

          QTimer::singleShot(
            1000,
            this,
            &LocalAiRuntimeInstaller::
              pollLlamaRuntime);
      });
}

bool LocalAiRuntimeInstaller::readPaddleStatus(
  const QJsonObject& object,
  bool* installed,
  QString* version,
  QString* operationState,
  QString* operationError) const
{
    const QJsonArray data =
      object.value(
              QStringLiteral("data"))
        .toArray();

    for (const QJsonValue& value : data) {
        const QJsonObject model =
          value.toObject();

        if (model.value(
                   QStringLiteral("id"))
                  .toString() !=
            kPaddleModelId) {
            continue;
        }

        if (installed) {
            *installed =
              model.value(
                     QStringLiteral("installed"))
                .toBool();
        }

        if (version) {
            *version =
              model.value(
                     QStringLiteral("version"))
                .toString();
        }

        const QJsonObject operation =
          model.value(
                 QStringLiteral("operation"))
            .toObject();

        if (operationState) {
            *operationState =
              operation.value(
                       QStringLiteral("state"))
                .toString();
        }

        if (operationError) {
            *operationError =
              operation.value(
                       QStringLiteral("error"))
                .toString();
        }

        return true;
    }

    return false;
}

void LocalAiRuntimeInstaller::ensurePaddleModel()
{
    emit installProgress(
      2,
      3,
      tr("Checking PaddleOCR-VL 1.6..."));

    getJson(
      apiUrl(
        QStringLiteral(
          "/v1/models?refresh=1")),
      [this](
        bool ok,
        const QJsonObject& models,
        const QString& error) {
          if (!ok) {
              failInstall(
                tr("Could not query the Local AI "
                   "Runtime model catalog: %1")
                  .arg(error));
              return;
          }

          bool installed = false;
          QString version;
          QString state;
          QString operationError;

          if (!readPaddleStatus(
                models,
                &installed,
                &version,
                &state,
                &operationError)) {
              failInstall(
                tr("PaddleOCR-VL 1.6 is not "
                   "available in the shared model "
                   "catalog."));
              return;
          }

          if (installed &&
              state != QStringLiteral("running")) {
              finishInstall();
              return;
          }

          if (state ==
              QStringLiteral("running")) {
              m_pollAttempts = 0;
              m_consecutiveErrors = 0;

              pollPaddleModel();
              return;
          }

          QJsonObject payload;
          payload.insert(
            QStringLiteral("model"),
            kPaddleModelId);

          postJson(
            apiUrl(
              QStringLiteral(
                "/v1/models/install")),
            payload,
            [this](
              bool postOk,
              const QJsonObject&,
              const QString& postError) {
                if (!postOk) {
                    failInstall(
                      tr("Could not start "
                         "PaddleOCR-VL installation: "
                         "%1")
                        .arg(postError));
                    return;
                }

                m_pollAttempts = 0;
                m_consecutiveErrors = 0;

                pollPaddleModel();
            });
      });
}

void LocalAiRuntimeInstaller::pollPaddleModel()
{
    ++m_pollAttempts;

    emit installProgress(
      2,
      3,
      tr("Downloading and verifying "
         "PaddleOCR-VL 1.6..."));

    getJson(
      apiUrl(QStringLiteral("/v1/models")),
      [this](
        bool ok,
        const QJsonObject& models,
        const QString& error) {
          if (!ok) {
              ++m_consecutiveErrors;

              if (m_consecutiveErrors >= 15) {
                  failInstall(
                    tr("Lost contact with Local AI "
                       "Runtime while installing "
                       "PaddleOCR-VL: %1")
                      .arg(error));
                  return;
              }
          } else {
              m_consecutiveErrors = 0;

              bool installed = false;
              QString version;
              QString state;
              QString operationError;

              if (!readPaddleStatus(
                    models,
                    &installed,
                    &version,
                    &state,
                    &operationError)) {
                  failInstall(
                    tr("PaddleOCR-VL disappeared "
                       "from the model catalog."));
                  return;
              }

              if (installed &&
                  state !=
                    QStringLiteral("running")) {
                  finishInstall();
                  return;
              }

              if (state ==
                  QStringLiteral("error")) {
                  failInstall(
                    tr("PaddleOCR-VL installation "
                       "failed: %1")
                      .arg(operationError));
                  return;
              }

              if (state ==
                    QStringLiteral("success") &&
                  !installed) {
                  failInstall(
                    tr("PaddleOCR-VL reported "
                       "success but is not installed."));
                  return;
              }
          }

          if (m_pollAttempts >=
              kModelPollLimit) {
              failInstall(
                tr("Timed out while installing "
                   "PaddleOCR-VL."));
              return;
          }

          QTimer::singleShot(
            1000,
            this,
            &LocalAiRuntimeInstaller::
              pollPaddleModel);
      });
}

void LocalAiRuntimeInstaller::runProcess(
  const QString& program,
  const QStringList& arguments,
  const QString& workingDirectory,
  std::function<void(bool, const QString&)> callback)
{
    auto* process =
      new QProcess(this);

    process->setProcessEnvironment(
      cleanProcessEnvironment());

    if (!workingDirectory.isEmpty()) {
        process->setWorkingDirectory(
          workingDirectory);
    }

    process->setProgram(program);
    process->setArguments(arguments);
    process->setProcessChannelMode(
      QProcess::MergedChannels);

    const auto completed =
      std::make_shared<bool>(false);

    connect(
      process,
      &QProcess::errorOccurred,
      this,
      [process, callback, completed](
        QProcess::ProcessError processError) {
          if (*completed ||
              processError !=
                QProcess::FailedToStart) {
              return;
          }

          *completed = true;

          const QString message =
            process->errorString();

          process->deleteLater();

          callback(false, message);
      });

    connect(
      process,
      qOverload<int, QProcess::ExitStatus>(
        &QProcess::finished),
      this,
      [process, callback, completed](
        int exitCode,
        QProcess::ExitStatus exitStatus) {
          if (*completed) {
              return;
          }

          *completed = true;

          const QString output =
            QString::fromUtf8(
              process->readAll());

          const bool ok =
            exitStatus ==
              QProcess::NormalExit &&
            exitCode == 0;

          process->deleteLater();

          callback(
            ok,
            output.trimmed());
      });

    process->start();
}

void LocalAiRuntimeInstaller::finishInstall()
{
    m_busy = false;

    resetTemporaryFiles();

    emit installProgress(
      3,
      3,
      tr("Local AI Runtime, llama.cpp and "
         "PaddleOCR-VL are ready."));

    emit installFinished(
      true,
      tr("Local AI Runtime is ready for OCR."));

    refreshStatus();
}

void LocalAiRuntimeInstaller::failInstall(
  const QString& message)
{
    m_busy = false;

    resetTemporaryFiles();

    emit installFinished(
      false,
      message);

    refreshStatus();
}

void LocalAiRuntimeInstaller::resetTemporaryFiles()
{
    if (m_archiveReply) {
        m_archiveReply->abort();
        m_archiveReply->deleteLater();
        m_archiveReply = nullptr;
    }

    if (m_archiveFile) {
        if (m_archiveFile->isOpen()) {
            m_archiveFile->close();
        }

        delete m_archiveFile;
        m_archiveFile = nullptr;
    }

    delete m_tempDir;
    m_tempDir = nullptr;

    m_archivePath.clear();
    m_archiveWriteFailed = false;
}
