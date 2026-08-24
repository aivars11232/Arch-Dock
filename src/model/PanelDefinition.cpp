#include "PanelDefinition.h"
#include "PanelRuntimeState.h"

#include <QCoreApplication>
#include <QSet>
#include <QUrl>
#include <QtGlobal>

#include <limits>

namespace
{

using ArchDock::PanelHostKind;

const QSet<QString> &knownLegacyKeys()
{
    static const QSet<QString> keys{
        QStringLiteral("schemaVersion"),
        QStringLiteral("extensions"),
        QStringLiteral("presetOrigin"),
        QStringLiteral("id"),
        QStringLiteral("name"),
        QStringLiteral("builtIn"),
        QStringLiteral("hostKind"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("nativePanelId"),
        QStringLiteral("nativeControlAppletId"),
        QStringLiteral("nativeDockAppletId"),
        QStringLiteral("nativeOwnershipToken"),
        QStringLiteral("nativeRecoveryState"),
        QStringLiteral("nativeRecoveryError"),
        QStringLiteral("freeDesktopContainmentId"),
        QStringLiteral("freeDockAppletId"),
        QStringLiteral("freeOwnershipToken"),
        QStringLiteral("freeHostMode"),
        QStringLiteral("freeHostState"),
        QStringLiteral("freeCreationState"),
        QStringLiteral("freeCreationError"),
        QStringLiteral("freeRollbackError"),
        QStringLiteral("freeRecoveryError"),
        QStringLiteral("type"),
        QStringLiteral("contentAppIds"),
        QStringLiteral("contentUrls"),
        QStringLiteral("kdeWidgets"),
        QStringLiteral("acceptDrops"),
        QStringLiteral("folderLayout"),
        QStringLiteral("folderSpeed"),
        QStringLiteral("folderEasing"),
        QStringLiteral("folderExpandOnClick"),
        QStringLiteral("edge"),
        QStringLiteral("alignment"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("x"),
        QStringLiteral("y"),
        QStringLiteral("offset"),
        QStringLiteral("floatingMargin"),
        QStringLiteral("thickness"),
        QStringLiteral("lengthMode"),
        QStringLiteral("minimumLength"),
        QStringLiteral("maximumLength"),
        QStringLiteral("visible"),
        QStringLiteral("visibilityMode"),
        QStringLiteral("revealZone"),
        QStringLiteral("openDelay"),
        QStringLiteral("closeDelay"),
        QStringLiteral("windowOverlapPolicy"),
        QStringLiteral("presentationMode"),
        QStringLiteral("collapseAxis"),
        QStringLiteral("collapseMechanism"),
        QStringLiteral("revealHandle"),
        QStringLiteral("layout"),
        QStringLiteral("layoutScale"),
        QStringLiteral("layoutAngle"),
        QStringLiteral("layoutRadius"),
        QStringLiteral("layoutRows"),
        QStringLiteral("layoutPadding"),
        QStringLiteral("pathSides"),
        QStringLiteral("pathOrientation"),
        QStringLiteral("pathAnchor"),
        QStringLiteral("rendererTier"),
        QStringLiteral("panelThemeId"),
        QStringLiteral("completeThemeId"),
        QStringLiteral("appearance"),
        QStringLiteral("shape"),
        QStringLiteral("opacity"),
        QStringLiteral("color"),
        QStringLiteral("border"),
        QStringLiteral("glow"),
        QStringLiteral("shadow"),
        QStringLiteral("blur"),
        QStringLiteral("surface2D"),
        QStringLiteral("surface2_5D"),
        QStringLiteral("surface3D"),
        QStringLiteral("themeAsset"),
        QStringLiteral("themeSource"),
        QStringLiteral("themeFit"),
        QStringLiteral("themeSourceKind"),
        QStringLiteral("themeSourceFormat"),
        QStringLiteral("themeSourceWidth"),
        QStringLiteral("themeSourceHeight"),
        QStringLiteral("themeSourceHasAlpha"),
        QStringLiteral("themeSuggestedFit"),
        QStringLiteral("themePreview"),
        QStringLiteral("themeAnalysisStatus"),
        QStringLiteral("themeConversionTool"),
        QStringLiteral("themeConversionAvailable"),
        QStringLiteral("themePackageFormat"),
        QStringLiteral("themePackageVersion"),
        QStringLiteral("themePackageId"),
        QStringLiteral("themePackageName"),
        QStringLiteral("themePackageAuthor"),
        QStringLiteral("themePackageManifest"),
        QStringLiteral("themeStatus"),
        QStringLiteral("themeRenderWidth"),
        QStringLiteral("themeRenderHeight"),
        QStringLiteral("themeRenderFit"),
        QStringLiteral("themeRenderOutcome"),
        QStringLiteral("iconStyle"),
        QStringLiteral("iconThemeId"),
        QStringLiteral("iconShape"),
        QStringLiteral("iconSize"),
        QStringLiteral("spacing"),
        QStringLiteral("iconGlobalDefaults"),
        QStringLiteral("iconOverrides"),
        QStringLiteral("iconAnimation"),
        QStringLiteral("animationTrigger"),
        QStringLiteral("animationSpeed"),
        QStringLiteral("animationIntensity"),
        QStringLiteral("physicsEnabled"),
        QStringLiteral("panelMotionProfile"),
        QStringLiteral("revealMotionProfile"),
        QStringLiteral("reducedMotion"),
    };
    return keys;
}

bool isOneOf(const QString &value, std::initializer_list<const char *> candidates)
{
    for (const char *candidate : candidates)
    {
        if (value == QLatin1String(candidate))
        {
            return true;
        }
    }
    return false;
}

QStringList normalizedStringList(const QVariant &value, bool urls)
{
    QStringList candidates;
    if (value.metaType().id() == QMetaType::QStringList)
    {
        candidates = value.toStringList();
    }
    else
    {
        const QVariantList values = value.toList();
        candidates.reserve(values.size());
        for (const QVariant &candidate : values)
        {
            candidates.append(candidate.toString());
        }
    }

    QStringList result;
    for (const QString &candidate : candidates)
    {
        QString normalized = candidate.trimmed();
        if (urls)
        {
            const QUrl url(normalized);
            if (!url.isValid() || url.isEmpty())
            {
                continue;
            }
            normalized = url.toString();
        }
        if (!normalized.isEmpty() && !result.contains(normalized))
        {
            result.append(normalized);
        }
    }
    return result;
}

int normalizedHostId(const QVariant &value)
{
    bool ok = false;
    const qlonglong candidate = value.toLongLong(&ok);
    return ok && candidate >= 0 && candidate <= std::numeric_limits<int>::max()
        ? static_cast<int>(candidate)
        : -1;
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage)
    {
        *errorMessage = message;
    }
}

void insertIfNotEmpty(QVariantMap *record, const QString &key, const QString &value)
{
    if (!value.isEmpty())
    {
        record->insert(key, value);
    }
}

void insertIfNotEmpty(QVariantMap *record, const QString &key, const QVariantMap &value)
{
    if (!value.isEmpty())
    {
        record->insert(key, value);
    }
}

QVariantMap persistentExtensions(const QVariantMap &extensions)
{
    QVariantMap result;
    for (auto it = extensions.cbegin(); it != extensions.cend(); ++it)
    {
        if (!ArchDock::PanelRuntimeState::isTransientLegacyKey(it.key()))
        {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

template<typename T>
void insertOptional(QVariantMap *record, const QString &key, const std::optional<T> &value)
{
    if (value.has_value())
    {
        record->insert(key, QVariant::fromValue(*value));
    }
}

}

namespace ArchDock
{

PanelDefinition PanelDefinition::defaults(const QString &id,
                                          const QString &name,
                                          const QString &edge,
                                          bool builtIn)
{
    PanelDefinition definition;
    definition.identity.id = id.trimmed();
    definition.identity.name = name;
    definition.identity.builtIn = builtIn;
    definition.placement.edge = normalizeLegacyValue(
        QStringLiteral("edge"), edge).toString();
    definition.host.kind = definition.placement.edge == QStringLiteral("free")
        ? PanelHostKind::FreeDesktop
        : PanelHostKind::NativeEdge;

    const bool freePanel = definition.host.kind == PanelHostKind::FreeDesktop;
    const bool vertical = definition.placement.edge == QStringLiteral("left") ||
        definition.placement.edge == QStringLiteral("right");
    definition.visibility.visible = definition.placement.edge == QStringLiteral("bottom") ||
        freePanel;
    definition.placement.dynamic = !freePanel &&
        definition.placement.edge != QStringLiteral("bottom");
    definition.placement.width = freePanel ? 420 : (vertical ? 76 : 720);
    definition.placement.height = freePanel ? 420 : (vertical ? 420 : 76);
    definition.placement.x = freePanel ? 240 : 180;
    definition.content.type = freePanel ? QStringLiteral("empty") : QStringLiteral("hybrid");
    definition.layout.pathType = freePanel ? QStringLiteral("circular") : QStringLiteral("adaptive");
    definition.layout.radius = freePanel ? 145 : 150;
    definition.surface.themeAnalysisStatus = QCoreApplication::translate(
        "PanelRegistry", "No source selected.");
    definition.surface.themeStatus = QCoreApplication::translate(
        "PanelRegistry", "Preset surface active.");
    return definition;
}

std::optional<PanelDefinition> PanelDefinition::fromLegacyMap(
    const QVariantMap &record,
    QString *errorMessage)
{
    if (record.isEmpty())
    {
        setError(errorMessage, QStringLiteral("panel record is empty"));
        return std::nullopt;
    }

    const int sourceVersion = record.value(QStringLiteral("schemaVersion"), 1).toInt();
    if (sourceVersion < 1 || sourceVersion > CurrentSchemaVersion)
    {
        setError(
            errorMessage,
            QStringLiteral("unsupported panel schema version: %1").arg(sourceVersion));
        return std::nullopt;
    }

    const QString id = record.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty())
    {
        setError(errorMessage, QStringLiteral("panel record has no id"));
        return std::nullopt;
    }

    const QString edge = normalizeLegacyValue(
        QStringLiteral("edge"),
        record.value(QStringLiteral("edge"), QStringLiteral("bottom"))).toString();
    PanelDefinition definition = defaults(
        id,
        record.value(QStringLiteral("name"), id).toString(),
        edge,
        record.value(QStringLiteral("builtIn"), false).toBool());
    definition.schemaVersion = CurrentSchemaVersion;

    const auto normalized = [&record](const QString &key, const QVariant &fallback)
    {
        return PanelDefinition::normalizeLegacyValue(key, record.value(key, fallback));
    };
    const auto setString = [&record](const QString &key, QString *target, bool lower = false)
    {
        if (!record.contains(key))
        {
            return;
        }
        QString value = record.value(key).toString().trimmed();
        *target = lower ? value.toLower() : value;
    };

    definition.host.kind = hostKindFromName(
        record.value(QStringLiteral("hostKind")).toString(), edge);
    definition.host.screenIndex = normalized(
        QStringLiteral("screen"), definition.host.screenIndex).toInt();
    definition.host.screenId = normalized(
        QStringLiteral("screenId"), definition.host.screenId).toString();
    definition.host.nativePanelId = normalizedHostId(
        record.value(QStringLiteral("nativePanelId"), definition.host.nativePanelId));
    definition.host.nativeControlAppletId = normalizedHostId(
        record.value(
            QStringLiteral("nativeControlAppletId"),
            definition.host.nativeControlAppletId));
    definition.host.nativeDockAppletId = normalizedHostId(
        record.value(QStringLiteral("nativeDockAppletId"), definition.host.nativeDockAppletId));
    setString(
        QStringLiteral("nativeOwnershipToken"),
        &definition.host.nativeOwnershipToken);
    setString(
        QStringLiteral("nativeRecoveryState"),
        &definition.host.nativeRecoveryState,
        true);
    setString(
        QStringLiteral("nativeRecoveryError"),
        &definition.host.nativeRecoveryError);
    definition.host.freeDesktopContainmentId = normalizedHostId(
        record.value(
            QStringLiteral("freeDesktopContainmentId"),
            definition.host.freeDesktopContainmentId));
    definition.host.freeDockAppletId = normalizedHostId(
        record.value(QStringLiteral("freeDockAppletId"), definition.host.freeDockAppletId));
    setString(
        QStringLiteral("freeOwnershipToken"),
        &definition.host.freeOwnershipToken);
    setString(QStringLiteral("freeHostMode"), &definition.host.freeHostMode, true);
    setString(QStringLiteral("freeHostState"), &definition.host.freeHostState, true);
    setString(
        QStringLiteral("freeCreationState"),
        &definition.host.freeCreationState,
        true);
    setString(
        QStringLiteral("freeCreationError"),
        &definition.host.freeCreationError);
    setString(
        QStringLiteral("freeRollbackError"),
        &definition.host.freeRollbackError);
    setString(
        QStringLiteral("freeRecoveryError"),
        &definition.host.freeRecoveryError);

    definition.content.type = normalized(
        QStringLiteral("type"), definition.content.type).toString();
    definition.content.applicationIds = normalized(
        QStringLiteral("contentAppIds"), definition.content.applicationIds).toStringList();
    definition.content.urls = normalized(
        QStringLiteral("contentUrls"), definition.content.urls).toStringList();
    definition.content.kdeWidgets = normalizedStringList(
        record.value(QStringLiteral("kdeWidgets"), definition.content.kdeWidgets), false);
    definition.content.acceptDrops = normalized(
        QStringLiteral("acceptDrops"), definition.content.acceptDrops).toBool();
    definition.content.folderLayout = normalized(
        QStringLiteral("folderLayout"), definition.content.folderLayout).toString();
    definition.content.folderSpeed = normalized(
        QStringLiteral("folderSpeed"), definition.content.folderSpeed).toInt();
    definition.content.folderEasing = normalized(
        QStringLiteral("folderEasing"), definition.content.folderEasing).toString();
    definition.content.folderExpandOnClick = normalized(
        QStringLiteral("folderExpandOnClick"),
        definition.content.folderExpandOnClick).toBool();

    definition.placement.edge = edge;
    definition.placement.alignment = normalized(
        QStringLiteral("alignment"), definition.placement.alignment).toString();
    definition.placement.dynamic = normalized(
        QStringLiteral("dynamic"), definition.placement.dynamic).toBool();
    definition.placement.width = normalized(
        QStringLiteral("width"), definition.placement.width).toInt();
    definition.placement.height = normalized(
        QStringLiteral("height"), definition.placement.height).toInt();
    definition.placement.x = normalized(
        QStringLiteral("x"), definition.placement.x).toInt();
    definition.placement.y = normalized(
        QStringLiteral("y"), definition.placement.y).toInt();
    if (record.contains(QStringLiteral("offset")))
    {
        definition.placement.offset = normalized(
            QStringLiteral("offset"), 0).toInt();
    }
    if (record.contains(QStringLiteral("floatingMargin")))
    {
        definition.placement.floatingMargin = normalized(
            QStringLiteral("floatingMargin"), 0).toInt();
    }
    if (record.contains(QStringLiteral("thickness")))
    {
        definition.placement.thickness = normalized(
            QStringLiteral("thickness"), 76).toInt();
    }
    if (record.contains(QStringLiteral("lengthMode")))
    {
        definition.placement.lengthMode = normalized(
            QStringLiteral("lengthMode"), QStringLiteral("fixed")).toString();
    }
    if (record.contains(QStringLiteral("minimumLength")))
    {
        definition.placement.minimumLength = normalized(
            QStringLiteral("minimumLength"), 48).toInt();
    }
    if (record.contains(QStringLiteral("maximumLength")))
    {
        definition.placement.maximumLength = normalized(
            QStringLiteral("maximumLength"), 4096).toInt();
    }

    definition.visibility.visible = normalized(
        QStringLiteral("visible"), definition.visibility.visible).toBool();
    definition.visibility.hostMode = normalized(
        QStringLiteral("visibilityMode"), definition.visibility.hostMode).toString();
    definition.visibility.revealZone = normalized(
        QStringLiteral("revealZone"), definition.visibility.revealZone).toInt();
    definition.visibility.openDelay = normalized(
        QStringLiteral("openDelay"), definition.visibility.openDelay).toInt();
    definition.visibility.closeDelay = normalized(
        QStringLiteral("closeDelay"), definition.visibility.closeDelay).toInt();
    setString(
        QStringLiteral("windowOverlapPolicy"),
        &definition.visibility.windowOverlapPolicy,
        true);

    setString(
        QStringLiteral("presentationMode"),
        &definition.presentation.mode,
        true);
    setString(
        QStringLiteral("collapseAxis"),
        &definition.presentation.collapseAxis,
        true);
    setString(
        QStringLiteral("collapseMechanism"),
        &definition.presentation.collapseMechanism,
        true);
    setString(
        QStringLiteral("revealHandle"),
        &definition.presentation.revealHandle,
        true);

    definition.layout.pathType = normalized(
        QStringLiteral("layout"), definition.layout.pathType).toString();
    definition.layout.scale = normalized(
        QStringLiteral("layoutScale"), definition.layout.scale).toReal();
    definition.layout.angle = normalized(
        QStringLiteral("layoutAngle"), definition.layout.angle).toReal();
    definition.layout.radius = normalized(
        QStringLiteral("layoutRadius"), definition.layout.radius).toInt();
    definition.layout.rows = normalized(
        QStringLiteral("layoutRows"), definition.layout.rows).toInt();
    definition.layout.padding = normalized(
        QStringLiteral("layoutPadding"), definition.layout.padding).toInt();
    definition.layout.polygonSides = normalized(
        QStringLiteral("pathSides"), definition.layout.polygonSides).toInt();
    definition.layout.orientation = normalized(
        QStringLiteral("pathOrientation"), definition.layout.orientation).toString();
    definition.layout.anchor = normalized(
        QStringLiteral("pathAnchor"), definition.layout.anchor).toString();

    setString(QStringLiteral("rendererTier"), &definition.surface.rendererTier, true);
    setString(QStringLiteral("panelThemeId"), &definition.surface.panelThemeId);
    setString(QStringLiteral("completeThemeId"), &definition.surface.completeThemeId);
    definition.surface.appearance = normalized(
        QStringLiteral("appearance"), definition.surface.appearance).toString();
    definition.surface.shape = normalized(
        QStringLiteral("shape"), definition.surface.shape).toString();
    definition.surface.opacity = normalized(
        QStringLiteral("opacity"), definition.surface.opacity).toReal();
    setString(QStringLiteral("color"), &definition.surface.color);
    definition.surface.border = record.value(QStringLiteral("border")).toMap();
    definition.surface.glow = record.value(QStringLiteral("glow")).toMap();
    definition.surface.shadow = record.value(QStringLiteral("shadow")).toMap();
    definition.surface.blur = record.value(QStringLiteral("blur")).toMap();
    definition.surface.parameters2D = record.value(QStringLiteral("surface2D")).toMap();
    definition.surface.parameters2_5D = record.value(QStringLiteral("surface2_5D")).toMap();
    definition.surface.parameters3D = record.value(QStringLiteral("surface3D")).toMap();
    setString(QStringLiteral("themeAsset"), &definition.surface.themeAsset);
    setString(QStringLiteral("themeSource"), &definition.surface.themeSource);
    definition.surface.themeFit = normalized(
        QStringLiteral("themeFit"), definition.surface.themeFit).toString();
    setString(QStringLiteral("themeSourceKind"), &definition.surface.themeSourceKind, true);
    setString(QStringLiteral("themeSourceFormat"), &definition.surface.themeSourceFormat, true);
    definition.surface.themeSourceWidth = qMax(
        0,
        record.value(
            QStringLiteral("themeSourceWidth"),
            definition.surface.themeSourceWidth).toInt());
    definition.surface.themeSourceHeight = qMax(
        0,
        record.value(
            QStringLiteral("themeSourceHeight"),
            definition.surface.themeSourceHeight).toInt());
    definition.surface.themeSourceHasAlpha = record.value(
        QStringLiteral("themeSourceHasAlpha"),
        definition.surface.themeSourceHasAlpha).toBool();
    definition.surface.themeSuggestedFit = normalized(
        QStringLiteral("themeSuggestedFit"),
        definition.surface.themeSuggestedFit).toString();
    setString(QStringLiteral("themePreview"), &definition.surface.themePreview);
    setString(
        QStringLiteral("themeAnalysisStatus"),
        &definition.surface.themeAnalysisStatus);
    setString(
        QStringLiteral("themeConversionTool"),
        &definition.surface.themeConversionTool);
    definition.surface.themeConversionAvailable = record.value(
        QStringLiteral("themeConversionAvailable"),
        definition.surface.themeConversionAvailable).toBool();
    setString(
        QStringLiteral("themePackageFormat"),
        &definition.surface.themePackageFormat);
    definition.surface.themePackageVersion = qMax(
        0,
        record.value(
            QStringLiteral("themePackageVersion"),
            definition.surface.themePackageVersion).toInt());
    setString(QStringLiteral("themePackageId"), &definition.surface.themePackageId);
    setString(QStringLiteral("themePackageName"), &definition.surface.themePackageName);
    setString(QStringLiteral("themePackageAuthor"), &definition.surface.themePackageAuthor);
    setString(
        QStringLiteral("themePackageManifest"),
        &definition.surface.themePackageManifest);
    setString(QStringLiteral("themeStatus"), &definition.surface.themeStatus);
    definition.surface.themeRenderWidth = qMax(
        0,
        record.value(
            QStringLiteral("themeRenderWidth"),
            definition.surface.themeRenderWidth).toInt());
    definition.surface.themeRenderHeight = qMax(
        0,
        record.value(
            QStringLiteral("themeRenderHeight"),
            definition.surface.themeRenderHeight).toInt());
    setString(QStringLiteral("themeRenderFit"), &definition.surface.themeRenderFit, true);
    setString(
        QStringLiteral("themeRenderOutcome"),
        &definition.surface.themeRenderOutcome,
        true);

    setString(QStringLiteral("iconStyle"), &definition.iconStyle.styleReference);
    setString(QStringLiteral("iconThemeId"), &definition.iconStyle.themeId);
    definition.iconStyle.shape = normalized(
        QStringLiteral("iconShape"), definition.iconStyle.shape).toString();
    definition.iconStyle.size = normalized(
        QStringLiteral("iconSize"), definition.iconStyle.size).toInt();
    definition.iconStyle.spacing = normalized(
        QStringLiteral("spacing"), definition.iconStyle.spacing).toReal();
    definition.iconStyle.globalDefaults = record.value(
        QStringLiteral("iconGlobalDefaults")).toMap();
    definition.iconStyle.perEntryOverrides = record.value(
        QStringLiteral("iconOverrides")).toMap();

    definition.motion.iconProfile = normalized(
        QStringLiteral("iconAnimation"), definition.motion.iconProfile).toString();
    definition.motion.trigger = normalized(
        QStringLiteral("animationTrigger"), definition.motion.trigger).toString();
    definition.motion.speed = normalized(
        QStringLiteral("animationSpeed"), definition.motion.speed).toReal();
    definition.motion.intensity = normalized(
        QStringLiteral("animationIntensity"), definition.motion.intensity).toReal();
    definition.motion.physicsEnabled = normalized(
        QStringLiteral("physicsEnabled"), definition.motion.physicsEnabled).toBool();
    setString(QStringLiteral("panelMotionProfile"), &definition.motion.panelProfile);
    setString(QStringLiteral("revealMotionProfile"), &definition.motion.revealProfile);
    definition.motion.reducedMotion = record.value(
        QStringLiteral("reducedMotion"), definition.motion.reducedMotion).toBool();

    const QVariant presetValue = record.value(QStringLiteral("presetOrigin"));
    const QVariantMap presetMap = presetValue.toMap();
    if (!presetMap.isEmpty())
    {
        PanelPresetOrigin origin;
        origin.panelPresetId = presetMap.value(
            QStringLiteral("panelPresetId")).toString().trimmed();
        origin.panelPresetRevision = qMax(
            0,
            presetMap.value(QStringLiteral("panelPresetRevision")).toInt());
        origin.iconPresetId = presetMap.value(
            QStringLiteral("iconPresetId")).toString().trimmed();
        origin.iconPresetRevision = qMax(
            0,
            presetMap.value(QStringLiteral("iconPresetRevision")).toInt());
        origin.customizedAfterApply = presetMap.value(
            QStringLiteral("customizedAfterApply")).toBool();
        origin.detachedFromPreset = presetMap.value(
            QStringLiteral("detachedFromPreset")).toBool();
        definition.presetOrigin = origin;
    }

    const QVariant extensionValue = record.value(QStringLiteral("extensions"));
    if (extensionValue.metaType().id() == QMetaType::QVariantMap)
    {
        definition.extensions = persistentExtensions(extensionValue.toMap());
    }
    else if (record.contains(QStringLiteral("extensions")))
    {
        definition.extensions.insert(QStringLiteral("extensions"), extensionValue);
    }
    if (record.contains(QStringLiteral("presetOrigin")) && presetMap.isEmpty() &&
        presetValue.isValid() && !presetValue.isNull())
    {
        definition.extensions.insert(QStringLiteral("presetOrigin"), presetValue);
    }
    for (auto it = record.cbegin(); it != record.cend(); ++it)
    {
        if (!knownLegacyKeys().contains(it.key()) &&
            !PanelRuntimeState::isTransientLegacyKey(it.key()))
        {
            definition.extensions.insert(it.key(), it.value());
        }
    }

    QString validationError;
    if (!definition.isValid(&validationError))
    {
        setError(errorMessage, validationError);
        return std::nullopt;
    }
    setError(errorMessage, QString{});
    return definition;
}

PanelDefinition PanelDefinition::normalized() const
{
    QString errorMessage;
    const std::optional<PanelDefinition> result = fromLegacyMap(
        toLegacyMap(), &errorMessage);
    return result.value_or(*this);
}

bool PanelDefinition::isValid(QString *errorMessage) const
{
    if (schemaVersion != CurrentSchemaVersion)
    {
        setError(
            errorMessage,
            QStringLiteral("panel definition schema version must be %1")
                .arg(CurrentSchemaVersion));
        return false;
    }
    if (identity.id.trimmed().isEmpty())
    {
        setError(errorMessage, QStringLiteral("panel definition id is empty"));
        return false;
    }
    const QString normalizedEdge = normalizeLegacyValue(
        QStringLiteral("edge"), placement.edge).toString();
    if (normalizedEdge != placement.edge.trimmed().toLower())
    {
        setError(errorMessage, QStringLiteral("panel definition edge is invalid"));
        return false;
    }
    if ((placement.edge == QStringLiteral("free")) !=
        (host.kind == PanelHostKind::FreeDesktop))
    {
        setError(errorMessage, QStringLiteral("panel host kind does not match its edge"));
        return false;
    }
    setError(errorMessage, QString{});
    return true;
}

QVariantMap PanelDefinition::toLegacyMap() const
{
    QVariantMap record = persistentExtensions(extensions);
    record.insert(QStringLiteral("schemaVersion"), CurrentSchemaVersion);
    record.insert(QStringLiteral("id"), identity.id);
    record.insert(QStringLiteral("name"), identity.name);
    record.insert(QStringLiteral("builtIn"), identity.builtIn);
    record.insert(QStringLiteral("hostKind"), hostKindName(host.kind));
    record.insert(QStringLiteral("screen"), host.screenIndex);
    record.insert(QStringLiteral("screenId"), host.screenId);
    record.insert(QStringLiteral("nativePanelId"), host.nativePanelId);
    record.insert(QStringLiteral("nativeControlAppletId"), host.nativeControlAppletId);
    record.insert(QStringLiteral("nativeDockAppletId"), host.nativeDockAppletId);
    record.insert(QStringLiteral("nativeOwnershipToken"), host.nativeOwnershipToken);
    record.insert(QStringLiteral("nativeRecoveryState"), host.nativeRecoveryState);
    record.insert(QStringLiteral("nativeRecoveryError"), host.nativeRecoveryError);
    record.insert(QStringLiteral("freeDesktopContainmentId"), host.freeDesktopContainmentId);
    record.insert(QStringLiteral("freeDockAppletId"), host.freeDockAppletId);
    record.insert(QStringLiteral("freeOwnershipToken"), host.freeOwnershipToken);
    record.insert(QStringLiteral("freeHostMode"), host.freeHostMode);
    record.insert(QStringLiteral("freeHostState"), host.freeHostState);
    record.insert(QStringLiteral("freeCreationState"), host.freeCreationState);
    record.insert(QStringLiteral("freeCreationError"), host.freeCreationError);
    record.insert(QStringLiteral("freeRollbackError"), host.freeRollbackError);
    record.insert(QStringLiteral("freeRecoveryError"), host.freeRecoveryError);

    record.insert(QStringLiteral("type"), content.type);
    record.insert(QStringLiteral("contentAppIds"), content.applicationIds);
    record.insert(QStringLiteral("contentUrls"), content.urls);
    record.insert(QStringLiteral("kdeWidgets"), content.kdeWidgets);
    record.insert(QStringLiteral("acceptDrops"), content.acceptDrops);
    record.insert(QStringLiteral("folderLayout"), content.folderLayout);
    record.insert(QStringLiteral("folderSpeed"), content.folderSpeed);
    record.insert(QStringLiteral("folderEasing"), content.folderEasing);
    record.insert(QStringLiteral("folderExpandOnClick"), content.folderExpandOnClick);

    record.insert(QStringLiteral("edge"), placement.edge);
    record.insert(QStringLiteral("alignment"), placement.alignment);
    record.insert(QStringLiteral("dynamic"), placement.dynamic);
    record.insert(QStringLiteral("width"), placement.width);
    record.insert(QStringLiteral("height"), placement.height);
    record.insert(QStringLiteral("x"), placement.x);
    record.insert(QStringLiteral("y"), placement.y);
    insertOptional(&record, QStringLiteral("offset"), placement.offset);
    insertOptional(&record, QStringLiteral("floatingMargin"), placement.floatingMargin);
    insertOptional(&record, QStringLiteral("thickness"), placement.thickness);
    insertOptional(&record, QStringLiteral("lengthMode"), placement.lengthMode);
    insertOptional(&record, QStringLiteral("minimumLength"), placement.minimumLength);
    insertOptional(&record, QStringLiteral("maximumLength"), placement.maximumLength);

    record.insert(QStringLiteral("visible"), visibility.visible);
    record.insert(QStringLiteral("visibilityMode"), visibility.hostMode);
    record.insert(QStringLiteral("revealZone"), visibility.revealZone);
    record.insert(QStringLiteral("openDelay"), visibility.openDelay);
    record.insert(QStringLiteral("closeDelay"), visibility.closeDelay);
    insertIfNotEmpty(
        &record,
        QStringLiteral("windowOverlapPolicy"),
        visibility.windowOverlapPolicy);

    insertIfNotEmpty(&record, QStringLiteral("presentationMode"), presentation.mode);
    insertIfNotEmpty(&record, QStringLiteral("collapseAxis"), presentation.collapseAxis);
    insertIfNotEmpty(
        &record,
        QStringLiteral("collapseMechanism"),
        presentation.collapseMechanism);
    insertIfNotEmpty(&record, QStringLiteral("revealHandle"), presentation.revealHandle);

    record.insert(QStringLiteral("layout"), layout.pathType);
    record.insert(QStringLiteral("layoutScale"), layout.scale);
    record.insert(QStringLiteral("layoutAngle"), layout.angle);
    record.insert(QStringLiteral("layoutRadius"), layout.radius);
    record.insert(QStringLiteral("layoutRows"), layout.rows);
    record.insert(QStringLiteral("layoutPadding"), layout.padding);
    record.insert(QStringLiteral("pathSides"), layout.polygonSides);
    record.insert(QStringLiteral("pathOrientation"), layout.orientation);
    record.insert(QStringLiteral("pathAnchor"), layout.anchor);

    insertIfNotEmpty(&record, QStringLiteral("rendererTier"), surface.rendererTier);
    insertIfNotEmpty(&record, QStringLiteral("panelThemeId"), surface.panelThemeId);
    insertIfNotEmpty(&record, QStringLiteral("completeThemeId"), surface.completeThemeId);
    record.insert(QStringLiteral("appearance"), surface.appearance);
    record.insert(QStringLiteral("shape"), surface.shape);
    record.insert(QStringLiteral("opacity"), surface.opacity);
    record.insert(QStringLiteral("color"), surface.color);
    insertIfNotEmpty(&record, QStringLiteral("border"), surface.border);
    insertIfNotEmpty(&record, QStringLiteral("glow"), surface.glow);
    insertIfNotEmpty(&record, QStringLiteral("shadow"), surface.shadow);
    insertIfNotEmpty(&record, QStringLiteral("blur"), surface.blur);
    insertIfNotEmpty(&record, QStringLiteral("surface2D"), surface.parameters2D);
    insertIfNotEmpty(&record, QStringLiteral("surface2_5D"), surface.parameters2_5D);
    insertIfNotEmpty(&record, QStringLiteral("surface3D"), surface.parameters3D);
    record.insert(QStringLiteral("themeAsset"), surface.themeAsset);
    record.insert(QStringLiteral("themeSource"), surface.themeSource);
    record.insert(QStringLiteral("themeFit"), surface.themeFit);
    record.insert(QStringLiteral("themeSourceKind"), surface.themeSourceKind);
    record.insert(QStringLiteral("themeSourceFormat"), surface.themeSourceFormat);
    record.insert(QStringLiteral("themeSourceWidth"), surface.themeSourceWidth);
    record.insert(QStringLiteral("themeSourceHeight"), surface.themeSourceHeight);
    record.insert(QStringLiteral("themeSourceHasAlpha"), surface.themeSourceHasAlpha);
    record.insert(QStringLiteral("themeSuggestedFit"), surface.themeSuggestedFit);
    record.insert(QStringLiteral("themePreview"), surface.themePreview);
    record.insert(QStringLiteral("themeAnalysisStatus"), surface.themeAnalysisStatus);
    record.insert(QStringLiteral("themeConversionTool"), surface.themeConversionTool);
    record.insert(
        QStringLiteral("themeConversionAvailable"),
        surface.themeConversionAvailable);
    record.insert(QStringLiteral("themePackageFormat"), surface.themePackageFormat);
    record.insert(QStringLiteral("themePackageVersion"), surface.themePackageVersion);
    record.insert(QStringLiteral("themePackageId"), surface.themePackageId);
    record.insert(QStringLiteral("themePackageName"), surface.themePackageName);
    record.insert(QStringLiteral("themePackageAuthor"), surface.themePackageAuthor);
    record.insert(QStringLiteral("themePackageManifest"), surface.themePackageManifest);
    record.insert(QStringLiteral("themeStatus"), surface.themeStatus);
    record.insert(QStringLiteral("themeRenderWidth"), surface.themeRenderWidth);
    record.insert(QStringLiteral("themeRenderHeight"), surface.themeRenderHeight);
    record.insert(QStringLiteral("themeRenderFit"), surface.themeRenderFit);
    record.insert(QStringLiteral("themeRenderOutcome"), surface.themeRenderOutcome);

    insertIfNotEmpty(&record, QStringLiteral("iconStyle"), iconStyle.styleReference);
    insertIfNotEmpty(&record, QStringLiteral("iconThemeId"), iconStyle.themeId);
    record.insert(QStringLiteral("iconShape"), iconStyle.shape);
    record.insert(QStringLiteral("iconSize"), iconStyle.size);
    record.insert(QStringLiteral("spacing"), iconStyle.spacing);
    insertIfNotEmpty(
        &record,
        QStringLiteral("iconGlobalDefaults"),
        iconStyle.globalDefaults);
    insertIfNotEmpty(
        &record,
        QStringLiteral("iconOverrides"),
        iconStyle.perEntryOverrides);

    record.insert(QStringLiteral("iconAnimation"), motion.iconProfile);
    record.insert(QStringLiteral("animationTrigger"), motion.trigger);
    record.insert(QStringLiteral("animationSpeed"), motion.speed);
    record.insert(QStringLiteral("animationIntensity"), motion.intensity);
    record.insert(QStringLiteral("physicsEnabled"), motion.physicsEnabled);
    insertIfNotEmpty(&record, QStringLiteral("panelMotionProfile"), motion.panelProfile);
    insertIfNotEmpty(&record, QStringLiteral("revealMotionProfile"), motion.revealProfile);
    record.insert(QStringLiteral("reducedMotion"), motion.reducedMotion);

    if (presetOrigin.has_value())
    {
        record.insert(
            QStringLiteral("presetOrigin"),
            QVariantMap{
                {QStringLiteral("panelPresetId"), presetOrigin->panelPresetId},
                {QStringLiteral("panelPresetRevision"), presetOrigin->panelPresetRevision},
                {QStringLiteral("iconPresetId"), presetOrigin->iconPresetId},
                {QStringLiteral("iconPresetRevision"), presetOrigin->iconPresetRevision},
                {QStringLiteral("customizedAfterApply"), presetOrigin->customizedAfterApply},
                {QStringLiteral("detachedFromPreset"), presetOrigin->detachedFromPreset},
            });
    }
    return record;
}

QVariantMap PanelDefinition::toPersistedMap() const
{
    QVariantMap record = toLegacyMap();
    const QVariantMap filteredExtensions = persistentExtensions(extensions);
    for (auto it = filteredExtensions.cbegin(); it != filteredExtensions.cend(); ++it)
    {
        if (!knownLegacyKeys().contains(it.key()))
        {
            record.remove(it.key());
        }
    }
    if (!filteredExtensions.isEmpty())
    {
        record.insert(QStringLiteral("extensions"), filteredExtensions);
    }
    return record;
}

QVariant PanelDefinition::normalizeLegacyValue(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("edge"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"top", "bottom", "left", "right", "free"})
            ? candidate
            : QStringLiteral("bottom");
    }
    if (key == QStringLiteral("alignment"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"start", "center", "end"})
            ? candidate
            : QStringLiteral("center");
    }
    if (key == QStringLiteral("screen"))
    {
        return qMax(0, value.toInt());
    }
    if (key == QStringLiteral("screenId"))
    {
        return value.toString().trimmed();
    }
    if (key == QStringLiteral("nativePanelId") ||
        key == QStringLiteral("nativeControlAppletId") ||
        key == QStringLiteral("nativeDockAppletId") ||
        key == QStringLiteral("freeDesktopContainmentId") ||
        key == QStringLiteral("freeDockAppletId"))
    {
        return normalizedHostId(value);
    }
    if (key == QStringLiteral("visibilityMode"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"always", "auto-hide", "dodge", "cover"})
            ? candidate
            : QStringLiteral("always");
    }
    if (key == QStringLiteral("revealZone"))
    {
        return qBound(1, value.toInt(), 64);
    }
    if (key == QStringLiteral("type"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"empty", "launcher", "tasks", "hybrid"})
            ? candidate
            : QStringLiteral("hybrid");
    }
    if (key == QStringLiteral("contentAppIds"))
    {
        return normalizedStringList(value, false);
    }
    if (key == QStringLiteral("contentUrls"))
    {
        return normalizedStringList(value, true);
    }
    if (key == QStringLiteral("shape"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"pill", "rounded", "hexagon"})
            ? candidate
            : QStringLiteral("pill");
    }
    if (key == QStringLiteral("iconShape"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"rounded", "square", "squircle", "circle", "hexagon"})
            ? candidate
            : QStringLiteral("rounded");
    }
    if (key == QStringLiteral("appearance"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(
            candidate,
            {"glass", "crystal", "neon", "minimal", "plasma", "lime",
             "floating-glass", "metallic", "futuristic", "organic", "platform",
             "plate", "pedestal"})
            ? candidate
            : QStringLiteral("glass");
    }
    if (key == QStringLiteral("layout"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(
            candidate,
            {"adaptive", "horizontal", "vertical", "diagonal", "circular", "ellipse",
             "ring", "radial", "arc", "semicircle", "fan", "spiral", "ribbon",
             "vertical-curve", "horizontal-curve", "polygon", "triangle", "square",
             "pentagon", "hexagon", "octagon", "star", "grid", "floating"})
            ? candidate
            : QStringLiteral("adaptive");
    }
    if (key == QStringLiteral("pathOrientation"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"upright", "tangent", "radial"})
            ? candidate
            : QStringLiteral("upright");
    }
    if (key == QStringLiteral("pathAnchor"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(
            candidate,
            {"top-left", "top", "top-right", "left", "center", "right",
             "bottom-left", "bottom", "bottom-right"})
            ? candidate
            : QStringLiteral("center");
    }
    if (key == QStringLiteral("iconAnimation"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(
            candidate,
            {"none", "bounce", "elastic", "pulse", "scale", "spin", "idle-rotate",
             "orbit", "swing", "wobble", "wiggle", "shake", "glow", "breathe",
             "float", "wave", "ripple", "magnetic", "spring"})
            ? candidate
            : QStringLiteral("scale");
    }
    if (key == QStringLiteral("animationTrigger"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"hover", "click", "launch", "running", "drop", "reveal", "idle"})
            ? candidate
            : QStringLiteral("hover");
    }
    if (key == QStringLiteral("folderLayout"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(
            candidate,
            {"fan", "grid", "stack", "arc", "spiral", "circular", "radial",
             "vertical", "horizontal", "elastic", "physics"})
            ? candidate
            : QStringLiteral("fan");
    }
    if (key == QStringLiteral("folderEasing"))
    {
        const QString candidate = value.toString().trimmed();
        return isOneOf(candidate, {"outCubic", "outBack", "outElastic", "spring"})
            ? candidate
            : QStringLiteral("outBack");
    }
    if (key == QStringLiteral("themeFit") || key == QStringLiteral("themeSuggestedFit"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"cover", "contain", "stretch", "tile"})
            ? candidate
            : QStringLiteral("cover");
    }
    if (key == QStringLiteral("lengthMode"))
    {
        const QString candidate = value.toString().trimmed().toLower();
        return isOneOf(candidate, {"fit", "fixed", "fill"})
            ? candidate
            : QStringLiteral("fixed");
    }
    if (key == QStringLiteral("opacity"))
    {
        return qBound<qreal>(0.0, value.toReal(), 1.0);
    }
    if (key == QStringLiteral("iconSize"))
    {
        return qBound(24, value.toInt(), 128);
    }
    if (key == QStringLiteral("spacing"))
    {
        return qBound<qreal>(0.0, value.toReal(), 48.0);
    }
    if (key == QStringLiteral("layoutScale"))
    {
        return qBound<qreal>(0.5, value.toReal(), 2.5);
    }
    if (key == QStringLiteral("layoutAngle"))
    {
        return qBound<qreal>(-180.0, value.toReal(), 180.0);
    }
    if (key == QStringLiteral("layoutRadius"))
    {
        return qBound(48, value.toInt(), 2048);
    }
    if (key == QStringLiteral("layoutRows"))
    {
        return qBound(1, value.toInt(), 8);
    }
    if (key == QStringLiteral("layoutPadding"))
    {
        return qBound(0, value.toInt(), 240);
    }
    if (key == QStringLiteral("pathSides"))
    {
        return qBound(3, value.toInt(), 12);
    }
    if (key == QStringLiteral("animationSpeed"))
    {
        return qBound<qreal>(0.2, value.toReal(), 3.0);
    }
    if (key == QStringLiteral("animationIntensity"))
    {
        return qBound<qreal>(0.1, value.toReal(), 2.5);
    }
    if (key == QStringLiteral("folderSpeed"))
    {
        return qBound(80, value.toInt(), 1200);
    }
    if (key == QStringLiteral("width") || key == QStringLiteral("height") ||
        key == QStringLiteral("thickness") || key == QStringLiteral("minimumLength") ||
        key == QStringLiteral("maximumLength"))
    {
        return qBound(48, value.toInt(), 4096);
    }
    if (key == QStringLiteral("x") || key == QStringLiteral("y") ||
        key == QStringLiteral("offset") || key == QStringLiteral("floatingMargin"))
    {
        return qMax(0, value.toInt());
    }
    if (key == QStringLiteral("openDelay") || key == QStringLiteral("closeDelay"))
    {
        return qBound(0, value.toInt(), 60000);
    }
    if (key == QStringLiteral("visible") || key == QStringLiteral("dynamic") ||
        key == QStringLiteral("acceptDrops") || key == QStringLiteral("physicsEnabled") ||
        key == QStringLiteral("folderExpandOnClick") || key == QStringLiteral("reducedMotion"))
    {
        return value.toBool();
    }
    return value;
}

QString PanelDefinition::hostKindName(PanelHostKind kind)
{
    return kind == PanelHostKind::FreeDesktop
        ? QStringLiteral("free-desktop")
        : QStringLiteral("native-edge");
}

PanelHostKind PanelDefinition::hostKindFromName(const QString &name, const QString &edge)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("free-desktop") || edge == QStringLiteral("free"))
    {
        return PanelHostKind::FreeDesktop;
    }
    return PanelHostKind::NativeEdge;
}

}
