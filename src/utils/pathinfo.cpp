// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "pathinfo.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>

const QString PathInfo::whiteIconPath()
{
    return QStringLiteral(":/img/material/white/");
}

const QString PathInfo::blackIconPath()
{
    return QStringLiteral(":/img/material/black/");
}

QStringList PathInfo::translationsPaths()
{
    const QString binaryPath =
      QFileInfo(qApp->applicationDirPath()).absoluteFilePath();
    const QString trPath =
      QDir::toNativeSeparators(binaryPath + QStringLiteral("/translations"));

#if defined(Q_OS_UNIX)
    // AppImage / normal install layout:
    //   <prefix>/bin/flameshot
    //   <prefix>/share/flameshot/translations/*.qm
    //
    // Using a path relative to the running executable is essential for
    // AppImage, whose mount point changes on every launch.
    const QString bundledSharePath = QDir::cleanPath(
      QDir(binaryPath).filePath(
        QStringLiteral("../share/flameshot/translations")));

    return QStringList()
           << bundledSharePath
           << QStringLiteral(APP_PREFIX) + "/share/flameshot/translations"
           << trPath
           << QStringLiteral("/usr/share/flameshot/translations")
           << QStringLiteral("/usr/local/share/flameshot/translations");
#endif
    return QStringList() << trPath;
}
