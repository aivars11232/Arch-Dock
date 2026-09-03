#include "PanelRegistry.h"
#include "DockSettings.h"
#include "model/PanelDefinition.h"
#include "model/IconEntryIdentity.h"
#include "model/PanelSettingsSchema.h"
#include "model/SettingsMigration.h"
#include "panel/IconOverrideTransaction.h"
#include "themes/ThemeAssetProcessor.h"
#include "themes/ThemePackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

#include <algorithm>
#include <limits>

namespace
{
constexpr int kFreeHostOwnershipTokenMaximumLength = 96;
constexpr auto kFreeCreationPending = "pending";
constexpr auto kFreeCreationComplete = "complete";
constexpr auto kFreeCreationRollbackPending = "rollback-pending";
constexpr auto kPanelRecordsKey = "dock/panels";
constexpr auto kPanelLegacyBackupKey = "dock/panelsLegacyV1Backup";

bool isEdge(const QString &edge)
{
    return edge == QStringLiteral("top") ||
           edge == QStringLiteral("bottom") ||
           edge == QStringLiteral("left") ||
           edge == QStringLiteral("right") ||
           edge == QStringLiteral("free");
}

bool isPanelType(const QString &type)
{
    return type == QStringLiteral("empty") ||
           type == QStringLiteral("launcher") ||
           type == QStringLiteral("tasks") ||
           type == QStringLiteral("hybrid");
}

bool isThemeFit(const QString &fit)
{
    return fit == QStringLiteral("cover") ||
           fit == QStringLiteral("contain") ||
           fit == QStringLiteral("stretch") ||
           fit == QStringLiteral("tile");
}

QVariantList builtInThemes()
{
    static const QVariantList themes = [] {
        QFile file(QStringLiteral(":/archdock/data/themes/builtin-themes.json"));
        if (!file.open(QIODevice::ReadOnly))
            return QVariantList{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonObject catalog = document.object();
        if (catalog.value(QStringLiteral("format")).toString() !=
                QStringLiteral("org.archdock.theme-catalog") ||
            catalog.value(QStringLiteral("version")).toInt() != 1)
        {
            return QVariantList{};
        }
        return catalog.value(QStringLiteral("themes")).toArray().toVariantList();
    }();
    return themes;
}

ArchDock::IconStyleStoreLoadResult builtInIconStyles()
{
    const QString installedCatalog = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("arch-dock/icon-styles/builtin-icon-styles.json"),
        QStandardPaths::LocateFile);
    if (!installedCatalog.isEmpty())
    {
        return ArchDock::IconStyleStore::loadCatalog(
            installedCatalog, QFileInfo(installedCatalog).absolutePath());
    }
#if defined(ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH) && \
    defined(ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT)
    const QString sourceCatalog = QString::fromUtf8(
        ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH);
    const QString sourcePackages = QString::fromUtf8(
        ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT);
    if (QFileInfo(sourceCatalog).isFile() && QFileInfo(sourcePackages).isDir())
    {
        return ArchDock::IconStyleStore::loadCatalog(
            sourceCatalog, sourcePackages);
    }
#endif
    return {
        {},
        {{QStringLiteral("missing-asset"), QString{}, QStringLiteral("error"),
          QStringLiteral("built-in icon-style catalog is unavailable")}},
    };
}

ArchDock::AnimationProfileCatalogLoadResult builtInAnimationProfiles()
{
    const QString installedCatalog = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral(
            "arch-dock/animation-profiles/builtin-animation-profiles.json"),
        QStandardPaths::LocateFile);
    if (!installedCatalog.isEmpty())
    {
        return ArchDock::AnimationProfileCatalog::loadCatalog(installedCatalog);
    }
#if defined(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH)
    const QString sourceCatalog = QString::fromUtf8(
        ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH);
    if (QFileInfo(sourceCatalog).isFile())
    {
        return ArchDock::AnimationProfileCatalog::loadCatalog(sourceCatalog);
    }
#endif
    // The catalog is also compiled in, so a stage-install without data files
    // still resolves every built-in profile.
    return ArchDock::AnimationProfileCatalog::loadCatalog(QStringLiteral(
        ":/archdock/data/animation-profiles/builtin-animation-profiles.json"));
}

bool normalizedCatalogPackageManifest(const QVariantMap &theme,
                                      QString *relativePath)
{
    const QString manifest = theme.value(
        QStringLiteral("packageManifest")).toString().trimmed();
    if (manifest.isEmpty() || manifest.contains(QLatin1Char('\\')) ||
        QDir::isAbsolutePath(manifest))
    {
        return false;
    }
    const QString clean = QDir::cleanPath(manifest);
    if (clean != manifest || clean == QStringLiteral(".") ||
        clean == QStringLiteral("..") ||
        clean.startsWith(QStringLiteral("../")))
    {
        return false;
    }
    if (relativePath)
    {
        *relativePath = clean;
    }
    return true;
}

QVariantList validatedThemeDefinitions(const QVariantList &themes)
{
    QVariantList result;
    result.reserve(themes.size());
    for (const QVariant &candidate : themes)
    {
        QString errorCode;
        if (!ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                candidate.toMap(), &errorCode).has_value())
        {
            qWarning().noquote()
                << "Theme catalog rejected:"
                << (errorCode.isEmpty()
                        ? QStringLiteral("invalid-capability-input")
                        : errorCode);
            return {};
        }
        const QVariantMap theme = candidate.toMap();
        if (theme.contains(QStringLiteral("packageManifest")))
        {
            QString packageManifest;
            if (!normalizedCatalogPackageManifest(theme, &packageManifest) ||
                packageManifest !=
                    theme.value(QStringLiteral("id")).toString() +
                        QStringLiteral("/archdock-theme.json"))
            {
                qWarning().noquote()
                    << "Theme catalog rejected: invalid-package-manifest";
                return {};
            }
        }
        result.append(theme);
    }
    return result;
}

constexpr int legacyThemePackageVersion = 1;
const QString themePackageFormat = QStringLiteral("org.archdock.theme");
const QString themePackageManifestName = QStringLiteral("archdock-theme.json");

struct LegacyThemePackage
{
    QString id;
    QString name;
    QString author;
    QString sourcePath;
    QString manifestPath;
    QString fit = QStringLiteral("cover");
};

QString normalizedThemePackageId(const QString &value)
{
    QString id;
    for (const QChar character : value.trimmed().toLower())
    {
        if (character.isLetterOrNumber())
        {
            id.append(character);
        }
        else if (!id.isEmpty() && !id.endsWith(QLatin1Char('-')))
        {
            id.append(QLatin1Char('-'));
        }
    }
    while (id.endsWith(QLatin1Char('-')))
    {
        id.chop(1);
    }
    return id.isEmpty() ? QStringLiteral("theme") : id.left(64);
}

QJsonObject legacyThemePackageManifest(const LegacyThemePackage &package,
                                       const QString &assetName)
{
    QJsonObject manifest{
        {QStringLiteral("format"), themePackageFormat},
        {QStringLiteral("version"), legacyThemePackageVersion},
        {QStringLiteral("id"), package.id},
        {QStringLiteral("name"), package.name},
        {QStringLiteral("surface"), QJsonObject{
             {QStringLiteral("asset"), assetName},
             {QStringLiteral("fit"), package.fit}}}};
    if (!package.author.isEmpty())
    {
        manifest.insert(QStringLiteral("author"), package.author);
    }
    return manifest;
}

bool writeLegacyThemePackageManifest(const QString &manifestPath,
                                     const LegacyThemePackage &package,
                                     const QString &assetName,
                                     QString *errorMessage)
{
    QSaveFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QByteArray contents = QJsonDocument(
        legacyThemePackageManifest(package, assetName)).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size() || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

QString themeSourceKind(const QString &format)
{
    if (format == QStringLiteral("blend"))
    {
        return QStringLiteral("scene");
    }
    if (format == QStringLiteral("svg") || format == QStringLiteral("svgz"))
    {
        return QStringLiteral("vector");
    }
    if (format == QStringLiteral("pdf") || format == QStringLiteral("psd") || format == QStringLiteral("xcf"))
    {
        return QStringLiteral("document");
    }
    return QStringLiteral("raster");
}

QString suggestedThemeFit(const QSize &sourceSize, const QSize &targetSize)
{
    if (!sourceSize.isValid() || !targetSize.isValid())
    {
        return QStringLiteral("cover");
    }

    const qreal sourceRatio = static_cast<qreal>(sourceSize.width()) / sourceSize.height();
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / targetSize.height();
    const qreal ratioDifference = qMax(sourceRatio, targetRatio) / qMin(sourceRatio, targetRatio);
    return ratioDifference >= 2.0 ? QStringLiteral("contain") : QStringLiteral("cover");
}

QString processorAssetId(const QString &value)
{
    QString result;
    result.reserve(qMin(value.size(), 48));
    bool previousWasSeparator = false;
    for (const QChar character : value.toLower())
    {
        const ushort code = character.unicode();
        const bool accepted = (code >= 'a' && code <= 'z') ||
            (code >= '0' && code <= '9') || character == QLatin1Char('.') ||
            character == QLatin1Char('_') || character == QLatin1Char('-');
        if (accepted)
        {
            result.append(character);
            previousWasSeparator = false;
        }
        else if (!result.isEmpty() && !previousWasSeparator)
        {
            result.append(QLatin1Char('-'));
            previousWasSeparator = true;
        }
        if (result.size() == 48)
        {
            break;
        }
    }
    while (result.endsWith(QLatin1Char('-')))
    {
        result.chop(1);
    }
    return result.isEmpty() ? QStringLiteral("asset") : result;
}

QString processorFailureMessage(
    const ArchDock::ThemeAssetProcessingResult &result,
    const QString &fallback)
{
    if (result.diagnostics.isEmpty())
    {
        return fallback;
    }
    const ArchDock::ThemeValidationDiagnostic &diagnostic =
        result.diagnostics.constFirst();
    return QStringLiteral("[%1] %2").arg(
        diagnostic.code,
        diagnostic.message.isEmpty() ? fallback : diagnostic.message);
}

QVariantMap analyzeThemeSource(const QString &sourcePath,
                               const QString &manifestPath,
                               const QSize &targetSize)
{
    const QFileInfo source(sourcePath);
    const QString format = source.suffix().toLower();
    QVariantMap analysis{
        {QStringLiteral("themeSourceKind"), themeSourceKind(format)},
        {QStringLiteral("themeSourceFormat"), format},
        {QStringLiteral("themeSourceWidth"), 0},
        {QStringLiteral("themeSourceHeight"), 0},
        {QStringLiteral("themeSourceHasAlpha"), false},
        {QStringLiteral("themeSuggestedFit"), QStringLiteral("cover")},
        {QStringLiteral("themePreview"), QString{}},
        {QStringLiteral("themeAnalysisStatus"), QString{}},
        {QStringLiteral("themeConversionTool"), QString{}},
        {QStringLiteral("themeConversionAvailable"), false}};

    ArchDock::ThemeAssetProcessingRequest request;
    request.sourcePath = source.absoluteFilePath();
    request.managedOutputRoot = QFileInfo(manifestPath).absolutePath() +
        QStringLiteral("/processed");
    request.assetId = processorAssetId(source.completeBaseName());
    request.previewBounds = QSize(320, 180);
    request.cleanupRequirements = {QStringLiteral("none")};
    const ArchDock::ThemeAssetProcessingResult processed =
        ArchDock::ThemeAssetProcessor::process(request);
    if (!processed.isValid())
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            format == QStringLiteral("blend")
                ? QObject::tr(
                      "3D scene saved as an offline source; automatic external conversion is disabled. %1")
                      .arg(processorFailureMessage(
                          processed, QObject::tr("A reviewed raster derivative is required.")))
                : QObject::tr("Bounded source analysis failed: %1")
                      .arg(processorFailureMessage(
                          processed, QObject::tr("The source could not be decoded."))));
        return analysis;
    }

    const QSize sourceSize = processed.inspection.sourceSize;
    analysis.insert(QStringLiteral("themeSourceWidth"), sourceSize.width());
    analysis.insert(QStringLiteral("themeSourceHeight"), sourceSize.height());
    analysis.insert(
        QStringLiteral("themeSourceHasAlpha"),
        processed.inspection.hasAlphaChannel);
    analysis.insert(
        QStringLiteral("themeSuggestedFit"),
        suggestedThemeFit(sourceSize, targetSize));
    for (const ArchDock::ThemeAssetDerivative &derivative :
         processed.derivatives)
    {
        if (derivative.role == QStringLiteral("preview"))
        {
            analysis.insert(
                QStringLiteral("themePreview"),
                QUrl::fromLocalFile(derivative.absolutePath).toString());
            break;
        }
    }
    if (processed.inspection.hasTransparentPixels)
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr(
                "Analyzed %1 x %2 source; %3 transparent pixels detected; bounded preview ready.")
                .arg(sourceSize.width())
                .arg(sourceSize.height())
                .arg(processed.inspection.transparentPixelCount));
    }
    else if (processed.inspection.hasAlphaChannel)
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr(
                "Analyzed %1 x %2 source; its alpha channel is fully opaque; bounded preview ready.")
                .arg(sourceSize.width())
                .arg(sourceSize.height()));
    }
    else
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr(
                "Analyzed %1 x %2 source without an alpha channel; bounded preview ready.")
                .arg(sourceSize.width())
                .arg(sourceSize.height()));
    }
    return analysis;
}

