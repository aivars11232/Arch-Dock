// PanelWindow: a panel's configuration and how it changes. Renderer
// configuration and capability resolution (what the panel's look and host can
// draw), the Panel Studio settings editor (fields, values, drafts and
// snapshots), and settings transactions, which change the registry and the
// panel's Plasma host together and roll both back when either fails.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../PanelVisibility.h"
#include "../ScreenIdentity.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "../model/PanelIconCapacity.h"
#include "../model/PanelNaming.h"
#include "../model/PanelSettingsSchema.h"
#include "../presets/PresetApplication.h"

#include <QDBusArgument>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QHash>
#include <QScreen>
#include <QSet>

#include <algorithm>
#include <utility>

using PanelWindowHelpers::plasmaScriptStringLiteral;
using PanelWindowHelpers::panelTypeNeedsDockApplet;

namespace
{
// A stored code word as a list shows it: "counter-clockwise" as
// "Counter clockwise".
QString readableChoice(const QString &choice)
{
    QString text = choice;
    text.replace(QLatin1Char('-'), QLatin1Char(' '));
    return text.isEmpty() ? text : text.left(1).toUpper() + text.mid(1);
}

// The settings an edge panel's Plasma panel itself carries: its edge,
// screen, size and place along the edge.
const QStringList &nativePlacementKeys()
{
    static const QStringList keys{
        QStringLiteral("edge"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("alignment"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("floatingMargin"),
        QStringLiteral("thickness"),
        QStringLiteral("lengthMode"),
        QStringLiteral("minimumLength"),
        QStringLiteral("maximumLength"),
    };
    return keys;
}

QVariantMap nativePlacementValues(const ArchDock::PanelDefinition &panel)
{
    const QVariantMap values = panel.toLegacyMap();
    QVariantMap placement;
    for (const QString &key : nativePlacementKeys())
        if (values.contains(key)) placement.insert(key, values.value(key));
    return placement;
}

// Layouts whose entries stand on a curved track, as LayoutEngine.curvedLayout()
// in the shared renderer decides: closed shapes, the star among them, and the
// open sweep paths.
bool curvedLayout(const QString &pathType)
{
    static const QStringList curved = {
        QStringLiteral("circular"), QStringLiteral("ring"), QStringLiteral("ellipse"),
        QStringLiteral("polygon"), QStringLiteral("triangle"), QStringLiteral("square"),
        QStringLiteral("pentagon"), QStringLiteral("hexagon"), QStringLiteral("octagon"),
        QStringLiteral("star"), QStringLiteral("arc"), QStringLiteral("semicircle"),
        QStringLiteral("fan"), QStringLiteral("radial")};
    return curved.contains(pathType);
}

// Whether light round a surface of this look makes sense: not round grass
// (Organic), marble (Pedestal) or a lime's peel, the natural materials (the
// owner, 2026-10-13: "where glow wouldn't really be practical, that setting
// ... should be invisible"). Glass, energy, circuitry and the tech metals
// glow.
bool glowFitsLook(const QString &look)
{
    // A picture is worn as the thing it shows, which mostly does not glow
    // (stone, wood, a mountain).
    const QString name = look.trimmed().toLower();
    return name != QStringLiteral("organic") && name != QStringLiteral("pedestal")
        && name != QStringLiteral("lime") && name != QStringLiteral("picture")
        && name != QStringLiteral("river");
}

// The looks a panel's surface is drawn in, its material and its texture
// ("none" for no texture): its flat look, or in 3D its platform's material
// and texture, "theme" being the flat look on a generated platform.
QStringList panelLooks(const ArchDock::PanelDefinition &candidate,
                       const std::optional<ArchDock::RendererTier> &tier)
{
    const QString flat = candidate.surface.appearance;
    if (tier != ArchDock::RendererTier::True3D)
        return {flat, flat};
    const QVariantMap &values = candidate.surface.parameters3D;
    const QString material = values.value(QStringLiteral("material"), QStringLiteral("theme")).toString();
    const QString texture = values.value(QStringLiteral("texture"), QStringLiteral("theme")).toString();
    const QString surface = material == QStringLiteral("theme") ? flat : material;
    return {surface, texture == QStringLiteral("theme") ? surface : texture};
}

// The same for a stand: its own material or the panel's, and its texture,
// the material's own, none, or a look's.
QStringList standLooks(const ArchDock::PanelDefinition &candidate,
                       const std::optional<ArchDock::RendererTier> &tier,
                       const QString &prefix)
{
    const QVariantMap &values = candidate.surface.parameters3D;
    const QString material = values.value(prefix + QStringLiteral("Material"), QStringLiteral("panel")).toString();
    const QString texture = values.value(prefix + QStringLiteral("Texture"), QStringLiteral("material")).toString();
    const QString surface = material == QStringLiteral("panel") ? panelLooks(candidate, tier).first() : material;
    return {surface, texture == QStringLiteral("material") ? surface : texture};
}

// Whether the panel is drawn with the picture it was given. A picture is
// worn in its own colours, so a colour setting changes nothing under it.
bool panelWearsPicture(const ArchDock::PanelDefinition &candidate,
                       const std::optional<ArchDock::RendererTier> &tier)
{
    return !candidate.surface.picture.isEmpty()
        && panelLooks(candidate, tier).contains(QStringLiteral("picture"));
}

// Whether a panel can wear a panel decoration: a flat panel in its flat
// look's place, a 3D panel standing on a generated platform in that
// platform's place.
bool decorationFits(const std::optional<ArchDock::RendererTier> &tier, bool generatedPlatform)
{
    return tier == ArchDock::RendererTier::Procedural2D
        || (tier == ArchDock::RendererTier::True3D && generatedPlatform);
}

// Whether it wears one: drawn in place of the look, the look's own settings
// draw nothing on it.
bool panelWearsDecoration(const ArchDock::PanelDefinition &candidate,
                          const std::optional<ArchDock::RendererTier> &tier, bool generatedPlatform)
{
    return decorationFits(tier, generatedPlatform)
        && !candidate.surface.parameters3D.value(QStringLiteral("panelDecoration")).toString().trimmed().isEmpty();
}

// The same for a stand wearing a picture of its own as its look.
bool standWearsOwnPicture(const ArchDock::PanelDefinition &candidate, const QString &prefix)
{
    const QVariantMap &values = candidate.surface.parameters3D;
    if (values.value(prefix + QStringLiteral("Picture")).toString().isEmpty())
        return false;
    return values.value(prefix + QStringLiteral("Material")).toString() == QStringLiteral("picture")
        || values.value(prefix + QStringLiteral("Texture")).toString() == QStringLiteral("picture");
}

bool looksGlow(const QStringList &looks)
{
    return glowFitsLook(looks.first()) && (looks.last() == QStringLiteral("none") || glowFitsLook(looks.last()));
}

// Whether the panel's surface takes light round it. Skins and baked
// artwork glow round their shape.
bool panelGlowFits(const ArchDock::PanelDefinition &candidate,
                   const std::optional<ArchDock::RendererTier> &tier)
{
    if (tier == ArchDock::RendererTier::Skinned2D || tier == ArchDock::RendererTier::Baked2_5D)
        return true;
    return looksGlow(panelLooks(candidate, tier));
}

bool standGlowFits(const ArchDock::PanelDefinition &candidate,
                   const std::optional<ArchDock::RendererTier> &tier,
                   const QString &prefix)
{
    const QVariantMap &values = candidate.surface.parameters3D;
    const QString material = values.value(prefix + QStringLiteral("Material"), QStringLiteral("panel")).toString();
    if (material == QStringLiteral("panel") && !panelGlowFits(candidate, tier))
        return false;
    return looksGlow(standLooks(candidate, tier, prefix));
}

// D-Bus variants preserve compound values as QDBusArgument. Decode only the
// bounded segment payload here; the model still validates every key and type.
QVariant decodedSegmentValue(QVariant value, int depth = 0)
{
    if (depth > 8)
        return {};
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return decodedSegmentValue(value.value<QDBusVariant>().variant(), depth + 1);
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
    {
        const auto argument = value.value<QDBusArgument>();
        const QString signature = argument.currentSignature();
        if (signature == QStringLiteral("av"))
            value = qdbus_cast<QVariantList>(argument);
        else if (signature == QStringLiteral("a{sv}"))
            value = qdbus_cast<QVariantMap>(argument);
        else if (signature == QStringLiteral("aa{sv}"))
        {
            QVariantList records;
            for (const auto &record : qdbus_cast<QList<QVariantMap>>(argument))
                records.append(record);
            value = records;
        }
        else
            return {};
    }
    if (value.metaType().id() == QMetaType::QVariantList)
    {
        auto list = value.toList();
        if (list.size() > 512)
            return {};
        for (auto &item : list)
            item = decodedSegmentValue(item, depth + 1);
        return list;
    }
    if (value.metaType().id() == QMetaType::QVariantMap)
    {
        auto map = value.toMap();
        if (map.size() > 16)
            return {};
        for (auto it = map.begin(); it != map.end(); ++it)
            it.value() = decodedSegmentValue(it.value(), depth + 1);
        return map;
    }
    return value;
}
}

QVariantMap PanelWindow::dockConfiguration(const QString &panelId) const
{
    QVariantMap configuration = m_settings.transactionSnapshot();
    const QVariantMap panel = m_panelRegistry.panelSnapshot(panelId);
    if (panel.isEmpty())
    {
        return {};
    }
    for (auto it = panel.cbegin(); it != panel.cend(); ++it)
    {
        configuration.insert(it.key(), it.value());
    }
    configuration.insert(QStringLiteral("panel"), panel);
    configuration.insert(
        QStringLiteral("globalSettings"), m_settings.transactionSnapshot());
    const QVariantMap capabilityResolution = resolvePanelCapabilities(panelId);
    configuration.insert(
        QStringLiteral("capabilityResolution"), capabilityResolution);
    configuration.insert(
        QStringLiteral("effectiveRendererTier"),
        capabilityResolution.value(QStringLiteral("renderer"))
            .toMap()
            .value(QStringLiteral("effectiveTier")));
    return configuration;
}

std::optional<ArchDock::PanelDefinition> PanelWindow::runtimePanelDefinition(
    const QString &panelId) const
{
    if (m_profileManager)
    {
        const auto preview = m_profileManager->previewDefinition(panelId);
        if (preview) return preview;
    }
    if (m_presetAudition)
    {
        const auto preview = m_presetAudition->previewDefinition(panelId);
        if (preview) return preview;
    }
    auto stored = m_panelRegistry.panelDefinition(panelId);
    // A draft made on settings that have since changed elsewhere waits for
    // Panel Studio to make it again on the new ones.
    if (stored && m_studioDraft && m_studioDraft->panel.identity.id == panelId &&
        m_studioDraft->baseRevision == stored->settingsRevision)
        return m_studioDraft->panel;
    return stored;
}

QVariantMap PanelWindow::drawnGlobalSettings() const
{
    QVariantMap globals = m_settings.transactionSnapshot();
    if (m_studioDraft) globals.insert(m_studioDraft->globals);
    return globals;
}

QVariantMap PanelWindow::panelRendererConfiguration(const QString &panelId) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        runtimePanelDefinition(panelId);
    if (!definition.has_value())
    {
        return {};
    }

    QVariantMap configuration = ArchDock::PanelSettingsSchema::runtimeValues(
        ArchDock::PanelSettingsFieldScope::Global,
        drawnGlobalSettings());
    const QVariantMap panelValues = ArchDock::PanelSettingsSchema::runtimeValues(
        ArchDock::PanelSettingsFieldScope::Panel,
        definition->toLegacyMap());
    for (auto it = panelValues.cbegin(); it != panelValues.cend(); ++it)
    {
        configuration.insert(it.key(), it.value());
    }
    // The panel shows its 3D gizmo only while Panel Studio holds a desktop
    // 3D edit of it.
    configuration.insert(QStringLiteral("sceneEditActive"), sceneEditActive(panelId));

    const QVariantMap capabilityResolution =
        m_panelRegistry.resolvePanelCapabilities(*definition).toVariantMap();
    configuration.insert(QStringLiteral("segmentCapabilities"),
        ArchDock::PanelCapabilityResolver::segmentCapabilities(*definition,
            m_panelRegistry.resolvePanelCapabilities(*definition), !m_systemStatus.availableSources().isEmpty()));
    configuration.insert(
        QStringLiteral("capabilityResolution"), capabilityResolution);
    configuration.insert(
        QStringLiteral("effectiveRendererTier"),
        capabilityResolution.value(QStringLiteral("renderer"))
            .toMap()
            .value(QStringLiteral("effectiveTier")));
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            *definition, &themeProjectionError);
    configuration.insert(
        QStringLiteral("themeDefinition"),
        themeProjection.value_or(QVariantMap{}));
    configuration.insert(
        QStringLiteral("themeProjectionStatus"),
        themeProjection.has_value()
            ? QStringLiteral("ready")
            : themeProjectionError.isEmpty()
                ? QStringLiteral("unavailable")
                : QStringLiteral("error"));
    configuration.insert(
        QStringLiteral("themeProjectionError"), themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            *definition, &iconStyleProjectionError);
    configuration.insert(
        QStringLiteral("iconStyleDefinition"),
        iconStyleProjection.value_or(QVariantMap{}));
    configuration.insert(
        QStringLiteral("iconStyleProjectionStatus"),
        iconStyleProjection.has_value()
            ? QStringLiteral("ready")
            : QStringLiteral("error"));
    configuration.insert(
        QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError);
    const QVariantMap animationResolution =
        m_panelRegistry.animationProfileResolution(definition->motion.iconProfile);
    configuration.insert(
        QStringLiteral("animationProfile"),
        animationResolution.value(QStringLiteral("profile")).toMap());
    configuration.insert(
        QStringLiteral("animationProfileFallbackApplied"),
        animationResolution.value(QStringLiteral("fallbackApplied")));
    configuration.insert(
        QStringLiteral("animationProfiles"),
        m_panelRegistry.animationProfileDefinitions());

