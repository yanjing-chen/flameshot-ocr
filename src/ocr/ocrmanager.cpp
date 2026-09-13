// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot OCR Contributors

#include "ocrmanager.h"

#include "utils/confighandler.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QPointer>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>

namespace
{
constexpr qint64 PADDLE_16_MODEL_SIZE = 935769056;
constexpr qint64 PADDLE_16_MMPROJ_SIZE = 881770560;

// Verified experimental CUDA runtime for the first v2.4 Runtime Manager.
// This package was built with CUDA Toolkit 12.8.1 and llama.cpp 72797e891.
// The sm86 build has been verified on an RTX 3050 Laptop GPU.
constexpr auto CUDA_RUNTIME_VERSION = "12.8-r1";
constexpr auto CUDA_RUNTIME_DISPLAY_VERSION = "12.8-r1 (sm86)";
constexpr auto CUDA_RUNTIME_ARCHIVE =
  "Flameshot-OCR-CUDA-12.8-r1-sm86.tar.zst";
constexpr auto CUDA_RUNTIME_URL =
  "https://github.com/yanjing-chen/flameshot-ocr/releases/download/"
  "cuda-runtime-12.8-r1-sm86/"
  "Flameshot-OCR-CUDA-12.8-r1-sm86.tar.zst";
constexpr qint64 CUDA_RUNTIME_ARCHIVE_SIZE = 515204221;
constexpr auto CUDA_RUNTIME_ARCHIVE_SHA256 =
  "21d4febc94847568194457170f9563371c5e244513087b167164b1e874efda45";

constexpr auto CUDA_RUNTIME_MANIFEST_URL =
  "https://github.com/yanjing-chen/flameshot-ocr/releases/download/"
  "cuda-runtime-manifest/cuda-runtimes.json";

constexpr qint64 CUDA_WRAPPER_SIZE = 206;
constexpr qint64 CUDA_SERVER_SIZE = 73419736;
constexpr qint64 CUDA_CUDART_SIZE = 728800;
constexpr qint64 CUDA_CUBLAS_SIZE = 116388640;
constexpr qint64 CUDA_CUBLASLT_SIZE = 751771728;

bool nvidiaDriverPresent()
{
    return QFileInfo::exists(QStringLiteral("/dev/nvidiactl")) ||
           QFileInfo::exists(QStringLiteral("/dev/nvidia0"));
}

QString bundledZstdExecutable()
{
    // AppImage layout:
    //   AppDir/usr/bin/flameshot
    //   AppDir/usr/lib/flameshot-ocr/tools/zstd
    const QString packaged = QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("../lib/flameshot-ocr/tools/zstd")));

    if (QFileInfo(packaged).isExecutable()) {
        return packaged;
    }

    return QStandardPaths::findExecutable(QStringLiteral("zstd"));
}