bool migrateLegacyThemePackage(QVariantMap *panel)
{
    const QUrl currentManifest(panel->value(QStringLiteral("themePackageManifest")).toString());
    const bool declaresPackage =
        panel->value(QStringLiteral("themePackageFormat")).toString() ==
            themePackageFormat &&
        panel->value(QStringLiteral("themePackageVersion")).toInt() > 0;
    if (declaresPackage)
    {
        const int declaredVersion = panel->value(
            QStringLiteral("themePackageVersion")).toInt();
        const ArchDock::ThemePackageLoadResult current =
            currentManifest.isLocalFile()
            ? ArchDock::ThemePackage::load(currentManifest.toLocalFile())
            : ArchDock::ThemePackageLoadResult{};
        if (current.isValid() && current.package->sourceVersion() == declaredVersion)
        {
            return false;
        }

        const QString code = current.primaryCode().isEmpty()
            ? QStringLiteral("missing-asset") : current.primaryCode();
        panel->insert(QStringLiteral("themeSource"), QString{});
        panel->insert(QStringLiteral("themeAsset"), QString{});
        panel->insert(QStringLiteral("themePreview"), QString{});
        panel->insert(QStringLiteral("themePackageFormat"), QString{});
        panel->insert(QStringLiteral("themePackageVersion"), 0);
        panel->insert(QStringLiteral("themePackageId"), QString{});
        panel->insert(QStringLiteral("themePackageName"), QString{});
        panel->insert(QStringLiteral("themePackageAuthor"), QString{});
        panel->insert(QStringLiteral("themePackageManifest"), QString{});
        panel->insert(QStringLiteral("themeRenderWidth"), 0);
        panel->insert(QStringLiteral("themeRenderHeight"), 0);
        panel->insert(QStringLiteral("themeRenderFit"), QString{});
        panel->insert(QStringLiteral("themeRenderOutcome"), QStringLiteral("fallback"));
        panel->insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Persisted theme package failed validation; safe fallback active."));
        panel->insert(
            QStringLiteral("themeStatus"),
            QObject::tr("Persisted theme rejected [%1]; preset surface active.").arg(code));
        return true;
    }

    const QUrl sourceUrl(panel->value(QStringLiteral("themeSource")).toString());
    const QFileInfo source(sourceUrl.toLocalFile());
    LegacyThemePackage package;
    if (sourceUrl.isLocalFile() && source.isFile())
    {
        const QString fingerprint = QString::fromLatin1(
            QCryptographicHash::hash(source.absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
        package.id = QStringLiteral("legacy-") + fingerprint;
        package.name = source.completeBaseName();
        package.sourcePath = source.absoluteFilePath();
        package.fit = panel->value(QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
        if (!isThemeFit(package.fit))
        {
            package.fit = QStringLiteral("cover");
        }
        package.manifestPath = source.absolutePath() + QLatin1Char('/') + package.id +
            QStringLiteral(".archdock-theme.json");

        QString errorMessage;
        if (writeLegacyThemePackageManifest(
                package.manifestPath, package, source.fileName(), &errorMessage))
        {
            panel->insert(QStringLiteral("themePackageFormat"), themePackageFormat);
            panel->insert(QStringLiteral("themePackageVersion"), legacyThemePackageVersion);
            panel->insert(QStringLiteral("themePackageId"), package.id);
            panel->insert(QStringLiteral("themePackageName"), package.name);
            panel->insert(QStringLiteral("themePackageAuthor"), QString{});
            panel->insert(QStringLiteral("themePackageManifest"), QUrl::fromLocalFile(package.manifestPath).toString());
            const QSize targetSize(
                panel->value(QStringLiteral("width"), 720).toInt(),
                panel->value(QStringLiteral("height"), 76).toInt());
            const QVariantMap analysis = analyzeThemeSource(
                package.sourcePath, package.manifestPath, targetSize);
            for (auto iterator = analysis.cbegin(); iterator != analysis.cend(); ++iterator)
            {
                panel->insert(iterator.key(), iterator.value());
            }
            return true;
        }
    }

    bool changed = false;
    const auto setDefault = [panel, &changed](const QString &key, const QVariant &value)
    {
        if (!panel->contains(key))
        {
            panel->insert(key, value);
            changed = true;
        }
    };
    setDefault(QStringLiteral("themePackageFormat"), QString{});
    setDefault(QStringLiteral("themePackageVersion"), 0);
    setDefault(QStringLiteral("themePackageId"), QString{});
    setDefault(QStringLiteral("themePackageName"), QString{});
    setDefault(QStringLiteral("themePackageAuthor"), QString{});
    setDefault(QStringLiteral("themePackageManifest"), QString{});
    setDefault(QStringLiteral("themeSourceKind"), QString{});
    setDefault(QStringLiteral("themeSourceFormat"), QString{});
    setDefault(QStringLiteral("themeSourceWidth"), 0);
    setDefault(QStringLiteral("themeSourceHeight"), 0);
    setDefault(QStringLiteral("themeSourceHasAlpha"), false);
    setDefault(QStringLiteral("themeSuggestedFit"), QStringLiteral("cover"));
    setDefault(QStringLiteral("themePreview"), QString{});
    setDefault(QStringLiteral("themeAnalysisStatus"), QObject::tr("No source selected."));
    setDefault(QStringLiteral("themeConversionTool"), QString{});
    setDefault(QStringLiteral("themeConversionAvailable"), false);
    return changed;
}

bool isVisibilityMode(const QString &mode)
{
    return mode == QStringLiteral("always") ||
           mode == QStringLiteral("auto-hide") ||
           mode == QStringLiteral("dodge") ||
           mode == QStringLiteral("cover");
}

int normalizedFreeHostId(const QVariant &value)
{
    bool ok = false;
    const qlonglong candidate = value.toLongLong(&ok);
    return ok && candidate >= 0 && candidate <= std::numeric_limits<int>::max()
        ? static_cast<int>(candidate)
        : -1;
}

bool hasFreeHostIdData(const QVariant &value)
{
    if (!value.isValid() || value.isNull() || value.toString().trimmed().isEmpty())
    {
        return false;
    }

    bool ok = false;
    const qlonglong candidate = value.toLongLong(&ok);
    return !ok || candidate != -1;
}

PanelRegistry::FreeHostState freeHostStateFromString(const QString &value)
{
    if (value == QStringLiteral("unhosted"))
    {
        return PanelRegistry::FreeHostState::Unhosted;
    }
    if (value == QStringLiteral("hosted-owned"))
    {
        return PanelRegistry::FreeHostState::HostedOwned;
    }
    if (value == QStringLiteral("detached"))
    {
        return PanelRegistry::FreeHostState::Detached;
    }
    return PanelRegistry::FreeHostState::HostedStale;
}

bool normalizeFreeHostRecord(QVariantMap *panel)
{
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free"))
    {
        return false;
    }

    const QVariant rawContainmentId = panel->value(QStringLiteral("freeDesktopContainmentId"), -1);
    const QVariant rawAppletId = panel->value(QStringLiteral("freeDockAppletId"), -1);
    const int containmentId = normalizedFreeHostId(rawContainmentId);
    const int appletId = normalizedFreeHostId(rawAppletId);
    const QString ownershipToken = panel->value(
        QStringLiteral("freeOwnershipToken")).toString().trimmed();
    const QString requestedHostMode = panel->value(
        QStringLiteral("freeHostMode")).toString().trimmed().toLower();
    const QString hostMode = QStringLiteral("desktop");

    const QString requestedState = panel->value(
        QStringLiteral("freeHostState")).toString().trimmed().toLower();
    const bool declaredDesktopMode = requestedHostMode == QStringLiteral("desktop");
    const bool defaultableDesktopMode = requestedHostMode.isEmpty() || declaredDesktopMode;
    const bool validToken = !ownershipToken.isEmpty() &&
        ownershipToken.size() <= kFreeHostOwnershipTokenMaximumLength;
    const bool completeAssociation = containmentId >= 0 && appletId >= 0 &&
        validToken && declaredDesktopMode;
    const bool hasAssociationData = hasFreeHostIdData(rawContainmentId) ||
        hasFreeHostIdData(rawAppletId) || !ownershipToken.isEmpty() ||
        !defaultableDesktopMode;

    QString normalizedState;
    if (requestedState == QStringLiteral("hosted-owned"))
    {
        normalizedState = completeAssociation
            ? QStringLiteral("hosted-owned")
            : QStringLiteral("hosted-stale");
    }
    else if (requestedState == QStringLiteral("hosted-stale"))
    {
        normalizedState = QStringLiteral("hosted-stale");
    }
    else if (requestedState == QStringLiteral("detached"))
    {
        normalizedState = !hasAssociationData && defaultableDesktopMode
            ? QStringLiteral("detached")
            : QStringLiteral("hosted-stale");
    }
    else if (requestedState.isEmpty() || requestedState == QStringLiteral("unhosted"))
    {
        normalizedState = !hasAssociationData && defaultableDesktopMode
            ? QStringLiteral("unhosted")
            : QStringLiteral("hosted-stale");
    }
    else
    {
        normalizedState = QStringLiteral("hosted-stale");
    }

    bool changed = false;
    const auto setValue = [panel, &changed](const QString &key, const QVariant &value)
    {
        if (!panel->contains(key) || panel->value(key) != value)
        {
            panel->insert(key, value);
            changed = true;
        }
    };
    setValue(QStringLiteral("freeDesktopContainmentId"), containmentId);
    setValue(QStringLiteral("freeDockAppletId"), appletId);
    setValue(QStringLiteral("freeOwnershipToken"), ownershipToken);
    setValue(QStringLiteral("freeHostMode"), hostMode);
    setValue(QStringLiteral("freeHostState"), normalizedState);
    setValue(QStringLiteral("screen"), qMax(0, panel->value(QStringLiteral("screen")).toInt()));
    setValue(QStringLiteral("screenId"), panel->value(QStringLiteral("screenId")).toString().trimmed());
    return changed;
}

}

PanelRegistry::PanelRegistry(QObject *parent)
    : PanelRegistry(builtInThemes(), parent)
{
}

PanelRegistry::PanelRegistry(const QVariantList &themeDefinitions,
                             QObject *parent)
    : QObject(parent),
      m_themeDefinitions(validatedThemeDefinitions(themeDefinitions))
{
    ArchDock::IconStyleStoreLoadResult iconStyles = builtInIconStyles();
    if (iconStyles.isValid())
    {
        m_iconStyleStore = std::move(iconStyles.store);
    }
    else
    {
        m_iconStyleStoreError = iconStyles.primaryCode().isEmpty()
            ? QStringLiteral("icon-style-store-unavailable")
            : iconStyles.primaryCode();
        qWarning().noquote() << "Icon-style catalog rejected:"
                             << m_iconStyleStoreError;
    }
    ArchDock::AnimationProfileCatalogLoadResult animationProfiles =
        builtInAnimationProfiles();
    if (animationProfiles.isValid())
    {
        m_animationProfileCatalog = std::move(animationProfiles.catalog);
    }
    else
    {
        m_animationProfileCatalogError =
            animationProfiles.primaryCode().isEmpty()
                ? QStringLiteral("animation-profile-catalog-unavailable")
                : animationProfiles.primaryCode();
        qWarning().noquote() << "Animation-profile catalog rejected:"
                             << m_animationProfileCatalogError;
    }
    load();
}

PanelRegistry::~PanelRegistry()
{
    m_activeRenders.clear();
    m_pendingRenders.clear();
}

QStringList PanelRegistry::panelIds() const
{
    QStringList ids;
    ids.reserve(m_panels.size());
    for (const QVariantMap &panel : m_panels)
    {
        ids.append(panel.value(QStringLiteral("id")).toString());
    }
    return ids;
}

QString PanelRegistry::activePanelId() const
{
    return m_activePanelId;
}

int PanelRegistry::revision() const
{
    return m_revision;
}

QString PanelRegistry::migrationDiagnostic() const
{
    return m_migrationDiagnostic;
}

QVariant PanelRegistry::panelValue(const QString &panelId, const QString &key) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? panel->value(key) : QVariant{};
}

QVariantMap PanelRegistry::panelSnapshot(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? *panel : QVariantMap{};
}

QString PanelRegistry::panelName(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? panel->value(QStringLiteral("name")).toString() : QString{};
}

bool PanelRegistry::isBuiltIn(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel && panel->value(QStringLiteral("builtIn")).toBool();
}

void PanelRegistry::setPanelValue(const QString &panelId, const QString &key, const QVariant &value)
{
    setPanelValues(panelId, {{key, value}});
}

void PanelRegistry::updatePanel(const QString &panelId, const QVariantMap &values)
{
    setPanelValues(panelId, values);
}

bool PanelRegistry::updatePanelChecked(const QString &panelId, const QVariantMap &values)
{
    return setPanelValuesChecked(panelId, values);
}

std::optional<ArchDock::PanelDefinition> PanelRegistry::panelDefinition(
    const QString &panelId,
    QString *errorMessage) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel not found: %1").arg(panelId);
        }
        return std::nullopt;
    }
    return ArchDock::PanelDefinition::fromLegacyMap(*panel, errorMessage);
}