    // A stable description of how this panel presents itself, published as one
    // record so a later Panel Preset can capture and restore it without having
    // to know which individual settings keys made it up. The id is derived
    // from the resolved values, so two panels that present identically carry
    // the same id and a preset can compare them.
    QStringList availableMechanisms;
    for (const QVariant &value :
         capabilityResolution.value(QStringLiteral("presentationMechanisms")).toList())
    {
        const QVariantMap decision = value.toMap();
        if (decision.value(QStringLiteral("available")).toBool())
        {
            availableMechanisms.append(
                decision.value(QStringLiteral("id")).toString());
        }
    }
    const QString profileId = QStringLiteral("%1:%2:%3:%4:%5")
        .arg(definition->presentation.mode,
             definition->presentation.collapseMechanism,
             definition->presentation.collapseAxis,
             definition->presentation.trigger,
             definition->presentation.revealHandle);
    configuration.insert(
        QStringLiteral("presentationProfile"),
        QVariantMap{
            {QStringLiteral("id"), profileId},
            {QStringLiteral("restingState"), definition->presentation.mode},
            {QStringLiteral("trigger"), definition->presentation.trigger},
            {QStringLiteral("mechanism"),
             definition->presentation.collapseMechanism},
            {QStringLiteral("axis"), definition->presentation.collapseAxis},
            {QStringLiteral("revealHandle"),
             definition->presentation.revealHandle},
            {QStringLiteral("openDelay"), definition->visibility.openDelay},
            {QStringLiteral("closeDelay"), definition->visibility.closeDelay},
            {QStringLiteral("availableMechanisms"), availableMechanisms},
        });
    return configuration;
}

std::optional<ArchDock::PanelDefinition>
PanelWindow::capabilityCandidateDefinition(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    if (!current.has_value())
    {
        return std::nullopt;
    }
    return capabilityCandidateDefinitionOn(*current, candidateValues);
}

std::optional<ArchDock::PanelDefinition>
PanelWindow::capabilityCandidateDefinitionOn(
    const ArchDock::PanelDefinition &current,
    const QVariantMap &candidateValues) const
{
    QString definitionError;
    for (auto iterator = candidateValues.cbegin();
         iterator != candidateValues.cend();
         ++iterator)
    {
        if (!ArchDock::PanelSettingsSchema::isTransactionPanelField(
                iterator.key()))
        {
            return std::nullopt;
        }
    }

    QVariantMap candidateRecord = current.toLegacyMap();
    for (auto iterator = candidateValues.cbegin();
         iterator != candidateValues.cend();
         ++iterator)
    {
        candidateRecord.insert(iterator.key(), iterator.key() == QStringLiteral("segments")
            ? decodedSegmentValue(iterator.value()) : iterator.value());
    }
    if (candidateValues.contains(QStringLiteral("screen")))
    {
        const int requestedScreen = candidateValues.value(
            QStringLiteral("screen")).toInt();
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (requestedScreen < 0 || requestedScreen >= screens.size())
        {
            return std::nullopt;
        }
        candidateRecord.insert(
            QStringLiteral("screenId"),
            ArchDock::persistentScreenId(screens.at(requestedScreen)));
    }
    candidateRecord.insert(
        QStringLiteral("settingsRevision"),
        QString::number(current.settingsRevision));
    const std::optional<ArchDock::PanelDefinition> candidate =
        ArchDock::PanelDefinition::fromLegacyMap(
            candidateRecord, &definitionError);
    return candidate;
}

QVariantMap PanelWindow::resolvePanelCapabilities(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> candidate =
        capabilityCandidateDefinition(panelId, candidateValues);
    if (!candidate.has_value())
    {
        ArchDock::CapabilityResolution unavailable;
        unavailable.reason = ArchDock::CapabilityReasonCode::InvalidCapabilityInput;
        return unavailable.toVariantMap();
    }

    return m_panelRegistry.resolvePanelCapabilities(*candidate).toVariantMap();
}

QVariantList PanelWindow::resolvedThemeDefinitions(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    return current ? resolvedThemeDefinitionsOn(*current, candidateValues) : QVariantList{};
}

QVariantList PanelWindow::resolvedThemeDefinitionsOn(
    const ArchDock::PanelDefinition &current,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> candidate =
        capabilityCandidateDefinitionOn(current, candidateValues);
    if (!candidate.has_value())
    {
        return {};
    }

    QVariantList result;
    const QVariantList themes = m_panelRegistry.themeDefinitions();
    result.reserve(themes.size());
    for (const QVariant &value : themes)
    {
        const QVariantMap catalogTheme = value.toMap();
        QVariantMap theme = catalogTheme;
        if (theme.contains(QStringLiteral("packageManifest")))
        {
            QString projectionError;
            const std::optional<QVariantMap> projection =
                m_panelRegistry.builtInThemeRuntimeProjection(
                    theme.value(QStringLiteral("id")).toString(),
                    &projectionError);
            if (!projection.has_value())
            {
                theme.insert(QStringLiteral("available"), false);
                theme.insert(
                    QStringLiteral("reasonCode"),
                    projectionError.isEmpty()
                        ? QStringLiteral("theme-package-unavailable")
                        : projectionError);
                theme.insert(
                    QStringLiteral("themeProjectionStatus"),
                    QStringLiteral("error"));
                theme.insert(
                    QStringLiteral("themeProjectionError"), projectionError);
                result.append(theme);
                continue;
            }
            for (auto iterator = projection->cbegin();
                 iterator != projection->cend(); ++iterator)
            {
                theme.insert(iterator.key(), iterator.value());
            }
            theme.insert(
                QStringLiteral("themeProjectionStatus"),
                QStringLiteral("ready"));
            theme.insert(
                QStringLiteral("themeProjectionError"), QString{});
        }
        if (!ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(theme)
                 .has_value())
        {
            continue;
        }

        // A card is resolved exactly as Load resolves it: the theme's own
        // renderer tier and layout on this panel. Resolved with the tier the
        // panel wears now, every theme of another tier reads as unavailable
        // or is drawn by the wrong renderer.
        const QVariantMap themed = m_panelRegistry.themeCandidateForTheme(
            *candidate, catalogTheme, QStringLiteral("complete"));
        const bool available = themed.value(QStringLiteral("success")).toBool();
        const QVariantMap resolution = themed.value(
            QStringLiteral("capabilityResolution")).toMap();
        theme.insert(QStringLiteral("available"), available);
        theme.insert(
            QStringLiteral("reasonCode"),
            available ? resolution.value(QStringLiteral("reasonCode"))
                      : themed.value(QStringLiteral("errorCode")));
        theme.insert(QStringLiteral("capabilityResolution"), resolution);
        result.append(theme);
    }
    return result;
}

QVariantList PanelWindow::iconStyleDefinitions() const
{
    return m_panelRegistry.iconStyleDefinitions();
}