QByteArray sha256ForFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray data = file.read(4 * 1024 * 1024);
        if (data.isEmpty() && file.error() != QFileDevice::NoError) {
            return {};
        }
        if (!data.isEmpty()) {
            hash.addData(data);
        }
    }

    return hash.result().toHex();
}

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
    const bool hasNvidiaDriver = nvidiaDriverPresent();

    // 1. CUDA on NVIDIA.
    if (hasNvidiaDriver) {
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

    if (hasNvidiaDriver) {
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

QString OcrManager::cudaRuntimeBaseDir() const
{
    return QStandardPaths::writableLocation(
             QStandardPaths::GenericDataLocation) +
           QStringLiteral("/flameshot-ocr/runtime/cuda");
}

QString OcrManager::cudaRuntimeCurrentDir() const
{
    return QDir(cudaRuntimeBaseDir()).filePath(QStringLiteral("current"));
}

bool OcrManager::nvidiaDriverAvailable() const
{
    return nvidiaDriverPresent();
}

bool OcrManager::cudaRuntimeInstalled() const
{
    const QString root = cudaRuntimeCurrentDir();
    const QFileInfo currentInfo(root);

    if (!currentInfo.isSymLink()) {
        return false;
    }

    const QString target =
      QDir::cleanPath(currentInfo.symLinkTarget());
    const QString version =
      QFileInfo(target).fileName();

    if (version.isEmpty()) {
        return false;
    }

    const QString wrapper =
      QDir(root).filePath(QStringLiteral("llama-server-cuda"));
    const QString server =
      QDir(root).filePath(QStringLiteral("llama-server-cuda.bin"));

    if (!QFileInfo(wrapper).isExecutable() ||
        !QFileInfo(server).isExecutable()) {
        return false;
    }

    CudaRuntimeInfo descriptor;

    if (m_cudaRuntimeInfo.isValid() &&
        m_cudaRuntimeInfo.version == version) {
        descriptor = m_cudaRuntimeInfo;
    } else {
        const CudaRuntimeInfo builtIn =
          builtInCudaRuntimeInfo();

        if (builtIn.version == version) {
            descriptor = builtIn;
        }
    }

    // For runtimes whose descriptor is known, validate every file against
    // the exact sizes recorded in the manifest/built-in descriptor.
    if (descriptor.isValid()) {
        for (const CudaRuntimeFileInfo& file :
             descriptor.requiredFiles) {
            const QString path =
              QDir(root).filePath(file.path);

            if (!fileMatchesSize(path, file.size)) {
                return false;
            }
        }

        return true;
    }

    // Older runtimes may remain on disk after future manifest updates.
    // Their original exact-size descriptor may no longer be loaded, so use a
    // conservative structural check rather than incorrectly marking a valid
    // previous version as absent.
    const QString cudart =
      QDir(root).filePath(QStringLiteral("lib/libcudart.so.12"));
    const QString cublas =
      QDir(root).filePath(QStringLiteral("lib/libcublas.so.12"));
    const QString cublasLt =
      QDir(root).filePath(QStringLiteral("lib/libcublasLt.so.12"));

    for (const QString& path :
         { wrapper, server, cudart, cublas, cublasLt }) {
        const QFileInfo info(path);

        if (!info.isFile() || info.size() <= 0) {
            return false;
        }
    }

    return true;
}

QString OcrManager::cudaRuntimeVersion() const
{
    const QFileInfo current(cudaRuntimeCurrentDir());

    if (current.isSymLink()) {
        const QString target = current.symLinkTarget();
        const QString name = QFileInfo(QDir::cleanPath(target)).fileName();
        if (!name.isEmpty()) {
            return name;
        }
    }

    if (cudaRuntimeInstalled()) {
        return QString::fromLatin1(CUDA_RUNTIME_DISPLAY_VERSION);
    }

    return {};
}

bool OcrManager::cudaRuntimeBusy() const
{
    return m_cudaDownloadReply != nullptr ||
           m_cudaExtractProcess != nullptr ||
           m_cudaSelfTestProcess != nullptr;
}

bool OcrManager::cudaRuntimeUpdateAvailable() const
{
    if (!cudaRuntimeInstalled() || !m_cudaRuntimeInfo.isValid()) {
        return false;
    }

    return cudaRuntimeVersion().trimmed() !=
           m_cudaRuntimeInfo.version.trimmed();
}

void OcrManager::checkCudaRuntimeUpdates(
  QObject* context,
  std::function<void(bool, const QString&)> callback)
{
    if (!context) {
        return;
    }

    QPointer<QObject> guard(context);

    QNetworkRequest request(
      QUrl(QString::fromLatin1(CUDA_RUNTIME_MANIFEST_URL)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_network.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, guard, callback]() {
        const auto error = reply->error();
        const QString errorText = reply->errorString();
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        if (!guard) {
            return;
        }

        if (error != QNetworkReply::NoError) {
            callback(
              false,
              tr("Could not check CUDA runtime updates: %1").arg(errorText));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc =
          QJsonDocument::fromJson(payload, &parseError);

        if (parseError.error != QJsonParseError::NoError ||
            !doc.isObject()) {
            callback(false, tr("The CUDA runtime manifest is invalid JSON."));
            return;
        }

        const QJsonObject root = doc.object();

        if (root.value(QStringLiteral("schema_version")).toInt(-1) != 1) {
            callback(
              false,
              tr("The CUDA runtime manifest uses an unsupported schema version."));
            return;
        }

        const QString latest =
          root.value(QStringLiteral("latest")).toString().trimmed();

        if (latest.isEmpty()) {
            callback(
              false,
              tr("The CUDA runtime manifest does not define a latest version."));
            return;
        }

        QJsonObject latestRuntime;
        const QJsonArray runtimes =
          root.value(QStringLiteral("runtimes")).toArray();

        for (const QJsonValue& value : runtimes) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject runtime = value.toObject();

            if (runtime.value(QStringLiteral("version")).toString() != latest) {
                continue;
            }

            if (runtime.value(QStringLiteral("platform")).toString() !=
                  QStringLiteral("linux-x86_64") ||
                runtime.value(QStringLiteral("backend")).toString() !=
                  QStringLiteral("cuda")) {
                continue;
            }

            latestRuntime = runtime;
            break;
        }

        if (latestRuntime.isEmpty()) {
            callback(
              false,
              tr("The latest CUDA runtime is not available for Linux x86_64."));
            return;
        }

        const QJsonObject archive =
          latestRuntime.value(QStringLiteral("archive")).toObject();

        CudaRuntimeInfo remoteInfo;
        remoteInfo.version = latest;

        remoteInfo.displayVersion =
          latestRuntime.value(QStringLiteral("display_version"))
            .toString()
            .trimmed();

        if (remoteInfo.displayVersion.isEmpty()) {
            remoteInfo.displayVersion = latest;
        }

        remoteInfo.archiveName =
          archive.value(QStringLiteral("name")).toString().trimmed();

        const QString archiveUrl =
          archive.value(QStringLiteral("url")).toString().trimmed();

        remoteInfo.url = QUrl(archiveUrl);
        remoteInfo.archiveSize =
          archive.value(QStringLiteral("size")).toVariant().toLongLong();

        const QString sha256 =
          archive.value(QStringLiteral("sha256"))
            .toString()
            .trimmed();

        const bool validSha256 =
          QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$"))
            .match(sha256)
            .hasMatch();

        if (validSha256) {
            remoteInfo.sha256 =
              sha256.toLatin1().toLower();
        }

        // archiveName must be a plain filename. It may not contain a path,
        // because downloads are stored below the managed .downloads folder.
        const bool validArchiveName =
          !remoteInfo.archiveName.isEmpty() &&
          QFileInfo(remoteInfo.archiveName).fileName() ==
            remoteInfo.archiveName &&
          remoteInfo.archiveName != QStringLiteral(".") &&
          remoteInfo.archiveName != QStringLiteral("..");

        const QJsonArray requiredFiles =
          latestRuntime.value(QStringLiteral("required_files")).toArray();

        bool requiredFilesValid = !requiredFiles.isEmpty();

        for (const QJsonValue& value : requiredFiles) {
            if (!value.isObject()) {
                requiredFilesValid = false;
                break;
            }

            const QJsonObject file = value.toObject();
            const QString rawPath =
              file.value(QStringLiteral("path")).toString().trimmed();
            const qint64 size =
              file.value(QStringLiteral("size")).toVariant().toLongLong();

            const QString cleanPath =
              QDir::cleanPath(rawPath);

            // required_files must always stay inside the extracted runtime
            // directory. Reject absolute paths and any ".." escape.
            const bool validPath =
              !rawPath.isEmpty() &&
              !QFileInfo(rawPath).isAbsolute() &&
              cleanPath != QStringLiteral("..") &&
              !cleanPath.startsWith(QStringLiteral("../")) &&
              cleanPath != QStringLiteral(".");

            if (!validPath || size <= 0) {
                requiredFilesValid = false;
                break;
            }

            bool duplicate = false;
            for (const CudaRuntimeFileInfo& existing :
                 remoteInfo.requiredFiles) {
                if (existing.path == cleanPath) {
                    duplicate = true;
                    break;
                }
            }

            if (duplicate) {
                requiredFilesValid = false;
                break;
            }

            remoteInfo.requiredFiles.append(
              { cleanPath, size });
        }

        if (!validArchiveName ||
            !remoteInfo.url.isValid() ||
            remoteInfo.url.scheme().compare(
              QStringLiteral("https"), Qt::CaseInsensitive) != 0 ||
            remoteInfo.archiveSize <= 0 ||
            !validSha256 ||
            !requiredFilesValid ||
            !remoteInfo.isValid()) {
            callback(
              false,
              tr("The latest CUDA runtime entry is incomplete or invalid."));
            return;
        }

        // From this point onward the installer uses the verified remote
        // descriptor rather than the compile-time fallback.
        m_cudaRuntimeInfo = remoteInfo;

        if (!cudaRuntimeInstalled()) {
            callback(
              true,
              tr("Latest available CUDA runtime: %1")
                .arg(remoteInfo.displayVersion));
            return;
        }

        const QString current = cudaRuntimeVersion().trimmed();

        if (current == remoteInfo.version) {
            callback(
              true,
              tr("CUDA runtime %1 is up to date.")
                .arg(remoteInfo.displayVersion));
            return;
        }

        callback(
          true,
          tr("CUDA runtime update available: %1 → %2")
            .arg(current.isEmpty() ? tr("unknown version") : current,
                 remoteInfo.displayVersion));
    });
}

OcrManager::CudaRuntimeInfo OcrManager::builtInCudaRuntimeInfo()
{
    CudaRuntimeInfo info;

    info.version =
      QString::fromLatin1(CUDA_RUNTIME_VERSION);
    info.displayVersion =
      QString::fromLatin1(CUDA_RUNTIME_DISPLAY_VERSION);
    info.archiveName =
      QString::fromLatin1(CUDA_RUNTIME_ARCHIVE);
    info.url =
      QUrl(QString::fromLatin1(CUDA_RUNTIME_URL));
    info.archiveSize =
      CUDA_RUNTIME_ARCHIVE_SIZE;
    info.sha256 =
      QByteArray(CUDA_RUNTIME_ARCHIVE_SHA256);

    info.requiredFiles = {
        { QStringLiteral("llama-server-cuda"),
          CUDA_WRAPPER_SIZE },
        { QStringLiteral("llama-server-cuda.bin"),
          CUDA_SERVER_SIZE },
        { QStringLiteral("lib/libcudart.so.12"),
          CUDA_CUDART_SIZE },
        { QStringLiteral("lib/libcublas.so.12"),
          CUDA_CUBLAS_SIZE },
        { QStringLiteral("lib/libcublasLt.so.12"),
          CUDA_CUBLASLT_SIZE },
    };

    return info;
}

void OcrManager::installCudaRuntime()
{
    if (!m_cudaRuntimeInfo.isValid()) {
        m_cudaRuntimeInfo = builtInCudaRuntimeInfo();
    }

    if (cudaRuntimeBusy()) {
        emit cudaRuntimeFinished(false,
                                 tr("A CUDA runtime installation is already running."));
        return;
    }

    if (!nvidiaDriverAvailable()) {
        emit cudaRuntimeFinished(
          false,
          tr("No active NVIDIA driver was detected. CUDA runtime installation "
             "is not required on this system."));
        return;
    }

    if (managedServerRunning()) {
        emit cudaRuntimeFinished(
          false,
          tr("Stop the managed OCR service before installing the CUDA runtime."));
        return;
    }

    if (cudaRuntimeInstalled() &&
        cudaRuntimeVersion().trimmed() ==
          m_cudaRuntimeInfo.version.trimmed()) {
        emit cudaRuntimeFinished(
          true,
          tr("CUDA runtime %1 is already installed.")
            .arg(m_cudaRuntimeInfo.displayVersion));
        return;
    }

    const QString base = cudaRuntimeBaseDir();
    const QString downloadDir =
      QDir(base).filePath(QStringLiteral(".downloads"));

    if (!QDir().mkpath(downloadDir)) {
        emit cudaRuntimeFinished(
          false, tr("Could not create the CUDA runtime download directory."));
        return;
    }

    m_cudaArchivePath =
      QDir(downloadDir).filePath(m_cudaRuntimeInfo.archiveName);
    const QString partPath = m_cudaArchivePath + QStringLiteral(".part");

    // Reuse a previously completed archive only after verifying both size
    // and SHA256.
    if (fileMatchesSize(m_cudaArchivePath,
                        m_cudaRuntimeInfo.archiveSize)) {
        const QByteArray hash = sha256ForFile(m_cudaArchivePath);
        if (hash == m_cudaRuntimeInfo.sha256) {
            emit cudaRuntimeProgress(m_cudaRuntimeInfo.archiveSize,
                                     m_cudaRuntimeInfo.archiveSize);
            startCudaRuntimeExtraction(m_cudaArchivePath);
            return;
        }
        QFile::remove(m_cudaArchivePath);
    } else if (QFileInfo::exists(m_cudaArchivePath)) {
        QFile::remove(m_cudaArchivePath);
    }

    qint64 offset =
      QFileInfo(partPath).isFile() ? QFileInfo(partPath).size() : 0;

    if (offset < 0 || offset >= m_cudaRuntimeInfo.archiveSize) {
        QFile::remove(partPath);
        offset = 0;
    }

    m_cudaDownloadOffset = offset;
    m_cudaCanceled = false;

    m_cudaDownloadFile = new QFile(partPath, this);
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    if (offset > 0) {
        mode |= QIODevice::Append;
    } else {
        mode |= QIODevice::Truncate;
    }

    if (!m_cudaDownloadFile->open(mode)) {
        failCudaRuntime(tr("Could not write CUDA runtime download file."));
        return;
    }

    QNetworkRequest request(m_cudaRuntimeInfo.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (offset > 0) {
        request.setRawHeader(
          "Range",
          QByteArray("bytes=") + QByteArray::number(offset) + "-");
    }

    m_cudaDownloadReply = m_network.get(request);

    connect(m_cudaDownloadReply,
            &QNetworkReply::metaDataChanged,
            this,
            [this]() {
                if (!m_cudaDownloadReply || !m_cudaDownloadFile ||
                    m_cudaDownloadOffset <= 0) {
                    return;
                }

                const int status =
                  m_cudaDownloadReply
                    ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();

                if (status == 200) {
                    // GitHub/CDN ignored Range. Restart safely from byte zero.
                    m_cudaDownloadFile->resize(0);
                    m_cudaDownloadOffset = 0;
                }
            });

    connect(m_cudaDownloadReply,
            &QNetworkReply::readyRead,
            this,
            [this]() {
                if (m_cudaDownloadReply && m_cudaDownloadFile) {
                    m_cudaDownloadFile->write(m_cudaDownloadReply->readAll());
                }
            });

    connect(m_cudaDownloadReply,
            &QNetworkReply::downloadProgress,
            this,
            [this](qint64 received, qint64) {
                const qint64 done =
                  qMin(m_cudaDownloadOffset + received,
                       m_cudaRuntimeInfo.archiveSize);
                emit cudaRuntimeProgress(done,
                                         m_cudaRuntimeInfo.archiveSize);
            });

    connect(m_cudaDownloadReply,
            &QNetworkReply::finished,
            this,
            [this, partPath]() {
                if (m_cudaDownloadFile) {
                    if (m_cudaDownloadReply) {
                        m_cudaDownloadFile->write(
                          m_cudaDownloadReply->readAll());
                    }
                    m_cudaDownloadFile->flush();
                    m_cudaDownloadFile->close();
                    m_cudaDownloadFile->deleteLater();
                    m_cudaDownloadFile = nullptr;
                }

                const bool canceled = m_cudaCanceled;
                const auto error =
                  m_cudaDownloadReply
                    ? m_cudaDownloadReply->error()
                    : QNetworkReply::UnknownNetworkError;
                const QString errorText =
                  m_cudaDownloadReply
                    ? m_cudaDownloadReply->errorString()
                    : tr("Unknown network error");

                if (m_cudaDownloadReply) {
                    m_cudaDownloadReply->deleteLater();
                    m_cudaDownloadReply = nullptr;
                }

                if (canceled) {
                    failCudaRuntime(
                      tr("CUDA runtime download canceled. "
                         "The partial file was kept for resume."));
                    return;
                }

                if (error != QNetworkReply::NoError) {
                    failCudaRuntime(
                      tr("CUDA runtime download failed: %1").arg(errorText));
                    return;
                }

                const QFileInfo downloaded(partPath);
                if (downloaded.size() !=
                    m_cudaRuntimeInfo.archiveSize) {
                    failCudaRuntime(
                      tr("CUDA runtime download size is incorrect "
                         "(%1 bytes, expected %2).")
                        .arg(downloaded.size())
                        .arg(m_cudaRuntimeInfo.archiveSize));
                    return;
                }

                QFile::remove(m_cudaArchivePath);
                if (!QFile::rename(partPath, m_cudaArchivePath)) {
                    failCudaRuntime(
                      tr("Could not finalize the CUDA runtime download."));
                    return;
                }

                emit cudaRuntimeProgress(m_cudaRuntimeInfo.archiveSize,
                                         m_cudaRuntimeInfo.archiveSize);

                const QByteArray hash = sha256ForFile(m_cudaArchivePath);
                if (hash != m_cudaRuntimeInfo.sha256) {
                    QFile::remove(m_cudaArchivePath);
                    failCudaRuntime(
                      tr("CUDA runtime SHA256 verification failed. "
                         "The downloaded archive was removed."));
                    return;
                }

                startCudaRuntimeExtraction(m_cudaArchivePath);
            });
}

void OcrManager::startCudaRuntimeExtraction(const QString& archivePath)
{
    const QString zstd = bundledZstdExecutable();
    if (zstd.isEmpty()) {
        failCudaRuntime(
          tr("zstd was not found. CUDA runtime extraction cannot continue."));
        return;
    }

    const QString tar =
      QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        failCudaRuntime(
          tr("tar was not found. CUDA runtime extraction cannot continue."));
        return;
    }

    const QString base = cudaRuntimeBaseDir();
    if (!QDir().mkpath(base)) {
        failCudaRuntime(tr("Could not create the CUDA runtime directory."));
        return;
    }

    const QString stagingName =
      QStringLiteral(".staging-") + m_cudaRuntimeInfo.version;
    m_cudaStagingDir = QDir(base).filePath(stagingName);

    if (QDir(m_cudaStagingDir).exists() &&
        !QDir(m_cudaStagingDir).removeRecursively()) {
        failCudaRuntime(
          tr("Could not clear the previous CUDA runtime staging directory."));
        return;
    }

    if (!QDir().mkpath(m_cudaStagingDir)) {
        failCudaRuntime(
          tr("Could not create the CUDA runtime staging directory."));
        return;
    }

    m_cudaCanceled = false;
    m_cudaExtractProcess = new QProcess(this);
    m_cudaExtractProcess->setProcessChannelMode(QProcess::MergedChannels);

    QStringList args;
    args << QStringLiteral("--use-compress-program=%1").arg(zstd)
         << QStringLiteral("--no-same-owner")
         << QStringLiteral("-xf")
         << archivePath
         << QStringLiteral("-C")
         << m_cudaStagingDir;

    connect(
      m_cudaExtractProcess,
      qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
      this,
      [this, archivePath](int exitCode, QProcess::ExitStatus exitStatus) {
          const QString processOutput =
            m_cudaExtractProcess
              ? QString::fromLocal8Bit(m_cudaExtractProcess->readAll())
              : QString();

          if (m_cudaExtractProcess) {
              m_cudaExtractProcess->deleteLater();
              m_cudaExtractProcess = nullptr;
          }

          if (m_cudaCanceled) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(tr("CUDA runtime installation canceled."));
              return;
          }

          if (exitStatus != QProcess::NormalExit || exitCode != 0) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("Could not extract the CUDA runtime.\n%1")
                  .arg(processOutput.trimmed()));
              return;
          }

          const QString wrapper =
            QDir(m_cudaStagingDir)
              .filePath(QStringLiteral("llama-server-cuda"));
          const QString server =
            QDir(m_cudaStagingDir)
              .filePath(QStringLiteral("llama-server-cuda.bin"));

          bool filesValid =
            QFileInfo(wrapper).isExecutable() &&
            QFileInfo(server).isExecutable();

          for (const CudaRuntimeFileInfo& file :
               m_cudaRuntimeInfo.requiredFiles) {
              const QString path =
                QDir(m_cudaStagingDir).filePath(file.path);

              if (!fileMatchesSize(path, file.size)) {
                  filesValid = false;
                  break;
              }
          }

          if (!filesValid) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("The extracted CUDA runtime is incomplete or invalid."));
              return;
          }

          // Real runtime sanity check before current is switched.
          QProcess probe;
          probe.setProcessChannelMode(QProcess::MergedChannels);
          probe.start(wrapper, { QStringLiteral("--list-devices") });

          if (!probe.waitForStarted(5000)) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("The CUDA runtime could not be started for verification."));
              return;
          }

          if (!probe.waitForFinished(15000)) {
              probe.kill();
              probe.waitForFinished(2000);
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("CUDA runtime verification timed out."));
              return;
          }

          const QString probeOutput =
            QString::fromLocal8Bit(probe.readAll());

          if (probe.exitStatus() != QProcess::NormalExit ||
              probe.exitCode() != 0 ||
              !probeOutput.contains(QStringLiteral("CUDA"),
                                    Qt::CaseInsensitive)) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("CUDA runtime verification failed.\n%1")
                  .arg(probeOutput.trimmed()));
              return;
          }

          const QRegularExpression cudaDevicePattern(
            QStringLiteral("(?m)^\\s*(CUDA\\d+)\\s*:"));
          const QRegularExpressionMatch cudaDeviceMatch =
            cudaDevicePattern.match(probeOutput);

          if (!cudaDeviceMatch.hasMatch()) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("CUDA runtime verification did not report a usable CUDA "
                   "device.\n%1")
                  .arg(probeOutput.trimmed()));
              return;
          }

          const QString cudaDevice =
            cudaDeviceMatch.captured(1);

          const QString base = cudaRuntimeBaseDir();
          const QString versionName =
            m_cudaRuntimeInfo.version;
          const QString finalDir =
            QDir(base).filePath(versionName);

          // A stale/incomplete directory with this version can be safely
          // replaced because current has not been switched yet.
          if (QDir(finalDir).exists() &&
              !QDir(finalDir).removeRecursively()) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("Could not replace the existing CUDA runtime directory."));
              return;
          }

          QDir baseDir(base);
          const QString stagingName =
            QFileInfo(m_cudaStagingDir).fileName();

          if (!baseDir.rename(stagingName, versionName)) {
              QDir(m_cudaStagingDir).removeRecursively();
              failCudaRuntime(
                tr("Could not finalize the CUDA runtime directory."));
              return;
          }

          const QString current =
            QDir(base).filePath(QStringLiteral("current"));
          const QString currentNew =
            QDir(base).filePath(QStringLiteral(".current-new"));
          const QString previous =
            QDir(base).filePath(QStringLiteral("previous"));
          const QString previousNew =
            QDir(base).filePath(QStringLiteral(".previous-new"));

          const QFileInfo currentInfo(current);
          if (currentInfo.exists() && !currentInfo.isSymLink()) {
              failCudaRuntime(
                tr("The CUDA runtime 'current' path is not a symbolic link."));
              return;
          }

          QString oldVersionName;

          if (currentInfo.isSymLink()) {
              oldVersionName =
                QFileInfo(QDir::cleanPath(currentInfo.symLinkTarget()))
                  .fileName();

              if (oldVersionName.isEmpty()) {
                  failCudaRuntime(
                    tr("Could not determine the currently active CUDA runtime."));
                  return;
              }
          }

          // Before activating an upgrade, atomically remember the old runtime
          // as "previous". First installations have no old current symlink and
          // therefore do not create a previous link.
          if (!oldVersionName.isEmpty() &&
              oldVersionName != versionName) {
              const QFileInfo previousInfo(previous);

              if (previousInfo.exists() && !previousInfo.isSymLink()) {
                  failCudaRuntime(
                    tr("The CUDA runtime 'previous' path is not a symbolic link."));
                  return;
              }

              QFile::remove(previousNew);

              const QByteArray oldTargetNative =
                QFile::encodeName(oldVersionName);
              const QByteArray previousNewNative =
                QFile::encodeName(previousNew);
              const QByteArray previousNative =
                QFile::encodeName(previous);

              if (::symlink(oldTargetNative.constData(),
                            previousNewNative.constData()) != 0) {
                  failCudaRuntime(
                    tr("Could not create the CUDA runtime rollback link."));
                  return;
              }

              if (::rename(previousNewNative.constData(),
                           previousNative.constData()) != 0) {
                  QFile::remove(previousNew);
                  failCudaRuntime(
                    tr("Could not record the previous CUDA runtime."));
                  return;
              }
          }

          QFile::remove(currentNew);

          const QByteArray targetNative =
            QFile::encodeName(versionName);
          const QByteArray currentNewNative =
            QFile::encodeName(currentNew);
          const QByteArray currentNative =
            QFile::encodeName(current);

          if (::symlink(targetNative.constData(),
                        currentNewNative.constData()) != 0) {
              failCudaRuntime(
                tr("Could not create the CUDA runtime version link."));
              return;
          }

          // POSIX rename replaces the old symlink atomically. The old runtime
          // directory remains on disk and "previous" points to it after an
          // upgrade.
          if (::rename(currentNewNative.constData(),
                       currentNative.constData()) != 0) {
              QFile::remove(currentNew);
              failCudaRuntime(
                tr("Could not activate the new CUDA runtime."));
              return;
          }

          // current now points at the new runtime. Perform an isolated
          // post-install health test against this exact runtime before the
          // installation is considered successful.
          startCudaRuntimeSelfTest(
            oldVersionName, cudaDevice, archivePath);
      });

    m_cudaExtractProcess->start(tar, args);

    if (!m_cudaExtractProcess->waitForStarted(5000)) {
        m_cudaExtractProcess->deleteLater();
        m_cudaExtractProcess = nullptr;
        QDir(m_cudaStagingDir).removeRecursively();
        failCudaRuntime(tr("Could not start CUDA runtime extraction."));
    }
}

