// PanelDefinition: reading and writing saved panel records and the flat
// settings view of a definition.
#include "PanelDefinition.h"
#include "PanelRuntimeState.h"
#include "PanelSettingsSchema.h"

#include <QCoreApplication>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QtGlobal>

#include <tuple>
#include <array>
#include <algorithm>
#include <cmath>

namespace
{

using ArchDock::PanelHostKind;
using ArchDock::PanelLayoutKind;

const std::array<std::pair<PanelLayoutKind, const char *>,
                 static_cast<std::size_t>(PanelLayoutKind::Count)> &panelLayoutNames()
{
    static const std::array<std::pair<PanelLayoutKind, const char *>,
                            static_cast<std::size_t>(PanelLayoutKind::Count)> names{{
        {PanelLayoutKind::Adaptive, "adaptive"},
        {PanelLayoutKind::Horizontal, "horizontal"},
        {PanelLayoutKind::Vertical, "vertical"},
        {PanelLayoutKind::Diagonal, "diagonal"},
        {PanelLayoutKind::Circular, "circular"},
        {PanelLayoutKind::Ellipse, "ellipse"},
        {PanelLayoutKind::Ring, "ring"},
        {PanelLayoutKind::Radial, "radial"},
        {PanelLayoutKind::Arc, "arc"},
        {PanelLayoutKind::Semicircle, "semicircle"},
        {PanelLayoutKind::Fan, "fan"},
        {PanelLayoutKind::Spiral, "spiral"},
        {PanelLayoutKind::Ribbon, "ribbon"},
        {PanelLayoutKind::VerticalCurve, "vertical-curve"},
        {PanelLayoutKind::HorizontalCurve, "horizontal-curve"},
        {PanelLayoutKind::Polygon, "polygon"},
        {PanelLayoutKind::Triangle, "triangle"},
        {PanelLayoutKind::Square, "square"},
        {PanelLayoutKind::Pentagon, "pentagon"},
        {PanelLayoutKind::Hexagon, "hexagon"},
        {PanelLayoutKind::Octagon, "octagon"},
        {PanelLayoutKind::Star, "star"},
        {PanelLayoutKind::Grid, "grid"},
        {PanelLayoutKind::Floating, "floating"},
    }};
    return names;
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

bool validIconOverrideIdentity(const QString &identity)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,255}$"));
    return pattern.match(identity).hasMatch() &&
        (identity.startsWith(QStringLiteral("desktop.")) ||
         identity.startsWith(QStringLiteral("application.")) ||
         identity.startsWith(QStringLiteral("free.sha256-")));
}

bool validIconOverrideReference(const QString &reference)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,255}$"));
    return reference.isEmpty() || pattern.match(reference).hasMatch();
}

