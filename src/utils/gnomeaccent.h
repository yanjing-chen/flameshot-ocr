// SPDX-License-Identifier: GPL-3.0-or-later
// GNOME desktop accent integration for Qt Widgets. No libadwaita dependency.
#pragma once

#include <QColor>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QDebug>
#include <QObject>
#include <QPalette>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

namespace GnomeAccent {
// Avoid accidentally using the AppImage-bundled gsettings or GLib schema paths.
inline QString hostGsettings()
{
    return QFileInfo::exists(QStringLiteral("/usr/bin/gsettings"))
      ? QStringLiteral("/usr/bin/gsettings") : QStringLiteral("gsettings");
}
inline QProcessEnvironment hostGsettingsEnv()
{
    auto env = QProcessEnvironment::systemEnvironment();
    for (const char* key : { "GSETTINGS_SCHEMA_DIR", "GIO_MODULE_DIR",
                             "GI_TYPELIB_PATH", "LD_LIBRARY_PATH",
                             "LD_PRELOAD", "GDK_PIXBUF_MODULEDIR" }) {
        env.remove(QString::fromLatin1(key));
    }
    return env;
}


inline QColor blend(const QColor& accent, const QColor& surface, int percent)
{
    const int n = qBound(0, percent, 100);
    auto mix = [n](int a, int b) { return (a*n + b*(100-n) + 50)/100; };
    return QColor(mix(accent.red(), surface.red()),
                  mix(accent.green(), surface.green()),
                  mix(accent.blue(), surface.blue()));
}

inline QColor current(const QPalette& palette)
{
    // GNOME 47+ exposes this enum through GSettings. Fall back cleanly for
    // missing gsettings, other desktops and unknown future accent names.
    QProcess process;
    process.setProcessEnvironment(hostGsettingsEnv());
    process.start(hostGsettings(),
                  {QStringLiteral("get"),
                   QStringLiteral("org.gnome.desktop.interface"),
                   QStringLiteral("accent-color")});
    if (!process.waitForStarted(300) || !process.waitForFinished(900) ||
        process.exitCode() != 0) {
        qWarning() << "GNOME accent: host gsettings query failed; using a fallback";
        if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(
              QStringLiteral("GNOME"), Qt::CaseInsensitive))
            return QColor(QStringLiteral("#3584e4"));
        return palette.color(QPalette::Highlight);
    }
    QString name = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    name.remove(QLatin1Char('\''));
    if (name == QLatin1String("blue")) return QColor(QStringLiteral("#3584e4"));
    if (name == QLatin1String("teal")) return QColor(QStringLiteral("#2190a4"));
    if (name == QLatin1String("green")) return QColor(QStringLiteral("#3a944a"));
    if (name == QLatin1String("yellow")) return QColor(QStringLiteral("#c88800"));
    if (name == QLatin1String("orange")) return QColor(QStringLiteral("#ed5b00"));
    if (name == QLatin1String("red")) return QColor(QStringLiteral("#e62d42"));
    if (name == QLatin1String("pink")) return QColor(QStringLiteral("#d56199"));
    if (name == QLatin1String("purple")) return QColor(QStringLiteral("#9141ac"));
    if (name == QLatin1String("slate")) return QColor(QStringLiteral("#6f8396"));
    return palette.color(QPalette::Highlight);
}

class Monitor : public QProcess
{
public:
    explicit Monitor(QObject* parent) : QProcess(parent) {}
    ~Monitor() override
    {
        if (state() != QProcess::NotRunning) {
            kill();
            waitForFinished(250);
        }
    }
};

inline void watch(QObject* owner, const std::function<void()>& update)
{
    auto* monitor = new Monitor(owner);
    QObject::connect(monitor, &QProcess::readyReadStandardOutput, owner,
                     [monitor, update]() {
        if (!monitor->readAllStandardOutput().trimmed().isEmpty()) update();
    });
    monitor->setProcessEnvironment(hostGsettingsEnv());
    monitor->start(hostGsettings(),
                   {QStringLiteral("monitor"),
                    QStringLiteral("org.gnome.desktop.interface"),
                    QStringLiteral("accent-color")});
}

} // namespace GnomeAccent
