#include "KWinActionBridge.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>
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

    auto *scripting = new QDBusInterface(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"),
        QDBusConnection::sessionBus(),
        this);
    if (!scripting->isValid())
    {
        script.remove();
        scripting->deleteLater();
        return;
    }

    const QDBusMessage loadReply = scripting->call(
        QStringLiteral("loadScript"),
        scriptPath,
        pluginName);
    if (loadReply.type() == QDBusMessage::ErrorMessage)
    {
        script.remove();
        scripting->deleteLater();
        return;
    }

    scripting->call(QStringLiteral("start"));
    QTimer::singleShot(
        500,
        this,
        [scripting, scriptPath, pluginName]
        {
            scripting->call(QStringLiteral("unloadScript"), pluginName);
            QFile::remove(scriptPath);
            scripting->deleteLater();
        });
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

    if (requestedAction === "activate") {
        window.minimized = false;
        workspace.activeWindow = window;
    } else if (requestedAction === "minimize") {
        window.minimized = true;
    } else if (requestedAction === "close") {
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