bool containsControlCharacter(const QString &value)
{
    for (const QChar character : value)
    {
        if (character.isNull() || character.category() == QChar::Other_Control)
        {
            return true;
        }
    }
    return false;
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

bool PanelIconStyleDefinition::EntryOverride::isEmpty() const
{
    return customGlyph.isEmpty() && customLabel.isEmpty() &&
        !tileEnabled.has_value() && styleReference.isEmpty() &&
        animationProfileReference.isEmpty() && extensions.isEmpty();
}

QVariantMap PanelIconStyleDefinition::EntryOverride::toVariantMap() const
{
    QVariantMap result;
    insertIfNotEmpty(&result, QStringLiteral("customGlyph"), customGlyph);
    insertIfNotEmpty(&result, QStringLiteral("customLabel"), customLabel);
    if (tileEnabled.has_value())
    {
        result.insert(QStringLiteral("tileEnabled"), *tileEnabled);
    }
    insertIfNotEmpty(&result, QStringLiteral("styleReference"), styleReference);
    insertIfNotEmpty(
        &result,
        QStringLiteral("animationProfileReference"),
        animationProfileReference);
    insertIfNotEmpty(&result, QStringLiteral("extensions"), extensions);
    return result;
}

std::optional<PanelIconStyleDefinition::EntryOverride>
PanelIconStyleDefinition::EntryOverride::fromVariantMap(
    const QVariantMap &record,
    QString *errorMessage)
{
    static const QSet<QString> allowedFields{
        QStringLiteral("customGlyph"),
        QStringLiteral("customLabel"),
        QStringLiteral("tileEnabled"),
        QStringLiteral("styleReference"),
        QStringLiteral("animationProfileReference"),
        QStringLiteral("extensions"),
    };
    for (auto it = record.cbegin(); it != record.cend(); ++it)
    {
        if (!allowedFields.contains(it.key()))
        {
            setError(errorMessage, QStringLiteral("unknown icon override field: %1")
                .arg(it.key()));
            return std::nullopt;
        }
    }

    EntryOverride result;
    const auto readString = [&record, errorMessage](
        const QString &key,
        qsizetype maximumBytes,
        QString *target)
    {
        if (!record.contains(key))
        {
            return true;
        }
        const QVariant value = record.value(key);
        if (value.metaType().id() != QMetaType::QString)
        {
            setError(errorMessage, QStringLiteral("icon override field '%1' must be a string")
                .arg(key));
            return false;
        }
        const QString normalized = value.toString().trimmed();
        if (normalized.toUtf8().size() > maximumBytes ||
            containsControlCharacter(normalized))
        {
            setError(errorMessage, QStringLiteral("icon override field '%1' is invalid")
                .arg(key));
            return false;
        }
        *target = normalized;
        return true;
    };
    if (!readString(QStringLiteral("customGlyph"), 4096, &result.customGlyph) ||
        !readString(QStringLiteral("customLabel"), 1024, &result.customLabel) ||
        !readString(QStringLiteral("styleReference"), 256,
                    &result.styleReference) ||
        !readString(QStringLiteral("animationProfileReference"), 256,
                    &result.animationProfileReference))
    {
        return std::nullopt;
    }
    if (!validIconOverrideReference(result.styleReference) ||
        !validIconOverrideReference(result.animationProfileReference))
    {
        setError(errorMessage, QStringLiteral(
            "icon override style or animation reference is invalid"));
        return std::nullopt;
    }
    if (record.contains(QStringLiteral("tileEnabled")))
    {
        const QVariant value = record.value(QStringLiteral("tileEnabled"));
        if (value.metaType().id() != QMetaType::Bool)
        {
            setError(errorMessage, QStringLiteral(
                "icon override field 'tileEnabled' must be a boolean"));
            return std::nullopt;
        }
        result.tileEnabled = value.toBool();
    }
    if (record.contains(QStringLiteral("extensions")))
    {
        const QVariant value = record.value(QStringLiteral("extensions"));
        if (value.metaType().id() != QMetaType::QVariantMap)
        {
            setError(errorMessage, QStringLiteral(
                "icon override field 'extensions' must be a map"));
            return std::nullopt;
        }
        result.extensions = value.toMap();
    }
    setError(errorMessage, QString{});
    return result;
}

std::optional<PanelLayoutKind> panelLayoutKindFromName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    for (const auto &[kind, candidate] : panelLayoutNames())
    {
        if (normalized == QLatin1String(candidate))
        {
            return kind;
        }
    }
    return std::nullopt;
}

