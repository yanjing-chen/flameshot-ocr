// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrmanager.h"

#include "utils/confighandler.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <sys/types.h>

namespace
{
constexpr qint64 PADDLE_16_MODEL_SIZE = 935769056;
constexpr qint64 PADDLE_16_MMPROJ_SIZE = 881770560;

bool fileMatchesSize(const QString& path, qint64 expected)
{
    const QFileInfo info(path);
    return info.isFile() && (expected <= 0 || info.size() == expected);
}

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

OcrModelInfo parseModel(const QJsonObject& obj)
{
    OcrModelInfo model;
    model.id = obj.value(QStringLiteral("id")).toString();
    model.name = obj.value(QStringLiteral("name")).toString();
    model.directory = obj.value(QStringLiteral("directory")).toString();
    model.modelFile = obj.value(QStringLiteral("model_file")).toString();
    model.mmprojFile = obj.value(QStringLiteral("mmproj_file")).toString();
    model.modelUrl = QUrl(obj.value(QStringLiteral("model_url")).toString());
    model.mmprojUrl = QUrl(obj.value(QStringLiteral("mmproj_url")).toString());
    model.modelSize =
      obj.value(QStringLiteral("model_size")).toVariant().toLongLong();
    model.mmprojSize =
      obj.value(QStringLiteral("mmproj_size")).toVariant().toLongLong();
    model.prompt = obj.value(QStringLiteral("prompt")).toString(
      QStringLiteral("OCR:"));
    model.verified = obj.value(QStringLiteral("supported")).toBool(false);
    return model;
}

bool validModel(const OcrModelInfo& model)
{
    return model.verified && !model.id.isEmpty() && !model.name.isEmpty() &&
           !model.directory.isEmpty() && !model.modelFile.isEmpty() &&
           !model.mmprojFile.isEmpty() && model.modelUrl.isValid() &&
           model.mmprojUrl.isValid() && model.modelSize > 0 &&
           model.mmprojSize > 0;
}

QString serviceDataDir()
{
    const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/flameshot-ocr");
    QDir().mkpath(dir);
    return dir;
}

QString managedPidPath()
{
    return serviceDataDir() + QStringLiteral("/llama-server.pid");
}

bool processAlive(qint64 pid)
{
    if (pid <= 0) {
        return false;
    }

    errno = 0;
    const int rc = ::kill(static_cast<pid_t>(pid), 0);
    return rc == 0 || errno == EPERM;
}

bool processHasFlameshotMarker(qint64 pid)
{
    if (!processAlive(pid)) {
        return false;
    }

    QFile environ(
      QStringLiteral("/proc/%1/environ").arg(QString::number(pid)));
    if (!environ.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = environ.readAll();
    const QList<QByteArray> entries = data.split('\0');
    for (const QByteArray& entry : entries) {
        if (entry == QByteArrayLiteral("FLAMESHOT_OCR_MANAGED=1")) {
            return true;
        }
    }
    return false;
}

qint64 readManagedPid()
{
    QFile file(managedPidPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    bool ok = false;
    const qint64 pid = file.readAll().trimmed().toLongLong(&ok);
    return ok && pid > 0 ? pid : 0;
}

void clearManagedPid()
{
    QFile::remove(managedPidPath());
}

bool writeManagedPid(qint64 pid)
{
    QSaveFile file(managedPidPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    file.write(QByteArray::number(pid));
    file.write("\n");
    return file.commit();
}

qint64 verifiedManagedPid()
{
    const qint64 pid = readManagedPid();
    if (pid <= 0) {
        return 0;
    }

    if (!processHasFlameshotMarker(pid)) {
        clearManagedPid();
        return 0;
    }

    return pid;
}

QList<OcrDeviceInfo> probeDevices(const QString& executable)
{
    QList<OcrDeviceInfo> devices;
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable()) {
        return devices;
    }

    QProcess probe;
    probe.start(executable, { QStringLiteral("--list-devices") });
    if (!probe.waitForStarted(3000) || !probe.waitForFinished(8000)) {
        return devices;
    }

    const QString output =
      QString::fromUtf8(probe.readAllStandardOutput()) +
      QString::fromUtf8(probe.readAllStandardError());

    const QRegularExpression deviceExpression(
      QStringLiteral(
        R"(((?:CUDA|Vulkan)\d+):\s*(.*?)(?=\s+(?:CUDA|Vulkan)\d+:|\r?\n|$))"),
      QRegularExpression::CaseInsensitiveOption);

    auto matches = deviceExpression.globalMatch(output);
    while (matches.hasNext()) {
        const auto match = matches.next();

        OcrDeviceInfo device;
        device.id = match.captured(1).trimmed();
        device.backend =
          device.id.startsWith(QStringLiteral("CUDA"), Qt::CaseInsensitive)
            ? QStringLiteral("CUDA")
            : QStringLiteral("Vulkan");
        device.name = match.captured(2).trimmed();

        device.name.remove(QRegularExpression(
          QStringLiteral(
            R"(\s+\(\d+\s+MiB,\s+\d+\s+MiB\s+free\)\s*$)"),
          QRegularExpression::CaseInsensitiveOption));

        if (device.name.isEmpty()) {
            device.name = device.id;
        }

        bool duplicate = false;
        for (const auto& existing : devices) {
            if (existing.id.compare(device.id, Qt::CaseInsensitive) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            devices.append(device);
        }
    }

    return devices;
}

int automaticDeviceScore(const OcrDeviceInfo& device)
{
    if (device.backend.compare(QStringLiteral("CUDA"), Qt::CaseInsensitive) ==
        0) {
        return 5000;
    }

    const QString name = device.name.toLower();
    if (name.contains(QStringLiteral("nvidia"))) {
        return 4000;
    }
    if (name.contains(QStringLiteral("amd")) ||
        name.contains(QStringLiteral("radeon"))) {
        return 3000;
    }
    if (name.contains(QStringLiteral("intel"))) {
        return 2000;
    }
    return 1000;
}

QString bestAutomaticDevice(const QList<OcrDeviceInfo>& devices)
{
    QString best;
    int bestScore = -1;

    for (const auto& device : devices) {
        const int score = automaticDeviceScore(device);
        if (score > bestScore) {
            bestScore = score;
            best = device.id;
        }
    }

    return best;
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

QList<OcrModelInfo> OcrManager::builtInModels()
{
    OcrModelInfo model;
    model.id = QStringLiteral("paddleocr-vl-1.6");
    model.name = QStringLiteral("PaddleOCR-VL-1.6");
    model.directory = QStringLiteral("PaddleOCR-VL-1.6");
    model.modelFile = QStringLiteral("PaddleOCR-VL-1.6-GGUF.gguf");
    model.mmprojFile =
      QStringLiteral("PaddleOCR-VL-1.6-GGUF-mmproj.gguf");
    model.modelUrl = QUrl(
      QStringLiteral("https://huggingface.co/PaddlePaddle/"
                     "PaddleOCR-VL-1.6-GGUF/resolve/main/"
                     "PaddleOCR-VL-1.6-GGUF.gguf"));
    model.mmprojUrl = QUrl(
      QStringLiteral("https://huggingface.co/PaddlePaddle/"
                     "PaddleOCR-VL-1.6-GGUF/resolve/main/"
                     "PaddleOCR-VL-1.6-GGUF-mmproj.gguf"));
    model.modelSize = PADDLE_16_MODEL_SIZE;
    model.mmprojSize = PADDLE_16_MMPROJ_SIZE;
    model.prompt = QStringLiteral("OCR:");
    model.verified = true;
    return { model };
}

QString OcrManager::manifestCachePath() const
{
    const QString base =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/flameshot-ocr");
    QDir().mkpath(base);
    return base + QStringLiteral("/models-manifest.json");
}

QList<OcrModelInfo> OcrManager::cachedRemoteModels() const
{
    QFile file(manifestCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }

    QList<OcrModelInfo> models;
    const QJsonArray array =
      doc.object().value(QStringLiteral("models")).toArray();
    for (const auto& value : array) {
        if (!value.isObject()) {
            continue;
        }
        const OcrModelInfo model = parseModel(value.toObject());
        if (validModel(model)) {
            models.append(model);
        }
    }
    return models;
}

QList<OcrModelInfo> OcrManager::availableModels() const
{
    QList<OcrModelInfo> models = builtInModels();
    const QList<OcrModelInfo> remote = cachedRemoteModels();

    for (const auto& candidate : remote) {
        bool replaced = false;
        for (auto& existing : models) {
            if (existing.id == candidate.id) {
                existing = candidate;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            models.append(candidate);
        }
    }
    return models;
}

QString OcrManager::latestModelId() const
{
    QFile file(manifestCachePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            const QString latest =
              doc.object().value(QStringLiteral("latest")).toString();
            for (const auto& model : availableModels()) {
                if (model.id == latest) {
                    return latest;
                }
            }
        }
    }
    return QStringLiteral("paddleocr-vl-1.6");
}

OcrModelInfo OcrManager::modelById(const QString& id) const
{
    for (const auto& model : availableModels()) {
        if (model.id == id) {
            return model;
        }
    }
    return {};
}

OcrModelInfo OcrManager::activeModel() const
{
    OcrModelInfo model = modelById(ConfigHandler().ocrModelId());
    if (model.id.isEmpty()) {
        model = builtInModels().first();
    }
    return model;
}

QString OcrManager::modelRoot() const
{
    const QString configured = ConfigHandler().ocrModelRoot().trimmed();
    if (!configured.isEmpty()) {
        return QDir::cleanPath(QDir::fromNativeSeparators(configured));
    }

    // Detect the path used during early Flameshot OCR development.
    const QString legacyRoot = QDir::homePath() + QStringLiteral("/Models");
    const QString legacyModel =
      legacyRoot +
      QStringLiteral("/PaddleOCR-VL-1.6/PaddleOCR-VL-1.6-GGUF.gguf");
    const QString legacyMmproj =
      legacyRoot +
      QStringLiteral("/PaddleOCR-VL-1.6/"
                     "PaddleOCR-VL-1.6-GGUF-mmproj.gguf");
    if (QFileInfo::exists(legacyModel) && QFileInfo::exists(legacyMmproj)) {
        return legacyRoot;
    }

    return QStandardPaths::writableLocation(
             QStandardPaths::GenericDataLocation) +
           QStringLiteral("/flameshot-ocr/models");
}

QString OcrManager::modelDirectory(const OcrModelInfo& model) const
{
    return QDir(modelRoot()).filePath(model.directory);
}

QString OcrManager::modelPath(const OcrModelInfo& model) const
{
    return QDir(modelDirectory(model)).filePath(model.modelFile);
}

QString OcrManager::mmprojPath(const OcrModelInfo& model) const
{
    return QDir(modelDirectory(model)).filePath(model.mmprojFile);
}

bool OcrManager::modelInstalled(const OcrModelInfo& model) const
{
    return fileMatchesSize(modelPath(model), model.modelSize) &&
           fileMatchesSize(mmprojPath(model), model.mmprojSize);
}

QString OcrManager::modelStatusText(const OcrModelInfo& model) const
{
    if (model.id.isEmpty()) {
        return tr("Unknown model");
    }

    const QFileInfo mainInfo(modelPath(model));
    const QFileInfo visionInfo(mmprojPath(model));

    if (modelInstalled(model)) {
        return tr("Installed and verified by file size");
    }
    if (!mainInfo.exists() && !visionInfo.exists()) {
        return tr("Not installed");
    }
    return tr("Incomplete or file size does not match");
}

QString OcrManager::serverExecutable() const
{
    // Explicit user configuration always wins.
    const QString configured = ConfigHandler().ocrServerPath().trimmed();
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable()) {
        return configured;
    }

    // User-managed runtimes live outside the AppImage so they can be replaced
    // independently from Flameshot itself.
    const QString userRuntimeRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/flameshot-ocr/runtime");

    // Versioned CUDA runtime layout:
    //   runtime/cuda/<version>/
    //   runtime/cuda/current -> <version>
    // The "current" symlink lets the runtime manager switch versions
    // atomically without replacing files that may still be in use.
    const QString userCudaCurrentRoot =
      QDir(userRuntimeRoot).filePath(QStringLiteral("cuda/current"));

    // AppImage layout:
    //   AppDir/usr/bin/flameshot
    //   AppDir/usr/lib/flameshot-ocr/runtime/llama-server-*
    const QString packagedRuntimeRoot = QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("../lib/flameshot-ocr/runtime")));

    QStringList runtimeRoots;
    runtimeRoots << userCudaCurrentRoot << userRuntimeRoot;
    if (packagedRuntimeRoot != userRuntimeRoot &&
        packagedRuntimeRoot != userCudaCurrentRoot) {
        runtimeRoots << packagedRuntimeRoot;
    }

    auto executableInRoot = [](const QString& root,
                               const QString& name) -> QString {
        const QString path = QDir(root).filePath(name);
        return QFileInfo(path).isExecutable() ? path : QString();
    };

    auto firstRuntimeWithDevice =
      [this, &runtimeRoots, &executableInRoot](const QString& fileName,
                                               const QString& devicePrefix)
      -> QString {
        for (const QString& root : runtimeRoots) {
            const QString candidate = executableInRoot(root, fileName);
            if (candidate.isEmpty()) {
                continue;
            }

            const QString device = chooseDevice(candidate);
            if (device.startsWith(devicePrefix)) {
                return candidate;
            }
        }
        return {};
    };

    // Avoid probing a CUDA runtime on machines without a loaded NVIDIA driver.
    const bool nvidiaDriverPresent =
      QFileInfo::exists(QStringLiteral("/dev/nvidiactl")) ||
      QFileInfo::exists(QStringLiteral("/dev/nvidia0"));

    // 1. CUDA on NVIDIA.
    if (nvidiaDriverPresent) {
        const QString cudaRuntime =
          firstRuntimeWithDevice(QStringLiteral("llama-server-cuda"),
                                 QStringLiteral("CUDA"));
        if (!cudaRuntime.isEmpty()) {
            return cudaRuntime;
        }
    }

    // 2. Vulkan on AMD / Intel / NVIDIA.
    const QString vulkanRuntime =
      firstRuntimeWithDevice(QStringLiteral("llama-server-vulkan"),
                             QStringLiteral("Vulkan"));
    if (!vulkanRuntime.isEmpty()) {
        return vulkanRuntime;
    }

    // Backward-compatible generic runtimes. A generic llama-server may itself
    // have CUDA or Vulkan support, so probe it before falling back to CPU.
    QStringList genericCandidates;
    for (const QString& root : runtimeRoots) {
        const QString generic =
          executableInRoot(root, QStringLiteral("llama-server"));
        if (!generic.isEmpty()) {
            genericCandidates << generic;
        }
    }

    const QString development =
      QDir::homePath() + QStringLiteral("/llama.cpp/build/bin/llama-server");
    if (QFileInfo(development).isExecutable() &&
        !genericCandidates.contains(development)) {
        genericCandidates << development;
    }

    const QString pathRuntime =
      QStandardPaths::findExecutable(QStringLiteral("llama-server"));
    if (!pathRuntime.isEmpty() && !genericCandidates.contains(pathRuntime)) {
        genericCandidates << pathRuntime;
    }

    if (nvidiaDriverPresent) {
        for (const QString& candidate : genericCandidates) {
            if (chooseDevice(candidate).startsWith(QStringLiteral("CUDA"))) {
                return candidate;
            }
        }
    }

    for (const QString& candidate : genericCandidates) {
        if (chooseDevice(candidate).startsWith(QStringLiteral("Vulkan"))) {
            return candidate;
        }
    }

    // 3. Reliable CPU fallback.
    for (const QString& root : runtimeRoots) {
        const QString cpuRuntime =
          executableInRoot(root, QStringLiteral("llama-server-cpu"));
        if (!cpuRuntime.isEmpty()) {
            return cpuRuntime;
        }
    }

    // Last resort: a generic executable can still run with -ngl 0.
    if (!genericCandidates.isEmpty()) {
        return genericCandidates.first();
    }

    return {};
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

QString OcrManager::activePrompt() const
{
    const QString prompt = activeModel().prompt;
    return prompt.isEmpty() ? QStringLiteral("OCR:") : prompt;
}

QList<OcrDeviceInfo> OcrManager::availableDevices() const
{
    QStringList candidates;

    auto appendExecutable = [&candidates](const QString& path) {
        if (!path.isEmpty() && QFileInfo(path).isExecutable() &&
            !candidates.contains(path)) {
            candidates.append(path);
        }
    };

    const QString configured = ConfigHandler().ocrServerPath().trimmed();
    appendExecutable(configured);

    const QString userRuntimeRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/flameshot-ocr/runtime");
    const QString userCudaCurrentRoot =
      QDir(userRuntimeRoot).filePath(QStringLiteral("cuda/current"));
    const QString packagedRuntimeRoot = QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("../lib/flameshot-ocr/runtime")));

    const QStringList roots = {
        userCudaCurrentRoot, userRuntimeRoot, packagedRuntimeRoot
    };
    const QStringList runtimeNames = {
        QStringLiteral("llama-server-cuda"),
        QStringLiteral("llama-server-vulkan"),
        QStringLiteral("llama-server")
    };

    for (const QString& root : roots) {
        for (const QString& name : runtimeNames) {
            appendExecutable(QDir(root).filePath(name));
        }
    }

    appendExecutable(QDir::homePath() +
                     QStringLiteral("/llama.cpp/build/bin/llama-server"));
    appendExecutable(
      QStandardPaths::findExecutable(QStringLiteral("llama-server")));

    QList<OcrDeviceInfo> result;
    for (const QString& executable : candidates) {
        const auto probed = probeDevices(executable);
        for (const auto& device : probed) {
            bool duplicate = false;
            for (const auto& existing : result) {
                if (existing.id.compare(device.id, Qt::CaseInsensitive) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                result.append(device);
            }
        }
    }

    return result;
}

QString OcrManager::chooseDevice(const QString& executable) const
{
    const QString preference = ConfigHandler().ocrDeviceId().trimmed();

    if (preference.compare(QStringLiteral("cpu"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    const QList<OcrDeviceInfo> devices = probeDevices(executable);
    if (devices.isEmpty()) {
        return {};
    }

    if (!preference.isEmpty() &&
        preference.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0) {
        for (const auto& device : devices) {
            if (device.id.compare(preference, Qt::CaseInsensitive) == 0) {
                return device.id;
            }
        }
        return {};
    }

    return bestAutomaticDevice(devices);
}

QString OcrManager::detectedDevice() const
{
    const QString executable = serverExecutable();
    const QString device = chooseDevice(executable);
    return device.isEmpty() ? tr("CPU") : device;
}

bool OcrManager::startServer(QString* error)
{
    if (verifiedManagedPid() > 0) {
        return true;
    }

    const QUrl base = serverBaseUrl();
    const QString host = base.host();
    if (host != QStringLiteral("127.0.0.1") &&
        host != QStringLiteral("localhost") &&
        host != QStringLiteral("::1")) {
        if (error) {
            *error = tr("Automatic server start is only allowed for localhost.");
        }
        return false;
    }

    const OcrModelInfo model = activeModel();
    if (!modelInstalled(model)) {
        if (error) {
            *error = tr("The selected OCR model is not installed completely.");
        }
        return false;
    }

    const QString executable = serverExecutable();
    if (executable.isEmpty()) {
        if (error) {
            *error =
              tr("llama-server was not found. Set its path in OCR settings.");
        }
        return false;
    }

    QStringList args;
    args << QStringLiteral("-m") << modelPath(model)
         << QStringLiteral("--mmproj") << mmprojPath(model)
         << QStringLiteral("--host") << QStringLiteral("127.0.0.1")
         << QStringLiteral("--port")
         << QString::number(base.port(8111))
         << QStringLiteral("--temp") << QStringLiteral("0")
         << QStringLiteral("--parallel") << QStringLiteral("1")
         << QStringLiteral("-c") << QStringLiteral("8192");

    const QString device = chooseDevice(executable);
    if (!device.isEmpty()) {
        args << QStringLiteral("--device") << device
             << QStringLiteral("-ngl") << QStringLiteral("99");
    } else {
        args << QStringLiteral("-ngl") << QStringLiteral("0");
    }

    const QString logPath =
      serviceDataDir() + QStringLiteral("/llama-server.log");

    QProcess detached;
    detached.setProgram(executable);
    detached.setArguments(args);
    detached.setStandardOutputFile(logPath, QIODevice::Append);
    detached.setStandardErrorFile(logPath, QIODevice::Append);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("FLAMESHOT_OCR_MANAGED"),
                       QStringLiteral("1"));
    detached.setProcessEnvironment(environment);

    qint64 pid = 0;
    if (!detached.startDetached(&pid) || pid <= 0) {
        if (error) {
            *error =
              tr("Could not start llama-server: %1").arg(detached.errorString());
        }
        return false;
    }

    if (!writeManagedPid(pid)) {
        ::kill(static_cast<pid_t>(pid), SIGTERM);
        if (error) {
            *error =
              tr("Could not start llama-server: %1").arg(managedPidPath());
        }
        return false;
    }

    emit serverStateChanged();
    return true;
}

void OcrManager::stopServer()
{
    const qint64 pid = verifiedManagedPid();
    if (pid <= 0) {
        clearManagedPid();
        emit serverStateChanged();
        return;
    }

    ::kill(static_cast<pid_t>(pid), SIGTERM);

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000 && processAlive(pid)) {
        QThread::msleep(100);
    }

    if (processAlive(pid) && processHasFlameshotMarker(pid)) {
        ::kill(static_cast<pid_t>(pid), SIGKILL);

        timer.restart();
        while (timer.elapsed() < 2000 && processAlive(pid)) {
            QThread::msleep(100);
        }
    }

    clearManagedPid();
    emit serverStateChanged();
}

bool OcrManager::managedServerRunning() const
{
    return verifiedManagedPid() > 0;
}

void OcrManager::testConnection(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    QPointer<QObject> guard(context);
    QNetworkRequest request(healthEndpoint());
    QNetworkReply* reply = m_network.get(request);

    connect(reply, &QNetworkReply::finished, this, [guard, reply, callback]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString error = ok ? QString() : reply->errorString();
        reply->deleteLater();
        if (guard) {
            callback(ok, error);
        }
    });
}

void OcrManager::waitUntilHealthy(
  QObject* context,
  int attemptsLeft,
  std::function<void(bool, const QString&)> callback)
{
    QPointer<QObject> guard(context);
    testConnection(
      context,
      [this, guard, attemptsLeft, callback](bool ok, const QString& error) {
          if (!guard) {
              return;
          }
          if (ok) {
              callback(true, {});
              return;
          }
          if (attemptsLeft <= 0) {
              callback(false,
                       tr("llama-server did not become ready: %1").arg(error));
              return;
          }
          QTimer::singleShot(
            500,
            guard.data(),
            [this, guard, attemptsLeft, callback]() {
                if (guard) {
                    waitUntilHealthy(
                      guard.data(), attemptsLeft - 1, callback);
                }
            });
      });
}

void OcrManager::ensureReady(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    testConnection(
      context,
      [this, context, callback](bool ok, const QString& error) {
          if (ok) {
              callback(true, {});
              return;
          }

          if (!ConfigHandler().ocrAutoStartServer()) {
              callback(false,
                       tr("OCR service is not running and automatic start is "
                          "disabled.\n%1")
                         .arg(error));
              return;
          }

          QString startError;
          if (!startServer(&startError)) {
              callback(false, startError);
              return;
          }

          // Up to 30 seconds for model loading on slower CPUs.
          waitUntilHealthy(context, 60, callback);
      });
}

void OcrManager::downloadModel(const QString& modelId)
{
    if (m_downloadReply) {
        emit downloadFinished(false, tr("A model download is already running."));
        return;
    }

    const OcrModelInfo model = modelById(modelId);
    if (model.id.isEmpty()) {
        emit downloadFinished(false, tr("Unknown OCR model."));
        return;
    }

    m_downloadModelDir = modelDirectory(model);
    if (!QDir().mkpath(m_downloadModelDir)) {
        emit downloadFinished(false, tr("Could not create the model directory."));
        return;
    }

    m_downloadQueue.clear();
    m_downloadQueue.append(
      { model.modelFile, model.modelUrl, model.modelSize });
    m_downloadQueue.append(
      { model.mmprojFile, model.mmprojUrl, model.mmprojSize });
    m_downloadIndex = 0;
    m_downloadCompleted = 0;
    m_downloadTotal = model.modelSize + model.mmprojSize;
    m_downloadCanceled = false;

    startNextDownload();
}

void OcrManager::startNextDownload()
{
    if (m_downloadCanceled) {
        failDownload(tr("Download canceled. Partial files were kept for resume."));
        return;
    }

    while (m_downloadIndex < m_downloadQueue.size()) {
        const DownloadItem item = m_downloadQueue.at(m_downloadIndex);
        const QString finalPath =
          QDir(m_downloadModelDir).filePath(item.fileName);

        if (fileMatchesSize(finalPath, item.expectedSize)) {
            m_downloadCompleted += item.expectedSize;
            ++m_downloadIndex;
            emit downloadProgress(
              m_downloadCompleted, m_downloadTotal, item.fileName);
            continue;
        }

        if (QFileInfo::exists(finalPath)) {
            QFile::remove(finalPath);
        }

        const QString partPath = finalPath + QStringLiteral(".part");
        qint64 offset = QFileInfo(partPath).isFile()
                          ? QFileInfo(partPath).size()
                          : 0;
        if (offset < 0 || offset >= item.expectedSize) {
            QFile::remove(partPath);
            offset = 0;
        }

        m_downloadOffset = offset;
        m_downloadFile = new QFile(partPath, this);
        QIODevice::OpenMode mode = QIODevice::WriteOnly;
        if (offset > 0) {
            mode |= QIODevice::Append;
        } else {
            mode |= QIODevice::Truncate;
        }

        if (!m_downloadFile->open(mode)) {
            failDownload(tr("Could not write %1").arg(partPath));
            return;
        }

        QNetworkRequest request(item.url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        if (offset > 0) {
            request.setRawHeader(
              "Range",
              QByteArray("bytes=") + QByteArray::number(offset) + "-");
        }

        m_downloadReply = m_network.get(request);

        connect(m_downloadReply,
                &QNetworkReply::metaDataChanged,
                this,
                [this]() {
                    if (!m_downloadReply || !m_downloadFile ||
                        m_downloadOffset <= 0) {
                        return;
                    }

                    const int status =
                      m_downloadReply
                        ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                    if (status == 200) {
                        // Server ignored Range; safely restart this file.
                        m_downloadFile->resize(0);
                        m_downloadOffset = 0;
                    }
                });

        connect(m_downloadReply,
                &QNetworkReply::readyRead,
                this,
                [this]() {
                    if (m_downloadReply && m_downloadFile) {
                        m_downloadFile->write(m_downloadReply->readAll());
                    }
                });

        connect(m_downloadReply,
                &QNetworkReply::downloadProgress,
                this,
                [this, item](qint64 received, qint64) {
                    const qint64 done =
                      m_downloadCompleted + m_downloadOffset + received;
                    emit downloadProgress(
                      qMin(done, m_downloadTotal),
                      m_downloadTotal,
                      item.fileName);
                });

        connect(m_downloadReply,
                &QNetworkReply::finished,
                this,
                [this, item, finalPath, partPath]() {
                    if (m_downloadFile) {
                        if (m_downloadReply) {
                            m_downloadFile->write(m_downloadReply->readAll());
                        }
                        m_downloadFile->flush();
                        m_downloadFile->close();
                        m_downloadFile->deleteLater();
                        m_downloadFile = nullptr;
                    }

                    const bool canceled = m_downloadCanceled;
                    const auto error =
                      m_downloadReply ? m_downloadReply->error()
                                      : QNetworkReply::UnknownNetworkError;
                    const QString errorText =
                      m_downloadReply ? m_downloadReply->errorString()
                                      : tr("Unknown network error");

                    if (m_downloadReply) {
                        m_downloadReply->deleteLater();
                        m_downloadReply = nullptr;
                    }

                    if (canceled) {
                        failDownload(
                          tr("Download canceled. Partial files were kept for "
                             "resume."));
                        return;
                    }

                    if (error != QNetworkReply::NoError) {
                        failDownload(
                          tr("Download failed for %1: %2")
                            .arg(item.fileName, errorText));
                        return;
                    }

                    const QFileInfo downloaded(partPath);
                    if (item.expectedSize > 0 &&
                        downloaded.size() != item.expectedSize) {
                        failDownload(
                          tr("Downloaded file size is incorrect for %1 "
                             "(%2 bytes, expected %3).")
                            .arg(item.fileName)
                            .arg(downloaded.size())
                            .arg(item.expectedSize));
                        return;
                    }

                    QFile::remove(finalPath);
                    if (!QFile::rename(partPath, finalPath)) {
                        failDownload(
                          tr("Could not finalize downloaded file %1.")
                            .arg(item.fileName));
                        return;
                    }

                    m_downloadCompleted += item.expectedSize;
                    ++m_downloadIndex;
                    emit downloadProgress(m_downloadCompleted,
                                          m_downloadTotal,
                                          item.fileName);
                    startNextDownload();
                });

        return;
    }

    m_downloadQueue.clear();
    m_downloadIndex = 0;
    emit downloadProgress(m_downloadTotal, m_downloadTotal, {});
    emit downloadFinished(true, tr("OCR model download completed."));
}

void OcrManager::cancelDownload()
{
    if (!m_downloadReply) {
        return;
    }
    m_downloadCanceled = true;
    m_downloadReply->abort();
}

void OcrManager::failDownload(const QString& message)
{
    if (m_downloadReply) {
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    m_downloadQueue.clear();
    m_downloadIndex = 0;
    emit downloadFinished(false, message);
}

bool OcrManager::removeModel(const QString& modelId, QString* error)
{
    const OcrModelInfo model = modelById(modelId);
    if (model.id.isEmpty()) {
        if (error) {
            *error = tr("Unknown OCR model.");
        }
        return false;
    }

    const QString dir = modelDirectory(model);
    if (!QDir(dir).exists()) {
        return true;
    }

    if (!QDir(dir).removeRecursively()) {
        if (error) {
            *error = tr("Could not remove model directory: %1").arg(dir);
        }
        return false;
    }
    return true;
}

void OcrManager::checkRemoteManifest(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    const QString urlText = ConfigHandler().ocrManifestUrl().trimmed();
    if (urlText.isEmpty()) {
        callback(
          false,
          tr("Remote model manifest URL is not configured. "
             "The built-in verified model list remains available."));
        return;
    }

    const QUrl url(urlText);
    if (!url.isValid() ||
        (url.scheme() != QStringLiteral("https") &&
         url.scheme() != QStringLiteral("http"))) {
        callback(false, tr("The remote model manifest URL is invalid."));
        return;
    }

    QPointer<QObject> guard(context);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network.get(request);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, guard, reply, callback]() {
                const QByteArray data = reply->readAll();
                if (reply->error() != QNetworkReply::NoError) {
                    const QString error = reply->errorString();
                    reply->deleteLater();
                    if (guard) {
                        callback(false, error);
                    }
                    return;
                }
                reply->deleteLater();

                QJsonParseError parseError;
                const QJsonDocument doc =
                  QJsonDocument::fromJson(data, &parseError);
                if (parseError.error != QJsonParseError::NoError ||
                    !doc.isObject()) {
                    if (guard) {
                        callback(false,
                                 tr("Remote model manifest is not valid JSON."));
                    }
                    return;
                }

                const QJsonObject root = doc.object();
                const QJsonArray models =
                  root.value(QStringLiteral("models")).toArray();
                bool hasSupportedModel = false;
                for (const auto& value : models) {
                    if (!value.isObject()) {
                        continue;
                    }
                    if (validModel(parseModel(value.toObject()))) {
                        hasSupportedModel = true;
                        break;
                    }
                }

                if (!hasSupportedModel) {
                    if (guard) {
                        callback(
                          false,
                          tr("Remote manifest contains no compatible verified "
                             "OCR models."));
                    }
                    return;
                }

                QFile file(manifestCachePath());
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                    file.write(data) != data.size()) {
                    if (guard) {
                        callback(
                          false,
                          tr("Could not save the remote model manifest."));
                    }
                    return;
                }
                file.close();

                emit manifestChanged();
                if (guard) {
                    callback(
                      true,
                      tr("Model list updated. Latest supported model: %1")
                        .arg(latestModelId()));
                }
            });
}