void OcrManager::startCudaRuntimeSelfTest(
  const QString& previousVersion,
  const QString& cudaDevice,
  const QString& archivePath)
{
    if (m_cudaCanceled) {
        failCudaRuntimeSelfTest(
          tr("CUDA runtime installation was canceled."),
          previousVersion);
        return;
    }

    const OcrModelInfo model = activeModel();

    // The archive has already passed SHA256, file-size and --list-devices
    // verification. If no OCR model is installed yet, there is nothing that
    // can be loaded for a full HTTP health test.
    if (!modelInstalled(model)) {
        finishCudaRuntimeInstall(archivePath);
        return;
    }

    const QString executable =
      QDir(cudaRuntimeCurrentDir())
        .filePath(QStringLiteral("llama-server-cuda"));

    if (!QFileInfo(executable).isExecutable()) {
        failCudaRuntimeSelfTest(
          tr("The newly activated CUDA runtime executable is unavailable."),
          previousVersion);
        return;
    }

    QTcpServer portProbe;

    if (!portProbe.listen(QHostAddress::LocalHost, 0)) {
        failCudaRuntimeSelfTest(
          tr("Could not allocate a temporary port for CUDA runtime verification."),
          previousVersion);
        return;
    }

    const quint16 port = portProbe.serverPort();
    portProbe.close();

    QStringList args;
    args << QStringLiteral("-m") << modelPath(model)
         << QStringLiteral("--mmproj") << mmprojPath(model)
         << QStringLiteral("--host") << QStringLiteral("127.0.0.1")
         << QStringLiteral("--port") << QString::number(port)
         << QStringLiteral("--temp") << QStringLiteral("0")
         << QStringLiteral("--parallel") << QStringLiteral("1")
         << QStringLiteral("-c") << QStringLiteral("8192")
         << QStringLiteral("--device") << cudaDevice
         << QStringLiteral("-ngl") << QStringLiteral("99");

    m_cudaSelfTestProcess = new QProcess(this);
    m_cudaSelfTestProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_cudaSelfTestProcess->start(executable, args);

    if (!m_cudaSelfTestProcess->waitForStarted(5000)) {
        const QString error =
          m_cudaSelfTestProcess->errorString();
        stopCudaRuntimeSelfTestProcess();
        failCudaRuntimeSelfTest(
          tr("The CUDA runtime self-test could not be started: %1")
            .arg(error),
          previousVersion);
        return;
    }

    QUrl healthUrl;
    healthUrl.setScheme(QStringLiteral("http"));
    healthUrl.setHost(QStringLiteral("127.0.0.1"));
    healthUrl.setPort(port);
    healthUrl.setPath(QStringLiteral("/health"));

    // Match the normal OCR startup allowance: about 30 seconds for model load.
    QTimer::singleShot(
      250,
      this,
      [this, healthUrl, previousVersion, archivePath]() {
          waitForCudaRuntimeSelfTest(
            healthUrl, 60, previousVersion, archivePath);
      });
}