QString panelLayoutKindName(PanelLayoutKind kind)
{
    for (const auto &[candidate, name] : panelLayoutNames())
    {
        if (candidate == kind)
        {
            return QString::fromLatin1(name);
        }
    }
    return QString{};
}

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
    if (record.contains(QStringLiteral("settingsRevision")))
    {
        bool revisionOk = false;
        definition.settingsRevision = record.value(
            QStringLiteral("settingsRevision")).toString().toULongLong(&revisionOk);
        if (!revisionOk)
        {
            setError(errorMessage, QStringLiteral("panel settings revision is invalid"));
            return std::nullopt;
        }
    }

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
    definition.host.nativePanelId = normalized(
        QStringLiteral("nativePanelId"), definition.host.nativePanelId).toInt();
    definition.host.nativeControlAppletId = normalized(
        QStringLiteral("nativeControlAppletId"),
        definition.host.nativeControlAppletId).toInt();
    definition.host.nativeDockAppletId = normalized(
        QStringLiteral("nativeDockAppletId"), definition.host.nativeDockAppletId).toInt();
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
    definition.host.freeDesktopContainmentId = normalized(
        QStringLiteral("freeDesktopContainmentId"),
        definition.host.freeDesktopContainmentId).toInt();
    definition.host.freeDockAppletId = normalized(
        QStringLiteral("freeDockAppletId"), definition.host.freeDockAppletId).toInt();
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
    definition.content.entryOrder = normalized(
        QStringLiteral("contentOrder"), definition.content.entryOrder).toStringList();
    definition.content.entryOrder = definition.content.canonicalEntryOrder();
    definition.content.kdeWidgets = normalized(
        QStringLiteral("kdeWidgets"), definition.content.kdeWidgets).toStringList();
    definition.content.acceptDrops = normalized(
        QStringLiteral("acceptDrops"), definition.content.acceptDrops).toBool();
    definition.content.folderLayout = normalized(
        QStringLiteral("folderLayout"), definition.content.folderLayout).toString();
    definition.content.folderFanOpening = normalized(
        QStringLiteral("folderFanOpening"), definition.content.folderFanOpening).toInt();
    definition.content.folderStackLength = normalized(
        QStringLiteral("folderStackLength"), definition.content.folderStackLength).toInt();
    definition.content.folderRingSize = normalized(
        QStringLiteral("folderRingSize"), definition.content.folderRingSize).toString();
    definition.content.folderSpeed = normalized(
        QStringLiteral("folderSpeed"), definition.content.folderSpeed).toInt();
    definition.content.folderEasing = normalized(
        QStringLiteral("folderEasing"), definition.content.folderEasing).toString();
    definition.content.folderExpandOnClick = normalized(
        QStringLiteral("folderExpandOnClick"),
        definition.content.folderExpandOnClick).toBool();
    definition.content.folderShowNames = normalized(
        QStringLiteral("folderShowNames"), definition.content.folderShowNames).toBool();
    definition.content.showBadges = normalized(QStringLiteral("showBadges"), definition.content.showBadges).toBool();
    definition.content.showProgress = normalized(QStringLiteral("showProgress"), definition.content.showProgress).toBool();
    definition.content.showTemporaryStatus = normalized(QStringLiteral("showTemporaryStatus"), definition.content.showTemporaryStatus).toBool();

    if (record.contains(QStringLiteral("segments")))
    {
        const QVariant value = record.value(QStringLiteral("segments"));
        if (value.metaType().id() != QMetaType::QVariantList || value.toList().isEmpty()
            || value.toList().size() > PanelSegmentDefinition::MaximumSegments)
        {
            setError(errorMessage, QStringLiteral("segments must be a list of 1 to 16 records"));
            return std::nullopt;
        }
        definition.segments.clear();
        for (const QVariant &row : value.toList())
        {
            if (row.metaType().id() != QMetaType::QVariantMap)
            {
                setError(errorMessage, QStringLiteral("each segment must be a record"));
                return std::nullopt;
            }
            const auto segment = PanelSegmentDefinition::fromVariantMap(row.toMap(), errorMessage);
            if (!segment) return std::nullopt;
            definition.segments.append(*segment);
        }
        std::sort(definition.segments.begin(), definition.segments.end(),
                  [](const auto &first, const auto &second) { return first.order < second.order; });
    }

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

    // Normalized through the schema rather than merely lower-cased, so a
    // legacy record holding an empty or unknown value resolves to the declared
    // default instead of reaching a renderer as a state it cannot draw.
    definition.presentation.mode = normalized(
        QStringLiteral("presentationMode"),
        definition.presentation.mode).toString();
    definition.presentation.trigger = normalized(
        QStringLiteral("presentationTrigger"),
        definition.presentation.trigger).toString();
    definition.presentation.collapseAxis = normalized(
        QStringLiteral("collapseAxis"),
        definition.presentation.collapseAxis).toString();
    definition.presentation.collapseMechanism = normalized(
        QStringLiteral("collapseMechanism"),
        definition.presentation.collapseMechanism).toString();
    definition.presentation.revealHandle = normalized(
        QStringLiteral("revealHandle"),
        definition.presentation.revealHandle).toString();

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
    definition.layout.rotationMode = normalized(
        QStringLiteral("panelRotationMode"), definition.layout.rotationMode).toString();
    definition.layout.rotationSpeed = normalized(
        QStringLiteral("panelRotationSpeed"), definition.layout.rotationSpeed).toReal();
    definition.layout.rotationTrigger = normalized(
        QStringLiteral("panelRotationTrigger"), definition.layout.rotationTrigger).toString();
    // A record saved before "Continuous motion moves" existed keeps the
    // motion it had (ADREP-TASK-002, PD-25): a panel that rotated turns as a
    // whole; any other moves its items, as a new panel does.
    definition.layout.motionTarget = record.contains(QStringLiteral("panelMotionTarget"))
        ? normalized(QStringLiteral("panelMotionTarget"), definition.layout.motionTarget).toString()
        : definition.layout.rotationMode == QStringLiteral("none")
            ? QStringLiteral("items") : QStringLiteral("panel");
    definition.layout.travelSpeed = normalized(
        QStringLiteral("panelTravelSpeed"), definition.layout.travelSpeed).toReal();
    definition.layout.scrollSensitivity = normalized(
        QStringLiteral("scrollSensitivity"), definition.layout.scrollSensitivity).toReal();

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
    definition.surface.glowIntensity = normalized(
        QStringLiteral("glowIntensity"),
        definition.surface.glowIntensity).toReal();
    definition.surface.border = record.value(QStringLiteral("border")).toMap();
    definition.surface.glow = record.value(QStringLiteral("glow")).toMap();
    definition.surface.shadow = record.value(QStringLiteral("shadow")).toMap();
    definition.surface.blur = record.value(QStringLiteral("blur")).toMap();
    definition.surface.parameters2D = record.value(QStringLiteral("surface2D")).toMap();
    for (const auto &[key, fallback] :
         {std::pair{"previousFlatLook", QVariant(QVariantMap{})},
          std::pair{"sparkleIntensity", QVariant(0.0)}}) {
        if (record.contains(QLatin1String(key)))
            definition.surface.parameters2D.insert(QLatin1String(key),
                normalized(QLatin1String(key), fallback));
    }
    definition.surface.parameters2_5D = record.value(QStringLiteral("surface2_5D")).toMap();
    definition.surface.parameters3D = record.value(QStringLiteral("surface3D")).toMap();
    if (record.contains(QStringLiteral("scene3DQuality")))
        definition.surface.parameters3D.insert(QStringLiteral("quality"),
            normalized(QStringLiteral("scene3DQuality"), QStringLiteral("medium")));
    if (record.contains(QStringLiteral("scene3DCameraPitch")))
        definition.surface.parameters3D.insert(QStringLiteral("cameraPitch"),
            normalized(QStringLiteral("scene3DCameraPitch"), 25.0));
    for (const auto &[key, parameter, fallback] :
         {std::tuple{"scene3DCameraYaw", "cameraYaw", QVariant(10.0)},
          std::tuple{"scene3DThickness", "thickness", QVariant(1.0)},
          std::tuple{"scene3DIconElevation", "iconElevation", QVariant(0.3)},
          std::tuple{"scene3DRoll", "roll", QVariant(0.0)},
          std::tuple{"scene3DPositionX", "positionX", QVariant(0.0)},
          std::tuple{"scene3DPositionY", "positionY", QVariant(0.0)},
          std::tuple{"scene3DPositionZ", "positionZ", QVariant(0.0)},
          std::tuple{"scene3DScale", "scale", QVariant(1.0)},
          std::tuple{"scene3DFieldOfView", "fieldOfView", QVariant(40.0)},
          std::tuple{"scene3DKeyLight", "keyLightBrightness", QVariant(1.0)},
          std::tuple{"scene3DFillLight", "fillLightBrightness", QVariant(0.4)},
          std::tuple{"scene3DTransitions", "transitions", QVariant(true)},
          std::tuple{"scene3DFloat", "float", QVariant(false)},
          std::tuple{"scene3DBand", "band", QVariant(0.11)},
          std::tuple{"scene3DBend", "bend", QVariant(0.0)},
          std::tuple{"scene3DFold", "fold", QVariant(0.0)},
          std::tuple{"scene3DColor", "color", QVariant(QString{})},
          std::tuple{"scene3DMaterial", "material", QVariant(QStringLiteral("theme"))},
          std::tuple{"scene3DTexture", "texture", QVariant(QStringLiteral("theme"))}}) {
        if (record.contains(QLatin1String(key)))
            definition.surface.parameters3D.insert(QLatin1String(parameter),
                normalized(QLatin1String(key), fallback));
    }
    if (record.contains(QStringLiteral("bakedTilt")))
        definition.surface.parameters2_5D.insert(QStringLiteral("tilt"),
            normalized(QStringLiteral("bakedTilt"), 0.0));
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

    const bool hasExplicitIconStyle = record.contains(QStringLiteral("iconStyle"));
    setString(QStringLiteral("iconStyle"), &definition.iconStyle.styleReference);
    setString(QStringLiteral("iconThemeId"), &definition.iconStyle.themeId);
    if (!hasExplicitIconStyle && !definition.iconStyle.themeId.isEmpty())
    {
        definition.iconStyle.styleReference = definition.iconStyle.themeId;
    }
    if (definition.iconStyle.styleReference.isEmpty())
    {
        definition.iconStyle.styleReference = QStringLiteral("plain-original");
    }
    definition.iconStyle.themeId = definition.iconStyle.styleReference;
    definition.iconStyle.shape = normalized(
        QStringLiteral("iconShape"), definition.iconStyle.shape).toString();
    definition.iconStyle.size = normalized(
        QStringLiteral("iconSize"), definition.iconStyle.size).toInt();
    definition.iconStyle.spacing = normalized(
        QStringLiteral("spacing"), definition.iconStyle.spacing).toReal();
    definition.iconStyle.tilesEnabled = normalized(QStringLiteral("iconTilesEnabled"), definition.iconStyle.tilesEnabled).toBool();
    definition.iconStyle.tileMode = normalized(QStringLiteral("iconTileMode"), definition.iconStyle.tileMode).toString();
    definition.iconStyle.tileColor = normalized(QStringLiteral("iconTileColor"), definition.iconStyle.tileColor).toString();
    definition.iconStyle.tileOpacity = normalized(QStringLiteral("iconTileOpacity"), definition.iconStyle.tileOpacity).toReal();
    definition.iconStyle.tileBorderColor = normalized(QStringLiteral("iconTileBorderColor"), definition.iconStyle.tileBorderColor).toString();
    definition.iconStyle.tileBorderWidth = normalized(QStringLiteral("iconTileBorderWidth"), definition.iconStyle.tileBorderWidth).toReal();
    definition.iconStyle.globalDefaults = record.value(
        QStringLiteral("iconGlobalDefaults")).toMap();
    const QVariant overridesValue = record.value(QStringLiteral("iconOverrides"));
    if (record.contains(QStringLiteral("iconOverrides")) &&
        overridesValue.metaType().id() != QMetaType::QVariantMap)
    {
        setError(errorMessage, QStringLiteral("icon overrides must be a map"));
        return std::nullopt;
    }
    const QVariantMap overrides = overridesValue.toMap();
    if (overrides.size() > 512)
    {
        setError(errorMessage, QStringLiteral("too many icon overrides"));
        return std::nullopt;
    }
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
    {
        if (!validIconOverrideIdentity(it.key()))
        {
            setError(errorMessage, QStringLiteral("invalid icon override identity: %1")
                .arg(it.key()));
            return std::nullopt;
        }
        if (it.value().metaType().id() != QMetaType::QVariantMap)
        {
            setError(errorMessage, QStringLiteral("icon override '%1' must be a map")
                .arg(it.key()));
            return std::nullopt;
        }
        QString overrideError;
        const auto parsed = PanelIconStyleDefinition::EntryOverride::fromVariantMap(
            it.value().toMap(), &overrideError);
        if (!parsed.has_value())
        {
            setError(errorMessage, QStringLiteral("icon override '%1': %2")
                .arg(it.key(), overrideError));
            return std::nullopt;
        }
        if (!parsed->isEmpty())
        {
            definition.iconStyle.perEntryOverrides.insert(it.key(), *parsed);
        }
    }

    definition.motion.iconProfile = normalized(
        QStringLiteral("iconAnimation"), definition.motion.iconProfile).toString();
    definition.motion.trigger = normalized(
        QStringLiteral("animationTrigger"), definition.motion.trigger).toString();
    definition.motion.speed = normalized(
        QStringLiteral("animationSpeed"), definition.motion.speed).toReal();
    definition.motion.intensity = normalized(
        QStringLiteral("animationIntensity"), definition.motion.intensity).toReal();
    definition.motion.magnifyRadius = normalized(
        QStringLiteral("magnificationRadius"),
        definition.motion.magnifyRadius).toReal();
    definition.motion.magnifyFalloff = normalized(
        QStringLiteral("magnificationFalloff"),
        definition.motion.magnifyFalloff).toString();
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
        if (!isDurableLegacyKey(it.key()) &&
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

QVariantMap PanelSegmentDefinition::toVariantMap() const
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("source"), source},
        {QStringLiteral("order"), order}, {QStringLiteral("entryIds"), entryIds},
        {QStringLiteral("background"), background}, {QStringLiteral("color"), color},
        {QStringLiteral("padding"), padding}, {QStringLiteral("spacing"), spacing},
        {QStringLiteral("corners"), corners}, {QStringLiteral("presentation"), presentation},
        {QStringLiteral("motionProfile"), motionProfile}};
}

