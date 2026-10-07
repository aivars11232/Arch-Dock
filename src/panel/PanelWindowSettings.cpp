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
#include "../model/PanelSettingsSchema.h"
#include "../presets/PresetApplication.h"

#include <QDBusArgument>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QScreen>
#include <QSet>

#include <utility>

using PanelWindowHelpers::plasmaScriptStringLiteral;
using PanelWindowHelpers::panelTypeNeedsDockApplet;

namespace
{
// Layouts whose entries stand on a curved track, as LayoutEngine.curvedLayout()
// in the shared renderer decides: closed shapes and the open sweep paths.
bool curvedLayout(const QString &pathType)
{
    static const QStringList curved = {
        QStringLiteral("circular"), QStringLiteral("ring"), QStringLiteral("ellipse"),
        QStringLiteral("polygon"), QStringLiteral("triangle"), QStringLiteral("square"),
        QStringLiteral("pentagon"), QStringLiteral("hexagon"), QStringLiteral("octagon"),
        QStringLiteral("arc"), QStringLiteral("semicircle"), QStringLiteral("fan"),
        QStringLiteral("radial")};
    return curved.contains(pathType);
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
    return m_panelRegistry.panelDefinition(panelId);
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
        m_settings.transactionSnapshot());
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
    QString definitionError;
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId, &definitionError);
    if (!current.has_value())
    {
        return std::nullopt;
    }

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

    QVariantMap candidateRecord = current->toLegacyMap();
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
        QString::number(current->settingsRevision));
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
    const std::optional<ArchDock::PanelDefinition> candidate =
        capabilityCandidateDefinition(panelId, candidateValues);
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
    // Whether the icon style draws layers of its own, which take the place of
    // the plain tile behind an icon. Resolved once, for the fields that ask.
    std::optional<bool> styleLayers;
    const auto iconStyleDrawsLayers = [&]() {
        if (!styleLayers.has_value())
        {
            styleLayers = false;
            const auto style = candidate.iconStyle.styleReference == QStringLiteral("plain-original")
                ? std::nullopt : m_panelRegistry.iconStyleRuntimeProjection(candidate);
            if (style)
                for (const QVariant &role : style->value(QStringLiteral("layers")).toMap())
                    if (!role.toList().isEmpty())
                        styleLayers = true;
        }
        return *styleLayers;
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
            // Only launcher and hybrid content take drops, and folders arrive
            // by being dropped.
            available = droppedContent;
            break;
        case ArchDock::EditorCapability::EdgePlacement:
        case ArchDock::EditorCapability::Alignment:
        case ArchDock::EditorCapability::DynamicPlacement:
        case ArchDock::EditorCapability::VisibilityMode:
            // Placement along a screen edge and Plasma's panel visibility
            // belong to edge panels; a free panel has neither (OF-02, OF-06).
            available = !freeHost;
            break;
        case ArchDock::EditorCapability::LengthMutation:
        case ArchDock::EditorCapability::ThicknessMutation:
            // A free panel is drawn from its layout's radius, scale and icons;
            // a stored width or height reaches no renderer (OF-03, PD-02).
            available = !freeHost && controlAvailable(descriptor->editor.capability);
            break;
        case ArchDock::EditorCapability::ArbitraryXyPlacement:
        case ArchDock::EditorCapability::DynamicTint:
        case ArchDock::EditorCapability::IconStateStyling:
            available = controlAvailable(descriptor->editor.capability);
            break;
        case ArchDock::EditorCapability::DynamicGlow:
            // The plain 2D surface draws no glow for this to scale.
            available = controlAvailable(descriptor->editor.capability) &&
                tier.has_value() && *tier != ArchDock::RendererTier::Procedural2D;
            break;
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
            available = resolution.rotation.available;
            // An open baked track (an arc) cannot turn under its artwork; any
            // other track is closed, as LayoutEngine.trackShape() reads it.
            if (available && baked)
            {
                available = bakedTrack().value(QStringLiteral("shape")).toString() != QStringLiteral("arc");
            }
            // Only the static layout angle takes the resolved degree range;
            // the rotation mode, speed and trigger fields share the gate but
            // keep their own schema bounds.
            if (available && key == QStringLiteral("layoutAngle"))
            {
                field.insert(QStringLiteral("minimumValue"),
                             resolution.rotation.minimumDegrees);
                field.insert(QStringLiteral("maximumValue"),
                             resolution.rotation.maximumDegrees);
            }
            break;
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
            break;
        case ArchDock::EditorCapability::ArtworkFit:
            available = resolution.available &&
                !candidate.surface.themeSource.trimmed().isEmpty();
            break;
        case ArchDock::EditorCapability::TileShape:
            // The shape of the tile drawn behind each icon, by default or by
            // an icon's own choice: a custom tile, or the plain tile of an
            // icon without styled layers of its own.
            available = controlAvailable(QStringLiteral("icon-state-styling")) &&
                (candidate.iconStyle.tileMode == QStringLiteral("custom") || !iconStyleDrawsLayers());
            break;
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
            // A curved free panel can open its folders along its own curve.
            if (freeHost && curvedLayout(candidate.layout.pathType))
            {
                choices.prepend(QStringLiteral("track"));
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
            // its own name (ADREP-TASK-001 truth matrix).
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
            for (const QString &choice : choices)
            {
                if (!layoutAvailable(choice))
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
            const bool alongDock = key == QStringLiteral("folderLayout") &&
                choice == QStringLiteral("track");
            options.append(QVariantMap{
                {QStringLiteral("label"), alongDock ? tr("Along the dock") : choice},
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
    if (outcome)
    {
        *outcome = {};
        outcome->panelId = panelId;
        outcome->expectedRevision = expectedRevision;
    }

    QString snapshotError;
    const std::optional<ArchDock::PanelDefinition> currentPanel =
        m_panelRegistry.panelDefinition(panelId, &snapshotError);
    if (!currentPanel.has_value())
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = QStringLiteral("panel-not-found");
            outcome->errorMessage = snapshotError;
        }
        return std::nullopt;
    }
    if (outcome)
    {
        outcome->previousRevision = currentPanel->settingsRevision;
        outcome->revision = currentPanel->settingsRevision;
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
            *currentPanel,
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

    if (draft->candidatePanel.segments != currentPanel->segments)
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
                outcome->revision = currentPanel->settingsRevision;
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
                outcome->revision = currentPanel->settingsRevision;
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
                && std::any_of(currentPanel->segments.cbegin(), currentPanel->segments.cend(),
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
            outcome->revision = currentPanel->settingsRevision;
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
    const QVariantMap currentValues = currentPanel->toLegacyMap();
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
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel->settingsRevision;
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
            outcome->revision = currentPanel->settingsRevision;
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

    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(*definition);
    const QVariantList panelFields = panelSettingsEditorFields(
        *definition, resolution, normalizedConsumer);
    const QVariantList globalFields = panelSettingsEditorFields(
        *definition, resolution, normalizedConsumer, ArchDock::PanelSettingsFieldScope::Global);
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
            *definition, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            *definition, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"),
         QVariant::fromValue<qulonglong>(definition->settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(*definition, panelFields)},
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
        {QStringLiteral("themes"), resolvedThemeDefinitions(panelId)},
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

    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(draft->candidatePanel);
    const QVariantList panelFields = panelSettingsEditorFields(
        draft->candidatePanel, resolution, normalizedConsumer);
    const QVariantList globalFields = panelSettingsEditorFields(
        draft->candidatePanel, resolution, normalizedConsumer,
        ArchDock::PanelSettingsFieldScope::Global);
    QSet<QString> globalKeys;
    for (const QVariant &value : globalFields)
    {
        globalKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    QVariantMap projectedGlobals;
    for (const QString &key : std::as_const(globalKeys))
    {
        projectedGlobals.insert(key, draft->candidateGlobals.value(key));
    }
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            draft->candidatePanel, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            draft->candidatePanel, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("resolved")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"), QVariant::fromValue<qulonglong>(expectedRevision)},
        {QStringLiteral("candidateRevision"),
         QVariant::fromValue<qulonglong>(draft->candidatePanel.settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(draft->candidatePanel, panelFields)},
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
         resolvedThemeDefinitions(panelId, panelValues)},
    };
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

    return commitPanelSettingsDraft(*draft, outcome);
}

QVariantMap PanelWindow::commitPanelSettingsDraft(
    ArchDock::PanelSettingsTransactionDraft draft,
    ArchDock::PanelSettingsTransactionOutcome outcome)
{
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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

    static const QStringList placementKeys{
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
    ArchDock::PanelSettingsHostResult placement;
    placement.component = QStringLiteral("native-placement");
    placement.required = ownedHost && anyChanged(placementKeys);
    if (placement.required)
    {
        QVariantMap placementValues;
        for (const QString &key : placementKeys)
        {
            if (candidate.contains(key))
            {
                placementValues.insert(key, candidate.value(key));
            }
        }
        ArchDock::PlasmaPanelPlacementApplyResult applied =
            applyNativePanelPlacementTransaction(
                panelId,
                draft.candidatePanel.host.nativePanelId,
                draft.candidatePanel.host.nativeOwnershipToken,
                placementValues,
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
            draft.candidateFreeHostGeometry, &freePlacement.errorCode);
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