void OcrManager::waitForCudaRuntimeSelfTest(
  const QUrl& healthUrl,
  int attemptsLeft,
  const QString& previousVersion,
  const QString& archivePath)
{
    if (m_cudaCanceled) {
        failCudaRuntimeSelfTest(
          tr("CUDA runtime installation was canceled."),
          previousVersion);
        return;
    }

    if (!m_cudaSelfTestProcess) {
        failCudaRuntimeSelfTest(
          tr("The CUDA runtime self-test process disappeared unexpectedly."),
          previousVersion);
        return;
    }

    if (m_cudaSelfTestProcess->state() == QProcess::NotRunning) {
        const QString output =
          QString::fromLocal8Bit(m_cudaSelfTestProcess->readAll())
            .trimmed();

        failCudaRuntimeSelfTest(
          tr("The CUDA runtime self-test exited before becoming healthy.\n%1")
            .arg(output),
          previousVersion);
        return;
    }

    QNetworkRequest request(healthUrl);
    QNetworkReply* reply = m_network.get(request);

    connect(
      reply,
      &QNetworkReply::finished,
      this,
      [this,
       reply,
       healthUrl,
       attemptsLeft,
       previousVersion,
       archivePath]() {
          const bool ok =
            reply->error() == QNetworkReply::NoError;
          const QString networkError =
            reply->errorString();
          reply->deleteLater();

          if (m_cudaCanceled) {
              failCudaRuntimeSelfTest(
                tr("CUDA runtime installation was canceled."),
                previousVersion);
              return;
          }

          if (ok) {
              stopCudaRuntimeSelfTestProcess();
              finishCudaRuntimeInstall(archivePath);
              return;
          }

          if (!m_cudaSelfTestProcess ||
              m_cudaSelfTestProcess->state() ==
                QProcess::NotRunning) {
              const QString output =
                m_cudaSelfTestProcess
                  ? QString::fromLocal8Bit(
                      m_cudaSelfTestProcess->readAll()).trimmed()
                  : QString();

              failCudaRuntimeSelfTest(
                tr("The CUDA runtime self-test exited before becoming healthy.\n%1")
                  .arg(output),
                previousVersion);
              return;
          }

          if (attemptsLeft <= 0) {
              const QString output =
                QString::fromLocal8Bit(
                  m_cudaSelfTestProcess->readAll()).trimmed();

              QString reason =
                tr("The CUDA runtime self-test did not become healthy: %1")
                  .arg(networkError);

              if (!output.isEmpty()) {
                  reason += QStringLiteral("\n") + output;
              }

              failCudaRuntimeSelfTest(
                reason, previousVersion);
              return;
          }

          QTimer::singleShot(
            500,
            this,
            [this,
             healthUrl,
             attemptsLeft,
             previousVersion,
             archivePath]() {
                waitForCudaRuntimeSelfTest(
                  healthUrl,
                  attemptsLeft - 1,
                  previousVersion,
                  archivePath);
            });
      });
}