std::optional<ArchDock::ThemeCapabilityProfile>
PanelRegistry::themeCapabilityProfile(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    const QString themeId = !definition.surface.completeThemeId.trimmed().isEmpty()
        ? definition.surface.completeThemeId.trimmed()
        : definition.surface.panelThemeId.trimmed();
    if (!themeId.isEmpty())
    {
        for (const QVariant &candidate : m_themeDefinitions)
        {
            const QVariantMap theme = candidate.toMap();
            if (theme.value(QStringLiteral("id")).toString() != themeId)
            {
                continue;
            }
            if (!theme.contains(QStringLiteral("packageManifest")))
            {
                return ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                    theme, errorCode);
            }
            const std::optional<QVariantMap> projection =
                builtInThemeRuntimeProjection(themeId, errorCode);
            if (!projection.has_value())
            {
                return std::nullopt;
            }
            return ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                *projection, errorCode);
        }
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-capability-undeclared");
        }
        return std::nullopt;
    }

    const QString manifestValue =
        definition.surface.themePackageManifest.trimmed();
    if (!manifestValue.isEmpty())
    {
        const QUrl manifestUrl(manifestValue);
        if (!manifestUrl.isLocalFile())
        {
            if (errorCode)
            {
                *errorCode = QStringLiteral("unsafe-package-root");
            }
            return std::nullopt;
        }
        const ArchDock::ThemePackageLoadResult loaded =
            ArchDock::ThemePackage::load(manifestUrl.toLocalFile());
        if (!loaded.isValid())
        {
            if (errorCode)
            {
                *errorCode = loaded.primaryCode().isEmpty()
                    ? QStringLiteral("invalid-capability-input")
                    : loaded.primaryCode();
            }
            return std::nullopt;
        }
        if (definition.surface.themePackageVersion > 0 &&
            loaded.package->sourceVersion() !=
                definition.surface.themePackageVersion)
        {
            if (errorCode)
            {
                *errorCode = QStringLiteral("unsupported-version");
            }
            return std::nullopt;
        }
        if (loaded.package->sourceVersion() >= 2)
        {
            return ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                loaded.package->definition().toVariantMap(), errorCode);
        }
        if (errorCode)
        {
            errorCode->clear();
        }
        return ArchDock::PanelCapabilityResolver::legacyThemeProfile(definition);
    }

    const bool hasManagedArtwork =
        !definition.surface.themeAsset.trimmed().isEmpty() ||
        !definition.surface.themeSource.trimmed().isEmpty();
    if (hasManagedArtwork)
    {
        if (errorCode)
        {
            errorCode->clear();
        }
        return ArchDock::PanelCapabilityResolver::legacyThemeProfile(definition);
    }

    if (errorCode)
    {
        errorCode->clear();
    }
    return ArchDock::PanelCapabilityResolver::proceduralThemeProfile();
}

std::optional<QVariantMap> PanelRegistry::builtInThemeRuntimeProjection(
    const QString &themeId,
    QString *errorCode) const
{
    const QString normalizedThemeId = themeId.trimmed();
    QVariantMap catalogTheme;
    for (const QVariant &candidate : m_themeDefinitions)
    {
        const QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() == normalizedThemeId)
        {
            catalogTheme = theme;
            break;
        }
    }
    if (catalogTheme.isEmpty())
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-not-found");
        }
        return std::nullopt;
    }
    if (!catalogTheme.contains(QStringLiteral("packageManifest")))
    {
        if (errorCode)
        {
            errorCode->clear();
        }
        return std::nullopt;
    }

    QString relativeManifest;
    if (!normalizedCatalogPackageManifest(catalogTheme, &relativeManifest))
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("invalid-package-manifest");
        }
        return std::nullopt;
    }

    const QString installedRelativePath =
        QStringLiteral("arch-dock/themes/") + relativeManifest;
    QString manifestPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        installedRelativePath,
        QStandardPaths::LocateFile);
#ifdef ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT
    if (manifestPath.isEmpty())
    {
        const QString sourceCandidate = QDir(
            QString::fromUtf8(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT))
                                            .filePath(relativeManifest);
        const QFileInfo sourceInfo(sourceCandidate);
        if (sourceInfo.isFile() && sourceInfo.isReadable())
        {
            manifestPath = sourceInfo.absoluteFilePath();
        }
    }
#endif
    if (manifestPath.isEmpty())
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-package-unavailable");
        }
        return std::nullopt;
    }

    const ArchDock::ThemePackageLoadResult loaded =
        ArchDock::ThemePackage::load(manifestPath);
    if (!loaded.isValid())
    {
        if (errorCode)
        {
            *errorCode = loaded.primaryCode().isEmpty()
                ? QStringLiteral("invalid-capability-input")
                : loaded.primaryCode();
        }
        return std::nullopt;
    }
    if (loaded.package->sourceVersion() !=
        catalogTheme.value(QStringLiteral("version")).toInt())
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("unsupported-version");
        }
        return std::nullopt;
    }
    if (loaded.package->definition().id != normalizedThemeId)
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-package-identity-mismatch");
        }
        return std::nullopt;
    }

    QString catalogCapabilityError;
    QString packageCapabilityError;
    const std::optional<ArchDock::ThemeCapabilityProfile> catalogProfile =
        ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
            catalogTheme, &catalogCapabilityError);
    const std::optional<ArchDock::ThemeCapabilityProfile> packageProfile =
        ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
            loaded.package->definition().toVariantMap(),
            &packageCapabilityError);
    if (!catalogProfile.has_value() || !packageProfile.has_value() ||
        *catalogProfile != *packageProfile)
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-capability-mismatch");
        }
        return std::nullopt;
    }

    if (errorCode)
    {
        errorCode->clear();
    }
    return loaded.package->runtimeProjection();
}

std::optional<QVariantMap> PanelRegistry::themeRuntimeProjection(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    const QString themeId = !definition.surface.completeThemeId.trimmed().isEmpty()
        ? definition.surface.completeThemeId.trimmed()
        : definition.surface.panelThemeId.trimmed();
    if (!themeId.isEmpty())
    {
        for (const QVariant &candidate : m_themeDefinitions)
        {
            const QVariantMap theme = candidate.toMap();
            if (theme.value(QStringLiteral("id")).toString() != themeId)
            {
                continue;
            }
            return builtInThemeRuntimeProjection(themeId, errorCode);
        }
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-capability-undeclared");
        }
        return std::nullopt;
    }

    const QString manifestValue =
        definition.surface.themePackageManifest.trimmed();
    if (manifestValue.isEmpty())
    {
        if (errorCode)
        {
            errorCode->clear();
        }
        return std::nullopt;
    }

    const QUrl manifestUrl(manifestValue);
    if (!manifestUrl.isLocalFile())
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("unsafe-package-root");
        }
        return std::nullopt;
    }

    const ArchDock::ThemePackageLoadResult loaded =
        ArchDock::ThemePackage::load(manifestUrl.toLocalFile());
    if (!loaded.isValid())
    {
        if (errorCode)
        {
            *errorCode = loaded.primaryCode().isEmpty()
                ? QStringLiteral("invalid-capability-input")
                : loaded.primaryCode();
        }
        return std::nullopt;
    }

    if (!definition.surface.themePackageFormat.trimmed().isEmpty() &&
        definition.surface.themePackageFormat.trimmed() != themePackageFormat)
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("unsupported-format");
        }
        return std::nullopt;
    }
    if (definition.surface.themePackageVersion > 0 &&
        loaded.package->sourceVersion() !=
            definition.surface.themePackageVersion)
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("unsupported-version");
        }
        return std::nullopt;
    }

    const QString expectedPackageId =
        definition.surface.themePackageId.trimmed();
    if (!expectedPackageId.isEmpty() &&
        loaded.package->definition().id != expectedPackageId)
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("theme-package-identity-mismatch");
        }
        return std::nullopt;
    }

    if (errorCode)
    {
        errorCode->clear();
    }
    return loaded.package->runtimeProjection();
}

ArchDock::CapabilityResolution PanelRegistry::resolvePanelCapabilities(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    const ArchDock::HostCapabilityProfile host =
        ArchDock::PanelCapabilityResolver::productionHostProfile(
            definition.host.kind);
    const std::optional<ArchDock::ThemeCapabilityProfile> theme =
        themeCapabilityProfile(definition, errorCode);
    if (!theme.has_value())
    {
        ArchDock::CapabilityResolution unavailable;
        unavailable.hostProfileId = host.id;
        unavailable.themeId = !definition.surface.completeThemeId.trimmed().isEmpty()
            ? definition.surface.completeThemeId.trimmed()
            : definition.surface.panelThemeId.trimmed();
        unavailable.reason = ArchDock::CapabilityReasonCode::InvalidCapabilityInput;
        return unavailable;
    }

    if (errorCode)
    {
        errorCode->clear();
    }
    return ArchDock::PanelCapabilityResolver::resolve(
        definition,
        host,
        *theme,
        ArchDock::PanelCapabilityResolver::productionRenderers(),
        ArchDock::PanelCapabilityResolver::productionPlatform());
}

bool PanelRegistry::persistPanelSettingsTransaction(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    QString *errorMessage)
{
    return persistPanelDefinitionTransaction(
        draft.previousPanel,
        draft.candidatePanel,
        draft.candidateGlobals,
        errorMessage);
}

