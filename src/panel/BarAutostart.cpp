#include "BarAutostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace ArchDock::BarAutostart
{
namespace
{
// A desktop entry's Exec value: the program quoted for the command line,
// then escaped as a string value, as the installed entries are written.
QString execValue(const QString &program)
{
    QString quoted = program;
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    quoted.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    quoted.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    quoted.replace(QLatin1Char('%'), QStringLiteral("%%"));
    quoted.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}
}

QString entryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/org.archdock.ArchDock.Bars.autostart.desktop");
}

QString entryText(const QString &program)
{
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=Arch Dock\n"
                          "Comment=Starts Arch Dock at login, for its bars\n"
                          "Exec=%1\n"
                          "Icon=org.archdock.ArchDock\n"
                          "Terminal=false\n"
                          "NoDisplay=true\n"
                          "X-KDE-autostart-phase=2\n")
        .arg(execValue(program));
}

bool sync(bool wanted, const QString &path, const QString &program, QString *error)
{
    if (!wanted)
    {
        if (!QFileInfo::exists(path) || QFile::remove(path))
            return true;
        if (error)
            *error = QStringLiteral("autostart-entry-not-removed");
        return false;
    }
    const QString text = entryText(program);
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && QString::fromUtf8(current.readAll()) == text)
        return true;
    current.close();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        if (error)
            *error = QStringLiteral("autostart-folder-not-made");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit())
    {
        if (error)
            *error = QStringLiteral("autostart-entry-not-written");
        return false;
    }
    return true;
}
}