// Panel Studio's truth (ADREP-TASK-001): a field is offered only where it acts
// on the candidate's host, layout, renderer and theme. Every editor capability
// has exactly one rule in the switch below, which the compiler checks; a
// capability name the schema does not define is never offered (fail closed).
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch"
QVariantList PanelWindow::panelSettingsEditorFields(
    const ArchDock::PanelDefinition &candidate,
    const ArchDock::CapabilityResolution &resolution,
    const QString &consumer,
    ArchDock::PanelSettingsFieldScope scope) const
{
    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const bool freeHost = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop;
    const std::optional<ArchDock::RendererTier> tier = resolution.available
        ? resolution.renderer.effectiveTier : std::nullopt;
    // A 3D panel on a platform the renderer generates, which a decoration can
    // stand in the place of.
    const bool generatedPlatform = tier == ArchDock::RendererTier::True3D
        && m_panelRegistry.standsOnGeneratedPlatform(candidate);
    const QString content = candidate.content.type;
    // Content that holds applications: launchers, running tasks or both.
    const bool applicationContent = content == QStringLiteral("launcher") ||
        content == QStringLiteral("tasks") || content == QStringLiteral("hybrid");
    // Content filled by dropping applications, folders and files on it.
    const bool droppedContent = content == QStringLiteral("launcher") ||
        content == QStringLiteral("hybrid");
    const auto controlAvailable = [&resolution](const QString &id)
    {
        for (const ArchDock::CapabilityDecision &decision : resolution.controls)
        {
            if (decision.id == id)
            {
                return decision.available;
            }
        }
        return false;
    };
    const auto layoutAvailable = [&resolution](const QString &id)
    {
        for (const ArchDock::CapabilityDecision &decision : resolution.layouts)
        {
            if (decision.id == id)
            {
                return decision.available;
            }
        }
        return false;
    };

    // The candidate's theme projection, built once for every field that asks
    // (ADFIX UF-08: it was built again for each 3D field of every edit).
    std::optional<std::optional<QVariantMap>> projectedTheme;
    const auto themeProjection = [&]() -> const std::optional<QVariantMap> & {
        if (!projectedTheme.has_value())
            projectedTheme = m_panelRegistry.themeRuntimeProjection(candidate);
        return *projectedTheme;
    };
    // PD-20/21: declarations govern shape and style controls. Resolve once;
    // repeated package projections would regress the Studio latency gate.
    std::optional<std::optional<QVariantMap>> projectedIconStyle;
    const auto iconStyleProjection = [&]() -> const std::optional<QVariantMap> & {
        if (!projectedIconStyle.has_value())
            projectedIconStyle = m_panelRegistry.iconStyleRuntimeProjection(candidate);
        return *projectedIconStyle;
    };
    std::optional<QVariantList> declaredIconLayers;
    const auto iconLayers = [&]() -> const QVariantList & {
        if (!declaredIconLayers.has_value())
        {
            declaredIconLayers.emplace();
            const auto &style = iconStyleProjection();
            if (style)
            {
                const auto roles = style->value(QStringLiteral("layers")).toMap();
                for (auto it = roles.cbegin(); it != roles.cend(); ++it)
                {
                    QVariantList layers = it.value().toList();
                    if (!it.value().toMap().isEmpty())
                        layers.append(it.value());
                    for (const QVariant &value : layers)
                    {
                        QVariantMap layer = value.toMap();
                        layer.insert(QStringLiteral("role"), it.key());
                        declaredIconLayers->append(layer);
                    }
                }
            }
        }
        return *declaredIconLayers;
    };
    const bool drawsTiles = candidate.iconStyle.tilesEnabled || std::any_of(
        candidate.iconStyle.perEntryOverrides.cbegin(), candidate.iconStyle.perEntryOverrides.cend(),
        [](const auto &entry) { return entry.tileEnabled.value_or(false); });
    const auto layerDrawn = [&](const QVariantMap &layer) {
        const QString option = layer.value(QStringLiteral("option")).toString();
        return drawsTiles && candidate.iconStyle.tileMode != QStringLiteral("custom")
            && layer.value(QStringLiteral("role")).toString() != QStringLiteral("mask")
            && !(option == QStringLiteral("pedestal") && !candidate.iconStyle.pedestalEnabled)
            && !(option == QStringLiteral("shape-override")
                 && candidate.iconStyle.shape == QStringLiteral("style-default"));
    };
    const auto hasIconLayer = [&](const auto &predicate, bool drawnOnly = false) {
        const auto &layers = iconLayers();
        return std::any_of(layers.cbegin(), layers.cend(), [&](const QVariant &value) {
            const QVariantMap layer = value.toMap();
            return (!drawnOnly || layerDrawn(layer)) && predicate(layer);
        });
    };
    const auto iconStyleDrawsLayers = [&]() {
        return hasIconLayer([](const QVariantMap &) { return true; }, true);
    };
    // On a baked 2.5D look the icons stand on the track its artwork declares
    // for the current state, not on the record's layout; only a closed track
    // can turn (LayoutEngine.trackSupportsRotation).
    const auto bakedTrack = [&]() -> QVariantMap {
        const auto &theme = themeProjection();
        if (!theme)
            return {};
        for (const QVariant &value : theme->value(QStringLiteral("tracks")).toList())
        {
            const QVariantMap track = value.toMap();
            const QString state = track.value(QStringLiteral("state")).toString();
            if (state.isEmpty() || state == candidate.presentation.mode)
                return track;
        }
        return {};
    };
    const bool baked = tier == ArchDock::RendererTier::Baked2_5D;

    QVariantList result;
    const QVariantList schemaFields = ArchDock::PanelSettingsSchema::editorDescriptors(
        scope, normalizedConsumer);
    for (const QVariant &value : schemaFields)
    {
        QVariantMap field = value.toMap();
        const QString key = field.value(QStringLiteral("key")).toString();
        const ArchDock::PanelSettingsFieldDescriptor *descriptor =
            ArchDock::PanelSettingsSchema::descriptor(scope, key);
        if (!descriptor)
        {
            continue;
        }
        if (!descriptor->editor.layouts.isEmpty() &&
            !descriptor->editor.layouts.contains(candidate.layout.pathType))
        {
            continue;
        }
        const std::optional<ArchDock::EditorCapability> capability =
            ArchDock::editorCapabilityFromName(descriptor->editor.capability);
        if (!capability.has_value())
        {
            continue;
        }

        bool available = false;
        // A field of this panel that draws nothing in its present state: not
        // shown, but kept and accepted, since it acts once the state changes
        // (a mechanism is chosen, the renderer changes) or a look needs it.
        bool inactive = false;
        QString lookOriginalLabel;
        switch (*capability)
        {
        case ArchDock::EditorCapability::ScreenPlacement:
            // Another display is all this control can choose.
            available = QGuiApplication::screens().size() > 1;
            break;
        case ArchDock::EditorCapability::ContentType:
        case ArchDock::EditorCapability::SurfaceOpacity:
            available = true;
            break;
        case ArchDock::EditorCapability::Visibility:
            // Only an edge panel's Plasma host can be hidden; a free panel is
            // drawn whatever it holds, and is removed instead (truth matrix).
            available = !freeHost;
            break;
        case ArchDock::EditorCapability::Segments:
        {
            const auto capabilities = ArchDock::PanelCapabilityResolver::segmentCapabilities(
                candidate, resolution, !m_systemStatus.availableSources().isEmpty());
            available = capabilities.value(QStringLiteral("available")).toBool();
            field.insert(QStringLiteral("segmentCapabilities"), capabilities);
            auto entries = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop
                ? freePanelEntries(candidate) : m_dockModel.panelEntries(candidate.content.type);
            entries.append(statusEntriesFor(candidate, true));
            for (auto &value : entries) {
                auto entry = value.toMap();
                if (!entry.value(QStringLiteral("isStatus")).toBool()) {
                    const auto overlay = entryOverlay(entry);
                    for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) entry.insert(it.key(), it.value());
                    if (!candidate.content.showBadges) entry[QStringLiteral("badgeText")] = QString{};
                    if (!candidate.content.showProgress) entry[QStringLiteral("progress")] = -1.0;
                    if (!candidate.content.showTemporaryStatus) entry[QStringLiteral("temporaryStatus")] = QString{};
                }
                value = entry;
            }
            field.insert(QStringLiteral("availableEntries"), entries);
            field.insert(QStringLiteral("segmentEntries"),
                ArchDock::PanelContentTransaction::segmentEntries(candidate, entries));
            break;
        }
        case ArchDock::EditorCapability::ApplicationOverlays:
            // Badges and progress come from the applications on this panel,
            // when the system reports them (PD-07).
            available = applicationContent && m_overlayModel.available();
            break;
        case ArchDock::EditorCapability::LaunchFeedback:
            available = applicationContent;
            break;
        case ArchDock::EditorCapability::DropInput:
        case ArchDock::EditorCapability::FolderContent:
        {
            // Only launcher and hybrid content take drops, and folders arrive
            // by being dropped.
            available = droppedContent;
            // A free panel opens its folders as a fan, a stack or a ring of
            // its own, each with its setting, shown while that layout is
            // chosen; an edge panel's folders open a popup (ADREP-TASK-003).
            static const QHash<QString, QString> shapeSettings{
                {QStringLiteral("folderFanOpening"), QStringLiteral("fan")},
                {QStringLiteral("folderStackLength"), QStringLiteral("stack")},
                {QStringLiteral("folderRingSize"), QStringLiteral("ring")},
            };
            if (available && shapeSettings.contains(key))
            {
                available = freeHost;
                if (candidate.content.folderLayout != shapeSettings.value(key))
                {
                    inactive = true;
                }
            }
            // A ring the size of the panel needs a dock with a radius: the
            // layouts that turn and travel. Elsewhere Small is the only size.
            if (available && key == QStringLiteral("folderRingSize"))
            {
                const auto *sensitivity = ArchDock::PanelSettingsSchema::descriptor(
                    scope, QStringLiteral("scrollSensitivity"));
                if (!sensitivity || !sensitivity->editor.layouts.contains(candidate.layout.pathType))
                {
                    inactive = true;
                }
            }
            break;
        }
        case ArchDock::EditorCapability::EdgePlacement:
        case ArchDock::EditorCapability::Alignment:
        case ArchDock::EditorCapability::DynamicPlacement:
        case ArchDock::EditorCapability::VisibilityMode:
            // Placement along a screen edge and Plasma's panel visibility
            // belong to edge panels; a free panel has neither (OF-02, OF-06).
            available = !freeHost;
            // A bar of Arch Dock's own stands anywhere along its edge; a
            // Plasma panel is placed along it by Plasma.
            if (key == QStringLiteral("offset"))
                available = available && candidate.host.edgeHost == QLatin1String("bar");
            break;
        case ArchDock::EditorCapability::LengthMutation:
        case ArchDock::EditorCapability::ThicknessMutation:
            // A free panel is drawn from its layout's radius, scale and icons;
            // a stored width or height reaches no renderer (OF-03, PD-02).
            available = !freeHost && controlAvailable(descriptor->editor.capability);
            break;
        case ArchDock::EditorCapability::ArbitraryXyPlacement:
            available = controlAvailable(descriptor->editor.capability);
            break;
        case ArchDock::EditorCapability::IconStateStyling:
        {
            available = controlAvailable(descriptor->editor.capability);
            // An icon's motion settings with no motion chosen move nothing,
            // so they are unseen until it has one (the owner, 2026-10-13: a
            // setting that cannot apply is gone).
            if (available && (key == QStringLiteral("animationTrigger") || key == QStringLiteral("animationSpeed")
                              || key == QStringLiteral("animationIntensity")))
                inactive = candidate.motion.iconProfile == QStringLiteral("none");
            const bool tileParameter = key == QStringLiteral("iconTileTexture")
                || key == QStringLiteral("iconTileThickness") || key == QStringLiteral("iconTileIconOffsetX")
                || key == QStringLiteral("iconTileIconOffsetY") || key == QStringLiteral("iconTileIconScale")
                || key == QStringLiteral("iconTileBevel") || key == QStringLiteral("iconTileMaterial")
                || key == QStringLiteral("iconTileElevation") || key == QStringLiteral("iconTileForm");
            if (available && tileParameter)
            {
                const bool only3D = key == QStringLiteral("iconTileBevel")
                    || key == QStringLiteral("iconTileMaterial") || key == QStringLiteral("iconTileElevation")
                    || key == QStringLiteral("iconTileForm");
                available = !only3D || tier == ArchDock::RendererTier::True3D;
                if (available && (key == QStringLiteral("iconTileTexture")
                         || key == QStringLiteral("iconTileThickness")))
                    available = candidate.iconStyle.tileMode == QStringLiteral("custom")
                        || !iconStyleDrawsLayers() || hasIconLayer([](const QVariantMap &layer) {
                            return layer.value(QStringLiteral("role")).toString() == QStringLiteral("base")
                                && layer.value(QStringLiteral("followsIconShape")).toBool();
                        });
                inactive = available && (!drawsTiles || (key == QStringLiteral("iconTileBevel")
                    && candidate.iconStyle.tileThickness <= 0));
            }
            const bool parameter = key == QStringLiteral("iconDiameter") || key == QStringLiteral("iconLogoSize")
                || key == QStringLiteral("iconOutlineWidth") || key == QStringLiteral("iconBodyColor")
                || key == QStringLiteral("iconOutlineColor") || key == QStringLiteral("iconGlowColor")
                || key == QStringLiteral("iconPedestalEnabled") || key == QStringLiteral("iconPedestalHeight")
                || key == QStringLiteral("iconPedestalColor");
            if (available && parameter)
            {
                const auto &style = iconStyleProjection();
                available = style && style->value(QStringLiteral("extensions")).toMap()
                    .contains(QStringLiteral("org.archdock.iconParameters"));
                // The colour of the light round every icon (Icons > Glow) is
                // this one too: offered while that glow shines, whatever the
                // style (the owner, 2026-10-13).
                const bool iconHalo = candidate.surface.parameters3D.value(QStringLiteral("iconGlow"), 0.0)
                    .toDouble() > 0.0;
                if (available && key.startsWith(QStringLiteral("iconPedestal")))
                {
                    available = drawsTiles && candidate.iconStyle.tileMode != QStringLiteral("custom")
                        && hasIconLayer([](const QVariantMap &layer) {
                            return layer.value(QStringLiteral("option")).toString() == QStringLiteral("pedestal");
                        });
                    inactive = available && key != QStringLiteral("iconPedestalEnabled")
                        && !candidate.iconStyle.pedestalEnabled;
                }
                else if (available && key == QStringLiteral("iconBodyColor"))
                {
                    available = hasIconLayer([](const QVariantMap &layer) {
                        return layer.value(QStringLiteral("role")).toString() == QStringLiteral("base")
                            && layer.value(QStringLiteral("option")).toString() != QStringLiteral("pedestal");
                    }, true);
                }
                else if (key == QStringLiteral("iconGlowColor"))
                {
                    available = iconHalo || (available && hasIconLayer([](const QVariantMap &layer) {
                        return layer.value(QStringLiteral("role")).toString() == QStringLiteral("glow");
                    }, true));
                }
                else if (available && (key == QStringLiteral("iconOutlineWidth")
                         || key == QStringLiteral("iconOutlineColor")))
                {
                    available = hasIconLayer([](const QVariantMap &layer) {
                        return layer.value(QStringLiteral("borderWidth")).toDouble() > 0;
                    }, true);
                    inactive = available && key == QStringLiteral("iconOutlineColor")
                        && candidate.iconStyle.outlineWidth == 0;
                }
            }
            break;
        }
        case ArchDock::EditorCapability::DynamicTint:
            // A theme's 2D tint capability does not tint its native mesh.
            // Native colour is the live material control on Panels > 3D.
            available = tier != ArchDock::RendererTier::True3D
                && controlAvailable(descriptor->editor.capability);
            // A picture covers the whole band in its own colours, and a
            // decoration is drawn in its own.
            if (available && key == QStringLiteral("color")
                && (panelWearsPicture(candidate, tier) || panelWearsDecoration(candidate, tier, generatedPlatform)))
                inactive = true;
            break;
        case ArchDock::EditorCapability::DynamicGlow:
        {
            // The look's own light: a skin's or baked platform's glow art,
            // and the bright parts of an energy look - bolts, gas, tubes,
            // circuitry - in 2D and 3D, named for the look (the owner,
            // 2026-10-13: "a settings on lightning glow too, same for other
            // themes where there can be practical settings"). A look with no
            // light of its own has nothing for it to scale.
            const QString look = panelLooks(candidate, tier).first().trimmed().toLower();
            const QString texture = panelLooks(candidate, tier).last().trimmed().toLower();
            static const QHash<QString, QString> names{
                {QStringLiteral("lightning"), QStringLiteral("Lightning glow")},
                {QStringLiteral("plasma"), QStringLiteral("Plasma glow")},
                {QStringLiteral("neon"), QStringLiteral("Neon glow")},
                {QStringLiteral("futuristic"), QStringLiteral("Circuit glow")},
                {QStringLiteral("aurora"), QStringLiteral("Aurora glow")}};
            const QString energy = names.contains(texture) ? texture : look;
            // A 3D platform's rim glows in every look; a flat surface has
            // light of its own only in an energy look.
            const bool lit = tier == ArchDock::RendererTier::Skinned2D
                || tier == ArchDock::RendererTier::Baked2_5D || tier == ArchDock::RendererTier::True3D;
            available = tier.has_value() && (lit ? controlAvailable(descriptor->editor.capability)
                                                 : names.contains(energy));
            if (available && names.contains(energy))
                field.insert(QStringLiteral("label"), names.value(energy));
            // A decoration is drawn with its own lights in the look's place.
            if (panelWearsDecoration(candidate, tier, generatedPlatform))
                inactive = true;
            break;
        }
        case ArchDock::EditorCapability::PresentationMechanism:
        {
            // Only mechanisms this panel's host and theme both declare may be
            // offered. The whole presentation group disappears when the panel
            // has no way to collapse at all, rather than presenting a resting
            // state or a reveal handle that nothing could ever draw. A free
            // panel has none (PD-01).
            QStringList mechanisms;
            for (const ArchDock::CapabilityDecision &decision :
                 resolution.presentationMechanisms)
            {
                if (decision.available)
                {
                    mechanisms.append(decision.id);
                }
            }
            const bool collapsible = std::any_of(
                mechanisms.cbegin(),
                mechanisms.cend(),
                [](const QString &mechanism)
                {
                    return mechanism != QStringLiteral("open");
                });
            available = resolution.available && collapsible;
            // With the open mechanism nothing opens or closes, so only the
            // choice of a mechanism and the resting state are shown; the
            // settings of a mechanism wait until one is chosen (PD-01).
            if (available && candidate.presentation.collapseMechanism == QStringLiteral("open") &&
                key != QStringLiteral("presentationMode") && key != QStringLiteral("collapseMechanism"))
            {
                inactive = true;
            }
            if (available && key == QStringLiteral("collapseMechanism"))
            {
                field.insert(QStringLiteral("choices"), mechanisms);
            }
            break;
        }
        case ArchDock::EditorCapability::Layout:
            // A native panel's applet lays its own row out along the edge, so
            // the record's layout, scale and padding never reach it.
            if (!freeHost)
            {
                break;
            }
            available = key == QStringLiteral("layout")
                ? true
                : layoutAvailable(candidate.layout.pathType);
            // A baked look places its icons on its artwork's own track, so
            // its Dock layout draws nothing different; it still decides the
            // look's compatibility and the 3D and flat drawings (truth matrix).
            if (available && key == QStringLiteral("layout") && baked)
            {
                inactive = true;
            }
            // Padding is the margin of a skin's artwork around the icons; the
            // other renderers draw nothing in it (OF-04, PD-03).
            if (available && key == QStringLiteral("layoutPadding"))
            {
                available = tier == ArchDock::RendererTier::Skinned2D;
            }
            // A true-3D icon stands upright facing the viewer on any path.
            if (available && key == QStringLiteral("pathOrientation"))
            {
                available = tier != ArchDock::RendererTier::True3D;
            }
            break;
        case ArchDock::EditorCapability::WholePanelRotation:
        {
            available = resolution.rotation.available;
            // An open baked track (an arc) cannot turn under its artwork; any
            // other track is closed, as LayoutEngine.trackShape() reads it.
            // Its entries still travel along it (ADREP-TASK-002), so the
            // motion settings stay and move the items only.
            const bool turns = !(available && baked &&
                bakedTrack().value(QStringLiteral("shape")).toString() == QStringLiteral("arc"));
            const bool itemMotion = key == QStringLiteral("panelRotationMode") ||
                key == QStringLiteral("panelRotationTrigger") ||
                key == QStringLiteral("panelMotionTarget") ||
                key == QStringLiteral("panelTravelSpeed") ||
                key == QStringLiteral("scrollSensitivity");
            if (available && !turns && !itemMotion)
            {
                available = false;
            }
            // Only the static layout angle takes the resolved degree range;
            // the motion fields share the gate but keep their own schema
            // bounds.
            if (available && key == QStringLiteral("layoutAngle"))
            {
                field.insert(QStringLiteral("minimumValue"),
                             resolution.rotation.minimumDegrees);
                field.insert(QStringLiteral("maximumValue"),
                             resolution.rotation.maximumDegrees);
            }
            // The speeds and the trigger act while continuous motion runs,
            // each speed on what it moves; the wheel follows "Continuous
            // motion moves" whether or not anything runs (PD-25).
            if (available)
            {
                const bool continuous = candidate.layout.rotationMode != QStringLiteral("none");
                const QString target = turns ? candidate.layout.motionTarget : QStringLiteral("items");
                if (key == QStringLiteral("panelMotionTarget") && !turns)
                {
                    inactive = true;
                }
                if (key == QStringLiteral("panelRotationTrigger") && !continuous)
                {
                    inactive = true;
                }
                if (key == QStringLiteral("panelTravelSpeed") &&
                    (!continuous || target == QStringLiteral("panel")))
                {
                    inactive = true;
                }
                if (key == QStringLiteral("panelRotationSpeed") &&
                    (!continuous || target == QStringLiteral("items")))
                {
                    inactive = true;
                }
            }
            break;
        }
        case ArchDock::EditorCapability::Scene3DQuality:
        case ArchDock::EditorCapability::Scene3DShape:
        {
            const auto renderer = std::find_if(resolution.rendererChoices.cbegin(),
                resolution.rendererChoices.cend(), [](const auto &choice) {
                    return choice.tier == ArchDock::RendererTier::True3D && choice.available;
                });
            available = freeHost && resolution.available
                && resolution.renderer.effectiveTier == ArchDock::RendererTier::True3D
                && renderer != resolution.rendererChoices.cend();
            if (available)
            {
                const auto &theme = themeProjection();
                available = theme && theme->value(QStringLiteral("valid")).toBool()
                    && !theme->value(QStringLiteral("scene3D")).toMap().isEmpty()
                    && !theme->value(QStringLiteral("scene3DResources")).toMap().isEmpty();
                if (available && key == QStringLiteral("scene3DCameraPitch"))
                    field.insert(QStringLiteral("defaultValue"), theme->value(QStringLiteral("scene3D"))
                        .toMap().value(QStringLiteral("cameraPitch"), 25.0));
                // Only a generated platform has a shape to adjust; a theme's
                // own mesh is drawn as its theme made it.
                if (available && *capability == ArchDock::EditorCapability::Scene3DShape)
                    available = theme->value(QStringLiteral("scene3D")).toMap()
                        .contains(QStringLiteral("generated"));
                // Pedestal height is Auto's column height; with another stand
                // choice it changes nothing, so it is kept unseen.
                if (available && key == QStringLiteral("scene3DIconElevation")
                    && candidate.surface.parameters3D.value(QStringLiteral("pedestalShape"),
                           QStringLiteral("none")).toString() != QStringLiteral("auto"))
                    inactive = true;
                // The platform colour does nothing under a picture or a
                // decoration.
                if (available && key == QStringLiteral("scene3DColor")
                    && (panelWearsPicture(candidate, tier) || panelWearsDecoration(candidate, tier, generatedPlatform)))
                    inactive = true;
            }
            break;
        }
        case ArchDock::EditorCapability::BakedTilt:
        {
            const auto &theme = themeProjection();
            if (freeHost && resolution.available
                && resolution.renderer.effectiveTier == ArchDock::RendererTier::Baked2_5D && theme)
            {
                const auto tracks = theme->value(QStringLiteral("tracks")).toList();
                for (const auto &trackValue : tracks)
                {
                    const auto track = trackValue.toMap();
                    const auto state = track.value(QStringLiteral("state")).toString();
                    if (!state.isEmpty() && state != candidate.presentation.mode) continue;
                    const auto tilt = track.value(QStringLiteral("tilt")).toMap();
                    if (tilt.isEmpty()) continue;
                    const double minimum = qMax(-60.0, tilt.value(QStringLiteral("minimumDegrees")).toDouble());
                    const double maximum = qMin(60.0, tilt.value(QStringLiteral("maximumDegrees")).toDouble());
                    if (maximum <= minimum) continue;
                    field.insert(QStringLiteral("minimumValue"), minimum);
                    field.insert(QStringLiteral("maximumValue"), maximum);
                    field.insert(QStringLiteral("defaultValue"), qBound(minimum,
                        tilt.value(QStringLiteral("defaultDegrees"), 0.0).toDouble(), maximum));
                    available = true;
                    break;
                }
            }
            break;
        }
        case ArchDock::EditorCapability::ProceduralSurface:
            available = tier == ArchDock::RendererTier::Procedural2D;
            if (key == QStringLiteral("sparkleIntensity"))
                available = available && (candidate.surface.appearance == QStringLiteral("crystal")
                    || candidate.surface.appearance == QStringLiteral("plasma"));
            // A decoration is drawn in the look's place.
            inactive = panelWearsDecoration(candidate, tier, generatedPlatform);
            break;
        case ArchDock::EditorCapability::Decoration:
            // A decoration remakes a flat panel's outline, or stands as a
            // solid in a generated 3D platform's place; its motion only while
            // one is worn, its speed only while it moves.
            available = decorationFits(tier, generatedPlatform);
            if (key != QStringLiteral("panelDecoration"))
                inactive = !panelWearsDecoration(candidate, tier, generatedPlatform)
                    || (key == QStringLiteral("panelDecorationSpeed")
                        && !candidate.surface.parameters3D.value(QStringLiteral("panelDecorationAnimation"), true).toBool());
            break;
        case ArchDock::EditorCapability::DecorationColours:
            // Which colours a decoration takes is its own: Panel Studio
            // offers only those (DecorationScene.colourControls).
            available = decorationFits(tier, generatedPlatform);
            inactive = !panelWearsDecoration(candidate, tier, generatedPlatform);
            break;
        case ArchDock::EditorCapability::ArtworkFit:
            available = resolution.available &&
                !candidate.surface.themeSource.trimmed().isEmpty();
            break;
        case ArchDock::EditorCapability::TileShape:
            // PD-20: a package-declared silhouette follows the chosen shape.
            available = controlAvailable(QStringLiteral("icon-state-styling")) &&
                drawsTiles && (candidate.iconStyle.tileMode == QStringLiteral("custom") || !iconStyleDrawsLayers()
                    || hasIconLayer([](const QVariantMap &layer) {
                        return layer.value(QStringLiteral("followsIconShape")).toBool();
                    }));
            break;
        case ArchDock::EditorCapability::Identity:
            available = true;
            break;
        case ArchDock::EditorCapability::PanelPicture:
            available = tier == ArchDock::RendererTier::Procedural2D || tier == ArchDock::RendererTier::True3D;
            inactive = panelWearsDecoration(candidate, tier, generatedPlatform);
            break;
        case ArchDock::EditorCapability::PictureRelief:
            // A panel wearing a picture stands out in relief, flat (its
            // picture embossed by its brightness) or in 3D (its mesh moving);
            // the direction, in or out of the band, is a 3D panel's.
            available = true;
            inactive = !panelWearsPicture(candidate, tier)
                || (key == QStringLiteral("panelPictureDirection") && tier != ArchDock::RendererTier::True3D);
            break;
        case ArchDock::EditorCapability::Pedestals:
        {
            // Pedestals stand under the icons on every host and renderer:
            // Auto is a column on flat and 3D panels alike; new panels have
            // none. A setting that draws nothing is kept unseen: the panel's
            // stand settings without a shape, the speed without a motion,
            // the folder items' settings without stands under them or
            // without Own settings (the owner's 2026-10-12 requests).
            available = true;
            const QVariantMap &values = candidate.surface.parameters3D;
            const QString shape = values.value(QStringLiteral("pedestalShape"), QStringLiteral("none")).toString();
            const QString folderShape = values.value(QStringLiteral("folderPedestalShape"), QStringLiteral("same")).toString();
            const bool drawn = shape != QStringLiteral("none");
            const bool folderDrawn = folderShape == QStringLiteral("same") ? drawn : folderShape != QStringLiteral("none");
            const bool folderOwn = values.value(QStringLiteral("folderPedestalOwnStyle"), false).toBool();
            if (key.startsWith(QStringLiteral("folderPedestal")))
            {
                if (key == QStringLiteral("folderPedestalOwnStyle"))
                    inactive = !folderDrawn;
                else if (key != QStringLiteral("folderPedestalShape"))
                    inactive = !folderDrawn || !folderOwn;
                if (key == QStringLiteral("folderPedestalAnimationSpeed"))
                    inactive = inactive || values.value(QStringLiteral("folderPedestalAnimation"),
                        QStringLiteral("none")).toString() == QStringLiteral("none");
                // A stand's colour does nothing under its own picture; its
                // relief is a 3D stand's wearing one.
                if (key == QStringLiteral("folderPedestalColor"))
                    inactive = inactive || standWearsOwnPicture(candidate, QStringLiteral("folderPedestal"));
                if (key.startsWith(QStringLiteral("folderPedestalPicture")) && key != QStringLiteral("folderPedestalPicture"))
                    inactive = inactive || tier != ArchDock::RendererTier::True3D
                        || !standWearsOwnPicture(candidate, QStringLiteral("folderPedestal"));
                // Glow where light round the stand makes sense; its reach
                // only while it shines.
                if (key.startsWith(QStringLiteral("folderPedestalGlow")))
                    inactive = inactive || !standGlowFits(candidate, tier,
                        folderOwn ? QStringLiteral("folderPedestal") : QStringLiteral("pedestal"));
                if (key == QStringLiteral("folderPedestalGlowReach"))
                    inactive = inactive || (values.value(QStringLiteral("folderPedestalGlow"), 0.0).toDouble() <= 0.0
                        && values.value(QStringLiteral("folderPedestalAnimation"),
                               QStringLiteral("none")).toString() != QStringLiteral("glow"));
                if (key == QStringLiteral("folderPedestalAnimation")
                    && !standGlowFits(candidate, tier, QStringLiteral("folderPedestal")))
                {
                    QStringList choices = field.value(QStringLiteral("choices")).toStringList();
                    choices.removeAll(QStringLiteral("glow"));
                    field.insert(QStringLiteral("choices"), choices);
                }
            }
            else if (key != QStringLiteral("pedestalShape"))
            {
                inactive = !drawn;
                // View angle is a level stand's: every stand of a flat panel,
                // a 3D panel's unless it lies on the panel (Stand facing:
                // Panel), where it takes the panel's own view. Stand facing
                // turns a 3D panel's stands only.
                const bool solidPanel = tier == ArchDock::RendererTier::True3D;
                if (key == QStringLiteral("pedestalViewAngle"))
                    inactive = inactive || (solidPanel && values.value(QStringLiteral("pedestalFacing"),
                        QStringLiteral("panel")).toString() == QStringLiteral("panel"));
                if (key == QStringLiteral("pedestalFacing"))
                    inactive = inactive || !solidPanel;
                if (key == QStringLiteral("pedestalAnimationSpeed"))
                    inactive = inactive || values.value(QStringLiteral("pedestalAnimation"),
                        QStringLiteral("none")).toString() == QStringLiteral("none");
                // A stand's colour does nothing under its own picture; its
                // relief is a 3D stand's wearing one.
                if (key == QStringLiteral("pedestalColor"))
                    inactive = inactive || standWearsOwnPicture(candidate, QStringLiteral("pedestal"));
                if (key.startsWith(QStringLiteral("pedestalPicture")) && key != QStringLiteral("pedestalPicture"))
                    inactive = inactive || tier != ArchDock::RendererTier::True3D
                        || !standWearsOwnPicture(candidate, QStringLiteral("pedestal"));
                // Glow where light round the stand makes sense; its reach and
                // colour only while it shines; no glowing motion without it.
                const bool glowFits = standGlowFits(candidate, tier, QStringLiteral("pedestal"));
                const bool shining = values.value(QStringLiteral("pedestalGlow"), 0.0).toDouble() > 0.0
                    || values.value(QStringLiteral("pedestalAnimation"), QStringLiteral("none")).toString()
                        == QStringLiteral("glow");
                if (key.startsWith(QStringLiteral("pedestalGlow")))
                    inactive = inactive || !glowFits;
                if (key == QStringLiteral("pedestalGlowReach") || key == QStringLiteral("pedestalGlowColor"))
                    inactive = inactive || !shining;
                if (key == QStringLiteral("pedestalAnimation") && !glowFits)
                {
                    QStringList choices = field.value(QStringLiteral("choices")).toStringList();
                    choices.removeAll(QStringLiteral("glow"));
                    field.insert(QStringLiteral("choices"), choices);
                }
            }
            break;
        }
        case ArchDock::EditorCapability::Glow:
        {
            // Light round the panel, its icons or its tiles, each with its
            // own settings on its own Appearance (the owner, 2026-10-13):
            // where light round the look makes sense, and the reach and
            // colour only while it shines. An icon's picture glows in any
            // look; a tile glows where tiles are drawn.
            available = true;
            const QVariantMap &values = candidate.surface.parameters3D;
            const auto shining = [&values](const QString &intensity) {
                return values.value(intensity, 0.0).toDouble() > 0.0;
            };
            if (key.startsWith(QStringLiteral("panelGlow")))
            {
                // A decoration's lights give their own glow.
                inactive = !panelGlowFits(candidate, tier) || panelWearsDecoration(candidate, tier, generatedPlatform)
                    || (key != QStringLiteral("panelGlow") && !shining(QStringLiteral("panelGlow")));
            }
            else if (key.startsWith(QStringLiteral("iconTileGlow")))
            {
                // Only a tile that is drawn can glow: a custom tile, or the
                // icon style's own base under the icon.
                const bool tileDrawn = drawsTiles && (candidate.iconStyle.tileMode == QStringLiteral("custom")
                    || hasIconLayer([](const QVariantMap &layer) {
                        return layer.value(QStringLiteral("role")).toString() == QStringLiteral("base")
                            && layer.value(QStringLiteral("option")).toString() != QStringLiteral("pedestal");
                    }, true));
                inactive = !tileDrawn
                    || !glowFitsLook(candidate.iconStyle.tileMaterial)
                    || (candidate.iconStyle.tileTexture != QStringLiteral("none")
                        && !glowFitsLook(candidate.iconStyle.tileTexture))
                    || (key != QStringLiteral("iconTileGlow") && !shining(QStringLiteral("iconTileGlow")));
            }
            else if (key.startsWith(QStringLiteral("iconGlow")))
            {
                inactive = key != QStringLiteral("iconGlow") && !shining(QStringLiteral("iconGlow"));
            }
            break;
        }
        case ArchDock::EditorCapability::Hologram:
        {
            // Holograms on every Arch Dock panel, flat or 3D, each on its
            // part's own Hologram tab: the icons, the panel itself and its
            // stands (only where stands are drawn), each switched on of its
            // own. Its settings show while it is on; the glow's colour and
            // reach while it glows; the icons' Turning only on a 3D panel,
            // where they are solids; the projector light's colours,
            // strength, height and motion while it is on.
            available = true;
            const QVariantMap &values = candidate.surface.parameters3D;
            const auto flag = [&values](const char *name, bool fallback) {
                return values.value(QLatin1String(name), fallback).toBool();
            };
            const bool standsDrawn = values.value(QStringLiteral("pedestalShape"), QStringLiteral("none")).toString()
                != QStringLiteral("none");
            const bool icons = flag("hologramIcons", false);
            const bool beam = icons && flag("hologramBeam", true);
            const auto glowOff = [&values, &key](const QString &prefix) {
                return (key == prefix + QStringLiteral("GlowReach") || key == prefix + QStringLiteral("GlowColor"))
                    && values.value(prefix + QStringLiteral("Glow"), 1.0).toDouble() <= 0.0;
            };
            // Each part's hologram switch is always offered: turned on while
            // the stands have no shape, Panel Studio gives them one.
            if (key == QStringLiteral("hologramIcons") || key == QStringLiteral("hologramPanel")
                || key == QStringLiteral("hologramPedestals"))
                inactive = false;
            else if (key.startsWith(QStringLiteral("hologramPedestal")))
                inactive = !standsDrawn || !flag("hologramPedestals", false)
                    || glowOff(QStringLiteral("hologramPedestal"));
            else if (key.startsWith(QStringLiteral("hologramPanel")))
                inactive = !flag("hologramPanel", false) || glowOff(QStringLiteral("hologramPanel"));
            else if (key == QStringLiteral("hologramIconTurning"))
                inactive = !icons || tier != ArchDock::RendererTier::True3D;
            else if (key.startsWith(QStringLiteral("hologramIcon")) || key == QStringLiteral("hologramBeam"))
                inactive = !icons || glowOff(QStringLiteral("hologramIcon"));
            else
                // The projector light's colours, strength, height and motion.
                inactive = !beam;
            break;
        }
        case ArchDock::EditorCapability::Lightning:
        {
            // How much of the dark storm shows behind a lightning look's
            // bolts, only where something is drawn in lightning; on a 3D
            // panel beside its material on the 3D page.
            available = true;
            const bool stand = key.startsWith(QStringLiteral("pedestal"));
            const bool drawn = candidate.surface.parameters3D.value(QStringLiteral("pedestalShape"),
                QStringLiteral("none")).toString() != QStringLiteral("none");
            const QStringList looks = stand ? standLooks(candidate, tier, QStringLiteral("pedestal"))
                                            : panelLooks(candidate, tier);
            const bool lightning = looks.contains(QStringLiteral("lightning"));
            const bool aurora = looks.contains(QStringLiteral("aurora"));
            if (key.endsWith(QStringLiteral("LookBackground")))
            {
                // The dark sky behind a clear-ground look's light.
                inactive = !lightning && !aurora;
                field.insert(QStringLiteral("label"), lightning ? QStringLiteral("Lightning background")
                                                                : QStringLiteral("Aurora background"));
            }
            else if (key.endsWith(QStringLiteral("LookColour")))
            {
                // A glowing look's palettes, its own first under its name.
                struct Palettes { const char *name; const char *original; QStringList others; };
                static const QList<std::pair<QString, Palettes>> palettes{
                    {QStringLiteral("lightning"), {"Lightning colour", "Storm",
                        {QStringLiteral("ice"), QStringLiteral("crimson"), QStringLiteral("toxic"),
                         QStringLiteral("solar"), QStringLiteral("arcane"), QStringLiteral("void"),
                         QStringLiteral("aurora"), QStringLiteral("rainbow")}}},
                    {QStringLiteral("aurora"), {"Aurora colour", "Borealis",
                        {QStringLiteral("emerald"), QStringLiteral("australis"), QStringLiteral("cosmic"),
                         QStringLiteral("solar"), QStringLiteral("rainbow")}}},
                    {QStringLiteral("plasma"), {"Plasma colour", "Violet", {QStringLiteral("rainbow")}}},
                    {QStringLiteral("neon"), {"Neon colour", "Cyan", {QStringLiteral("rainbow")}}},
                    {QStringLiteral("futuristic"), {"Circuit colour", "Original", {QStringLiteral("rainbow")}}},
                    // Polished metal's faint rainbow; a lime stays a lime
                    // (the owner, 2026-10-13: a colour that changes what a
                    // look is "defeats the purpose").
                    {QStringLiteral("metallic"), {"Metal finish", "Steel",
                        {QStringLiteral("chameleon"), QStringLiteral("iridescent")}}},
                    {QStringLiteral("plate"), {"Metal finish", "Steel",
                        {QStringLiteral("chameleon"), QStringLiteral("iridescent")}}},
                    // Water in its moods, and lava in its heats.
                    {QStringLiteral("river"), {"Water", "River",
                        {QStringLiteral("clear"), QStringLiteral("deep"), QStringLiteral("jade"),
                         QStringLiteral("sunset")}}},
                    {QStringLiteral("magma"), {"Lava", "Magma",
                        {QStringLiteral("ember"), QStringLiteral("blaze"), QStringLiteral("molten")}}}};
                const auto found = std::find_if(palettes.cbegin(), palettes.cend(), [&](const auto &entry) {
                    return looks.last() == entry.first || looks.first() == entry.first;
                });
                inactive = found == palettes.cend();
                if (!inactive)
                {
                    field.insert(QStringLiteral("label"), QString::fromLatin1(found->second.name));
                    field.insert(QStringLiteral("choices"),
                                 QStringList{QStringLiteral("original")} + found->second.others);
                    lookOriginalLabel = QString::fromLatin1(found->second.original);
                }
            }
            else if (key == QStringLiteral("lookMotionSpeed"))
            {
                // Only an animated look moves: lightning strikes, plasma,
                // circuitry and aurora flow; reduced motion keeps them still.
                inactive = !lightning && !aurora && !looks.contains(QStringLiteral("plasma"))
                    && !looks.contains(QStringLiteral("futuristic"));
                field.insert(QStringLiteral("label"), lightning ? QStringLiteral("Strike rate")
                                                                : QStringLiteral("Flow speed"));
            }
            if (stand)
                inactive = inactive || !drawn;
            else if (tier == ArchDock::RendererTier::True3D)
                field.insert(QStringLiteral("section"), QStringLiteral("panels-3d"));
            // A decoration is drawn in the panel's look's place.
            if (!stand && panelWearsDecoration(candidate, tier, generatedPlatform))
                inactive = true;
            break;
        }
        case ArchDock::EditorCapability::GlobalRenderer:
            // Running-application indicators belong to edge panels whose
            // content shows running applications (OF-08, PD-06).
            available = key != QStringLiteral("showIndicators") ||
                (!freeHost && (content == QStringLiteral("tasks") || content == QStringLiteral("hybrid")));
            break;
        case ArchDock::EditorCapability::Count:
            break;
        }
        if (!available)
        {
            continue;
        }

        QStringList choices = field.value(QStringLiteral("choices")).toStringList();
        if (key == QStringLiteral("folderLayout"))
        {
            choices = {QStringLiteral("fan"), QStringLiteral("grid"),
                       QStringLiteral("stack"), QStringLiteral("arc"), QStringLiteral("ring")};
            // A curved free panel can open its folders along its own curve,
            // and any free panel as a Möbius band; an edge panel's popup
            // draws no band.
            if (freeHost && curvedLayout(candidate.layout.pathType))
            {
                choices.prepend(QStringLiteral("track"));
            }
            if (freeHost || candidate.content.folderLayout == QStringLiteral("mobius"))
            {
                choices.append(QStringLiteral("mobius"));
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("iconStyle"))
        {
            choices.clear();
            for (const QVariant &styleValue : m_panelRegistry.iconStyleDefinitions())
            {
                const QString styleId = styleValue.toMap()
                    .value(QStringLiteral("id")).toString();
                if (!styleId.isEmpty())
                {
                    choices.append(styleId);
                }
            }
            if (choices.isEmpty())
            {
                continue;
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("layout"))
        {
            // A layout that draws exactly like another is no choice: a ring
            // and a circle place icons and platforms alike, and on a free
            // panel adaptive is the horizontal row. The current layout keeps
            // its own name.
            const auto drawing = [freeHost](const QString &layout)
            {
                if (layout == QStringLiteral("ring"))
                {
                    return QStringLiteral("circular");
                }
                if (freeHost && layout == QStringLiteral("adaptive"))
                {
                    return QStringLiteral("horizontal");
                }
                return layout;
            };
            const QString current = candidate.layout.pathType;
            QSet<QString> drawings{drawing(current)};
            QStringList availableChoices;
            // A panel set to 3D is offered the layouts its look can stand on
            // a 3D platform; any other one would turn the panel flat.
            const bool threeD = candidate.surface.rendererTier == QStringLiteral("true3d");
            const QStringList drawnIn3D = threeD
                ? m_panelRegistry.layoutsDrawnIn3D(candidate, choices) : QStringList{};
            for (const QString &choice : choices)
            {
                if (!layoutAvailable(choice))
                {
                    continue;
                }
                // Triangle to octagon are the polygon with that many
                // corners, set beside it; and on a free panel adaptive is
                // listed as the row it is.
                if (ArchDock::namedPolygonCorners(choice) > 0 ||
                    (freeHost && choice == QStringLiteral("adaptive") && choice != current))
                {
                    continue;
                }
                if (threeD && choice != current && !drawnIn3D.contains(choice))
                {
                    continue;
                }
                if (choice == current || !drawings.contains(drawing(choice)))
                {
                    availableChoices.append(choice);
                    drawings.insert(drawing(choice));
                }
            }
            choices = availableChoices;
            if (choices.isEmpty())
            {
                continue;
            }
            // One layout left is no choice to show, but it is kept: a look
            // may need the one layout it is made for.
            if (choices.size() < 2)
            {
                inactive = true;
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("type") && !freeHost)
        {
            // An edge panel of the "empty" type is a plain Plasma panel with
            // no Arch Dock in it; the Studio only makes Arch Dock panels. The
            // current type stays listed so the field shows it.
            if (candidate.content.type != QStringLiteral("empty"))
            {
                choices.removeAll(QStringLiteral("empty"));
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("edge"))
        {
            choices = {
                QStringLiteral("top"),
                QStringLiteral("bottom"),
                QStringLiteral("left"),
                QStringLiteral("right"),
            };
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("visibilityMode") && !freeHost)
        {
            const QStringList supportedModes =
                ArchDock::supportedNativeVisibilityModes(
                    nativeVisibilityCapabilities(candidate.identity.id));
            QStringList supportedChoices;
            for (const QString &choice : choices)
            {
                if (supportedModes.contains(choice))
                {
                    supportedChoices.append(choice);
                }
            }
            choices = supportedChoices;
            field.insert(QStringLiteral("choices"), choices);
        }

        QVariantList options;
        if (key == QStringLiteral("iconStyle"))
        {
            for (const QVariant &styleValue : m_panelRegistry.iconStyleDefinitions())
            {
                const QVariantMap style = styleValue.toMap();
                options.append(QVariantMap{
                    {QStringLiteral("description"),
                     style.value(QStringLiteral("description"))},
                    {QStringLiteral("label"), style.value(QStringLiteral("name"))},
                    {QStringLiteral("value"), style.value(QStringLiteral("id"))},
                });
            }
        }
        else for (const QString &choice : choices)
        {
            // Plain names where the stored value is a code word.
            static const QHash<QString, QHash<QString, const char *>> labels{
                {QStringLiteral("folderLayout"), {{QStringLiteral("track"), QT_TR_NOOP("Along the dock")}}},
                // The surface a panel's own design gives it: its flat panelpaper
                // on a generated platform, a decoration's artwork on its own
                // mesh; and the picture the user imported, in every look list.
                {QStringLiteral("scene3DMaterial"), {{QStringLiteral("theme"), QT_TR_NOOP("Default")},
                                                     {QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("scene3DTexture"), {{QStringLiteral("theme"), QT_TR_NOOP("Default")},
                                                    {QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("appearance"), {{QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("pedestalMaterial"), {{QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("pedestalTexture"), {{QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("folderPedestalMaterial"), {{QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("folderPedestalTexture"), {{QStringLiteral("picture"), QT_TR_NOOP("Imported picture")}}},
                {QStringLiteral("folderRingSize"),
                 {{QStringLiteral("small"), QT_TR_NOOP("Small")},
                  {QStringLiteral("panel"), QT_TR_NOOP("Same as panel")}}},
                {QStringLiteral("panelRotationMode"),
                 {{QStringLiteral("none"), QT_TR_NOOP("Off")},
                  {QStringLiteral("clockwise"), QT_TR_NOOP("Clockwise")},
                  {QStringLiteral("counter-clockwise"), QT_TR_NOOP("Counterclockwise")}}},
                {QStringLiteral("panelRotationTrigger"),
                 {{QStringLiteral("idle"), QT_TR_NOOP("Always")},
                  {QStringLiteral("hover"), QT_TR_NOOP("While the pointer is over the panel")}}},
                {QStringLiteral("panelMotionTarget"),
                 {{QStringLiteral("items"), QT_TR_NOOP("Items along the path")},
                  {QStringLiteral("panel"), QT_TR_NOOP("Whole panel")},
                  {QStringLiteral("both"), QT_TR_NOOP("Both")}}},
            };
            const char *label = labels.value(key).value(choice, nullptr);
            // A shape by its name, any other code word made readable.
            QString text = label ? tr(label)
                : key == QStringLiteral("layout") || key == QStringLiteral("folderLayout")
                ? ArchDock::panelShapeName(choice) : readableChoice(choice);
            // A glowing look's own palette under its own name, the others
            // capitalised.
            if (key.endsWith(QStringLiteral("LookColour")))
                text = choice == QStringLiteral("original") && !lookOriginalLabel.isEmpty()
                    ? lookOriginalLabel : choice.left(1).toUpper() + choice.mid(1);
            options.append(QVariantMap{
                {QStringLiteral("label"), text},
                {QStringLiteral("value"), choice},
            });
        }
        if (key == QStringLiteral("screen"))
        {
            options.clear();
            const QVariantList screens = availableScreens();
            for (const QVariant &screenValue : screens)
            {
                const QVariantMap screen = screenValue.toMap();
                options.append(QVariantMap{
                    {QStringLiteral("label"), screen.value(QStringLiteral("label"))},
                    {QStringLiteral("value"), screen.value(QStringLiteral("index"))},
                });
            }
            field.insert(QStringLiteral("minimumValue"), 0);
            field.insert(QStringLiteral("maximumValue"), qMax(0, options.size() - 1));
        }
        // A free panel's radius and icon size stop where its own icons would
        // no longer fit side by side - never on the wrong side of the value
        // it has, which a panel already holding more may be past.
        if (freeHost && (key == QStringLiteral("layoutRadius") || key == QStringLiteral("iconSize")))
        {
            const int lowest = field.value(QStringLiteral("minimumValue")).toInt();
            const int highest = field.value(QStringLiteral("maximumValue")).toInt();
            const double seen = predictedSeenRoom(candidate);
            if (key == QStringLiteral("layoutRadius"))
            {
                if (const auto least = ArchDock::panelMinimumRadius(candidate, seen))
                    field.insert(QStringLiteral("minimumValue"),
                                 qBound(lowest, qMin(*least, candidate.layout.radius), highest));
            }
            else if (const auto most = ArchDock::panelMaximumIconSize(candidate, seen))
            {
                field.insert(QStringLiteral("maximumValue"),
                             qBound(lowest, qMax(*most, candidate.iconStyle.size), highest));
            }
        }
        if (!options.isEmpty())
        {
            field.insert(QStringLiteral("options"), options);
        }
        if (inactive)
        {
            field.insert(QStringLiteral("inactive"), true);
        }
        result.append(field);
    }
    return result;
}
#pragma GCC diagnostic pop

QVariantMap PanelWindow::panelSettingsEditorValues(
    const ArchDock::PanelDefinition &candidate,
    const QVariantList &fields) const
{
    QSet<QString> includedKeys;
    QVariantMap projectedDefaults;
    for (const QVariant &value : fields)
    {
        const auto field = value.toMap();
        const auto key = field.value(QStringLiteral("key")).toString();
        includedKeys.insert(key);
        projectedDefaults.insert(key, field.value(QStringLiteral("defaultValue")));
    }
    for (const ArchDock::PanelSettingsFieldDescriptor &field :
         ArchDock::PanelSettingsSchema::fields())
    {
        if (field.scope == ArchDock::PanelSettingsFieldScope::Panel &&
            field.access == ArchDock::PanelSettingsFieldAccess::Editor &&
            !field.editor.isPresented())
        {
            includedKeys.insert(field.key);
        }
    }

    const QVariantMap record = candidate.toLegacyMap();
    QVariantMap result;
    for (const QString &key : std::as_const(includedKeys))
    {
        const ArchDock::PanelSettingsFieldDescriptor *field =
            ArchDock::PanelSettingsSchema::panelDescriptor(key);
        if (field)
        {
            result.insert(key, record.value(key, projectedDefaults.value(key, field->defaultValue)));
        }
    }
    return result;
}

std::optional<ArchDock::PanelSettingsTransactionDraft>
PanelWindow::preparePanelSettingsDraft(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    ArchDock::PanelSettingsTransactionOutcome *outcome) const
{
    QString snapshotError;
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId, &snapshotError);
    if (!current.has_value())
    {
        if (outcome)
        {
            *outcome = {};
            outcome->panelId = panelId;
            outcome->expectedRevision = expectedRevision;
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = QStringLiteral("panel-not-found");
            outcome->errorMessage = snapshotError;
        }
        return std::nullopt;
    }
    return preparePanelSettingsDraftOn(*current, expectedRevision, panelValues, globalValues, outcome);
}

std::optional<ArchDock::PanelSettingsTransactionDraft>
PanelWindow::preparePanelSettingsDraftOn(
    const ArchDock::PanelDefinition &currentPanel,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    ArchDock::PanelSettingsTransactionOutcome *outcome,
    const QStringList *takenNames) const
{
    const QString panelId = currentPanel.identity.id;
    if (outcome)
    {
        *outcome = {};
        outcome->panelId = panelId;
        outcome->expectedRevision = expectedRevision;
    }
    if (outcome)
    {
        outcome->previousRevision = currentPanel.settingsRevision;
        outcome->revision = currentPanel.settingsRevision;
    }

    const QVariantMap currentGlobals = m_settings.transactionSnapshot();
    QVariantMap candidateGlobals;
    QString globalError;
    if (!m_settings.stageTransaction(globalValues, &candidateGlobals, &globalError))
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = QStringLiteral("invalid-global-settings");
            outcome->errorMessage = globalError;
        }
        return std::nullopt;
    }

    ArchDock::PanelSettingsTransactionRequest request;
    request.panelId = panelId;
    request.expectedRevision = expectedRevision;
    request.panelValues = panelValues;
    if (request.panelValues.contains(QStringLiteral("segments")))
        request.panelValues.insert(QStringLiteral("segments"),
            decodedSegmentValue(request.panelValues.value(QStringLiteral("segments"))));
    request.globalValues = globalValues;
    std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        ArchDock::PanelSettingsTransaction::prepare(
            currentPanel,
            currentGlobals,
            candidateGlobals,
            request,
            outcome,
            [this](const ArchDock::PanelDefinition &candidate)
            {
                return m_panelRegistry.resolvePanelCapabilities(candidate);
            });
    if (!draft.has_value())
    {
        return std::nullopt;
    }

    if (draft->candidatePanel.segments != currentPanel.segments)
    {
        QVariantList entries = draft->candidatePanel.host.kind == ArchDock::PanelHostKind::FreeDesktop
            ? freePanelEntries(draft->candidatePanel)
            : m_dockModel.panelEntries(draft->candidatePanel.content.type);
        entries.append(statusEntriesFor(draft->candidatePanel, true));
        QString segmentError;
        const auto projected = ArchDock::PanelContentTransaction::segmentEntries(
            draft->candidatePanel, entries, &segmentError, true);
        Q_UNUSED(projected);
        if (!segmentError.isEmpty())
        {
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel.settingsRevision;
                outcome->errorCode = QStringLiteral("invalid-segment-ownership");
                outcome->errorMessage = segmentError;
            }
            return std::nullopt;
        }
    }

    if (panelValues.contains(QStringLiteral("screen")))
    {
        const int requestedScreen = panelValues.value(QStringLiteral("screen")).toInt();
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (requestedScreen < 0 || requestedScreen >= screens.size())
        {
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel.settingsRevision;
                outcome->errorCode = QStringLiteral("screen-out-of-range");
                outcome->errorMessage = QStringLiteral(
                    "the requested screen is not available");
            }
            return std::nullopt;
        }
        draft->candidatePanel.host.screenIndex = requestedScreen;
        draft->candidatePanel.host.screenId = ArchDock::persistentScreenId(
            screens.at(requestedScreen));
    }

    // A panel nobody named goes by what it is, given another shape or edge.
    m_panelRegistry.followShapeName(currentPanel, draft->candidatePanel, takenNames);

    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(draft->candidatePanel);
    const auto segmentCapabilities = ArchDock::PanelCapabilityResolver::segmentCapabilities(
        draft->candidatePanel, resolution, !m_systemStatus.availableSources().isEmpty());
    const bool inheritedSegment = draft->candidatePanel.segments ==
        QList<ArchDock::PanelSegmentDefinition>{ArchDock::PanelSegmentDefinition{}};
    QString segmentError;
    if (!inheritedSegment && !segmentCapabilities.value(QStringLiteral("available")).toBool())
        segmentError = segmentCapabilities.value(QStringLiteral("reasonCode")).toString();
    if (!inheritedSegment)
    {
        for (const auto &segment : draft->candidatePanel.segments)
        {
            const bool retainedStatus = segment.source == QStringLiteral("status")
                && std::any_of(currentPanel.segments.cbegin(), currentPanel.segments.cend(),
                    [&segment](const auto &old) { return old == segment; });
            if ((!retainedStatus && !segmentCapabilities.value(QStringLiteral("sources")).toStringList().contains(segment.source)) ||
                !segmentCapabilities.value(QStringLiteral("motionProfiles")).toStringList().contains(segment.motionProfile))
                segmentError = QStringLiteral("the segment source or motion profile is unavailable");
            if (segment.background != QStringLiteral("solid") && segment.corners != QStringLiteral("inherited"))
                segmentError = QStringLiteral("segment corner overrides require a solid background");
        }
    }
    if (!segmentError.isEmpty())
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->revision = currentPanel.settingsRevision;
            outcome->errorCode = QStringLiteral("unavailable-segment-feature");
            outcome->errorMessage = segmentError;
        }
        return std::nullopt;
    }
    if (outcome)
    {
        outcome->capabilityResolution = resolution;
    }
    const QVariantList broadFields = panelSettingsEditorFields(
        draft->candidatePanel, resolution, QStringLiteral("studio"));
    QSet<QString> availableFields;
    QVariantMap tiltEditor;
    for (const QVariant &value : broadFields)
    {
        availableFields.insert(
            value.toMap().value(QStringLiteral("key")).toString());
        if (value.toMap().value(QStringLiteral("key")).toString() == QStringLiteral("bakedTilt"))
            tiltEditor = value.toMap();
    }
    const QVariantMap currentValues = currentPanel.toLegacyMap();
    const QVariantMap candidateValues = draft->candidatePanel.toLegacyMap();
    if (!tiltEditor.isEmpty() && panelValues.contains(QStringLiteral("bakedTilt"))
        && candidateValues.value(QStringLiteral("bakedTilt")) != currentValues.value(QStringLiteral("bakedTilt")))
    {
        const double tilt = candidateValues.value(QStringLiteral("bakedTilt")).toDouble();
        if (tilt < tiltEditor.value(QStringLiteral("minimumValue")).toDouble()
            || tilt > tiltEditor.value(QStringLiteral("maximumValue")).toDouble())
        {
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->errorCode = QStringLiteral("tilt-out-of-range");
                outcome->errorMessage = QStringLiteral("the requested tilt exceeds this theme's declared range");
            }
            return std::nullopt;
        }
    }
    for (auto it = panelValues.cbegin(); it != panelValues.cend(); ++it)
    {
        const ArchDock::PanelSettingsFieldDescriptor *field =
            ArchDock::PanelSettingsSchema::panelDescriptor(it.key());
        if (field && field->access == ArchDock::PanelSettingsFieldAccess::Editor &&
            field->editor.isPresented() && !availableFields.contains(it.key()))
        {
            // Studio submits its full snapshot when changing capabilities, and
            // presets and profiles carry values for fields that do not act on
            // this panel. A value that stays as it is changes nothing and is
            // retained; only a change to a field the panel does not offer is
            // refused (ADREP-TASK-001 truth rules).
            if (candidateValues.value(it.key(), field->defaultValue) ==
                    currentValues.value(it.key(), field->defaultValue))
            {
                continue;
            }
            // PD-17 restores the exact saved look, including material values
            // whose controls disappear in the flat tier. Only values equal
            // to this panel's durable snapshot can use this exception.
            const auto remembered = currentPanel.surface.parameters2D
                .value(QStringLiteral("previousFlatLook")).toMap();
            const auto restoredTier = draft->candidatePanel.surface.rendererTier;
            if (panelValues.contains(QStringLiteral("previousFlatLook"))
                && panelValues.value(QStringLiteral("previousFlatLook")).toMap().isEmpty()
                && restoredTier != QStringLiteral("true3d")
                && restoredTier != QStringLiteral("baked2.5d")
                && ArchDock::PanelSettingsSchema::flatLookValues(currentValues).contains(it.key())
                && remembered.contains(it.key())
                && candidateValues.value(it.key()) == remembered.value(it.key()))
                continue;
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel.settingsRevision;
                outcome->errorCode = QStringLiteral("unavailable-panel-field");
                outcome->errorMessage = QStringLiteral(
                    "the field '%1' is unavailable for the resolved candidate")
                    .arg(it.key());
            }
            return std::nullopt;
        }
    }

    QString validationError;
    if (!draft->candidatePanel.isValid(&validationError))
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->revision = currentPanel.settingsRevision;
            outcome->errorCode = QStringLiteral("invalid-panel-definition");
            outcome->errorMessage = validationError;
        }
        return std::nullopt;
    }
    return draft;
}

QVariantMap PanelWindow::panelSettingsEditorSnapshot(
    const QString &panelId,
    const QString &consumer) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    if (!definition.has_value())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("validation-failed")},
            {QStringLiteral("errorCode"), QStringLiteral("panel-not-found")},
            {QStringLiteral("panelId"), panelId},
        };
    }
    return panelSettingsEditorSnapshotOf(*definition, consumer);
}