bool PanelRegistry::persistPanelDefinitionTransaction(
    const ArchDock::PanelDefinition &previousPanel,
    const ArchDock::PanelDefinition &candidatePanel,
    const QVariantMap &globalSettings,
    QString *errorMessage)
{
    qsizetype panelIndex = -1;
    for (qsizetype index = 0; index < m_panels.size(); ++index)
    {
        if (m_panels.at(index).value(QStringLiteral("id")).toString() ==
            previousPanel.identity.id)
        {
            panelIndex = index;
            break;
        }
    }
    if (panelIndex < 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel disappeared before transaction commit");
        }
        return false;
    }

    QString currentError;
    const std::optional<ArchDock::PanelDefinition> current =
        ArchDock::PanelDefinition::fromLegacyMap(m_panels.at(panelIndex), &currentError);
    if (!current.has_value() || *current != previousPanel)
    {
        if (errorMessage)
        {
            *errorMessage = current.has_value()
                ? QStringLiteral("panel changed before transaction commit")
                : currentError;
        }
        return false;
    }
    if (candidatePanel.settingsRevision !=
            previousPanel.settingsRevision + 1 ||
        candidatePanel.identity.id != previousPanel.identity.id ||
        candidatePanel.identity.builtIn != previousPanel.identity.builtIn)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("candidate revision is not the next panel revision");
        }
        return false;
    }

    QList<QVariantMap> stagedPanels = m_panels;
    stagedPanels[panelIndex] = candidatePanel.toLegacyMap();
    if (!persistPanelState(stagedPanels, globalSettings, errorMessage))
    {
        return false;
    }
    m_panels = std::move(stagedPanels);
    return true;
}

bool PanelRegistry::rollbackPanelSettingsTransaction(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    quint64 committedRevision,
    quint64 *rollbackRevision,
    QString *errorMessage)
{
    if (rollbackRevision)
    {
        *rollbackRevision = 0;
    }
    if (committedRevision == std::numeric_limits<quint64>::max())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel revision cannot advance for rollback");
        }
        return false;
    }

    qsizetype panelIndex = -1;
    for (qsizetype index = 0; index < m_panels.size(); ++index)
    {
        if (m_panels.at(index).value(QStringLiteral("id")).toString() ==
            draft.previousPanel.identity.id)
        {
            panelIndex = index;
            break;
        }
    }
    if (panelIndex < 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel disappeared before transaction rollback");
        }
        return false;
    }

    QString currentError;
    const std::optional<ArchDock::PanelDefinition> current =
        ArchDock::PanelDefinition::fromLegacyMap(m_panels.at(panelIndex), &currentError);
    if (!current.has_value() || current->settingsRevision != committedRevision)
    {
        if (errorMessage)
        {
            *errorMessage = current.has_value()
                ? QStringLiteral("panel changed before transaction rollback")
                : currentError;
        }
        return false;
    }

    ArchDock::PanelDefinition restoredPanel = draft.previousPanel;
    restoredPanel.settingsRevision = committedRevision + 1;
    QList<QVariantMap> stagedPanels = m_panels;
    stagedPanels[panelIndex] = restoredPanel.toLegacyMap();
    if (!persistPanelState(stagedPanels, draft.previousGlobals, errorMessage))
    {
        return false;
    }
    m_panels = std::move(stagedPanels);
    if (rollbackRevision)
    {
        *rollbackRevision = restoredPanel.settingsRevision;
    }
    return true;
}

void PanelRegistry::notifyPanelSettingsTransactionAdopted(
    bool nativeTopologyChanged)
{
    changed(nativeTopologyChanged);
}

std::optional<PanelRegistry::FreeHostAssociation> PanelRegistry::freeHostAssociation(
    const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free"))
    {
        return std::nullopt;
    }

    FreeHostAssociation association;
    association.desktopContainmentId = panel->value(
        QStringLiteral("freeDesktopContainmentId"), -1).toInt();
    association.dockAppletId = panel->value(QStringLiteral("freeDockAppletId"), -1).toInt();
    association.ownershipToken = panel->value(
        QStringLiteral("freeOwnershipToken")).toString();
    association.screenIndex = panel->value(QStringLiteral("screen")).toInt();
    association.screenId = panel->value(QStringLiteral("screenId")).toString();
    association.hostMode = panel->value(QStringLiteral("freeHostMode")).toString();
    association.state = freeHostStateFromString(
        panel->value(QStringLiteral("freeHostState")).toString());
    return association;
}

QString PanelRegistry::beginFreePanelCreation(const QString &ownershipToken)
{
    const QString normalizedToken = ownershipToken.trimmed();
    if (normalizedToken.isEmpty() ||
        normalizedToken.size() > kFreeHostOwnershipTokenMaximumLength)
    {
        return {};
    }

    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("free-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(
        id,
        tr("Free panel %1").arg(sequence - 1),
        QStringLiteral("bottom"),
        false);
    panel.insert(QStringLiteral("edge"), QStringLiteral("free"));
    panel.insert(QStringLiteral("visible"), true);
    panel.insert(QStringLiteral("type"), QStringLiteral("empty"));
    panel.insert(QStringLiteral("contentAppIds"), QStringList{});
    panel.insert(QStringLiteral("contentUrls"), QStringList{});
    panel.insert(QStringLiteral("layout"), QStringLiteral("circular"));
    panel.insert(QStringLiteral("width"), 420);
    panel.insert(QStringLiteral("height"), 420);
    panel.insert(QStringLiteral("layoutRadius"), 145);
    panel.insert(QStringLiteral("x"), 240);
    panel.insert(QStringLiteral("y"), 180);
    panel.insert(QStringLiteral("freeOwnershipToken"), normalizedToken);
    panel.insert(QStringLiteral("freeCreationState"),
                 QString::fromLatin1(kFreeCreationPending));
    panel.insert(QStringLiteral("freeCreationError"), QString{});
    panel.insert(QStringLiteral("freeRollbackError"), QString{});
    (void)normalizeFreeHostRecord(&panel);

    const QString previousActivePanelId = m_activePanelId;
    m_panels.append(panel);
    m_activePanelId = id;
    if (!saveChecked())
    {
        m_panels.removeLast();
        m_activePanelId = previousActivePanelId;
        return {};
    }

    if (m_activePanelId != previousActivePanelId)
    {
        emit activePanelIdChanged();
    }
    changed(false);
    return id;
}

bool PanelRegistry::commitVerifiedFreeHostAssociation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId,
    const QString &hostMode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedMode = hostMode.trimmed().toLower();
    const QString creationState = panel
        ? panel->value(QStringLiteral("freeCreationState")).toString()
        : QString{};
    const QString reservedToken = panel
        ? panel->value(QStringLiteral("freeOwnershipToken")).toString()
        : QString{};
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        desktopContainmentId < 0 || dockAppletId < 0 || screenIndex < 0 ||
        normalizedToken.isEmpty() ||
        normalizedToken.size() > kFreeHostOwnershipTokenMaximumLength ||
        normalizedMode != QStringLiteral("desktop") ||
        (!creationState.isEmpty() && creationState != QStringLiteral("idle") &&
         (creationState != QLatin1String(kFreeCreationPending) ||
          reservedToken != normalizedToken)))
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), desktopContainmentId},
         {QStringLiteral("freeDockAppletId"), dockAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), screenIndex},
         {QStringLiteral("screenId"), screenId.trimmed()},
         {QStringLiteral("freeHostMode"), normalizedMode},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")}});
}

bool PanelRegistry::completeFreePanelCreation(
    const QString &panelId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const auto association = freeHostAssociation(panelId);
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("freeCreationState")).toString() !=
            QLatin1String(kFreeCreationPending) ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
        !association.has_value() || association->state != FreeHostState::HostedOwned ||
        association->desktopContainmentId < 0 || association->dockAppletId < 0 ||
        association->ownershipToken != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeCreationState"),
          QString::fromLatin1(kFreeCreationComplete)},
         {QStringLiteral("freeCreationError"), QString{}},
         {QStringLiteral("freeRollbackError"), QString{}}});
}

bool PanelRegistry::recordRecoverableFreePanelCreation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId,
    const QString &creationError,
    const QString &rollbackError)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString creationState = panel
        ? panel->value(QStringLiteral("freeCreationState")).toString()
        : QString{};
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        (creationState != QLatin1String(kFreeCreationPending) &&
         creationState != QLatin1String(kFreeCreationRollbackPending)) ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
        creationError.trimmed().isEmpty())
    {
        return false;
    }

    const int retainedContainmentId = desktopContainmentId >= 0
        ? desktopContainmentId
        : panel->value(QStringLiteral("freeDesktopContainmentId"), -1).toInt();
    const int retainedAppletId = dockAppletId >= 0
        ? dockAppletId
        : panel->value(QStringLiteral("freeDockAppletId"), -1).toInt();
    const int retainedScreenIndex = screenIndex >= 0
        ? screenIndex
        : panel->value(QStringLiteral("screen"), 0).toInt();
    const QString retainedScreenId = screenId.trimmed().isEmpty()
        ? panel->value(QStringLiteral("screenId")).toString()
        : screenId.trimmed();

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), retainedContainmentId},
         {QStringLiteral("freeDockAppletId"), retainedAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), retainedScreenIndex},
         {QStringLiteral("screenId"), retainedScreenId},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-stale")},
         {QStringLiteral("freeCreationState"),
          QString::fromLatin1(kFreeCreationRollbackPending)},
         {QStringLiteral("freeCreationError"), creationError.trimmed()},
         {QStringLiteral("freeRollbackError"), rollbackError.trimmed()}});
}

bool PanelRegistry::discardFreePanelCreation(
    const QString &panelId,
    const QString &ownershipToken)
{
    const QString normalizedToken = ownershipToken.trimmed();
    for (int index = 0; index < m_panels.size(); ++index)
    {
        const QVariantMap panel = m_panels.at(index);
        const QString creationState = panel.value(
            QStringLiteral("freeCreationState")).toString();
        if (panel.value(QStringLiteral("id")).toString() != panelId ||
            panel.value(QStringLiteral("builtIn")).toBool() ||
            panel.value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
            normalizedToken.isEmpty() ||
            panel.value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
            (creationState != QLatin1String(kFreeCreationPending) &&
             creationState != QLatin1String(kFreeCreationRollbackPending)))
        {
            continue;
        }

        const QString previousActivePanelId = m_activePanelId;
        m_panels.removeAt(index);
        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
        }
        if (!saveChecked())
        {
            m_panels.insert(index, panel);
            m_activePanelId = previousActivePanelId;
            return false;
        }

        if (m_activePanelId != previousActivePanelId)
        {
            emit activePanelIdChanged();
        }
        changed(false);
        return true;
    }
    return false;
}

bool PanelRegistry::rebindRecoveredFreeHostAssociation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        desktopContainmentId < 0 || dockAppletId < 0 || screenIndex < 0 ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), desktopContainmentId},
         {QStringLiteral("freeDockAppletId"), dockAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), screenIndex},
         {QStringLiteral("screenId"), screenId.trimmed()},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")},
         {QStringLiteral("freeRecoveryError"), QString{}}});
}

bool PanelRegistry::detachFreeHostAssociation(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &recoveryError)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), -1},
         {QStringLiteral("freeDockAppletId"), -1},
         {QStringLiteral("freeOwnershipToken"), QString{}},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("detached")},
         {QStringLiteral("freeRecoveryError"), recoveryError.trimmed()}});
}

bool PanelRegistry::recordFreeHostRecoveryError(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        normalizedToken.isEmpty() || normalizedError.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeHostState"), QStringLiteral("hosted-stale")},
         {QStringLiteral("freeRecoveryError"), normalizedError}});
}

bool PanelRegistry::removeDetachedFreePanel(const QString &panelId)
{
    for (int index = 0; index < m_panels.size(); ++index)
    {
        const QVariantMap panel = m_panels.at(index);
        if (panel.value(QStringLiteral("id")).toString() != panelId)
        {
            continue;
        }

        const auto association = freeHostAssociation(panelId);
        if (panel.value(QStringLiteral("builtIn")).toBool() ||
            panel.value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
            !association.has_value() || association->state != FreeHostState::Detached ||
            association->desktopContainmentId >= 0 || association->dockAppletId >= 0 ||
            !association->ownershipToken.trimmed().isEmpty())
        {
            return false;
        }

        const QString previousActivePanelId = m_activePanelId;
        m_panels.removeAt(index);
        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
        }
        if (!saveChecked())
        {
            m_panels.insert(index, panel);
            m_activePanelId = previousActivePanelId;
            return false;
        }

        if (m_activePanelId != previousActivePanelId)
        {
            emit activePanelIdChanged();
        }
        changed(false);
        return true;
    }
    return false;
}