void OcrManager::stopCudaRuntimeSelfTestProcess()
{
    if (!m_cudaSelfTestProcess) {
        return;
    }

    if (m_cudaSelfTestProcess->state() != QProcess::NotRunning) {
        m_cudaSelfTestProcess->terminate();

        if (!m_cudaSelfTestProcess->waitForFinished(2000)) {
            m_cudaSelfTestProcess->kill();
            m_cudaSelfTestProcess->waitForFinished(2000);
        }
    }

    m_cudaSelfTestProcess->deleteLater();
    m_cudaSelfTestProcess = nullptr;
}

bool OcrManager::rollbackCudaRuntime(
  const QString& previousVersion,
  QString* error)
{
    const QString base = cudaRuntimeBaseDir();
    const QString current =
      QDir(base).filePath(QStringLiteral("current"));

    const QFileInfo currentInfo(current);

    if ((currentInfo.exists() || currentInfo.isSymLink()) &&
        !currentInfo.isSymLink()) {
        if (error) {
            *error =
              tr("The CUDA runtime 'current' path is not a symbolic link.");
        }
        return false;
    }

    // A failed first installation has no previous runtime. Deactivate the
    // failed runtime by removing current.
    if (previousVersion.isEmpty()) {
        if (currentInfo.isSymLink() && !QFile::remove(current)) {
            if (error) {
                *error =
                  tr("Could not deactivate the failed CUDA runtime.");
            }
            return false;
        }
        return true;
    }

    const QString previous =
      QDir(base).filePath(QStringLiteral("previous"));
    const QFileInfo previousInfo(previous);

    if (!previousInfo.isSymLink()) {
        if (error) {
            *error =
              tr("The CUDA runtime rollback link is unavailable.");
        }
        return false;
    }

    const QString rollbackVersion =
      QFileInfo(QDir::cleanPath(previousInfo.symLinkTarget()))
        .fileName();

    if (rollbackVersion.isEmpty() ||
        rollbackVersion != previousVersion ||
        !QDir(QDir(base).filePath(rollbackVersion)).exists()) {
        if (error) {
            *error =
              tr("The previous CUDA runtime is unavailable or invalid.");
        }
        return false;
    }

    const QString currentRollback =
      QDir(base).filePath(QStringLiteral(".current-rollback"));

    QFile::remove(currentRollback);

    const QByteArray targetNative =
      QFile::encodeName(rollbackVersion);
    const QByteArray rollbackNative =
      QFile::encodeName(currentRollback);
    const QByteArray currentNative =
      QFile::encodeName(current);

    if (::symlink(targetNative.constData(),
                  rollbackNative.constData()) != 0) {
        if (error) {
            *error =
              tr("Could not create the CUDA runtime rollback activation link.");
        }
        return false;
    }

    if (::rename(rollbackNative.constData(),
                 currentNative.constData()) != 0) {
        QFile::remove(currentRollback);
        if (error) {
            *error =
              tr("Could not reactivate the previous CUDA runtime.");
        }
        return false;
    }

    return true;
}

