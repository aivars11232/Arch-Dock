// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Arch Dock is built against one Qt and may run with a newer one after a
// system update. It keeps working then, except that the bars use one window
// part of Qt's that can change with Qt's minor version. A run with another
// minor version is therefore said once in the log, in plain words.

#include <QString>
#include <QVersionNumber>
#include <QtGlobal>

namespace ArchDock
{
// The log line for a run with another Qt minor version than the build's, or
// an empty string when the major and minor versions match.
inline QString qtBuildMismatch(const QVersionNumber &built, const QVersionNumber &running)
{
    if (built.majorVersion() == running.majorVersion() && built.minorVersion() == running.minorVersion())
        return {};
    return QStringLiteral("Arch Dock was built for Qt %1.%2 and runs with Qt %3.%4. It keeps working, but "
                          "its bars may stay shown instead of hiding until a rebuilt Arch Dock is installed.")
        .arg(built.majorVersion())
        .arg(built.minorVersion())
        .arg(running.majorVersion())
        .arg(running.minorVersion());
}

// The same for this program: the Qt it was compiled with and the Qt it runs with.
inline QString qtBuildMismatch()
{
    return qtBuildMismatch(QVersionNumber(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH),
                           QVersionNumber::fromString(QLatin1StringView(qVersion())));
}
} // namespace ArchDock