bool PanelRegistry::commitVerifiedNativePanelAssociation(
    const QString &panelId,
    int containmentId,
    int dockAppletId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        containmentId < 0 || normalizedToken.isEmpty())
    {
        return false;
    }

    const QString type = panel->value(QStringLiteral("type")).toString();
    if (!isPanelType(type) ||
        (type == QStringLiteral("empty") ? dockAppletId != -1 : dockAppletId < 0))
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), containmentId},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), dockAppletId},
         {QStringLiteral("nativeOwnershipToken"), normalizedToken},
         {QStringLiteral("nativeRecoveryState"), QStringLiteral("ready")},
         {QStringLiteral("nativeRecoveryError"), QString{}}});
}

bool PanelRegistry::rebindRecoveredNativePanelAssociation(
    const QString &panelId,
    int containmentId,
    int dockAppletId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        containmentId < 0 || dockAppletId < -1 || normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    const QString type = panel->value(QStringLiteral("type")).toString();
    if (!isPanelType(type) || (type == QStringLiteral("empty") && dockAppletId != -1))
    {
        return false;
    }

    const bool rendererReady = type == QStringLiteral("empty") || dockAppletId >= 0;
    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), containmentId},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), dockAppletId},
         {QStringLiteral("nativeOwnershipToken"), normalizedToken},
         {QStringLiteral("nativeRecoveryState"), rendererReady
              ? QStringLiteral("ready")
              : QStringLiteral("recovering")},
         {QStringLiteral("nativeRecoveryError"), QString{}}});
}

bool PanelRegistry::detachMissingNativePanelAssociation(
    const QString &panelId,
    const QString &ownershipToken,
    bool recordVisible)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), -1},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), QString{}},
         {QStringLiteral("nativeRecoveryState"), recordVisible
              ? QStringLiteral("recovering")
              : QStringLiteral("detached")},
         {QStringLiteral("nativeRecoveryError"), QStringLiteral("owned-host-not-found")}});
}

bool PanelRegistry::recordNativePanelRecoveryConflict(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedToken.isEmpty() || normalizedError.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativeRecoveryState"), QStringLiteral("conflict")},
         {QStringLiteral("nativeRecoveryError"), normalizedError}});
}

bool PanelRegistry::recordNativePanelRecoveryFailure(
    const QString &panelId,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedError.isEmpty())
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), -1},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), QString{}},
         {QStringLiteral("nativeRecoveryState"), QStringLiteral("recoverable-error")},
         {QStringLiteral("nativeRecoveryError"), normalizedError}});
}

QVariantList PanelRegistry::themeDefinitions() const
{
    return m_themeDefinitions;
}

QVariantList PanelRegistry::iconStyleDefinitions() const
{
    return m_iconStyleStore.has_value()
        ? m_iconStyleStore->catalogEntries() : QVariantList{};
}

QVariantList PanelRegistry::animationProfileDefinitions() const
{
    return m_animationProfileCatalog.has_value()
        ? m_animationProfileCatalog->profileProjections() : QVariantList{};
}

QVariantMap PanelRegistry::animationProfileResolution(
    const QString &requestedProfileId) const
{
    if (!m_animationProfileCatalog.has_value())
    {
        return {
            {QStringLiteral("errorCode"), m_animationProfileCatalogError},
            {QStringLiteral("fallbackApplied"), true},
            {QStringLiteral("profile"), QVariantMap{}},
            {QStringLiteral("profileId"), QString{}},
            {QStringLiteral("requestedProfileId"), requestedProfileId},
            {QStringLiteral("resolved"), false},
        };
    }
    return m_animationProfileCatalog->resolve(requestedProfileId);
}

QVariantMap PanelRegistry::iconStyleDefinition(const QString &styleId) const
{
    if (!m_iconStyleStore.has_value())
    {
        return {
            {QStringLiteral("diagnostics"), QVariantList{}},
            {QStringLiteral("errorCode"), m_iconStyleStoreError},
            {QStringLiteral("loadable"), false},
            {QStringLiteral("requestedStyleId"), styleId.trimmed()},
            {QStringLiteral("selectionStatus"), QStringLiteral("invalid")},
            {QStringLiteral("valid"), false},
        };
    }
    return m_iconStyleStore->resolve(styleId);
}

QVariantMap PanelRegistry::resolveIconEntryOverride(
    const ArchDock::PanelDefinition &definition,
    const QVariantMap &entry) const
{
    return ArchDock::IconOverrideTransaction::resolve(
        definition,
        ArchDock::IconEntryIdentity::forEntry(entry),
        entry.value(
            QStringLiteral("baseIconName"),
            entry.value(QStringLiteral("iconName"))).toString(),
        entry.value(
            QStringLiteral("baseDisplayName"),
            entry.value(QStringLiteral("displayName"))).toString(),
        [this](const QString &styleReference)
        {
            return m_iconStyleStore.has_value()
                ? m_iconStyleStore->resolve(styleReference)
                : QVariantMap{
                    {QStringLiteral("errorCode"), m_iconStyleStoreError},
                    {QStringLiteral("loadable"), false},
                    {QStringLiteral("valid"), false},
                };
        });
}

std::optional<QVariantMap> PanelRegistry::iconStyleRuntimeProjection(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    if (!m_iconStyleStore.has_value())
    {
        if (errorCode)
        {
            *errorCode = m_iconStyleStoreError.isEmpty()
                ? QStringLiteral("icon-style-store-unavailable")
                : m_iconStyleStoreError;
        }
        return std::nullopt;
    }
    QVariantMap projection = m_iconStyleStore->resolve(
        definition.iconStyle.styleReference);
    if (!projection.value(QStringLiteral("valid")).toBool())
    {
        if (errorCode)
        {
            *errorCode = QStringLiteral("icon-style-fallback-unavailable");
        }
        return std::nullopt;
    }
    if (errorCode)
    {
        errorCode->clear();
    }
    return projection;
}

QVariantMap PanelRegistry::themeCandidate(const QString &panelId,
                                          const QString &themeId,
                                          const QString &layer) const
{
    const QString normalizedLayer = layer.trimmed().toLower();
    if (!record(panelId) || (normalizedLayer != QStringLiteral("panel") &&
                             normalizedLayer != QStringLiteral("icon") &&
                             normalizedLayer != QStringLiteral("complete")))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("validation-failed")},
            {QStringLiteral("errorCode"), record(panelId)
                 ? QStringLiteral("invalid-theme-layer")
                 : QStringLiteral("panel-not-found")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("themeId"), themeId},
            {QStringLiteral("layer"), normalizedLayer},
            {QStringLiteral("values"), QVariantMap{}},
        };
    }

    for (const QVariant &candidate : m_themeDefinitions)
    {
        const QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() != themeId)
            continue;

        if (theme.contains(QStringLiteral("packageManifest")))
        {
            QString projectionError;
            if (!builtInThemeRuntimeProjection(
                    themeId, &projectionError).has_value())
            {
                return {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("status"), QStringLiteral("validation-failed")},
                    {QStringLiteral("errorCode"), projectionError.isEmpty()
                         ? QStringLiteral("theme-package-unavailable")
                         : projectionError},
                    {QStringLiteral("panelId"), panelId},
                    {QStringLiteral("themeId"), themeId},
                    {QStringLiteral("layer"), normalizedLayer},
                    {QStringLiteral("values"), QVariantMap{}},
                };
            }
        }

        QVariantMap changes;
        const auto mergeStyle = [&changes](const QVariantMap &style)
        {
            for (auto it = style.cbegin(); it != style.cend(); ++it)
                changes.insert(it.key(), it.value());
        };
        if (normalizedLayer == QStringLiteral("panel") || normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("panelStyle")).toMap());
            changes.insert(QStringLiteral("panelThemeId"), themeId);
        }
        if (normalizedLayer == QStringLiteral("icon") || normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("iconStyle")).toMap());
        }
        if (normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("tileStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("indicatorStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("animationStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("layoutStyle")).toMap());
            changes.insert(QStringLiteral("completeThemeId"), themeId);
        }

        QVariantMap normalizedChanges;
        for (auto iterator = changes.cbegin(); iterator != changes.cend(); ++iterator)
        {
            const ArchDock::PanelSettingsFieldDescriptor *field =
                ArchDock::PanelSettingsSchema::panelDescriptor(iterator.key());
            if (!field || field->access != ArchDock::PanelSettingsFieldAccess::Editor)
            {
                return {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("status"), QStringLiteral("validation-failed")},
                    {QStringLiteral("errorCode"), QStringLiteral("theme-field-unavailable")},
                    {QStringLiteral("errorMessage"), iterator.key()},
                    {QStringLiteral("panelId"), panelId},
                    {QStringLiteral("themeId"), themeId},
                    {QStringLiteral("layer"), normalizedLayer},
                    {QStringLiteral("values"), QVariantMap{}},
                };
            }
            normalizedChanges.insert(
                iterator.key(),
                ArchDock::PanelSettingsSchema::normalizePanelValue(
                    iterator.key(), iterator.value()));
        }

        QVariantMap candidateRecord = *record(panelId);
        for (auto iterator = normalizedChanges.cbegin();
             iterator != normalizedChanges.cend(); ++iterator)
        {
            candidateRecord.insert(iterator.key(), iterator.value());
        }
        QString definitionError;
        const std::optional<ArchDock::PanelDefinition> definition =
            ArchDock::PanelDefinition::fromLegacyMap(
                candidateRecord, &definitionError);
        QString capabilityError;
        const std::optional<ArchDock::ThemeCapabilityProfile> profile =
            ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                theme, &capabilityError);
        if (!definition.has_value() || !profile.has_value())
        {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("status"), QStringLiteral("validation-failed")},
                {QStringLiteral("errorCode"), !definition.has_value()
                     ? QStringLiteral("invalid-theme-candidate")
                     : QStringLiteral("invalid-theme-capabilities")},
                {QStringLiteral("errorMessage"), !definition.has_value()
                     ? definitionError : capabilityError},
                {QStringLiteral("panelId"), panelId},
                {QStringLiteral("themeId"), themeId},
                {QStringLiteral("layer"), normalizedLayer},
                {QStringLiteral("values"), QVariantMap{}},
            };
        }
        const ArchDock::CapabilityResolution resolution =
            ArchDock::PanelCapabilityResolver::resolve(
                *definition,
                ArchDock::PanelCapabilityResolver::productionHostProfile(
                    definition->host.kind),
                *profile,
                ArchDock::PanelCapabilityResolver::productionRenderers(),
                ArchDock::PanelCapabilityResolver::productionPlatform());
        if (!resolution.available)
        {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("status"), QStringLiteral("capability-unavailable")},
                {QStringLiteral("errorCode"),
                 ArchDock::capabilityReasonCodeName(resolution.reason)},
                {QStringLiteral("panelId"), panelId},
                {QStringLiteral("themeId"), themeId},
                {QStringLiteral("layer"), normalizedLayer},
                {QStringLiteral("values"), QVariantMap{}},
                {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
            };
        }
        return {
            {QStringLiteral("success"), true},
            {QStringLiteral("status"), QStringLiteral("resolved")},
            {QStringLiteral("errorCode"), QString{}},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("themeId"), themeId},
            {QStringLiteral("recommendedIconStyleId"),
             theme.value(QStringLiteral("iconStyleRef"))
                 .toMap().value(QStringLiteral("id"))},
            {QStringLiteral("layer"), normalizedLayer},
            {QStringLiteral("values"), normalizedChanges},
            {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        };
    }
    return {
        {QStringLiteral("success"), false},
        {QStringLiteral("status"), QStringLiteral("validation-failed")},
        {QStringLiteral("errorCode"), QStringLiteral("theme-not-found")},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("themeId"), themeId},
        {QStringLiteral("layer"), normalizedLayer},
        {QStringLiteral("values"), QVariantMap{}},
    };
}

