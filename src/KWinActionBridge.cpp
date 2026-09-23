#include "KWinActionBridge.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

namespace
{
    QString javascriptString(const QString &value)
    {
        const QByteArray encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
    }
}

KWinActionBridge::KWinActionBridge(QObject *parent)
    : QObject(parent)
{
}

void KWinActionBridge::requestAction(const QString &internalId, const QString &action)
{
    if (internalId.isEmpty() ||
        action != QStringLiteral("activate") &&
            action != QStringLiteral("minimize") &&
            action != QStringLiteral("restore") &&
            action != QStringLiteral("close"))
    {
        return;
    }

    runScript(commandScript(internalId, action));
}

void KWinActionBridge::focusWindow(const QString &resourceClass, const QString &caption)
{
    if (resourceClass.isEmpty() || caption.isEmpty())
    {
        return;
    }

    runScript(focusScript(resourceClass, caption));
}

void KWinActionBridge::runScript(const QString &source)
{
    const QString cacheDirectory = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + QStringLiteral("/kwin-actions");
    QDir().mkpath(cacheDirectory);

    const QString pluginName = QStringLiteral("org.archdock.action.") +
                               QUuid::createUuid().toString(QUuid::Id128);
    const QString scriptPath = cacheDirectory + QLatin1Char('/') + pluginName + QStringLiteral(".js");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        return;
    }
    script.write(source.toUtf8());
    script.close();

    QDBusInterface scripting(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"),
        QDBusConnection::sessionBus());
    if (!scripting.isValid())
    {
        script.remove();
        return;
    }

    const QDBusReply<int> loadReply = scripting.call(
        QStringLiteral("loadScript"),
        scriptPath,
        pluginName);
    if (!loadReply.isValid() || loadReply.value() < 0)
    {
        qWarning() << "Failed to load Arch Dock KWin action:"
                   << loadReply.error().message() << "script id" << loadReply.value();
        script.remove();
        return;
    }

    // Global start() reapplies package enablement and can unload our watcher.
    // Run only this action; KWin replies after evaluating its script.
    QDBusInterface actionScript(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting/Script%1").arg(loadReply.value()),
        QStringLiteral("org.kde.kwin.Script"),
        QDBusConnection::sessionBus());
    const QDBusMessage runReply = actionScript.call(QStringLiteral("run"));
    if (runReply.type() == QDBusMessage::ErrorMessage)
    {
        qWarning() << "Failed to run Arch Dock KWin action:" << runReply.errorMessage();
    }
    scripting.call(QStringLiteral("unloadScript"), pluginName);
    script.remove();
}

QString KWinActionBridge::commandScript(const QString &internalId, const QString &action)
{
    return QStringLiteral(R"(
const targetId = %1;
const requestedAction = %2;
const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    const window = windows[index];
    if (window.internalId.toString() !== targetId) {
        continue;
    }

    if (window.deleted === true) {
        break;
    }
    if (requestedAction === "activate" && window.wantsInput === true) {
        window.minimized = false;
        workspace.activeWindow = window;
    } else if (requestedAction === "minimize" && window.minimizable === true) {
        window.minimized = true;
    } else if (requestedAction === "restore" && window.minimizable === true) {
        window.minimized = false;
    } else if (requestedAction === "close" && window.closeable === true) {
        window.closeWindow();
    }
    break;
}
)")
        .arg(javascriptString(internalId), javascriptString(action));
}

QString KWinActionBridge::focusScript(const QString &resourceClass, const QString &caption)
{
    return QStringLiteral(R"(
const targetClass = %1;
const targetCaption = %2;
const windows = workspace.windowList();
for (let index = windows.length - 1; index >= 0; --index) {
    const window = windows[index];
    if (window.resourceClass !== targetClass || window.caption !== targetCaption) {
        continue;
    }

    window.minimized = false;
    workspace.activeWindow = window;
    break;
}
)")
        .arg(javascriptString(resourceClass), javascriptString(caption));
}
