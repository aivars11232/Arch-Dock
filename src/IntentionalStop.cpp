#include "IntentionalStop.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <unistd.h>

namespace ArchDock::IntentionalStop
{
namespace
{
constexpr qint64 MaximumBytes = 4096;
const QString Format = QStringLiteral("org.archdock.intentional-stop");

void error(QString *target, const QString &value)
{
    if (target) *target = value;
}

bool ownedByUser(const QFileInfo &info)
{
    return info.ownerId() == static_cast<uint>(::getuid());
}

// Anything at the state path that is not this user's small regular file is
// not a request this code wrote. Removing it removes a link, never its target.
bool safeStateFile(const QFileInfo &info)
{
    return !info.isSymLink() && info.isFile() && ownedByUser(info) &&
        info.size() <= MaximumBytes;
}
}

QString statePath()
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    const QFileInfo directory(runtime);
    if (runtime.isEmpty() || !directory.isAbsolute() || directory.isSymLink() ||
        !directory.isDir() || !ownedByUser(directory))
        return {};
    return QDir(runtime).filePath(QStringLiteral("arch-dock-intentional-stop.json"));
}

QString sessionIdentity()
{
    QFile bootId(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    QString boot;
    if (bootId.open(QIODevice::ReadOnly)) boot = QString::fromLatin1(bootId.read(64)).trimmed();
    if (boot.isEmpty()) boot = QStringLiteral("unknown-boot");
    return boot + QLatin1Char('/') + qEnvironmentVariable("XDG_SESSION_ID");
}

bool active()
{
    const QString path = statePath();
    if (path.isEmpty()) return false;
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) return false;
    QJsonObject object;
    if (safeStateFile(info))
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            object = QJsonDocument::fromJson(file.read(MaximumBytes)).object();
    }
    if (object.value(QStringLiteral("format")).toString() == Format &&
        object.value(QStringLiteral("session")).toString() == sessionIdentity())
        return true;
    // A request from an earlier login, or something this code did not write.
    QFile::remove(path);
    return false;
}

bool record(const QString &reason, QString *errorText)
{
    error(errorText, QString{});
    const QString path = statePath();
    if (path.isEmpty())
    {
        error(errorText, QStringLiteral("The user runtime directory is unavailable."));
        return false;
    }
    const QFileInfo existing(path);
    if ((existing.exists() || existing.isSymLink()) && !safeStateFile(existing) &&
        !QFile::remove(path))
    {
        error(errorText, QStringLiteral("An unexpected entry occupies %1.").arg(path));
        return false;
    }
    const QJsonObject object{
        {QStringLiteral("format"), Format},
        {QStringLiteral("session"), sessionIdentity()},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("stoppedAt"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    // The rename replaces the entry itself; it never writes through a link.
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0 || !file.commit())
    {
        error(errorText, QStringLiteral("Could not record the stop in %1.").arg(path));
        return false;
    }
    return true;
}

bool clear(QString *errorText)
{
    error(errorText, QString{});
    const QString path = statePath();
    if (path.isEmpty()) return true;
    const QFileInfo info(path);
    if ((info.exists() || info.isSymLink()) && !QFile::remove(path))
    {
        error(errorText, QStringLiteral("Could not clear the stop recorded in %1.").arg(path));
        return false;
    }
    return true;
}
}