std::optional<PanelSegmentDefinition> PanelSegmentDefinition::fromVariantMap(
    const QVariantMap &record, QString *errorMessage)
{
    PanelSegmentDefinition result;
    const QVariantMap defaults = result.toVariantMap();
    const auto fail = [errorMessage](const QString &field) -> std::optional<PanelSegmentDefinition> {
        setError(errorMessage, QStringLiteral("invalid segment field: %1").arg(field));
        return std::nullopt;
    };
    for (auto it = record.cbegin(); it != record.cend(); ++it)
        if (!defaults.contains(it.key())) return fail(it.key());
    for (const QString &key : {QStringLiteral("id"), QStringLiteral("source"),
         QStringLiteral("background"), QStringLiteral("color"), QStringLiteral("corners"),
         QStringLiteral("presentation"), QStringLiteral("motionProfile")})
        if (record.contains(key) && record.value(key).metaType().id() != QMetaType::QString)
            return fail(key);
    result.id = record.value(QStringLiteral("id")).toString();
    static const QRegularExpression identifier(QStringLiteral("^[a-z][a-z0-9-]{0,47}$"));
    if (!identifier.match(result.id).hasMatch()) return fail(QStringLiteral("id"));
    result.source = record.value(QStringLiteral("source"), result.source).toString();
    if (!QStringList{QStringLiteral("inherited"), QStringLiteral("launcher"), QStringLiteral("tasks"),
         QStringLiteral("custom"), QStringLiteral("status")}.contains(result.source))
        return fail(QStringLiteral("source"));
    result.background = record.value(QStringLiteral("background"), result.background).toString();
    if (!QStringList{QStringLiteral("inherited"), QStringLiteral("solid"), QStringLiteral("none")}.contains(result.background))
        return fail(QStringLiteral("background"));
    result.color = record.value(QStringLiteral("color"), result.color).toString();
    static const QRegularExpression colorPattern(QStringLiteral("^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$"));
    if (!colorPattern.match(result.color).hasMatch()) return fail(QStringLiteral("color"));
    result.corners = record.value(QStringLiteral("corners"), result.corners).toString();
    if (!QStringList{QStringLiteral("inherited"), QStringLiteral("square"),
         QStringLiteral("rounded"), QStringLiteral("capsule")}.contains(result.corners))
        return fail(QStringLiteral("corners"));
    result.presentation = record.value(QStringLiteral("presentation"), result.presentation).toString();
    if (result.presentation != QStringLiteral("open") && result.presentation != QStringLiteral("closed"))
        return fail(QStringLiteral("presentation"));
    result.motionProfile = record.value(QStringLiteral("motionProfile"), result.motionProfile).toString();
    if (result.motionProfile.size() > 96 || result.motionProfile.contains(QChar::Null))
        return fail(QStringLiteral("motionProfile"));
    const auto integer = [&record](const QString &key, int *target, int minimum, int maximum) {
        if (!record.contains(key)) return true;
        const QVariant value = record.value(key);
        if (value.metaType().id() == QMetaType::Bool || value.metaType().id() == QMetaType::QString) return false;
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok || !std::isfinite(number) || number != std::floor(number)
            || number < minimum || number > maximum) return false;
        *target = static_cast<int>(number);
        return true;
    };
    if (!integer(QStringLiteral("order"), &result.order, 0, MaximumSegments - 1)) return fail(QStringLiteral("order"));
    if (!integer(QStringLiteral("padding"), &result.padding, -1, 64)) return fail(QStringLiteral("padding"));
    if (!integer(QStringLiteral("spacing"), &result.spacing, -1, 64)) return fail(QStringLiteral("spacing"));
    if (record.contains(QStringLiteral("entryIds")))
    {
        const QVariant value = record.value(QStringLiteral("entryIds"));
        if (value.metaType().id() != QMetaType::QStringList && value.metaType().id() != QMetaType::QVariantList)
            return fail(QStringLiteral("entryIds"));
        QSet<QString> seen;
        for (const QVariant &entry : value.toList())
        {
            const QString id = entry.toString();
            if (entry.metaType().id() != QMetaType::QString || id.isEmpty() || id.size() > 8192
                || id.contains(QChar::Null) || seen.contains(id) || seen.size() >= 512)
                return fail(QStringLiteral("entryIds"));
            seen.insert(id);
            result.entryIds.append(id);
        }
    }
    const QStringList statusIds{QStringLiteral("status:battery"), QStringLiteral("status:network"),
        QStringLiteral("status:cpu"), QStringLiteral("status:memory"),
        QStringLiteral("status:disk"), QStringLiteral("status:gpu")};
    for (const QString &id : result.entryIds)
    {
        if ((result.source == QStringLiteral("status") && !statusIds.contains(id))
            || (result.source != QStringLiteral("status") && id.startsWith(QStringLiteral("status:"))))
            return fail(QStringLiteral("entryIds"));
    }
    setError(errorMessage, QString{});
    return result;
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
    if (segments.isEmpty() || segments.size() > PanelSegmentDefinition::MaximumSegments)
    {
        setError(errorMessage, QStringLiteral("a panel must contain 1 to 16 segments"));
        return false;
    }
    QSet<QString> segmentIds;
    QSet<QString> assignedEntries;
    QSet<int> segmentOrders;
    for (const PanelSegmentDefinition &segment : segments)
    {
        if (!PanelSegmentDefinition::fromVariantMap(segment.toVariantMap(), errorMessage)) return false;
        if (segmentIds.contains(segment.id) || segmentOrders.contains(segment.order)
            || segment.order >= segments.size())
        {
            setError(errorMessage, QStringLiteral("duplicate segment identity or invalid segment order"));
            return false;
        }
        segmentIds.insert(segment.id);
        segmentOrders.insert(segment.order);
        for (const QString &entry : segment.entryIds)
        {
            if (assignedEntries.contains(entry))
            {
                setError(errorMessage, QStringLiteral("entry belongs to multiple segments: %1").arg(entry));
                return false;
            }
            assignedEntries.insert(entry);
        }
    }
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
    if (iconStyle.perEntryOverrides.size() > 512)
    {
        setError(errorMessage, QStringLiteral("too many icon overrides"));
        return false;
    }
    for (auto it = iconStyle.perEntryOverrides.cbegin();
         it != iconStyle.perEntryOverrides.cend(); ++it)
    {
        if (!validIconOverrideIdentity(it.key()) || it->isEmpty())
        {
            setError(errorMessage, QStringLiteral("icon override is invalid: %1")
                .arg(it.key()));
            return false;
        }
        QString overrideError;
        if (!PanelIconStyleDefinition::EntryOverride::fromVariantMap(
                it->toVariantMap(), &overrideError).has_value())
        {
            setError(errorMessage, overrideError);
            return false;
        }
    }
    setError(errorMessage, QString{});
    return true;
}