QVariantMap PanelWindow::panelSettingsEditorSnapshotOf(
    const ArchDock::PanelDefinition &panel,
    const QString &consumer) const
{
    const QString panelId = panel.identity.id;
    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(panel);
    const QVariantList panelFields = panelSettingsEditorFields(
        panel, resolution, normalizedConsumer);
    const QVariantList globalFields = panelSettingsEditorFields(
        panel, resolution, normalizedConsumer, ArchDock::PanelSettingsFieldScope::Global);
    QSet<QString> globalKeys;
    for (const QVariant &value : globalFields)
    {
        globalKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    QVariantMap globalValues;
    const QVariantMap globalSnapshot = m_settings.editorTransactionSnapshot();
    for (const QString &key : std::as_const(globalKeys))
    {
        globalValues.insert(key, globalSnapshot.value(key));
    }
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            panel, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            panel, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"),
         QVariant::fromValue<qulonglong>(panel.settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(panel, panelFields)},
        {QStringLiteral("flatLookValues"),
         ArchDock::PanelSettingsSchema::flatLookValues(panel.toLegacyMap())},
        {QStringLiteral("globalValues"), globalValues},
        {QStringLiteral("panelFields"), panelFields},
        {QStringLiteral("globalFields"), globalFields},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themeDefinition"),
         themeProjection.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"),
         themeProjection.has_value()
             ? QStringLiteral("ready")
             : themeProjectionError.isEmpty()
                 ? QStringLiteral("unavailable")
                 : QStringLiteral("error")},
        {QStringLiteral("themeProjectionError"), themeProjectionError},
        {QStringLiteral("iconStyleDefinition"),
         iconStyleProjection.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"),
         iconStyleProjection.has_value()
             ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themes"), resolvedThemeDefinitionsOn(panel)},
    };
}