bool PanelRegistry::applyTheme(const QString &panelId,
                               const QString &themeId,
                               const QString &layer)
{
    const QVariantMap candidate = themeCandidate(panelId, themeId, layer);
    if (!candidate.value(QStringLiteral("success")).toBool())
    {
        return false;
    }
    setPanelValues(panelId, candidate.value(QStringLiteral("values")).toMap());
    return true;
}

QString PanelRegistry::addPanel(const QString &edge, const QString &type)
{
    const QString normalizedEdge = edge.trimmed().toLower();
    if (!isEdge(normalizedEdge))
    {
        return {};
    }

    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("panel-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(
        id,
        tr("Panel %1").arg(sequence - 1),
        normalizedEdge,
        false);
    panel.insert(QStringLiteral("visible"), true);
    const QString normalizedType = normalizeValue(QStringLiteral("type"), type).toString();
    panel.insert(QStringLiteral("type"), normalizedType);
    if (normalizedType == QStringLiteral("empty"))
    {
        panel.insert(QStringLiteral("dynamic"), false);
    }
    (void)normalizeFreeHostRecord(&panel);
    m_panels.append(panel);
    m_activePanelId = id;
    save();
    emit activePanelIdChanged();
    changed(true);
    return id;
}

QString PanelRegistry::addFreePanel()
{
    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("free-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(id, tr("Free panel %1").arg(sequence - 1),
                                  QStringLiteral("bottom"), false);
    panel.insert(QStringLiteral("edge"), QStringLiteral("free"));
    panel.insert(QStringLiteral("visible"), true);
    panel.insert(QStringLiteral("type"), QStringLiteral("empty"));
    panel.insert(QStringLiteral("contentAppIds"), QStringList{});
    panel.insert(QStringLiteral("contentUrls"), QStringList{});
    panel.insert(QStringLiteral("layout"), QStringLiteral("circular"));
    panel.insert(QStringLiteral("width"), 420);
    panel.insert(QStringLiteral("height"), 420);
    panel.insert(QStringLiteral("layoutRadius"), 145);
    panel.insert(QStringLiteral("x"), 240);
    panel.insert(QStringLiteral("y"), 180);
    (void)normalizeFreeHostRecord(&panel);
    const QString previousActivePanelId = m_activePanelId;
    m_panels.append(panel);
    m_activePanelId = id;
    if (!saveChecked())
    {
        m_panels.removeLast();
        m_activePanelId = previousActivePanelId;
        return {};
    }
    if (m_activePanelId != previousActivePanelId)
    {
        emit activePanelIdChanged();
    }
    changed(false);
    return id;
}

void PanelRegistry::removePanel(const QString &panelId)
{
    for (int index = 0; index < m_panels.size(); ++index)
    {
        QVariantMap &panel = m_panels[index];
        if (panel.value(QStringLiteral("id")).toString() != panelId)
        {
            continue;
        }

        const bool nativePanel = panel.value(QStringLiteral("edge")).toString() !=
            QStringLiteral("free");
        if (panel.value(QStringLiteral("builtIn")).toBool())
        {
            panel.insert(QStringLiteral("visible"), false);
        }
        else
        {
            m_panels.removeAt(index);
        }

        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
            emit activePanelIdChanged();
        }
        save();
        changed(nativePanel);
        return;
    }
}

bool PanelRegistry::importTheme(const QString &panelId, const QUrl &sourceUrl)
{
    if (!record(panelId) || !sourceUrl.isLocalFile())
    {
        return false;
    }

    const QFileInfo source(sourceUrl.toLocalFile());
    if (!source.exists() || !source.isFile())
    {
        return false;
    }

    ArchDock::ThemePackageLoadResult loaded;
    if (source.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0)
    {
        loaded = ArchDock::ThemePackage::load(source.absoluteFilePath());
    }
    else
    {
        LegacyThemePackage legacy;
        legacy.id = normalizedThemePackageId(source.completeBaseName());
        legacy.name = source.completeBaseName();
        legacy.sourcePath = source.absoluteFilePath();
        legacy.fit = record(panelId)->value(
            QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
        if (!isThemeFit(legacy.fit))
        {
            legacy.fit = QStringLiteral("cover");
        }
        const QByteArray manifestBytes = QJsonDocument(
            legacyThemePackageManifest(legacy, source.fileName()))
                                             .toJson(QJsonDocument::Compact);
        loaded = ArchDock::ThemePackage::loadBytes(
            manifestBytes,
            source.absolutePath(),
            QDir(source.absolutePath()).filePath(themePackageManifestName));
    }
    if (!loaded.isValid())
    {
        const QString code = loaded.primaryCode().isEmpty()
            ? QStringLiteral("invalid-value") : loaded.primaryCode();
        const QString message = loaded.primaryMessage().isEmpty()
            ? tr("Theme package validation failed.") : loaded.primaryMessage();
        setPanelValues(
            panelId,
            {{QStringLiteral("themeStatus"),
              tr("Theme package rejected [%1]: %2").arg(code, message)}});
        return false;
    }

    QString capabilityError;
    const std::optional<ArchDock::ThemeCapabilityProfile> themeProfile =
        ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
            loaded.package->definition().toVariantMap(), &capabilityError);
    std::optional<ArchDock::PanelDefinition> capabilityDefinition =
        panelDefinition(panelId, &capabilityError);
    if (!themeProfile.has_value() || !capabilityDefinition.has_value())
    {
        setPanelValues(
            panelId,
            {{QStringLiteral("themeStatus"),
              tr("Theme package rejected [%1].")
                  .arg(capabilityError.isEmpty()
                           ? QStringLiteral("invalid-capability-input")
                           : capabilityError)}});
        return false;
    }
    capabilityDefinition->surface.themeSource = QUrl::fromLocalFile(
        loaded.package->primarySurfacePath()).toString();
    capabilityDefinition->surface.themePackageFormat = themePackageFormat;
    capabilityDefinition->surface.themePackageVersion = loaded.package->sourceVersion();
    capabilityDefinition->surface.themePackageId = loaded.package->definition().id;
    const ArchDock::CapabilityResolution capabilityResolution =
        ArchDock::PanelCapabilityResolver::resolve(
            *capabilityDefinition,
            ArchDock::PanelCapabilityResolver::productionHostProfile(
                capabilityDefinition->host.kind),
            *themeProfile,
            ArchDock::PanelCapabilityResolver::productionRenderers(),
            ArchDock::PanelCapabilityResolver::productionPlatform());
    if (!capabilityResolution.available)
    {
        setPanelValues(
            panelId,
            {{QStringLiteral("themeStatus"),
              tr("Theme package is incompatible [%1]; the active theme was kept.")
                  .arg(ArchDock::capabilityReasonCodeName(
                      capabilityResolution.reason))}});
        return false;
    }

    const QString managedRoot = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/themes/") + panelId +
        QStringLiteral("/packages");
    ArchDock::ThemePackageMaterializeResult materialized =
        loaded.package->materialize(managedRoot);
    if (!materialized.isValid())
    {
        const QString code = materialized.diagnostics.isEmpty()
            ? QStringLiteral("invalid-value")
            : materialized.diagnostics.first().code;
        const QString message = materialized.diagnostics.isEmpty()
            ? tr("Theme package could not be installed.")
            : materialized.diagnostics.first().message;
        setPanelValues(
            panelId,
            {{QStringLiteral("themeStatus"),
              tr("Theme package installation failed [%1]: %2").arg(code, message)}});
        return false;
    }
    const ArchDock::ThemePackage &managed = *materialized.package;

    const QVariantMap *currentPanel = record(panelId);
    const QSize targetSize(
        currentPanel ? currentPanel->value(QStringLiteral("width"), 720).toInt() : 720,
        currentPanel ? currentPanel->value(QStringLiteral("height"), 76).toInt() : 76);
    const QString managedSource = managed.primarySurfacePath();
    QVariantMap analysis = analyzeThemeSource(
        managedSource, managed.manifestPath(), targetSize);
    if (managedSource.isEmpty())
    {
        analysis.insert(QStringLiteral("themeSourceKind"), QString{});
        analysis.insert(QStringLiteral("themeSourceFormat"), QString{});
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            tr("No 2D surface is declared; the resolved safe renderer remains active."));
    }
    const ArchDock::ThemeDefinition &theme = managed.definition();
    QString fit = managed.sourceVersion() == 1
        ? managed.legacyFit()
        : (currentPanel
               ? currentPanel->value(
                     QStringLiteral("themeFit"), QStringLiteral("cover")).toString()
               : QStringLiteral("cover"));
    if (!isThemeFit(fit))
    {
        fit = QStringLiteral("cover");
    }
    QVariantMap values{
        {QStringLiteral("panelThemeId"), QString{}},
        {QStringLiteral("completeThemeId"), QString{}},
        {QStringLiteral("rendererTier"), QString{}},
        {QStringLiteral("themeSource"), managedSource.isEmpty()
             ? QString{} : QUrl::fromLocalFile(managedSource).toString()},
        {QStringLiteral("themeAsset"), QString{}},
        {QStringLiteral("themeFit"), fit},
        {QStringLiteral("themePackageFormat"), themePackageFormat},
        {QStringLiteral("themePackageVersion"), managed.sourceVersion()},
        {QStringLiteral("themePackageId"), theme.id},
        {QStringLiteral("themePackageName"), theme.name},
        {QStringLiteral("themePackageAuthor"), theme.author},
        {QStringLiteral("themePackageManifest"),
         QUrl::fromLocalFile(managed.manifestPath()).toString()},
        {QStringLiteral("themeStatus"), managedSource.isEmpty()
             ? tr("Theme package \"%1\" imported; safe renderer fallback active.")
                   .arg(theme.name)
             : tr("Theme package \"%1\" imported; preparing panel skin.")
                   .arg(theme.name)},
        {QStringLiteral("themeRenderWidth"), 0},
        {QStringLiteral("themeRenderHeight"), 0},
        {QStringLiteral("themeRenderFit"), QString{}},
        {QStringLiteral("themeRenderOutcome"), managedSource.isEmpty()
             ? QStringLiteral("fallback") : QString{}}};
    for (auto iterator = analysis.cbegin(); iterator != analysis.cend(); ++iterator)
    {
        values.insert(iterator.key(), iterator.value());
    }

    setPanelValues(
        panelId,
        values);

    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return false;
    }

    if (!managedSource.isEmpty())
    {
        renderTheme(
            panelId,
            panel->value(QStringLiteral("width"), 720).toInt(),
            panel->value(QStringLiteral("height"), 76).toInt());
    }
    return true;
}

bool PanelRegistry::renderTheme(const QString &panelId,
                                int width,
                                int height,
                                qreal devicePixelRatio,
                                bool force)
{
    const RenderRequest request = makeRenderRequest(panelId, width, height, devicePixelRatio);
    if (request.panelId.isEmpty())
    {
        return false;
    }

    const QVariantMap *panel = record(panelId);
    const QUrl currentAsset(panel ? panel->value(QStringLiteral("themeAsset")).toString() : QString{});
    const bool targetMatches = panel &&
        panel->value(QStringLiteral("themeRenderWidth")).toInt() == request.width &&
        panel->value(QStringLiteral("themeRenderHeight")).toInt() == request.height &&
        panel->value(QStringLiteral("themeRenderFit")).toString() == request.fit;
    const QString outcome = panel
        ? panel->value(QStringLiteral("themeRenderOutcome")).toString()
        : QString{};
    const bool assetMatches = outcome == QStringLiteral("ready") &&
        QFileInfo(currentAsset.toLocalFile()).exists();
    if (!force && targetMatches && (assetMatches || outcome == QStringLiteral("error")))
    {
        return true;
    }

    const auto matches = [](const RenderRequest &left, const RenderRequest &right)
    {
        return left.panelId == right.panelId &&
            left.sourcePath == right.sourcePath &&
            left.outputPath == right.outputPath &&
            left.fit == right.fit &&
            left.width == right.width &&
            left.height == right.height;
    };
    const auto active = m_activeRenders.constFind(panelId);
    if (active != m_activeRenders.cend())
    {
        if (!matches(active.value(), request))
        {
            m_pendingRenders.insert(panelId, request);
        }
        return true;
    }

    m_activeRenders.insert(panelId, request);
    startRender(request);
    return true;
}

