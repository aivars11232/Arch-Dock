#include "IconEntryIdentity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace
{

const QRegularExpression stableIdentityPattern(
    QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,255}$"));

bool isDirectIdentityValue(const QString &value)
{
    return value.size() <= 220 && stableIdentityPattern.match(value).hasMatch();
}

QString digest(const QByteArray &value)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}

}

namespace ArchDock
{

QString IconEntryIdentity::forApplication(const QString &appId,
                                          const QString &desktopFileName)
{
    const QString explicitDesktopId = normalizedDesktopEntryId(desktopFileName);
    if (!explicitDesktopId.isEmpty())
    {
        return namespacedIdentity(QStringLiteral("desktop"), explicitDesktopId);
    }

    const QString sourceAppId = appId.trimmed();
    const QString normalizedAppId = sourceAppId.toLower();
    if (normalizedAppId.startsWith(QStringLiteral("free-url:")))
    {
        return forFreeUrl(QUrl::fromEncoded(
            sourceAppId.mid(9).toUtf8(), QUrl::StrictMode));
    }
    if (normalizedAppId.startsWith(QStringLiteral("file:")))
    {
        return forFreeUrl(QUrl::fromLocalFile(sourceAppId.mid(5)));
    }

    const QString appDesktopId = normalizedDesktopEntryId(normalizedAppId);
    if (normalizedAppId.endsWith(QStringLiteral(".desktop")) &&
        !appDesktopId.isEmpty())
    {
        return namespacedIdentity(QStringLiteral("desktop"), appDesktopId);
    }
    return namespacedIdentity(QStringLiteral("application"), normalizedAppId);
}

QString IconEntryIdentity::forFreeUrl(const QUrl &url)
{
    if (!url.isValid() || !url.isLocalFile())
    {
        return {};
    }
    const QFileInfo fileInfo(url.toLocalFile());
    const QString path = fileInfo.exists() && !fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.canonicalFilePath()
        : fileInfo.absoluteFilePath();
    if (path.isEmpty())
    {
        return {};
    }
    const QUrl normalizedUrl = QUrl::fromLocalFile(QDir::cleanPath(path));
    return QStringLiteral("free.sha256-") +
        digest(normalizedUrl.toEncoded(QUrl::FullyEncoded));
}

QString IconEntryIdentity::forEntry(const QVariantMap &entry)
{
    const QString derived = forApplication(
        entry.value(QStringLiteral("appId")).toString(),
        entry.value(QStringLiteral("desktopFileName")).toString());
    if (!derived.isEmpty())
    {
        return derived;
    }
    const QString supplied = entry.value(
        QStringLiteral("stableIdentity")).toString().trimmed();
    if (isValid(supplied))
    {
        return supplied;
    }
    return {};
}

bool IconEntryIdentity::isValid(const QString &identity)
{
    const QString value = identity.trimmed();
    if (!stableIdentityPattern.match(value).hasMatch())
    {
        return false;
    }
    return value.startsWith(QStringLiteral("desktop.")) ||
        value.startsWith(QStringLiteral("application.")) ||
        value.startsWith(QStringLiteral("free.sha256-"));
}

QString IconEntryIdentity::normalizedDesktopEntryId(const QString &identifier)
{
    const QString input = identifier.trimmed();
    if (input.isEmpty())
    {
        return {};
    }
    QString desktopId = QFileInfo(input).fileName().toLower();
    if (!desktopId.endsWith(QStringLiteral(".desktop")))
    {
        return {};
    }
    return stableIdentityPattern.match(desktopId).hasMatch()
        ? desktopId : QString{};
}

QString IconEntryIdentity::namespacedIdentity(const QString &nameSpace,
                                              const QString &value)
{
    if (value.isEmpty())
    {
        return {};
    }
    const QString direct = nameSpace + QLatin1Char('.') + value;
    if (isDirectIdentityValue(direct))
    {
        return direct;
    }
    return nameSpace + QStringLiteral(".sha256-") + digest(value.toUtf8());
}

}