void OcrManager::finishCudaRuntimeInstall(
  const QString& archivePath)
{
    stopCudaRuntimeSelfTestProcess();

    QFile::remove(archivePath);
    m_cudaArchivePath.clear();
    m_cudaStagingDir.clear();

    emit cudaRuntimeChanged();
    emit cudaRuntimeFinished(
      true,
      tr("CUDA runtime %1 was installed and verified successfully.")
        .arg(m_cudaRuntimeInfo.displayVersion));
}

void OcrManager::failCudaRuntimeSelfTest(
  const QString& reason,
  const QString& previousVersion)
{
    stopCudaRuntimeSelfTestProcess();

    QString rollbackError;
    const bool rolledBack =
      rollbackCudaRuntime(previousVersion, &rollbackError);

    m_cudaStagingDir.clear();

    emit cudaRuntimeChanged();

    if (!rolledBack) {
        failCudaRuntime(
          tr("CUDA runtime post-install verification failed: %1\n"
             "Automatic rollback also failed: %2")
            .arg(reason, rollbackError));
        return;
    }

    if (previousVersion.isEmpty()) {
        failCudaRuntime(
          tr("CUDA runtime post-install verification failed: %1\n"
             "The new runtime was deactivated.")
            .arg(reason));
        return;
    }

    failCudaRuntime(
      tr("CUDA runtime post-install verification failed: %1\n"
         "Rolled back to %2.")
        .arg(reason, previousVersion));
}

void OcrManager::cancelCudaRuntimeInstall()
{
    m_cudaCanceled = true;

    if (m_cudaDownloadReply) {
        m_cudaDownloadReply->abort();
        return;
    }

    if (m_cudaExtractProcess) {
        m_cudaExtractProcess->kill();
        return;
    }

    if (m_cudaSelfTestProcess) {
        // The health-check loop will observe m_cudaCanceled and perform the
        // same rollback path as any other post-install verification failure.
        m_cudaSelfTestProcess->kill();
    }
}

void OcrManager::failCudaRuntime(const QString& message)
{
    if (m_cudaDownloadReply) {
        m_cudaDownloadReply->deleteLater();
        m_cudaDownloadReply = nullptr;
    }

    if (m_cudaDownloadFile) {
        m_cudaDownloadFile->close();
        m_cudaDownloadFile->deleteLater();
        m_cudaDownloadFile = nullptr;
    }

    emit cudaRuntimeFinished(false, message);
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