void PanelRegistry::clearTheme(const QString &panelId)
{
    if (!record(panelId))
    {
        return;
    }

    m_pendingRenders.remove(panelId);
    m_activeRenders.remove(panelId);
    setPanelValues(
        panelId,
        {{QStringLiteral("themeSource"), QString{}},
         {QStringLiteral("themeAsset"), QString{}},
         {QStringLiteral("themeSourceKind"), QString{}},
         {QStringLiteral("themeSourceFormat"), QString{}},
         {QStringLiteral("themeSourceWidth"), 0},
         {QStringLiteral("themeSourceHeight"), 0},
         {QStringLiteral("themeSourceHasAlpha"), false},
         {QStringLiteral("themeSuggestedFit"), QStringLiteral("cover")},
         {QStringLiteral("themePreview"), QString{}},
         {QStringLiteral("themeAnalysisStatus"), tr("No source selected.")},
         {QStringLiteral("themeConversionTool"), QString{}},
         {QStringLiteral("themeConversionAvailable"), false},
         {QStringLiteral("themePackageFormat"), QString{}},
         {QStringLiteral("themePackageVersion"), 0},
         {QStringLiteral("themePackageId"), QString{}},
         {QStringLiteral("themePackageName"), QString{}},
         {QStringLiteral("themePackageAuthor"), QString{}},
         {QStringLiteral("themePackageManifest"), QString{}},
         {QStringLiteral("themeStatus"), tr("Preset surface active.")},
         {QStringLiteral("themeRenderWidth"), 0},
         {QStringLiteral("themeRenderHeight"), 0},
         {QStringLiteral("themeRenderFit"), QString{}},
         {QStringLiteral("themeRenderOutcome"), QString{}}});
}

void PanelRegistry::setActivePanelId(const QString &panelId)
{
    if (panelId == m_activePanelId || !record(panelId))
    {
        return;
    }

    m_activePanelId = panelId;
    QSettings settings;
    settings.setValue(QStringLiteral("dock/activePanel"), m_activePanelId);
    settings.sync();
    emit activePanelIdChanged();
}

QVariantMap *PanelRegistry::record(const QString &panelId)
{
    for (QVariantMap &panel : m_panels)
    {
        if (panel.value(QStringLiteral("id")).toString() == panelId)
        {
            return &panel;
        }
    }
    return nullptr;
}

const QVariantMap *PanelRegistry::record(const QString &panelId) const
{
    for (const QVariantMap &panel : m_panels)
    {
        if (panel.value(QStringLiteral("id")).toString() == panelId)
        {
            return &panel;
        }
    }
    return nullptr;
}

QVariantMap PanelRegistry::makePanel(const QString &id,
                                     const QString &name,
                                     const QString &edge,
                                     bool builtIn) const
{
    return ArchDock::PanelDefinition::defaults(id, name, edge, builtIn).toLegacyMap();
}

QVariant PanelRegistry::normalizeValue(const QString &key, const QVariant &value) const
{
    return ArchDock::PanelSettingsSchema::normalizePanelValue(key, value);
}

PanelRegistry::RenderRequest PanelRegistry::makeRenderRequest(const QString &panelId,
                                                               int width,
                                                               int height,
                                                               qreal devicePixelRatio) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return {};
    }

    const QUrl sourceUrl(panel->value(QStringLiteral("themeSource")).toString());
    const QString sourcePath = sourceUrl.toLocalFile();
    const QFileInfo source(sourcePath);
    if (sourcePath.isEmpty() || !source.exists() || !source.isFile())
    {
        return {};
    }

    qreal scale = devicePixelRatio;
    if (scale <= 0)
    {
        if (QScreen *screen = QGuiApplication::primaryScreen())
        {
            scale = screen->devicePixelRatio();
        }
    }
    scale = qBound<qreal>(1.0, scale, 4.0);

    RenderRequest request;
    request.panelId = panelId;
    request.sourcePath = source.absoluteFilePath();
    request.fit = panel->value(QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
    request.width = qBound(64, qRound(qMax(1, width) * scale), 4096);
    request.height = qBound(64, qRound(qMax(1, height) * scale), 4096);
    const QString themeDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/themes/") + panelId;
    QDir().mkpath(themeDirectory);
    const QByteArray identity = (request.sourcePath + QLatin1Char('|') +
        QString::number(source.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') +
        request.fit).toUtf8();
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(12));
    request.outputPath = themeDirectory + QStringLiteral("/skin-%1-%2x%3-%4.png")
        .arg(fingerprint)
        .arg(request.width)
        .arg(request.height)
        .arg(request.fit);
    return request;
}

bool PanelRegistry::renderWithQt(RenderRequest *request, QString *errorMessage) const
{
    if (!request)
    {
        if (errorMessage)
        {
            *errorMessage = tr("The panel skin request is missing.");
        }
        return false;
    }

    ArchDock::ThemeAssetProcessingRequest processingRequest;
    processingRequest.sourcePath = request->sourcePath;
    processingRequest.managedOutputRoot =
        QFileInfo(request->outputPath).absolutePath() +
        QStringLiteral("/processed-renders");
    processingRequest.assetId = QStringLiteral("render-") +
        processorAssetId(request->panelId);
    processingRequest.previewBounds = QSize(320, 180);
    processingRequest.targetSize = QSize(request->width, request->height);
    processingRequest.scaleFactors = {1.0};
    processingRequest.fit = request->fit;
    processingRequest.cleanupRequirements = {QStringLiteral("none")};
    const ArchDock::ThemeAssetProcessingResult processed =
        ArchDock::ThemeAssetProcessor::process(processingRequest);
    if (!processed.isValid())
    {
        if (errorMessage)
        {
            *errorMessage = tr("Bounded panel rendering failed: %1").arg(
                processorFailureMessage(
                    processed,
                    tr("Qt could not create a verified panel derivative.")));
        }
        return false;
    }

    const auto variant = std::find_if(
        processed.derivatives.cbegin(), processed.derivatives.cend(),
        [](const ArchDock::ThemeAssetDerivative &derivative)
        {
            return derivative.id == QStringLiteral("scale-1000") &&
                derivative.role == QStringLiteral("variant");
        });
    if (variant == processed.derivatives.cend() ||
        !QFileInfo(variant->absolutePath).isFile())
    {
        if (errorMessage)
        {
            *errorMessage = tr("The bounded processor did not publish the requested panel derivative.");
        }
        return false;
    }
    request->outputPath = variant->absolutePath;
    return true;
}

void PanelRegistry::setPanelValues(const QString &panelId, const QVariantMap &values)
{
    (void)setPanelValuesChecked(panelId, values);
}

bool PanelRegistry::setPanelValuesChecked(const QString &panelId, const QVariantMap &values)
{
    QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return false;
    }

    const QVariantMap previousValues = *panel;
    QVariantMap candidate = previousValues;
    const bool wasNativePanel = previousValues.value(QStringLiteral("edge")).toString() !=
        QStringLiteral("free");
    bool contentOnly = true;
    bool topologyChanged = false;
    static const QSet<QString> topologyKeys{
        QStringLiteral("visible"),
        QStringLiteral("edge"),
        QStringLiteral("alignment"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("visibilityMode"),
        QStringLiteral("revealZone"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("type")};
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator)
    {
        if (iterator.key() == QStringLiteral("id") ||
            iterator.key() == QStringLiteral("builtIn") ||
            iterator.key() == QStringLiteral("schemaVersion") ||
            iterator.key() == QStringLiteral("settingsRevision"))
        {
            continue;
        }

        const QVariant normalized = normalizeValue(iterator.key(), iterator.value());
        if (candidate.value(iterator.key()) == normalized)
        {
            continue;
        }
        candidate.insert(iterator.key(), normalized);
        topologyChanged = topologyChanged || topologyKeys.contains(iterator.key());
        if (iterator.key() != QStringLiteral("contentUrls") &&
            iterator.key() != QStringLiteral("contentAppIds"))
        {
            contentOnly = false;
        }
    }
    if (normalizeFreeHostRecord(&candidate))
    {
        contentOnly = false;
    }
    if (values.contains(QStringLiteral("edge")))
    {
        candidate.insert(
            QStringLiteral("hostKind"),
            candidate.value(QStringLiteral("edge")).toString() == QStringLiteral("free")
                ? QStringLiteral("free-desktop")
                : QStringLiteral("native-edge"));
    }

    QString definitionError;
    const std::optional<ArchDock::PanelDefinition> definition =
        ArchDock::PanelDefinition::fromLegacyMap(candidate, &definitionError);
    if (!definition.has_value())
    {
        qWarning().noquote() << "Panel update rejected:" << definitionError;
        return false;
    }

    candidate = definition->toLegacyMap();
    if (candidate == previousValues)
    {
        return true;
    }

    ArchDock::PanelDefinition revisedDefinition = *definition;
    if (revisedDefinition.settingsRevision == std::numeric_limits<quint64>::max())
    {
        qWarning() << "Panel update rejected: settings revision exhausted for" << panelId;
        return false;
    }
    revisedDefinition.settingsRevision += 1;
    candidate = revisedDefinition.toLegacyMap();

    *panel = candidate;
    if (!saveChecked())
    {
        *panel = previousValues;
        return false;
    }
    ++m_revision;
    if (!contentOnly)
    {
        emit panelsChanged();
    }
    const bool isNativePanel = panel->value(QStringLiteral("edge")).toString() !=
        QStringLiteral("free");
    if (topologyChanged && (wasNativePanel || isNativePanel))
    {
        emit nativePanelTopologyChanged();
    }
    emit revisionChanged();
    return true;
}

void PanelRegistry::startRender(const RenderRequest &request)
{
    setPanelValues(
        request.panelId,
        {{QStringLiteral("themeStatus"), tr("Rendering %1 x %2 panel skin...")
            .arg(request.width)
            .arg(request.height)},
         {QStringLiteral("themeRenderOutcome"), QStringLiteral("rendering")}});

    if (QFileInfo(request.sourcePath).suffix().compare(
            QStringLiteral("blend"), Qt::CaseInsensitive) == 0)
    {
        finishRender(
            request,
            false,
            tr("The .blend source is retained for offline review; Arch Dock does not execute external scene converters."));
        return;
    }

    RenderRequest completedRequest = request;
    QString error;
    const bool success = renderWithQt(&completedRequest, &error);
    finishRender(
        completedRequest,
        success,
        success ? tr("Verified bounded Qt panel derivative is ready.") : error);
}

void PanelRegistry::finishRender(const RenderRequest &request, bool success, const QString &message)
{
    m_activeRenders.remove(request.panelId);
    const QVariantMap *panel = record(request.panelId);
    const QString currentSource = panel
        ? QUrl(panel->value(QStringLiteral("themeSource")).toString()).toLocalFile()
        : QString{};
    if (currentSource == request.sourcePath)
    {
        if (success)
        {
            setPanelValues(
                request.panelId,
                {{QStringLiteral("themeAsset"), QUrl::fromLocalFile(request.outputPath).toString()},
                 {QStringLiteral("themeStatus"), message},
                 {QStringLiteral("themeRenderWidth"), request.width},
                 {QStringLiteral("themeRenderHeight"), request.height},
                 {QStringLiteral("themeRenderFit"), request.fit},
                 {QStringLiteral("themeRenderOutcome"), QStringLiteral("ready")}});
        }
        else
        {
            setPanelValues(
                request.panelId,
                {{QStringLiteral("themeStatus"), message.isEmpty()
                    ? tr("This design file could not be rendered.") : message},
                 {QStringLiteral("themeRenderWidth"), request.width},
                 {QStringLiteral("themeRenderHeight"), request.height},
                 {QStringLiteral("themeRenderFit"), request.fit},
                 {QStringLiteral("themeRenderOutcome"), QStringLiteral("error")}});
        }
    }

    const auto pending = m_pendingRenders.take(request.panelId);
    if (!pending.panelId.isEmpty() && pending.sourcePath != currentSource)
    {
        return;
    }
    if (!pending.panelId.isEmpty())
    {
        m_activeRenders.insert(pending.panelId, pending);
        startRender(pending);
    }
}