QVariantMap PanelWindow::resolvePanelSettingsEditorDraft(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    const QString &consumer) const
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        preparePanelSettingsDraft(
            panelId,
            expectedRevision,
            panelValues,
            globalValues,
            &outcome);
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }
    return panelSettingsEditorProjection(*draft, expectedRevision, panelValues, consumer);
}

QVariantMap PanelWindow::panelSettingsEditorProjection(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QString &consumer) const
{
    const QString panelId = draft.candidatePanel.identity.id;
    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(draft.candidatePanel);
    const QVariantList panelFields = panelSettingsEditorFields(
        draft.candidatePanel, resolution, normalizedConsumer);
    const QVariantList globalFields = panelSettingsEditorFields(
        draft.candidatePanel, resolution, normalizedConsumer,
        ArchDock::PanelSettingsFieldScope::Global);
    QSet<QString> globalKeys;
    for (const QVariant &value : globalFields)
    {
        globalKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    QVariantMap projectedGlobals;
    for (const QString &key : std::as_const(globalKeys))
    {
        projectedGlobals.insert(key, draft.candidateGlobals.value(key));
    }
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            draft.candidatePanel, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            draft.candidatePanel, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("resolved")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"), QVariant::fromValue<qulonglong>(expectedRevision)},
        {QStringLiteral("candidateRevision"),
         QVariant::fromValue<qulonglong>(draft.candidatePanel.settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(draft.candidatePanel, panelFields)},
        {QStringLiteral("flatLookValues"),
         ArchDock::PanelSettingsSchema::flatLookValues(draft.candidatePanel.toLegacyMap())},
        {QStringLiteral("globalValues"), projectedGlobals},
        {QStringLiteral("panelFields"), panelFields},
        {QStringLiteral("globalFields"), globalFields},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themeDefinition"),
         themeProjection.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"),
         themeProjection.has_value()
             ? QStringLiteral("ready")
             : themeProjectionError.isEmpty()
                 ? QStringLiteral("unavailable")
                 : QStringLiteral("error")},
        {QStringLiteral("themeProjectionError"), themeProjectionError},
        {QStringLiteral("iconStyleDefinition"),
         iconStyleProjection.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"),
         iconStyleProjection.has_value()
             ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themes"),
         resolvedThemeDefinitionsOn(draft.previousPanel, panelValues)},
    };
}

