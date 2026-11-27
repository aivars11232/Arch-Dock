// SPDX-License-Identifier: GPL-3.0-or-later
#include "ArchDockVersion.h"

#include "archdock-version-constants.h"

#include <QCryptographicHash>

namespace ArchDock::Version
{
QString release()
{
    return QStringLiteral(ARCHDOCK_GENERATED_RELEASE);
}

QString displayVersion()
{
    const QString built = release();
    return built.isEmpty() ? QStringLiteral(ARCHDOCK_GENERATED_VERSION " (development build)") : built;
}

QString buildKind()
{
    return QStringLiteral(ARCHDOCK_GENERATED_BUILD_KIND);
}

bool isTestBuild()
{
    return buildKind() == QLatin1String("test");
}

QString repository()
{
    return QStringLiteral(ARCHDOCK_GENERATED_REPOSITORY);
}

QString keyId()
{
    return QStringLiteral(ARCHDOCK_GENERATED_KEY_ID);
}

QByteArray keySpki()
{
    return QByteArray::fromHex(QByteArrayLiteral(ARCHDOCK_GENERATED_KEY_SPKI_HEX));
}

QString keyFingerprint()
{
    // Computed from the key's bytes, so the fingerprint can never disagree
    // with the key it names.
    const QByteArray key = keySpki();
    return key.isEmpty() ? QString()
                         : QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
}

bool updatesConfigured()
{
    return !release().isEmpty() && !keyId().isEmpty() && !keySpki().isEmpty();
}

int updaterProtocol()
{
    return ARCHDOCK_GENERATED_UPDATER_PROTOCOL;
}

QString toolFolder()
{
    return QStringLiteral(ARCHDOCK_GENERATED_TOOL_FOLDER);
}

QString backendPath()
{
    return QStringLiteral(ARCHDOCK_GENERATED_BACKEND_PATH);
}

QString helperPath()
{
    return QStringLiteral(ARCHDOCK_GENERATED_HELPER_PATH);
}

QString finisherPath()
{
    return QStringLiteral(ARCHDOCK_GENERATED_FINISHER_PATH);
}

QString policyPath()
{
    return QStringLiteral(ARCHDOCK_GENERATED_POLICY_PATH);
}

QString pacmanLock()
{
    return QStringLiteral(ARCHDOCK_GENERATED_PACMAN_LOCK);
}

QString apiAddress()
{
    return QStringLiteral(ARCHDOCK_GENERATED_API);
}

QString downloadsAddress()
{
    return QStringLiteral(ARCHDOCK_GENERATED_DOWNLOADS);
}

QString qtBuildVersion()
{
    return QStringLiteral(ARCHDOCK_GENERATED_QT_BUILD);
}

QString rehearsalRoot()
{
    return QStringLiteral(ARCHDOCK_GENERATED_REHEARSAL_ROOT);
}
} // namespace ArchDock::Version