void PanelRegistry::load()
{
    QSettings settings;
    const int legacyScreen = qMax(0, settings.value(QStringLiteral("dock/monitorIndex"), 0).toInt());
    const bool legacyAutoHide = settings.value(QStringLiteral("dock/autoHide"), false).toBool();
    const QString panelRecordsKey = QString::fromLatin1(kPanelRecordsKey);
    const bool hasStoredPanels = settings.contains(panelRecordsKey);
    const QByteArray storedPanels = settings.value(panelRecordsKey).toByteArray();
    bool initializeBuiltIns = !hasStoredPanels;
    bool compatibilityRewriteRequired = false;

    if (hasStoredPanels && storedPanels.isEmpty())
    {
        m_persistenceBlocked = true;
        setMigrationDiagnostic(QStringLiteral(
            "invalid-json: stored panel registry is empty"));
        qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
        return;
    }

    if (hasStoredPanels)
    {
        const ArchDock::PanelMigrationResult migration =
            ArchDock::SettingsMigration::migratePanelRecords(storedPanels);
        if (migration.status != ArchDock::PanelMigrationStatus::Success)
        {
            m_persistenceBlocked = true;
            setMigrationDiagnostic(
                ArchDock::SettingsMigration::statusName(migration.status) +
                QStringLiteral(": ") + migration.diagnostic);
            qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
            return;
        }

        m_legacySource = storedPanels;
        m_legacyRewritePending = migration.rewriteRequired;
        const QJsonArray sourceRecords = QJsonDocument::fromJson(storedPanels).array();
        for (qsizetype index = 0; index < migration.definitions.size(); ++index)
        {
            QVariantMap panel = migration.definitions.at(index).toLegacyMap();
            const QVariantMap sourceRecord = sourceRecords.at(index).toObject().toVariantMap();
            if (!sourceRecord.contains(QStringLiteral("screen")))
            {
                panel.insert(QStringLiteral("screen"), legacyScreen);
                compatibilityRewriteRequired = true;
            }
            if (!sourceRecord.contains(QStringLiteral("visibilityMode")))
            {
                panel.insert(
                    QStringLiteral("visibilityMode"),
                    panel.value(QStringLiteral("id")).toString() == QStringLiteral("bottom") && legacyAutoHide
                        ? QStringLiteral("auto-hide")
                        : QStringLiteral("always"));
                compatibilityRewriteRequired = true;
            }
            compatibilityRewriteRequired = normalizeFreeHostRecord(&panel) ||
                compatibilityRewriteRequired;
            compatibilityRewriteRequired = migrateLegacyThemePackage(&panel) ||
                compatibilityRewriteRequired;

            QString definitionError;
            const std::optional<ArchDock::PanelDefinition> normalized =
                ArchDock::PanelDefinition::fromLegacyMap(panel, &definitionError);
            if (!normalized.has_value())
            {
                m_panels.clear();
                m_persistenceBlocked = true;
                setMigrationDiagnostic(
                    QStringLiteral("invalid-record: compatibility migration failed: ") +
                    definitionError);
                qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
                return;
            }
            m_panels.append(normalized->toLegacyMap());
        }

        if (m_panels.isEmpty())
        {
            initializeBuiltIns = true;
            m_legacyRewritePending = true;
        }
    }

    if (initializeBuiltIns)
    {
        const bool suiteVisible = settings.value(QStringLiteral("dock/desktopSuite"), false).toBool();
        QVariantMap bottom = makePanel(QStringLiteral("bottom"), tr("Bottom panel"), QStringLiteral("bottom"), true);
        bottom.insert(QStringLiteral("visible"), settings.value(QStringLiteral("dock/bottomPanelVisible"), true).toBool());
        bottom.insert(QStringLiteral("edge"), normalizeValue(
            QStringLiteral("edge"), settings.value(QStringLiteral("dock/position"), QStringLiteral("bottom"))));
        bottom.insert(QStringLiteral("alignment"), normalizeValue(
            QStringLiteral("alignment"), settings.value(QStringLiteral("dock/alignment"), QStringLiteral("center"))));
        bottom.insert(QStringLiteral("screen"), legacyScreen);
        bottom.insert(
            QStringLiteral("visibilityMode"),
            legacyAutoHide ? QStringLiteral("auto-hide") : QStringLiteral("always"));
        bottom.insert(QStringLiteral("type"), normalizeValue(
            QStringLiteral("type"), settings.value(QStringLiteral("dock/bottomPanelType"), QStringLiteral("hybrid"))));
        bottom.insert(QStringLiteral("iconSize"), normalizeValue(
            QStringLiteral("iconSize"), settings.value(QStringLiteral("dock/iconSize"), 52)));
        bottom.insert(QStringLiteral("spacing"), normalizeValue(
            QStringLiteral("spacing"), settings.value(QStringLiteral("dock/spacing"), 8.0)));
        bottom.insert(QStringLiteral("appearance"), normalizeValue(
            QStringLiteral("appearance"), settings.value(QStringLiteral("dock/appearancePreset"), QStringLiteral("glass"))));
        bottom.insert(QStringLiteral("opacity"), normalizeValue(
            QStringLiteral("opacity"), settings.value(QStringLiteral("dock/panelOpacity"), 0.9)));
        m_panels.append(bottom);

        QVariantMap top = makePanel(QStringLiteral("top"), tr("Top panel"), QStringLiteral("top"), true);
        top.insert(QStringLiteral("visible"), suiteVisible && settings.value(QStringLiteral("dock/topLauncherVisible"), true).toBool());
        top.insert(QStringLiteral("type"), settings.value(QStringLiteral("dock/topPanelType"), QStringLiteral("hybrid")).toString());
        top.insert(QStringLiteral("screen"), legacyScreen);
        m_panels.append(top);

        QVariantMap side = makePanel(QStringLiteral("side"), tr("Side panel"), QStringLiteral("right"), true);
        side.insert(QStringLiteral("visible"), suiteVisible && settings.value(QStringLiteral("dock/sideRailVisible"), true).toBool());
        side.insert(QStringLiteral("type"), settings.value(QStringLiteral("dock/sidePanelType"), QStringLiteral("hybrid")).toString());
        side.insert(QStringLiteral("screen"), legacyScreen);
        m_panels.append(side);
        compatibilityRewriteRequired = true;
    }

    const QString requestedActive = settings.value(QStringLiteral("dock/activePanel"), m_activePanelId).toString();
    if (record(requestedActive))
    {
        m_activePanelId = requestedActive;
    }

    if (m_legacyRewritePending || compatibilityRewriteRequired)
    {
        (void)saveChecked();
    }
}

void PanelRegistry::save()
{
    (void)saveChecked();
}

bool PanelRegistry::saveChecked()
{
    if (m_persistenceBlocked)
    {
        if (m_migrationDiagnostic.isEmpty())
        {
            setMigrationDiagnostic(QStringLiteral(
                "persistence-blocked: panel registry source requires repair"));
        }
        return false;
    }

    QString serializationError;
    const QByteArray serialized = serializePanels(&serializationError);
    if (!serializationError.isEmpty())
    {
        setMigrationDiagnostic(QStringLiteral("serialization-failed: ") + serializationError);
        qWarning().noquote() << "Panel registry save blocked:" << m_migrationDiagnostic;
        return false;
    }

    QSettings settings;
    if (m_legacyRewritePending && !ensureLegacyBackupChecked(settings))
    {
        return false;
    }

    settings.setValue(QString::fromLatin1(kPanelRecordsKey), serialized);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        setMigrationDiagnostic(QStringLiteral(
            "write-failed: QSettings could not persist panel registry schema v2"));
        qWarning().noquote() << "Panel registry save failed:" << m_migrationDiagnostic;
        return false;
    }

    m_legacySource.clear();
    m_legacyRewritePending = false;
    setMigrationDiagnostic(QString{});
    return true;
}

bool PanelRegistry::ensureLegacyBackupChecked(QSettings &settings)
{
    const QString backupKey = QString::fromLatin1(kPanelLegacyBackupKey);
    if (settings.contains(backupKey))
    {
        if (settings.value(backupKey).toByteArray() == m_legacySource)
        {
            return true;
        }
        setMigrationDiagnostic(QStringLiteral(
            "backup-conflict: existing legacy panel backup does not match the source"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }

    settings.setValue(backupKey, m_legacySource);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        setMigrationDiagnostic(QStringLiteral(
            "backup-write-failed: QSettings could not persist the legacy panel source"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }

    QSettings verifier;
    verifier.sync();
    if (verifier.status() != QSettings::NoError ||
        !verifier.contains(backupKey) ||
        verifier.value(backupKey).toByteArray() != m_legacySource)
    {
        setMigrationDiagnostic(QStringLiteral(
            "backup-verification-failed: legacy panel source readback was not exact"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }
    return true;
}

QByteArray PanelRegistry::serializePanels(QString *errorMessage) const
{
    return serializePanels(m_panels, errorMessage);
}

QByteArray PanelRegistry::serializePanels(
    const QList<QVariantMap> &panels,
    QString *errorMessage) const
{
    QList<ArchDock::PanelDefinition> definitions;
    definitions.reserve(panels.size());
    QSet<QString> panelIds;
    for (qsizetype index = 0; index < panels.size(); ++index)
    {
        QString definitionError;
        const std::optional<ArchDock::PanelDefinition> definition =
            ArchDock::PanelDefinition::fromLegacyMap(panels.at(index), &definitionError);
        if (!definition.has_value())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("panel %1 is invalid: %2")
                    .arg(index)
                    .arg(definitionError);
            }
            return {};
        }
        if (panelIds.contains(definition->identity.id))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("panel %1 duplicates id '%2'")
                    .arg(index)
                    .arg(definition->identity.id);
            }
            return {};
        }
        panelIds.insert(definition->identity.id);
        definitions.append(*definition);
    }
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return ArchDock::SettingsMigration::serializeVersionTwo(definitions);
}

bool PanelRegistry::persistPanelState(
    const QList<QVariantMap> &panels,
    const QVariantMap &globalSettings,
    QString *errorMessage)
{
    if (m_persistenceBlocked)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "panel registry persistence is blocked pending source repair");
        }
        return false;
    }

    QString serializationError;
    const QByteArray serialized = serializePanels(panels, &serializationError);
    if (!serializationError.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = serializationError;
        }
        return false;
    }

    QSettings settings;
    if (m_legacyRewritePending && !ensureLegacyBackupChecked(settings))
    {
        if (errorMessage)
        {
            *errorMessage = m_migrationDiagnostic;
        }
        return false;
    }
    settings.setValue(QString::fromLatin1(kPanelRecordsKey), serialized);
    DockSettings::writeTransaction(settings, globalSettings);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("QSettings could not persist the settings transaction");
        }
        return false;
    }

    QSettings verifier;
    if (verifier.value(QString::fromLatin1(kPanelRecordsKey)).toByteArray() != serialized)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel settings transaction readback did not match");
        }
        return false;
    }
    for (auto it = globalSettings.cbegin(); it != globalSettings.cend(); ++it)
    {
        if (verifier.value(QStringLiteral("dock/") + it.key()) != it.value())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "global settings transaction readback did not match field: %1")
                    .arg(it.key());
            }
            return false;
        }
    }

    m_legacySource.clear();
    m_legacyRewritePending = false;
    setMigrationDiagnostic(QString{});
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

void PanelRegistry::setMigrationDiagnostic(const QString &diagnostic)
{
    if (m_migrationDiagnostic == diagnostic)
    {
        return;
    }
    m_migrationDiagnostic = diagnostic;
    emit migrationDiagnosticChanged();
}

void PanelRegistry::changed(bool nativeTopologyChanged)
{
    ++m_revision;
    emit panelsChanged();
    if (nativeTopologyChanged)
    {
        emit nativePanelTopologyChanged();
    }
    emit revisionChanged();
}