QVariantMap PanelWindow::previewPanelSettingsEditorDraft(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    const QString &consumer)
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        preparePanelSettingsDraft(panelId, expectedRevision, panelValues, globalValues, &outcome);
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }
    QVariantMap projection = panelSettingsEditorProjection(*draft, expectedRevision, panelValues, consumer);
    // A desktop preview or a profile already shows this panel its own way;
    // Panel Studio leaves it to them.
    const bool previewed = profileBusy() ||
        (m_presetAudition && m_presetAudition->previewDefinition(panelId));
    const auto refusal = draftIconRoomRefusal(*draft);
    if (refusal)
    {
        projection.insert(QStringLiteral("previewRefusal"), *refusal);
    }
    else if (!previewed)
    {
        StudioDraft shown;
        shown.panel = draft->candidatePanel;
        // Visibility, as in a desktop preview, changes on Apply: a panel
        // being edited stays in view.
        shown.panel.visibility = draft->previousPanel.visibility;
        shown.baseRevision = draft->previousPanel.settingsRevision;
        for (auto it = draft->candidateGlobals.cbegin(); it != draft->candidateGlobals.cend(); ++it)
        {
            if (draft->previousGlobals.value(it.key()) != it.value())
            {
                shown.globals.insert(it.key(), it.value());
            }
        }
        if (m_studioDraft && m_studioDraft->panel.identity.id != panelId)
        {
            clearPanelSettingsPreview();
        }
        // Where a free panel stands and how big its widget is before the
        // first draft is drawn: a draft can grow the widget (a Möbius strip
        // reaches further than a ring), and Cancel puts both back.
        if (m_studioDraftHost.panelId != panelId)
        {
            m_studioDraftHost = {panelId, nativePlacementValues(draft->previousPanel), std::nullopt, std::nullopt};
            if (draft->previousPanel.host.kind == ArchDock::PanelHostKind::FreeDesktop)
            {
                QString geometryError;
                m_studioDraftHost.freeGeometry = presetFreeHostGeometry(draft->previousPanel, &geometryError);
            }
        }
        auto unchanged = draft->candidatePanel;
        unchanged.settingsRevision = draft->previousPanel.settingsRevision;
        if (unchanged == draft->previousPanel && shown.globals.isEmpty())
        {
            clearPanelSettingsPreview();
        }
        else
        {
            m_studioDraft = std::move(shown);
            notifyDockRevision();
            notifyDockEntriesRevision();
            // Plasma follows a moment later, outside this call: its scripts
            // wait for the panel to settle.
            m_studioDraftHostTimer.start();
        }
    }
    projection.insert(QStringLiteral("previewShown"), m_studioDraft.has_value() &&
        m_studioDraft->panel.identity.id == panelId && !refusal);
    return projection;
}