QVariantMap PanelDefinition::toLegacyMap() const
{
    QVariantMap record = persistentExtensions(extensions);
    record.insert(QStringLiteral("schemaVersion"), CurrentSchemaVersion);
    record.insert(
        QStringLiteral("settingsRevision"),
        QVariant::fromValue<qulonglong>(settingsRevision));
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
    record.insert(QStringLiteral("contentOrder"), content.canonicalEntryOrder());
    record.insert(QStringLiteral("kdeWidgets"), content.kdeWidgets);
    record.insert(QStringLiteral("acceptDrops"), content.acceptDrops);
    record.insert(QStringLiteral("folderLayout"), content.folderLayout);
    record.insert(QStringLiteral("folderFanOpening"), content.folderFanOpening);
    record.insert(QStringLiteral("folderStackLength"), content.folderStackLength);
    record.insert(QStringLiteral("folderRingSize"), content.folderRingSize);
    record.insert(QStringLiteral("folderSpeed"), content.folderSpeed);
    record.insert(QStringLiteral("folderEasing"), content.folderEasing);
    record.insert(QStringLiteral("folderExpandOnClick"), content.folderExpandOnClick);
    record.insert(QStringLiteral("folderShowNames"), content.folderShowNames);
    record.insert(QStringLiteral("showBadges"), content.showBadges);
    record.insert(QStringLiteral("showProgress"), content.showProgress);
    record.insert(QStringLiteral("showTemporaryStatus"), content.showTemporaryStatus);
    auto orderedSegments = segments;
    std::sort(orderedSegments.begin(), orderedSegments.end(),
              [](const auto &first, const auto &second) { return first.order < second.order; });
    QVariantList segmentRecords;
    for (const auto &segment : orderedSegments)
        segmentRecords.append(segment.toVariantMap());
    record.insert(QStringLiteral("segments"), segmentRecords);

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
    insertIfNotEmpty(
        &record,
        QStringLiteral("presentationTrigger"),
        presentation.trigger);
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
    record.insert(QStringLiteral("panelRotationMode"), layout.rotationMode);
    record.insert(QStringLiteral("panelRotationSpeed"), layout.rotationSpeed);
    record.insert(QStringLiteral("panelRotationTrigger"), layout.rotationTrigger);
    record.insert(QStringLiteral("panelMotionTarget"), layout.motionTarget);
    record.insert(QStringLiteral("panelTravelSpeed"), layout.travelSpeed);
    record.insert(QStringLiteral("scrollSensitivity"), layout.scrollSensitivity);

    insertIfNotEmpty(&record, QStringLiteral("rendererTier"), surface.rendererTier);
    insertIfNotEmpty(&record, QStringLiteral("panelThemeId"), surface.panelThemeId);
    insertIfNotEmpty(&record, QStringLiteral("completeThemeId"), surface.completeThemeId);
    record.insert(QStringLiteral("appearance"), surface.appearance);
    record.insert(QStringLiteral("shape"), surface.shape);
    record.insert(QStringLiteral("opacity"), surface.opacity);
    record.insert(QStringLiteral("color"), surface.color);
    record.insert(QStringLiteral("glowIntensity"), surface.glowIntensity);
    insertIfNotEmpty(&record, QStringLiteral("border"), surface.border);
    insertIfNotEmpty(&record, QStringLiteral("glow"), surface.glow);
    insertIfNotEmpty(&record, QStringLiteral("shadow"), surface.shadow);
    insertIfNotEmpty(&record, QStringLiteral("blur"), surface.blur);
    insertIfNotEmpty(&record, QStringLiteral("surface2D"), surface.parameters2D);
    for (const auto *key : {"previousFlatLook", "sparkleIntensity"})
        if (surface.parameters2D.contains(QLatin1String(key)))
            record.insert(QLatin1String(key), surface.parameters2D.value(QLatin1String(key)));
    insertIfNotEmpty(&record, QStringLiteral("surface2_5D"), surface.parameters2_5D);
    insertIfNotEmpty(&record, QStringLiteral("surface3D"), surface.parameters3D);
    if (surface.parameters3D.contains(QStringLiteral("quality")))
        record.insert(QStringLiteral("scene3DQuality"),
            surface.parameters3D.value(QStringLiteral("quality")));
    if (surface.parameters3D.contains(QStringLiteral("cameraPitch")))
        record.insert(QStringLiteral("scene3DCameraPitch"),
            surface.parameters3D.value(QStringLiteral("cameraPitch")));
    for (const auto &[key, parameter] :
         {std::pair{"scene3DCameraYaw", "cameraYaw"},
          std::pair{"scene3DThickness", "thickness"},
          std::pair{"scene3DIconElevation", "iconElevation"},
          std::pair{"scene3DRoll", "roll"},
          std::pair{"scene3DPositionX", "positionX"},
          std::pair{"scene3DPositionY", "positionY"},
          std::pair{"scene3DPositionZ", "positionZ"},
          std::pair{"scene3DScale", "scale"},
          std::pair{"scene3DFieldOfView", "fieldOfView"},
          std::pair{"scene3DKeyLight", "keyLightBrightness"},
          std::pair{"scene3DFillLight", "fillLightBrightness"},
          std::pair{"scene3DTransitions", "transitions"},
          std::pair{"scene3DFloat", "float"},
          std::pair{"scene3DBand", "band"},
          std::pair{"scene3DBend", "bend"},
          std::pair{"scene3DFold", "fold"},
          std::pair{"scene3DColor", "color"},
          std::pair{"scene3DMaterial", "material"},
          std::pair{"scene3DTexture", "texture"}}) {
        if (surface.parameters3D.contains(QLatin1String(parameter)))
            record.insert(QLatin1String(key), surface.parameters3D.value(QLatin1String(parameter)));
    }
    if (surface.parameters2_5D.contains(QStringLiteral("tilt")))
        record.insert(QStringLiteral("bakedTilt"),
            surface.parameters2_5D.value(QStringLiteral("tilt")));
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
    insertIfNotEmpty(&record, QStringLiteral("iconThemeId"), iconStyle.styleReference);
    record.insert(QStringLiteral("iconShape"), iconStyle.shape);
    record.insert(QStringLiteral("iconSize"), iconStyle.size);
    record.insert(QStringLiteral("spacing"), iconStyle.spacing);
    record.insert(QStringLiteral("iconTilesEnabled"), iconStyle.tilesEnabled);
    record.insert(QStringLiteral("iconTileMode"), iconStyle.tileMode);
    record.insert(QStringLiteral("iconTileColor"), iconStyle.tileColor);
    record.insert(QStringLiteral("iconTileOpacity"), iconStyle.tileOpacity);
    record.insert(QStringLiteral("iconTileBorderColor"), iconStyle.tileBorderColor);
    record.insert(QStringLiteral("iconTileBorderWidth"), iconStyle.tileBorderWidth);
    insertIfNotEmpty(
        &record,
        QStringLiteral("iconGlobalDefaults"),
        iconStyle.globalDefaults);
    QVariantMap serializedIconOverrides;
    for (auto it = iconStyle.perEntryOverrides.cbegin();
         it != iconStyle.perEntryOverrides.cend(); ++it)
    {
        serializedIconOverrides.insert(it.key(), it->toVariantMap());
    }
    insertIfNotEmpty(
        &record,
        QStringLiteral("iconOverrides"),
        serializedIconOverrides);

    record.insert(QStringLiteral("iconAnimation"), motion.iconProfile);
    record.insert(QStringLiteral("animationTrigger"), motion.trigger);
    record.insert(QStringLiteral("animationSpeed"), motion.speed);
    record.insert(QStringLiteral("animationIntensity"), motion.intensity);
    record.insert(QStringLiteral("magnificationRadius"), motion.magnifyRadius);
    record.insert(QStringLiteral("magnificationFalloff"), motion.magnifyFalloff);
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
    record.insert(
        QStringLiteral("settingsRevision"),
        QString::number(settingsRevision));
    const QVariantMap filteredExtensions = persistentExtensions(extensions);
    for (auto it = filteredExtensions.cbegin(); it != filteredExtensions.cend(); ++it)
    {
        if (!isDurableLegacyKey(it.key()))
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

QString PanelContent::urlEntryId(const QString &url)
{
    const QUrl parsed(url.trimmed());
    if (!parsed.isValid() || parsed.isEmpty())
    {
        return {};
    }
    return QStringLiteral("free-url:") + QString::fromUtf8(parsed.toEncoded());
}

bool PanelContent::isUrlEntryId(const QString &entryId)
{
    return entryId.startsWith(QStringLiteral("free-url:"));
}

QString PanelContent::urlFromEntryId(const QString &entryId)
{
    if (!isUrlEntryId(entryId))
    {
        return {};
    }
    const QUrl parsed = QUrl::fromEncoded(entryId.mid(9).toUtf8());
    return parsed.isValid() && !parsed.isEmpty() ? parsed.toString() : QString{};
}

QStringList PanelContent::knownEntryIds() const
{
    QStringList known;
    known.reserve(applicationIds.size() + urls.size());
    for (const QString &applicationId : applicationIds)
    {
        const QString trimmed = applicationId.trimmed();
        if (!trimmed.isEmpty() && !isUrlEntryId(trimmed) && !known.contains(trimmed))
        {
            known.append(trimmed);
        }
    }
    for (const QString &url : urls)
    {
        const QString entryId = urlEntryId(url);
        if (!entryId.isEmpty() && !known.contains(entryId))
        {
            known.append(entryId);
        }
    }
    return known;
}

QStringList PanelContent::canonicalEntryOrder() const
{
    const QStringList known = knownEntryIds();
    QStringList result;
    result.reserve(known.size());
    for (const QString &entryId : entryOrder)
    {
        const QString trimmed = entryId.trimmed();
        if (known.contains(trimmed) && !result.contains(trimmed))
        {
            result.append(trimmed);
        }
    }
    for (const QString &entryId : known)
    {
        if (!result.contains(entryId))
        {
            result.append(entryId);
        }
    }
    return result;
}

bool PanelDefinition::isDurableLegacyKey(const QString &key)
{
    return PanelSettingsSchema::isKnownPanelField(key);
}

QVariant PanelDefinition::normalizeLegacyValue(const QString &key, const QVariant &value)
{
    return PanelSettingsSchema::normalizePanelValue(key, value);
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