void PanelWindow::clearPanelSettingsPreview()
{
    m_studioDraftHostTimer.stop();
    const bool shown = m_studioDraft.has_value();
    m_studioDraft.reset();
    if (!m_studioDraftHost.panelId.isEmpty())
    {
        m_studioDraftRestores.append(m_studioDraftHost);
        m_studioDraftHost = {};
        // Once the widget draws the saved panel again.
        QTimer::singleShot(400, this, &PanelWindow::restoreStudioDraftHosts);
    }
    if (shown)
    {
        notifyDockRevision();
        notifyDockEntriesRevision();
    }
}

void PanelWindow::showStudioDraftHost()
{
    if (m_studioDraftHostBusy || !m_studioDraft || profileBusy())
    {
        return;
    }
    const ArchDock::PanelDefinition panel = m_studioDraft->panel;
    const QString panelId = panel.identity.id;
    const auto stored = m_panelRegistry.panelDefinition(panelId);
    if (!stored || stored->settingsRevision != m_studioDraft->baseRevision)
    {
        return;
    }
    if (m_studioDraftHost.panelId != panelId)
    {
        m_studioDraftHost = {panelId, nativePlacementValues(*stored), std::nullopt, std::nullopt};
    }
    m_studioDraftHostBusy = true;
    if (panel.host.kind == ArchDock::PanelHostKind::NativeEdge)
    {
        // The edge panel's Plasma panel is placed from the panel as it is
        // drawn, the draft.
        const QVariantMap placement = nativePlacementValues(panel);
        if (placement != m_studioDraftHost.placement && synchronizeNativePanelPlacement(panelId) &&
            m_studioDraftHost.panelId == panelId)
        {
            m_studioDraftHost.placement = placement;
        }
    }
    else
    {
        const std::pair<int, int> wanted{panel.placement.x, panel.placement.y};
        const std::pair<int, int> standing = m_studioDraftHost.freePosition.value_or(
            std::pair<int, int>{stored->placement.x, stored->placement.y});
        QString error;
        auto geometry = wanted != standing ? presetFreeHostGeometry(*stored, &error) : std::nullopt;
        if (geometry)
        {
            // Where it stood first, to go back to: the place Plasma gives
            // it, which a move by hand changes and the saved settings do not.
            if (!m_studioDraftHost.freeGeometry)
            {
                m_studioDraftHost.freeGeometry = geometry;
            }
            geometry->insert(QStringLiteral("x"), wanted.first);
            geometry->insert(QStringLiteral("y"), wanted.second);
            if (setPresetFreeHostGeometry(*stored, *geometry, &error, true) &&
                m_studioDraftHost.panelId == panelId)
            {
                m_studioDraftHost.freePosition = wanted;
            }
        }
    }
    m_studioDraftHostBusy = false;
    // A change made while Plasma was busy is shown next; a draft dropped
    // meanwhile is put back.
    if (m_studioDraft && m_studioDraft->panel != panel)
    {
        m_studioDraftHostTimer.start();
    }
    if (!m_studioDraftRestores.isEmpty())
    {
        QTimer::singleShot(0, this, &PanelWindow::restoreStudioDraftHosts);
    }
}

void PanelWindow::restoreStudioDraftHosts()
{
    while (!m_studioDraftHostBusy && !m_studioDraftRestores.isEmpty())
    {
        const StudioDraftHost host = m_studioDraftRestores.takeFirst();
        const auto stored = m_panelRegistry.panelDefinition(host.panelId);
        if (!stored)
        {
            continue;
        }
        m_studioDraftHostBusy = true;
        if (stored->host.kind == ArchDock::PanelHostKind::NativeEdge)
        {
            if (host.placement != nativePlacementValues(*stored) && !synchronizeNativePanelPlacement(host.panelId))
            {
                qWarning() << "Could not put the Plasma panel of" << host.panelId << "back after Panel Studio's draft";
            }
        }
        else if (host.freeGeometry)
        {
            QString error;
            const auto standing = presetFreeHostGeometry(*stored, &error);
            if (standing && *standing != *host.freeGeometry &&
                !setPresetFreeHostGeometry(*stored, *host.freeGeometry, &error, false))
            {
                qWarning() << "Could not put free panel" << host.panelId << "back after Panel Studio's draft:" << error;
            }
        }
        m_studioDraftHostBusy = false;
    }
}

std::optional<QString> PanelWindow::draftIconRoomRefusal(
    const ArchDock::PanelSettingsTransactionDraft &draft) const
{
    // The room a free 3D panel's icons get is measured on the panel as it
    // is drawn, which is Panel Studio's draft or a desktop preview while
    // one is shown.
    const ArchDock::PanelDefinition drawn =
        runtimePanelDefinition(draft.previousPanel.identity.id).value_or(draft.previousPanel);
    return ArchDock::panelIconRoomRefusal(draft.previousPanel, draft.candidatePanel,
                                          seenRoomFactor(draft.previousPanel),
                                          predictedSeenRoom(draft.candidatePanel, &drawn));
}

std::optional<ArchDock::PanelSettingsTransactionDraft> PanelWindow::profilePanelDraft(
    const QString &profileId, const QString &panelId, qulonglong expectedRevision,
    const QVariantMap &panelValues, ArchDock::PanelSettingsTransactionOutcome *outcome) const
{
    const auto fail = [&](const QString &code) -> std::optional<ArchDock::PanelSettingsTransactionDraft> {
        if (outcome)
        {
            *outcome = {};
            outcome->panelId = panelId;
            outcome->expectedRevision = expectedRevision;
            outcome->status = code == QStringLiteral("stale-profile-revision")
                ? ArchDock::PanelSettingsTransactionStatus::RevisionConflict
                : ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = code;
        }
        return std::nullopt;
    };
    if (!m_profileManager) return fail(QStringLiteral("profile-panel-not-found"));
    QString error;
    int revision = 0;
    const auto panel = m_profileManager->profilePanel(profileId, panelId, &revision, &error);
    if (!panel) return fail(error.isEmpty() ? QStringLiteral("profile-panel-not-found") : error);
    if (static_cast<qulonglong>(revision) != expectedRevision) return fail(QStringLiteral("stale-profile-revision"));
    // A new automatic name is unique among the profile's panels: they are
    // the arrangement the profile puts on the desktop.
    const QStringList taken = m_profileManager->profilePanelNames(profileId, panelId);
    auto draft = preparePanelSettingsDraftOn(*panel, panel->settingsRevision, panelValues, {}, outcome, &taken);
    if (outcome)
    {
        outcome->expectedRevision = outcome->previousRevision = outcome->revision = expectedRevision;
    }
    return draft;
}

QVariantMap PanelWindow::profilePanelEditorSnapshot(const QString &profileId, const QString &panelId,
                                                    const QString &consumer) const
{
    QString error;
    int revision = 0;
    const auto panel = m_profileManager
        ? m_profileManager->profilePanel(profileId, panelId, &revision, &error) : std::nullopt;
    if (!panel)
    {
        return {{QStringLiteral("success"), false},
                {QStringLiteral("status"), QStringLiteral("validation-failed")},
                {QStringLiteral("errorCode"), error.isEmpty() ? QStringLiteral("profile-panel-not-found") : error},
                {QStringLiteral("panelId"), panelId}};
    }
    QVariantMap snapshot = panelSettingsEditorSnapshotOf(*panel, consumer);
    snapshot.insert(QStringLiteral("revision"), QVariant::fromValue<qulonglong>(revision));
    snapshot.insert(QStringLiteral("globalFields"), QVariantList{});
    snapshot.insert(QStringLiteral("globalValues"), QVariantMap{});
    snapshot.insert(QStringLiteral("profileId"), profileId);
    return snapshot;
}

QVariantMap PanelWindow::resolveProfilePanelEditorDraft(const QString &profileId, const QString &panelId,
                                                        qulonglong expectedRevision, const QVariantMap &panelValues,
                                                        const QString &consumer) const
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const auto draft = profilePanelDraft(profileId, panelId, expectedRevision, panelValues, &outcome);
    if (!draft) return outcome.toVariantMap();
    QVariantMap projection = panelSettingsEditorProjection(*draft, expectedRevision, panelValues, consumer);
    projection.insert(QStringLiteral("globalFields"), QVariantList{});
    projection.insert(QStringLiteral("globalValues"), QVariantMap{});
    projection.insert(QStringLiteral("profileId"), profileId);
    return projection;
}

QVariantMap PanelWindow::applyProfilePanelSettings(const QString &profileId, const QString &panelId,
                                                   qulonglong expectedRevision, const QVariantMap &panelValues)
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    auto draft = profilePanelDraft(profileId, panelId, expectedRevision, panelValues, &outcome);
    if (!draft) return outcome.toVariantMap();
    // The room its icons get is the room they had on the desktop, where
    // the profile puts the panel.
    if (const auto refusal = ArchDock::panelIconRoomRefusal(draft->previousPanel, draft->candidatePanel,
            seenRoomFactor(draft->previousPanel), predictedSeenRoom(draft->candidatePanel, &draft->previousPanel)))
    {
        outcome.status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
        outcome.errorCode = QStringLiteral("panel-too-small");
        outcome.errorMessage = *refusal;
        return outcome.toVariantMap();
    }
    ArchDock::PresetApplication::markCustomized(draft->previousPanel, &draft->candidatePanel);
    const QVariantMap saved = m_profileManager->replaceProfilePanel(profileId,
        static_cast<int>(expectedRevision), draft->candidatePanel);
    if (!saved.value(QStringLiteral("success")).toBool())
    {
        const QString code = saved.value(QStringLiteral("errorCode")).toString();
        outcome.status = code == QStringLiteral("stale-profile-revision")
            ? ArchDock::PanelSettingsTransactionStatus::RevisionConflict
            : ArchDock::PanelSettingsTransactionStatus::PersistenceFailed;
        outcome.errorCode = code;
        return outcome.toVariantMap();
    }
    outcome.status = ArchDock::PanelSettingsTransactionStatus::Succeeded;
    outcome.revision = saved.value(QStringLiteral("revision")).toULongLong();
    return outcome.toVariantMap();
}

QVariantMap PanelWindow::profilePanelThemeCandidate(const QString &profileId, const QString &panelId,
                                                    const QString &themeId) const
{
    const auto panel = m_profileManager ? m_profileManager->profilePanel(profileId, panelId) : std::nullopt;
    if (!panel)
        return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-panel-not-found")},
                {QStringLiteral("values"), QVariantMap{}}};
    return themeCandidateFor(*panel, themeId);
}

QVariantMap PanelWindow::applyPanelSettingsTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues)
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        preparePanelSettingsDraft(
            panelId,
            expectedRevision,
            panelValues,
            globalValues,
            &outcome);
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }
    // A change made in Panel Studio never leaves a panel's icons less room
    // than they need.
    if (const auto refusal = draftIconRoomRefusal(*draft))
    {
        outcome.status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
        outcome.errorCode = QStringLiteral("panel-too-small");
        outcome.errorMessage = *refusal;
        return outcome.toVariantMap();
    }

    return commitPanelSettingsDraft(*draft, outcome);
}

QVariantMap PanelWindow::commitPanelSettingsDraft(
    ArchDock::PanelSettingsTransactionDraft draft,
    ArchDock::PanelSettingsTransactionOutcome outcome)
{
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

    // Panel Studio's Apply commits the draft the desktop shows. Another
    // change to the panel leaves that draft for Panel Studio to make again
    // on the new settings.
    bool studioApply = false;
    if (m_studioDraft && m_studioDraft->panel.identity.id == draft.candidatePanel.identity.id)
    {
        auto committed = draft.candidatePanel;
        committed.visibility = m_studioDraft->panel.visibility;
        studioApply = committed == m_studioDraft->panel;
    }
    ArchDock::PresetApplication::markCustomized(draft.previousPanel, &draft.candidatePanel);
    const auto &freeHost = draft.candidatePanel.host;
    const bool freePositionChanged = freeHost.kind == ArchDock::PanelHostKind::FreeDesktop
        && (draft.previousPanel.placement.x != draft.candidatePanel.placement.x
            || draft.previousPanel.placement.y != draft.candidatePanel.placement.y);
    if (freePositionChanged && freeHost.freeDesktopContainmentId >= 0
        && freeHost.freeDockAppletId >= 0 && !freeHost.freeOwnershipToken.isEmpty())
    {
        QString error;
        const auto geometry = presetFreeHostGeometry(draft.previousPanel, &error);
        if (!geometry)
        {
            outcome.status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome.errorCode = error;
            outcome.errorMessage = QStringLiteral("the owned free-panel position could not be verified");
            return outcome.toVariantMap();
        }
        draft.previousFreeHostGeometry = *geometry;
        draft.candidateFreeHostGeometry = *geometry;
        draft.candidateFreeHostGeometry.insert(QStringLiteral("x"), draft.candidatePanel.placement.x);
        draft.candidateFreeHostGeometry.insert(QStringLiteral("y"), draft.candidatePanel.placement.y);
    }
    QString persistenceError;
    if (!m_panelRegistry.persistPanelSettingsTransaction(draft, &persistenceError))
    {
        const std::optional<ArchDock::PanelDefinition> latest =
            m_panelRegistry.panelDefinition(draft.previousPanel.identity.id);
        const bool conflict = latest.has_value() &&
            latest->settingsRevision != draft.previousPanel.settingsRevision;
        outcome.status = conflict
            ? ArchDock::PanelSettingsTransactionStatus::RevisionConflict
            : ArchDock::PanelSettingsTransactionStatus::PersistenceFailed;
        outcome.errorCode = conflict
            ? QStringLiteral("stale-revision")
            : QStringLiteral("persistence-failed");
        outcome.errorMessage = persistenceError;
        return outcome.toVariantMap();
    }

    outcome.hostResults = applyPanelSettingsHosts(draft);
    if (ArchDock::PanelSettingsTransaction::requiredHostsSucceeded(
            outcome.hostResults))
    {
        m_settingsTransactionAdoptionActive = true;
        m_settings.adoptTransaction(draft.candidateGlobals);
        m_settingsTransactionAdoptionActive = false;

        ArchDock::PanelSettingsHostResult renderer;
        renderer.component = QStringLiteral("renderer-notification");
        renderer.required = true;
        renderer.success = true;
        renderer.status = QStringLiteral("published");
        outcome.hostResults.append(renderer);
        outcome.status = ArchDock::PanelSettingsTransactionStatus::Succeeded;
        outcome.revision = draft.candidatePanel.settingsRevision;
        // Panel Studio's draft of this panel is now its saved settings, and
        // Plasma already shows them.
        if (studioApply)
        {
            m_studioDraftHostTimer.stop();
            m_studioDraft.reset();
            m_studioDraftHost = {};
        }
        m_panelRegistry.notifyPanelSettingsTransactionAdopted(
            panelSettingsTopologyChanged(
                draft.previousPanel, draft.candidatePanel));
        return outcome.toVariantMap();
    }

    const QList<ArchDock::PanelSettingsHostResult> hostRollbackResults =
        rollbackPanelSettingsHosts(draft);
    outcome.hostResults.append(hostRollbackResults);
    quint64 rollbackRevision = 0;
    QString rollbackError;
    if (m_panelRegistry.rollbackPanelSettingsTransaction(
            draft,
            draft.candidatePanel.settingsRevision,
            &rollbackRevision,
            &rollbackError))
    {
        outcome.status = ArchDock::PanelSettingsTransactionStatus::HostFailed;
        outcome.errorCode = QStringLiteral("required-host-apply-failed");
        outcome.errorMessage = QStringLiteral(
            "a required host change failed; the persisted draft was rolled back");
        outcome.rolledBack = true;
        outcome.rollbackRevision = rollbackRevision;
        m_panelRegistry.notifyPanelSettingsTransactionAdopted(
            panelSettingsTopologyChanged(
                draft.previousPanel, draft.candidatePanel));
        return outcome.toVariantMap();
    }

    m_settingsTransactionAdoptionActive = true;
    m_settings.adoptTransaction(draft.candidateGlobals);
    m_settingsTransactionAdoptionActive = false;
    outcome.status = ArchDock::PanelSettingsTransactionStatus::RollbackFailed;
    outcome.errorCode = QStringLiteral("rollback-persistence-failed");
    outcome.errorMessage = rollbackError;
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(
        panelSettingsTopologyChanged(draft.previousPanel, draft.candidatePanel));
    return outcome.toVariantMap();
}

bool PanelWindow::setDockConfiguration(const QString &panelId,
                                       const QString &key,
                                       const QVariant &value)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const bool globalField =
        ArchDock::PanelSettingsSchema::supportsMutationInterface(
            ArchDock::PanelSettingsFieldScope::Global,
            key,
            QStringLiteral("dock-configuration"));
    const bool panelField =
        ArchDock::PanelSettingsSchema::supportsMutationInterface(
            ArchDock::PanelSettingsFieldScope::Panel,
            key,
            QStringLiteral("dock-configuration"));
    if (globalField == panelField)
    {
        return false;
    }
    const qulonglong expectedRevision = m_panelRegistry.panelValue(
        panelId, QStringLiteral("settingsRevision")).toULongLong();
    const QVariantMap result = applyPanelSettingsTransaction(
        panelId,
        expectedRevision,
        panelField ? QVariantMap{{key, value}} : QVariantMap{},
        globalField ? QVariantMap{{key, value}} : QVariantMap{});
    return result.value(QStringLiteral("success")).toBool();
}

QList<ArchDock::PanelSettingsHostResult> PanelWindow::applyPanelSettingsHosts(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    bool includeVisibility)
{
    QList<ArchDock::PanelSettingsHostResult> results;
    const QVariantMap before = draft.previousPanel.toLegacyMap();
    const QVariantMap candidate = draft.candidatePanel.toLegacyMap();
    const QString panelId = draft.candidatePanel.identity.id;
    const bool nativeHost =
        draft.candidatePanel.host.kind == ArchDock::PanelHostKind::NativeEdge;
    const bool ownedHost = nativeHost &&
        draft.candidatePanel.host.nativePanelId >= 0 &&
        !draft.candidatePanel.host.nativeOwnershipToken.trimmed().isEmpty();
    const auto anyChanged = [&before, &candidate](const QStringList &keys)
    {
        for (const QString &key : keys)
        {
            if (before.value(key) != candidate.value(key))
            {
                return true;
            }
        }
        return false;
    };

    ArchDock::PanelSettingsHostResult placement;
    placement.component = QStringLiteral("native-placement");
    placement.required = ownedHost && anyChanged(nativePlacementKeys());
    if (placement.required)
    {
        ArchDock::PlasmaPanelPlacementApplyResult applied =
            applyNativePanelPlacementTransaction(
                panelId,
                draft.candidatePanel.host.nativePanelId,
                draft.candidatePanel.host.nativeOwnershipToken,
                nativePlacementValues(draft.candidatePanel),
                false);
        placement.success = applied.success();
        placement.status = applied.status;
        placement.errorCode = applied.errorCode;
        placement.details = applied.toVariantMap();
        recordNativePanelPlacementResult(panelId, std::move(applied));
    }
    results.append(placement);

    ArchDock::PanelSettingsHostResult freePlacement;
    freePlacement.component = QStringLiteral("free-placement");
    freePlacement.required = !draft.candidateFreeHostGeometry.isEmpty();
    if (freePlacement.required)
    {
        freePlacement.success = setPresetFreeHostGeometry(draft.candidatePanel,
            draft.candidateFreeHostGeometry, &freePlacement.errorCode, true);
        freePlacement.status = freePlacement.success ? QStringLiteral("applied") : QStringLiteral("failed");
        freePlacement.details = {{QStringLiteral("requested"), draft.candidateFreeHostGeometry}};
    }
    results.append(freePlacement);

    const bool visibilityChanged = anyChanged({
        QStringLiteral("visible"),
        QStringLiteral("visibilityMode"),
    });
    ArchDock::PanelSettingsHostResult visibility;
    visibility.component = QStringLiteral("native-visibility");
    visibility.required = includeVisibility && ownedHost && visibilityChanged;
    if (visibility.required)
    {
        const ArchDock::PanelVisibilityMode mode =
            ArchDock::panelVisibilityModeFromString(
                draft.candidatePanel.visibility.hostMode);
        const bool applied = reconcileNativePanelVisibility(
            panelId,
            draft.candidatePanel.host.nativePanelId,
            draft.candidatePanel.host.nativeOwnershipToken,
            mode,
            draft.candidatePanel.visibility.visible);
        visibility.details = nativePanelVisibilityStatus(panelId);
        const QString effectiveMode = visibility.details.value(
            QStringLiteral("effectiveMode")).toString();
        visibility.success = applied &&
            (effectiveMode.isEmpty() ||
             effectiveMode == draft.candidatePanel.visibility.hostMode);
        visibility.status = visibility.details.value(
            QStringLiteral("status"),
            visibility.success ? QStringLiteral("applied")
                               : QStringLiteral("failed")).toString();
        visibility.errorCode = visibility.details.value(
            QStringLiteral("errorCode")).toString();
        if (applied && !visibility.success)
        {
            visibility.errorCode = QStringLiteral("uncommitted-visibility-fallback");
        }
    }
    results.append(visibility);

    ArchDock::PanelSettingsHostResult rendererHost;
    rendererHost.component = QStringLiteral("native-renderer");
    const bool typeChanged = before.value(QStringLiteral("type")) !=
        candidate.value(QStringLiteral("type"));
    rendererHost.required = ownedHost && typeChanged;
    if (rendererHost.required)
    {
        const QString oldType = draft.previousPanel.content.type;
        const QString newType = draft.candidatePanel.content.type;
        if (panelTypeNeedsDockApplet(oldType) && panelTypeNeedsDockApplet(newType) &&
            nativeDockAppletIsOwned(
                panelId,
                draft.candidatePanel.host.nativePanelId,
                draft.candidatePanel.host.nativeDockAppletId))
        {
            const int applied = evaluatePlasmaScriptResult(
                QStringLiteral(
                    "var result = (function() { var panel = panelById(%1);"
                    "if (!panel) return 0; panel.currentConfigGroup = ['ArchDock'];"
                    "if (String(panel.readConfig('panelId', '')) !== %4 || "
                    "String(panel.readConfig('ownerToken', '')) !== %5) return 0;"
                    "var dock = panel ? panel.widgetById(%2) : null;"
                    "if (!dock || dock.type !== 'org.archdock.dock') return 0;"
                    "dock.currentConfigGroup = ['General'];"
                    "if (String(dock.readConfig('panelId', '')) !== %4) return 0;"
                    "dock.writeConfig('panelType', %3); dock.reloadConfig();"
                    "return String(dock.readConfig('panelType', '')) === %3 ? 1 : 0; })();"
                    "print('ARCHDOCK_RESULT:' + String(result));")
                    .arg(draft.candidatePanel.host.nativePanelId)
                    .arg(draft.candidatePanel.host.nativeDockAppletId)
                    .arg(plasmaScriptStringLiteral(newType))
                    .arg(plasmaScriptStringLiteral(panelId))
                    .arg(plasmaScriptStringLiteral(draft.candidatePanel.host.nativeOwnershipToken)));
            rendererHost.success = applied == 1;
            rendererHost.status = rendererHost.success
                ? QStringLiteral("applied")
                : QStringLiteral("failed");
            rendererHost.errorCode = rendererHost.success
                ? QString{}
                : QStringLiteral("renderer-type-readback-failed");
        }
        else
        {
            rendererHost.success = false;
            rendererHost.status = QStringLiteral("unsupported");
            rendererHost.errorCode = QStringLiteral(
                "renderer-topology-change-not-atomic");
        }
    }
    results.append(rendererHost);
    return results;
}

QList<ArchDock::PanelSettingsHostResult> PanelWindow::rollbackPanelSettingsHosts(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    bool includeVisibility)
{
    ArchDock::PanelSettingsTransactionDraft reverseDraft{
        draft.candidatePanel,
        draft.previousPanel,
        draft.candidateGlobals,
        draft.previousGlobals,
        draft.candidateFreeHostGeometry,
        draft.previousFreeHostGeometry,
    };
    QList<ArchDock::PanelSettingsHostResult> results =
        applyPanelSettingsHosts(reverseDraft, includeVisibility);
    for (ArchDock::PanelSettingsHostResult &result : results)
    {
        result.component.prepend(QStringLiteral("rollback-"));
    }
    return results;
}

bool PanelWindow::panelSettingsTopologyChanged(
    const ArchDock::PanelDefinition &before,
    const ArchDock::PanelDefinition &after)
{
    return before.host != after.host ||
        before.placement != after.placement ||
        before.visibility != after.visibility ||
        before.content.type != after.content.type;
}

bool PanelWindow::setDockStringConfiguration(const QString &panelId,
                                             const QString &key,
                                             const QString &value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockIntegerConfiguration(const QString &panelId,
                                              const QString &key,
                                              int value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockRealConfiguration(const QString &panelId,
                                           const QString &key,
                                           double value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockBooleanConfiguration(const QString &panelId,
                                              const QString &key,
                                              bool value)
{
    return setDockConfiguration(panelId, key, value);
}
