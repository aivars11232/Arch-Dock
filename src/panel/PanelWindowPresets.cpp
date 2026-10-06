// PanelWindow: presets and profiles. Preset resources and previews, 3D scene
// edits, preset auditions (a preset tried on the real desktop, then kept or
// cancelled), profile operations, and the record of the Plasma hosts an
// audition touched, so that cancelling it restores them.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../PanelVisibility.h"
#include "../ScreenIdentity.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "../model/PanelSettingsSchema.h"
#include "../presets/PresetApplication.h"
#include "../persistence/ConfigurationBackup.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QEventLoop>
#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUuid>

#include <cmath>
#include <utility>

using PanelWindowHelpers::plasmaScriptStringLiteral;
using PanelWindowHelpers::isNativeDockPanelType;
using PanelWindowHelpers::panelTypeNeedsDockApplet;

QVariantMap PanelWindow::presetResource(const QString &kind, const QString &presetId,
    bool builtInOnly) const
{
    if (!ArchDock::PresetIdentity::isValidId(presetId) ||
        (kind != QStringLiteral("panel") && kind != QStringLiteral("icon"))) return {};
    if (ArchDock::PresetIdentity::isUserId(presetId))
    {
        if (builtInOnly) return {};
        const ArchDock::UserPresetStore store(m_presetLibrary.userRoot());
        if (kind == QStringLiteral("panel"))
        {
            for (const auto &preset : store.panelPresets())
                if (preset.identity.id == presetId) return preset.toVariantMap();
        }
        else
        {
            for (const auto &preset : store.iconPresets())
                if (preset.identity.id == presetId) return preset.toVariantMap();
        }
        return {};
    }
    const auto cards = kind == QStringLiteral("panel")
        ? m_presetLibrary.panelPresets(QStringLiteral("builtin"))
        : m_presetLibrary.iconPresets(QStringLiteral("builtin"));
    for (const auto &entry : cards)
    {
        if (entry.toMap().value(QStringLiteral("id")).toString() != presetId) continue;
        QVector<ArchDock::PresetValidationDiagnostic> diagnostics;
        return ArchDock::readPresetDefinitionFile(QDir(m_presetLibrary.builtInRoot()).filePath(
            (kind == QStringLiteral("panel") ? QStringLiteral("panels/") : QStringLiteral("icons/")) +
                presetId + QStringLiteral(".json")), &diagnostics).value_or(QVariantMap{});
    }
    return {};
}

std::optional<ArchDock::PresetPreviewSession::Prepared> PanelWindow::preparePresetPreview(
    const QVariantMap &request, QString *errorCode) const
{
    using namespace ArchDock;
    const auto fail = [errorCode](const QString &code) -> std::optional<PresetPreviewSession::Prepared>
    {
        if (errorCode) *errorCode = code;
        return std::nullopt;
    };
    if (request.value(QStringLiteral("kind")).toString() == QStringLiteral("scene3d"))
        return prepareSceneEditPreview(request, errorCode);
    const QSet<QString> allowed{QStringLiteral("kind"), QStringLiteral("presetId"),
        QStringLiteral("panelId"), QStringLiteral("newPanel"), QStringLiteral("useRecommendedIcons")};
    for (auto it = request.cbegin(); it != request.cend(); ++it)
        if (!allowed.contains(it.key())) return fail(QStringLiteral("invalid-preview-request"));
    const QString kind = request.value(QStringLiteral("kind")).toString();
    const QString presetId = request.value(QStringLiteral("presetId")).toString();
    auto snapshot = m_panelRegistry.panelDefinition(request.value(QStringLiteral("panelId")).toString());
    PresetPreviewSession::Prepared prepared;
    prepared.record.kind = kind;
    const auto object = presetResource(kind, presetId);
    if (object.isEmpty()) return fail(QStringLiteral("preset-not-found"));
    const QString scope = PresetIdentity::isUserId(presetId) ? QStringLiteral("user") : QStringLiteral("builtin");
    const auto cards = kind == QStringLiteral("panel") ? m_presetLibrary.panelPresets(scope)
                                                        : m_presetLibrary.iconPresets(scope);
    for (const auto &card : cards)
        if (card.toMap().value(QStringLiteral("id")).toString() == presetId)
            prepared.fallback = card.toMap().value(QStringLiteral("compatibility")).toMap();
    if (!prepared.fallback.value(QStringLiteral("available")).toBool())
        return fail(QStringLiteral("preset-unavailable"));
    if (kind == QStringLiteral("panel"))
    {
        prepared.panelPreset = PanelPresetDefinition::fromVariantMap(object);
        if (!prepared.panelPreset) return fail(QStringLiteral("invalid-panel-preset"));
        auto &preset = *prepared.panelPreset;
        const bool temporary = request.value(QStringLiteral("newPanel")).toBool() || !snapshot ||
            !preset.compatibility.hostKinds.contains(PanelDefinition::hostKindName(snapshot->host.kind)) ||
            (snapshot->host.kind == PanelHostKind::NativeEdge &&
                panelTypeNeedsDockApplet(snapshot->content.type) != panelTypeNeedsDockApplet(preset.panel.configuration.content.type));
        prepared.record.temporary = temporary;
        if (temporary)
        {
            const bool free = preset.panel.configuration.host.kind == PanelHostKind::FreeDesktop;
            const QString id = (free ? QStringLiteral("free-x") : QStringLiteral("panel-x")) +
                QUuid::createUuid().toString(QUuid::Id128).left(8);
            snapshot = PanelDefinition::defaults(id, preset.identity.name,
                free ? QStringLiteral("free") : preset.panel.configuration.placement.edge, false).normalized();
            snapshot->visibility.visible = true;
            snapshot->settingsRevision = 0;
            snapshot->host.screenIndex = qBound(0, m_settings.monitorIndex(), QGuiApplication::screens().size() - 1);
            if (QGuiApplication::screens().isEmpty()) return fail(QStringLiteral("screen-unavailable"));
            snapshot->host.screenId = persistentScreenId(QGuiApplication::screens().at(snapshot->host.screenIndex));
            prepared.record.previewToken = QStringLiteral("archdock-preview-") +
                QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        // A compatible free host keeps its host kind even when a reusable
        // preset's canonical example is an edge panel.
        if (snapshot->host.kind == PanelHostKind::FreeDesktop)
        {
            preset.panel.configuration.host.kind = PanelHostKind::FreeDesktop;
            preset.panel.configuration.placement.edge = QStringLiteral("free");
        }
        if (prepared.fallback.value(QStringLiteral("fallbackApplied")).toBool())
        {
            preset.panel.configuration.surface.completeThemeId =
                prepared.fallback.value(QStringLiteral("effectiveThemeId")).toString();
            preset.panel.configuration.surface.panelThemeId = preset.panel.configuration.surface.completeThemeId;
            preset.panel.configuration.surface.rendererTier =
                prepared.fallback.value(QStringLiteral("effectiveRendererTier")).toString();
        }
        const bool recommended = request.value(QStringLiteral("useRecommendedIcons"), temporary ||
            !snapshot->presetOrigin || snapshot->presetOrigin->iconPresetId.isEmpty()).toBool();
        if (recommended && !preset.panel.recommendedIconPresetId.isEmpty())
        {
            prepared.recommendedIcons = IconPresetDefinition::fromVariantMap(
                presetResource(QStringLiteral("icon"), preset.panel.recommendedIconPresetId));
            if (!prepared.recommendedIcons) return fail(QStringLiteral("recommended-icons-unavailable"));
        }
        auto draft = PresetApplication::preparePanel(*snapshot, m_settings.transactionSnapshot(),
            preset, {}, prepared.recommendedIcons, errorCode);
        if (!draft) return std::nullopt;
        prepared.draft = *draft;
    }
    else if (kind == QStringLiteral("icon"))
    {
        if (!snapshot || request.value(QStringLiteral("newPanel")).toBool())
            return fail(QStringLiteral("icon-preview-needs-panel"));
        prepared.iconPreset = IconPresetDefinition::fromVariantMap(object);
        if (!prepared.iconPreset) return fail(QStringLiteral("invalid-icon-preset"));
        if (prepared.fallback.value(QStringLiteral("fallbackApplied")).toBool())
        {
            auto &icon = prepared.iconPreset->icon;
            icon.iconStyleId = prepared.fallback.value(QStringLiteral("effectiveIconStyleId")).toString();
            icon.motion.profileId = prepared.fallback.value(QStringLiteral("effectiveMotionProfileId")).toString();
            icon.visualOverrides.clear(); icon.stateOverrides.clear();
        }
        auto draft = PresetApplication::prepareIcon(*snapshot, m_settings.transactionSnapshot(),
            *prepared.iconPreset, {}, errorCode);
        if (!draft) return std::nullopt;
        prepared.draft = *draft;
    }
    else return fail(QStringLiteral("invalid-preset-kind"));
    prepared.record.panelId = snapshot->identity.id;
    prepared.record.hostKind = PanelDefinition::hostKindName(snapshot->host.kind);
    prepared.record.snapshot = *snapshot;
    if (!prepared.record.temporary)
    {
        const bool free = snapshot->host.kind == PanelHostKind::FreeDesktop;
        prepared.record.previewToken = free ? snapshot->host.freeOwnershipToken : snapshot->host.nativeOwnershipToken;
        prepared.record.containmentId = free ? snapshot->host.freeDesktopContainmentId : snapshot->host.nativePanelId;
        prepared.record.appletId = free ? snapshot->host.freeDockAppletId : snapshot->host.nativeDockAppletId;
    }
    return prepared;
}

// A desktop 3D edit auditions the panel itself: a free panel drawn in 3D, its
// own settings as the starting draft, and the 3D page's settings as the only
// ones that may change.
std::optional<ArchDock::PresetPreviewSession::Prepared> PanelWindow::prepareSceneEditPreview(
    const QVariantMap &request, QString *errorCode) const
{
    using namespace ArchDock;
    const auto fail = [errorCode](const QString &code) -> std::optional<PresetPreviewSession::Prepared>
    {
        if (errorCode) *errorCode = code;
        return std::nullopt;
    };
    for (auto it = request.cbegin(); it != request.cend(); ++it)
        if (it.key() != QStringLiteral("kind") && it.key() != QStringLiteral("panelId"))
            return fail(QStringLiteral("invalid-preview-request"));
    const auto snapshot = m_panelRegistry.panelDefinition(request.value(QStringLiteral("panelId")).toString());
    if (!snapshot) return fail(QStringLiteral("panel-not-found"));
    if (snapshot->host.kind != PanelHostKind::FreeDesktop)
        return fail(QStringLiteral("scene-edit-needs-free-panel"));
    if (m_panelRegistry.resolvePanelCapabilities(*snapshot).renderer.effectiveTier != RendererTier::True3D)
        return fail(QStringLiteral("scene-edit-needs-3d"));
    PresetPreviewSession::Prepared prepared;
    prepared.record.kind = QStringLiteral("scene3d");
    prepared.record.temporary = false;
    prepared.fallback = {{QStringLiteral("available"), true}};
    auto draft = PresetApplication::prepareSceneEdit(*snapshot, m_settings.transactionSnapshot(), {}, errorCode);
    if (!draft) return std::nullopt;
    prepared.draft = *draft;
    prepared.record.panelId = snapshot->identity.id;
    prepared.record.hostKind = PanelDefinition::hostKindName(snapshot->host.kind);
    prepared.record.snapshot = *snapshot;
    prepared.record.previewToken = snapshot->host.freeOwnershipToken;
    prepared.record.containmentId = snapshot->host.freeDesktopContainmentId;
    prepared.record.appletId = snapshot->host.freeDockAppletId;
    return prepared;
}

bool PanelWindow::sceneEditActive(const QString &panelId) const
{
    if (!m_presetAudition || !m_presetAudition->active()) return false;
    const QVariantMap status = m_presetAudition->status();
    return status.value(QStringLiteral("kind")).toString() == QStringLiteral("scene3d")
        && status.value(QStringLiteral("panelId")).toString() == panelId;
}

QVariantMap PanelWindow::updateSceneEditDraft(const QString &panelId, const QVariantMap &values)
{
    if (!sceneEditActive(panelId))
        return {{QStringLiteral("success"), false},
                {QStringLiteral("errorCode"), QStringLiteral("scene-edit-not-active")}};
    QVariantMap merged = m_presetAudition->status().value(QStringLiteral("customizations")).toMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        merged.insert(it.key(), it.value());
    return m_presetAudition->updateDraft(merged);
}

QVariantMap PanelWindow::presetEditorProjection(const ArchDock::PanelDefinition &candidate) const
{
    const auto resolution = m_panelRegistry.resolvePanelCapabilities(candidate);
    const auto fields = panelSettingsEditorFields(candidate, resolution, QStringLiteral("studio"));
    QString themeError, iconError;
    const auto theme = m_panelRegistry.themeRuntimeProjection(candidate, &themeError);
    const auto icons = m_panelRegistry.iconStyleRuntimeProjection(candidate, &iconError);
    QVariantMap themeCandidates;
    for (const auto &entry : m_panelRegistry.themeDefinitions())
    {
        const QString id = entry.toMap().value(QStringLiteral("id")).toString();
        const auto prepared = m_panelRegistry.themeCandidateForDefinition(candidate, id, QStringLiteral("complete"));
        auto values = prepared.value(QStringLiteral("values")).toMap();
        if (prepared.value(QStringLiteral("success")).toBool())
        {
            auto record = candidate.toLegacyMap();
            record.insert(values);
            const auto themed = ArchDock::PanelDefinition::fromLegacyMap(record);
            QSet<QString> available;
            if (themed)
                for (const auto &field : panelSettingsEditorFields(*themed,
                         m_panelRegistry.resolvePanelCapabilities(*themed), QStringLiteral("studio")))
                    available.insert(field.toMap().value(QStringLiteral("key")).toString());
            for (auto it = values.begin(); it != values.end();)
            {
                const auto *field = ArchDock::PanelSettingsSchema::panelDescriptor(it.key());
                if (field && field->editor.isPresented() && !available.contains(it.key()))
                    it = values.erase(it);
                else ++it;
            }
        }
        themeCandidates.insert(id, QVariantMap{{QStringLiteral("success"), prepared.value(QStringLiteral("success")).toBool()},
            {QStringLiteral("errorCode"), prepared.value(QStringLiteral("errorCode")).toString()},
            {QStringLiteral("errorMessage"), prepared.value(QStringLiteral("errorMessage")).toString()},
            {QStringLiteral("values"), values}});
    }
    return {{QStringLiteral("success"), true}, {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("panelId"), candidate.identity.id}, {QStringLiteral("revision"), candidate.settingsRevision},
        {QStringLiteral("consumer"), QStringLiteral("studio")},
        {QStringLiteral("panelValues"), panelSettingsEditorValues(candidate, fields)},
        {QStringLiteral("panelFields"), fields}, {QStringLiteral("globalFields"), QVariantList{}},
        {QStringLiteral("globalValues"), m_settings.transactionSnapshot()},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themes"), m_panelRegistry.themeDefinitions()},
        {QStringLiteral("themeCandidates"), themeCandidates},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"), m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themeDefinition"), theme.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"), theme ? QStringLiteral("ready") : QStringLiteral("unavailable")},
        {QStringLiteral("themeProjectionError"), themeError},
        {QStringLiteral("iconStyleDefinition"), icons.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"), icons ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconError}};
}

bool PanelWindow::profileBusy() const
{
    return m_profileManager && m_profileManager->active();
}

ArchDock::PresetPreviewRecord PanelWindow::profileHostRecord(
    const ArchDock::PanelDefinition &definition) const
{
    ArchDock::PresetPreviewRecord record;
    record.panelId = definition.identity.id;
    record.snapshot = definition;
    record.hostKind = ArchDock::PanelDefinition::hostKindName(definition.host.kind);
    record.previewToken = ArchDock::ProfileApplyTransaction::hostToken(definition);
    const bool free = definition.host.kind == ArchDock::PanelHostKind::FreeDesktop;
    record.containmentId = free ? definition.host.freeDesktopContainmentId : definition.host.nativePanelId;
    record.appletId = free ? definition.host.freeDockAppletId : definition.host.nativeDockAppletId;
    return record;
}

ArchDock::ProfileApplyTransaction::Operations PanelWindow::profileOperations()
{
    using namespace ArchDock;
    ProfileApplyTransaction::Operations operations;
    operations.guard = [this] {
        if (m_nativePanelRecoveryActive || m_settingsTransactionAdoptionActive)
            return QStringLiteral("panel-lifecycle-active");
        if (m_presetAudition && m_presetAudition->active()) return QStringLiteral("preset-audition-active");
        for (const auto &locks : std::as_const(m_panelInteractionGuards))
        {
            if (locks.editMode) return QStringLiteral("edit-mode-active");
            if (locks.popupOpen) return QStringLiteral("popup-open");
            if (locks.dragActive) return QStringLiteral("drag-active");
        }
        return QString{};
    };
    operations.snapshot = [this](QString *error) { return m_panelRegistry.panelDefinitions(error); };
    operations.backupConfiguration = [](QString *error) {
        QSettings settings;
        settings.sync();
        if (settings.status() != QSettings::NoError)
        { if (error) *error = QStringLiteral("configuration-unreadable"); return false; }
        ConfigurationBackup backup;
        return !backup.capture(QStringLiteral("profile-apply"), error).isEmpty() &&
            backup.prune(settings.value(QStringLiteral("backup/retentionCount"), 5).toInt(), error);
    };
    operations.matches = [this](const QList<PanelDefinition> &before, QString *error) {
        return m_panelRegistry.panelSetMatches(before, error);
    };
    operations.prepare = [this](const QList<PanelDefinition> &before, QList<PanelDefinition> &panels,
        QStringList *diagnostics, QString *error) {
        const auto fail = [error](const QString &code) { if (error) *error = code; return false; };
        QDBusInterface shell(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
            QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
        if (!shell.isValid()) return fail(QStringLiteral("plasmashell-unavailable"));
        QStringList screenIds;
        for (const auto *screen : QGuiApplication::screens()) screenIds.append(persistentScreenId(screen));
        if (screenIds.isEmpty()) return fail(QStringLiteral("profile-no-screens"));
        ProfileReferences references;
        for (const auto &theme : m_panelRegistry.themeDefinitions())
            references.themes.insert(theme.toMap().value(QStringLiteral("id")).toString());
        for (const auto &icons : m_panelRegistry.iconStyleDefinitions())
            references.iconStyles.insert(icons.toMap().value(QStringLiteral("id")).toString());
        if (const auto *catalog = m_panelRegistry.animationProfileCatalog())
        {
            for (const auto &id : catalog->profileIds()) references.motions.insert(id);
            for (const auto &id : catalog->legacyNameMap().keys()) references.motions.insert(id);
        }
        auto effective = ProfileDefinition::capture(QStringLiteral("Effective profile"), panels);
        panels = effective.resolved(references, diagnostics);
        for (auto &panel : panels)
        {
            const auto screen = resolveScreen(screenIds, panel.host.screenId, panel.host.screenIndex);
            if (screen.usedFallback && diagnostics) diagnostics->append(panel.identity.id + QStringLiteral(":screen-fallback"));
            panel.host.screenIndex = screen.index;
            if (panel.host.screenId.isEmpty()) panel.host.screenId = screenIds.at(screen.index);
            if (panel.host.kind == PanelHostKind::FreeDesktop)
            {
                if (!panel.identity.id.startsWith(QStringLiteral("free-"))) return fail(QStringLiteral("profile-free-id-required"));
            }
            else
            {
                const auto placement = normalizedNativePanelPlacement(panel);
                if (!placement.isValid() || !placement.isSupported() || !placement.placement)
                    return fail(QStringLiteral("profile-native-placement-unsupported"));
                if (!supportedNativeVisibilityModes({true, true, m_windowWatcher.available()}).contains(panel.visibility.hostMode))
                    return fail(QStringLiteral("profile-visibility-unsupported"));
            }
            // Refuse a foreign physical host using this logical ID. Only the
            // exact previous ownership token may be present before mutation.
            QString previousToken;
            for (const auto &old : before)
                if (old.identity.id == panel.identity.id) previousToken = ProfileApplyTransaction::hostToken(old);
            const auto foreign = evaluatePlasmaScriptResultOptional(QStringLiteral(R"JS(
var count = 0;
var all = panels();
for (var i = 0; i < all.length; ++i) {
    var host = panelById(Number(all[i].id)); if (!host) continue;
    host.currentConfigGroup = ['ArchDock'];
    if (String(host.readConfig('panelId', '')) === %1 &&
        (%2 === '' || String(host.readConfig('ownerToken', '')) !== %2)) ++count;
}
var spaces = desktops();
for (var i = 0; i < spaces.length; ++i) {
    var desktop = desktopById(Number(spaces[i].id)); if (!desktop) continue;
    var docks = desktop.widgets('org.archdock.dock');
    for (var j = 0; j < docks.length; ++j) {
        var dock = desktop.widgetById(Number(docks[j].id)); if (!dock) continue;
        dock.currentConfigGroup = ['General'];
        if (String(dock.readConfig('panelId', '')) === %1 &&
            (%2 === '' || String(dock.readConfig('ownerToken', '')) !== %2)) ++count;
    }
}
print('ARCHDOCK_RESULT:' + String(count));
)JS").arg(plasmaScriptStringLiteral(panel.identity.id)).arg(plasmaScriptStringLiteral(previousToken)));
            if (!foreign || *foreign != 0) return fail(QStringLiteral("profile-foreign-host-conflict"));
        }
        return true;
    };
    operations.capture = [this](ProfileHostSnapshot &snapshot, QString *error) {
        auto record = profileHostRecord(snapshot.definition);
        if (!presetAuditionOperations().captureHost(record, error)) return false;
        snapshot.hostState = record.hostState;
        if (snapshot.definition.host.kind == PanelHostKind::NativeEdge)
        {
            const auto visibility = evaluatePlasmaScriptResultOptional(QStringLiteral(R"JS(
var result = (function() {
    var p = panelById(%1); if (!p) return -1;
    p.currentConfigGroup = ['ArchDock'];
    if (String(p.readConfig('panelId', '')) !== %2 || String(p.readConfig('ownerToken', '')) !== %3) return -1;
    var mode = ['none', 'autohide', 'dodgewindows'].indexOf(String(p.hiding));
    if (mode < 0) return -1;
    return mode * 2 + (String(p.readConfig('temporaryHidden', '0')) === '1' ? 1 : 0);
})(); print('ARCHDOCK_RESULT:' + String(result));
)JS").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.panelId)).arg(plasmaScriptStringLiteral(record.previewToken)));
            if (!visibility || *visibility < 0 || *visibility > 5)
            { if (error) *error = QStringLiteral("profile-visibility-snapshot-unavailable"); return false; }
            snapshot.hostState.insert(QStringLiteral("nativeVisibility"), *visibility);
        }
        return true;
    };
    operations.create = [this](PanelDefinition &panel, const QString &token, QString *error) {
        auto record = profileHostRecord(panel);
        record.previewToken = token;
        return presetAuditionOperations().createHost(record, panel, error);
    };
    operations.apply = [this](const PanelDefinition &, const PanelDefinition &panel, QString *error) {
        if (panel.host.kind == PanelHostKind::FreeDesktop)
        {
            auto geometry = presetFreeHostGeometry(panel, error);
            if (!geometry) return false;
            geometry->insert(QStringLiteral("x"), panel.placement.x);
            geometry->insert(QStringLiteral("y"), panel.placement.y);
            return setPresetFreeHostGeometry(panel, *geometry, error);
        }
        const auto placement = normalizedNativePanelPlacement(panel);
        const PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
        if (!placement.placement || !adapter.applyPlacement(panel.host.nativePanelId, panel.identity.id,
            panel.host.nativeOwnershipToken, *placement.placement).success())
        { if (error) *error = QStringLiteral("profile-placement-readback-failed"); return false; }
        const auto mode = panelVisibilityModeFromString(panel.visibility.hostMode);
        const auto visibility = resolveNativeVisibility(mode, nativePanelVisibilityDecision(panel, mode, panel.visibility.visible),
            !panel.visibility.visible, {true, true, m_windowWatcher.available()});
        m_nativePanelVisibilityStateCache.remove(panel.identity.id);
        if (visibility.fallbackApplied || !adapter.applyVisibility(panel.host.nativePanelId, panel.identity.id,
            panel.host.nativeOwnershipToken, visibility.hostMode, !panel.visibility.visible).success())
        { if (error) *error = QStringLiteral("profile-visibility-readback-failed"); return false; }
        return true;
    };
    operations.verify = [this](const PanelDefinition &panel, QString *error) {
        auto record = profileHostRecord(panel);
        return presetAuditionOperations().captureHost(record, error);
    };
    operations.restore = [this](ProfileHostSnapshot &snapshot, QString *error) {
        auto &panel = snapshot.definition;
        const auto visibility = snapshot.hostState.value(QStringLiteral("nativeVisibility"));
        if (panel.host.kind == PanelHostKind::NativeEdge && (!PresetParsing::isInteger(visibility) ||
            visibility.toInt() < 0 || visibility.toInt() > 5))
        { if (error) *error = QStringLiteral("invalid-profile-visibility-snapshot"); return false; }
        const QString token = ProfileApplyTransaction::hostToken(panel);
        bool missing = false;
        if (panel.host.kind == PanelHostKind::FreeDesktop)
        {
            const auto found = discoverOwnedFreePanelHost(panel.identity.id, token);
            missing = found.outcome == FreePanelHostDiscoveryOutcome::Missing;
            if (!missing && found.outcome != FreePanelHostDiscoveryOutcome::Unique)
            { if (error) *error = QStringLiteral("profile-restore-host-conflict"); return false; }
            if (!missing) { panel.host.freeDesktopContainmentId = found.host.desktopContainmentId; panel.host.freeDockAppletId = found.host.dockAppletId; }
        }
        else
        {
            const auto found = discoverNativePanel(panel.identity.id, token, panel.content.type);
            missing = found.status == NativePanelDiscoveryStatus::Missing;
            if (!missing && found.status != NativePanelDiscoveryStatus::Unique)
            { if (error) *error = QStringLiteral("profile-restore-host-conflict"); return false; }
            if (!missing) { panel.host.nativePanelId = found.containmentId; panel.host.nativeDockAppletId = found.dockAppletId; }
        }
        if (missing)
        {
            auto record = profileHostRecord(panel);
            if (!presetAuditionOperations().createHost(record, panel, error)) return false;
        }
        auto record = profileHostRecord(panel);
        record.hostState = snapshot.hostState;
        record.hostState.remove(QStringLiteral("nativeVisibility"));
        if (!restorePresetPreviewHost(record, error, true)) return false;
        if (panel.host.kind == PanelHostKind::NativeEdge)
        {
            constexpr PlasmaPanelHidingMode modes[]{PlasmaPanelHidingMode::None, PlasmaPanelHidingMode::AutoHide,
                PlasmaPanelHidingMode::DodgeWindows};
            const PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
            m_nativePanelVisibilityStateCache.remove(panel.identity.id);
            if (!adapter.applyVisibility(panel.host.nativePanelId, panel.identity.id, token,
                modes[visibility.toInt() / 2], visibility.toInt() % 2 != 0).success())
            { if (error) *error = QStringLiteral("profile-restore-visibility-failed"); return false; }
        }
        return true;
    };
    operations.remove = [this](const PanelDefinition &panel, QString *error) {
        auto record = profileHostRecord(panel); record.temporary = true;
        return removePresetPreviewHost(record, error, true);
    };
    operations.commit = [this](const QList<PanelDefinition> &before, const QList<PanelDefinition> &panels, QString *error) {
        return m_panelRegistry.persistPanelSetTransaction(before, panels, error);
    };
    operations.publish = [this] { m_panelRegistry.notifyPanelSettingsTransactionAdopted(true); };
    return operations;
}

ArchDock::PresetPreviewSession::Operations PanelWindow::presetAuditionOperations()
{
    using namespace ArchDock;
    PresetPreviewSession::Operations operations;
    operations.prepare = [this](const QVariantMap &request, QString *error) { return preparePresetPreview(request, error); };
    operations.guard = [this](const QString &panelId) {
        if (profileBusy()) return QStringLiteral("profile-recovery-or-apply-active");
        QStringList targets{panelId};
        if (m_presetAudition && m_presetAudition->active())
            targets.append(m_presetAudition->status().value(QStringLiteral("panelId")).toString());
        for (const QString &target : targets)
        {
            const auto locks = m_panelInteractionGuards.value(target);
            if (locks.editMode) return QStringLiteral("edit-mode-active");
            if (locks.popupOpen) return QStringLiteral("popup-open");
            if (locks.dragActive) return QStringLiteral("drag-active");
        }
        return QString{};
    };
    operations.captureHost = [this](PresetPreviewRecord &record, QString *error) {
        if (!capturePresetPreviewHost(record, error)) return false;
        if (record.hostKind == QStringLiteral("native-edge") && record.appletId >= 0 &&
            record.hostState.value(QStringLiteral("panelType")).toString() != record.snapshot.content.type)
        {
            if (error) *error = QStringLiteral("preview-renderer-not-verified");
            return false;
        }
        return true;
    };
    operations.createHost = [this](PresetPreviewRecord &record, PanelDefinition &candidate, QString *error) {
        if (candidate.host.kind == PanelHostKind::FreeDesktop)
        {
            const auto created = createConfiguredFreePanelHost(candidate.host.screenIndex, record.panelId, record.previewToken);
            record.containmentId = created.host.desktopContainmentId; record.appletId = created.host.dockAppletId;
            if (created.outcome != FreePanelHostMutationOutcome::Verified)
            {
                if (error) *error = QStringLiteral("preview-free-host-creation-failed");
                return false;
            }
            candidate.host.freeDesktopContainmentId = record.containmentId;
            candidate.host.freeDockAppletId = record.appletId;
            candidate.host.freeOwnershipToken = record.previewToken;
            candidate.host.freeHostState = QStringLiteral("hosted-owned");
            candidate.host.freeCreationState = QStringLiteral("idle");
        }
        else
        {
            record.containmentId = createNativePanelCandidate(candidate, record.previewToken, error);
            if (record.containmentId < 0) return false;
            const auto dockId = verifiedNativeDockAppletId(record.panelId, record.containmentId, candidate.content.type);
            if (!dockId || !nativePanelIsOwned(record.panelId, record.containmentId, record.previewToken))
            {
                if (error) *error = QStringLiteral("preview-native-host-not-verified");
                return false;
            }
            record.appletId = *dockId;
            candidate.host.nativePanelId = record.containmentId; candidate.host.nativeDockAppletId = *dockId;
            candidate.host.nativeOwnershipToken = record.previewToken;
            candidate.host.nativeRecoveryState = QStringLiteral("ready");
        }
        candidate = candidate.normalized();
        return true;
    };
    operations.applyHost = [this](const PanelDefinition &previous, const PanelDefinition &candidate, QString *error) {
        const bool native = candidate.host.kind == PanelHostKind::NativeEdge;
        if (!(native ? nativePanelIsOwned(candidate.identity.id, candidate.host.nativePanelId, candidate.host.nativeOwnershipToken)
            : freePanelHostVerification(candidate.host.freeDesktopContainmentId, candidate.host.freeDockAppletId,
                candidate.identity.id, candidate.host.freeOwnershipToken) == FreePanelHostVerificationOutcome::Owned))
        {
            if (error) *error = QStringLiteral("preview-host-not-verified");
            return false;
        }
        if (native && previous.host.nativePanelId < 0)
        {
            const auto placement = normalizedNativePanelPlacement(candidate);
            const PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
            if (!placement.isValid() || !placement.isSupported() || !placement.placement ||
                !adapter.applyPlacement(candidate.host.nativePanelId, candidate.identity.id,
                    candidate.host.nativeOwnershipToken, *placement.placement).success())
            {
                if (error) *error = QStringLiteral("preview-native-placement-failed");
                return false;
            }
        }
        const auto results = applyPanelSettingsHosts({previous, candidate, {}, {}}, false);
        if (!PanelSettingsTransaction::requiredHostsSucceeded(results))
        {
            for (const auto &result : results)
                if (result.required && !result.success && error) { *error = result.errorCode; break; }
            return false;
        }
        return true;
    };
    operations.restoreHost = [this](const PresetPreviewRecord &record, const PanelDefinition &, QString *error) {
        return restorePresetPreviewHost(record, error);
    };
    operations.commitExisting = [this](const PanelSettingsTransactionDraft &draft, QString *error) {
        PanelSettingsTransactionOutcome outcome;
        outcome.panelId = draft.previousPanel.identity.id;
        outcome.expectedRevision = outcome.previousRevision = outcome.revision = draft.previousPanel.settingsRevision;
        const auto result = commitPanelSettingsDraft(draft, outcome);
        if (!result.value(QStringLiteral("success")).toBool() && error)
            *error = result.value(QStringLiteral("errorCode")).toString();
        return result.value(QStringLiteral("success")).toBool();
    };
    operations.convertHost = [this](const PresetPreviewRecord &record, PanelDefinition &candidate, QString *error) {
        return convertPresetPreviewHost(record, candidate, error);
    };
    operations.adoptHost = [this](const PanelDefinition &candidate, QString *error) {
        const bool owned = candidate.host.kind == PanelHostKind::FreeDesktop
            ? freePanelHostVerification(candidate.host.freeDesktopContainmentId, candidate.host.freeDockAppletId,
                candidate.identity.id, candidate.host.freeOwnershipToken) == FreePanelHostVerificationOutcome::Owned
            : nativePanelIsOwned(candidate.identity.id, candidate.host.nativePanelId, candidate.host.nativeOwnershipToken);
        if (!owned) { if (error) *error = QStringLiteral("managed-host-not-verified"); return false; }
        return m_panelRegistry.adoptPreviewPanel(candidate, error);
    };
    operations.removeHost = [this](const PresetPreviewRecord &record, QString *error) { return removePresetPreviewHost(record, error); };
    operations.recover = [this](const PresetPreviewRecord &record, QString *error) { return recoverPresetPreviewHost(record, error); };
    operations.saveCustom = [this](const PresetPreviewSession::Prepared &prepared, const QString &name) {
        const UserPresetStore store(m_presetLibrary.userRoot());
        QString error, id;
        if (prepared.panelPreset)
        {
            auto source = *prepared.panelPreset;
            const auto tier = m_panelRegistry.resolvePanelCapabilities(prepared.draft.candidatePanel).renderer.effectiveTier;
            if (prepared.draft.candidatePanel.surface.rendererTier.isEmpty() && tier)
                source.preview.rendererTier = rendererTierName(*tier);
            const auto saved = store.save(PresetApplication::panelSnapshot(source,
                prepared.draft.candidatePanel, name), &error);
            if (saved) id = saved->identity.id;
        }
        else if (prepared.iconPreset)
        {
            const auto saved = store.save(PresetApplication::iconSnapshot(*prepared.iconPreset,
                prepared.draft.candidatePanel, name), &error);
            if (saved) id = saved->identity.id;
        }
        return QVariantMap{{QStringLiteral("success"), !id.isEmpty()}, {QStringLiteral("errorCode"), error},
            {QStringLiteral("presetId"), id}};
    };
    operations.restoreBuiltIn = [this](const PresetPreviewSession::Prepared &current, QString *error)
        -> std::optional<PresetPreviewSession::Prepared> {
        const auto identity = current.panelPreset ? current.panelPreset->identity : current.iconPreset->identity;
        QString id = identity.builtIn ? identity.id : identity.derivedFromPresetId;
        QVariantMap resource;
        for (int depth = 0; depth < 16 && !id.isEmpty(); ++depth)
        {
            resource = presetResource(current.record.kind, id);
            const auto source = resource.value(QStringLiteral("identity")).toMap();
            if (source.value(QStringLiteral("builtIn")).toBool()) break;
            id = source.value(QStringLiteral("derivedFromPresetId")).toString();
            resource.clear();
        }
        if (resource.isEmpty()) { if (error) *error = QStringLiteral("no-built-in-source"); return std::nullopt; }
        const auto resolved = preparePresetPreview({{QStringLiteral("kind"), current.record.kind},
            {QStringLiteral("presetId"), id}, {QStringLiteral("panelId"), current.draft.previousPanel.identity.id},
            {QStringLiteral("useRecommendedIcons"), current.recommendedIcons.has_value()}}, error);
        if (!resolved) return std::nullopt;
        auto restored = current;
        restored.fallback = resolved->fallback;
        std::optional<PanelSettingsTransactionDraft> draft;
        if (current.panelPreset)
        {
            restored.panelPreset = resolved->panelPreset;
            if (!restored.panelPreset || !restored.panelPreset->compatibility.hostKinds.contains(
                PanelDefinition::hostKindName(current.draft.previousPanel.host.kind)))
            { if (error) *error = QStringLiteral("built-in-host-incompatible"); return std::nullopt; }
            if (current.draft.previousPanel.host.kind == PanelHostKind::FreeDesktop)
            {
                restored.panelPreset->panel.configuration.host.kind = PanelHostKind::FreeDesktop;
                restored.panelPreset->panel.configuration.placement.edge = QStringLiteral("free");
            }
            restored.recommendedIcons = resolved->recommendedIcons;
            draft = PresetApplication::preparePanel(current.draft.previousPanel, current.draft.previousGlobals,
                *restored.panelPreset, {}, restored.recommendedIcons, error);
        }
        else
        {
            restored.iconPreset = resolved->iconPreset;
            if (!restored.iconPreset) return std::nullopt;
            draft = PresetApplication::prepareIcon(current.draft.previousPanel, current.draft.previousGlobals,
                *restored.iconPreset, {}, error);
        }
        if (!draft) return std::nullopt;
        restored.draft = *draft;
        return restored;
    };
    operations.validDefault = [this](const QString &kind, const QString &id) {
        if (presetResource(kind, id).isEmpty()) return false;
        const QString scope = PresetIdentity::isUserId(id) ? QStringLiteral("user") : QStringLiteral("builtin");
        const auto cards = kind == QStringLiteral("panel") ? m_presetLibrary.panelPresets(scope) : m_presetLibrary.iconPresets(scope);
        for (const auto &card : cards)
            if (card.toMap().value(QStringLiteral("id")).toString() == id)
                return card.toMap().value(QStringLiteral("compatibility")).toMap().value(QStringLiteral("available")).toBool();
        return false;
    };
    operations.validateDraft = [this](PresetPreviewSession::Prepared &prepared, QString *error) {
        auto &candidate = prepared.draft.candidatePanel;
        const auto fail = [error](const QString &code) { if (error) *error = code; return false; };
        if (candidate.identity != prepared.draft.previousPanel.identity ||
            candidate.host.kind != prepared.draft.previousPanel.host.kind ||
            prepared.draft.candidateGlobals != prepared.draft.previousGlobals)
            return fail(QStringLiteral("preview-protected-state-change"));
        const auto screens = QGuiApplication::screens();
        if (candidate.host.screenIndex < 0 || candidate.host.screenIndex >= screens.size())
            return fail(QStringLiteral("screen-unavailable"));
        if (prepared.record.temporary ||
            (prepared.record.kind == QStringLiteral("panel") && candidate.host.screenId.isEmpty()) ||
            candidate.host.screenIndex != prepared.draft.previousPanel.host.screenIndex)
            candidate.host.screenId = persistentScreenId(screens.at(candidate.host.screenIndex));
        if (!candidate.isValid(error)) return false;
        if (prepared.record.kind == QStringLiteral("icon") && !PresetApplication::iconOnlyChange(prepared.draft.previousPanel, candidate))
            return fail(QStringLiteral("icon-only-violation"));
        if ((prepared.iconPreset && !prepared.iconPreset->icon.perStateAnimationOverrides.isEmpty()) ||
            (prepared.recommendedIcons && !prepared.recommendedIcons->icon.perStateAnimationOverrides.isEmpty()))
            return fail(QStringLiteral("per-state-icon-motion-unavailable"));
        if (!m_panelRegistry.resolvePanelCapabilities(candidate).available)
            return fail(QStringLiteral("preview-capability-unavailable"));
        QString projectionError;
        if (!m_panelRegistry.themeRuntimeProjection(candidate, &projectionError) && !projectionError.isEmpty())
            return fail(projectionError);
        const auto style = m_panelRegistry.iconStyleRuntimeProjection(candidate, &projectionError);
        if (!style || !projectionError.isEmpty() || (candidate.iconStyle.globalDefaults.contains(QStringLiteral("presetOverrides")) &&
            !style->value(QStringLiteral("presetOverridesApplied")).toBool())) return fail(QStringLiteral("preview-icons-unavailable"));
        if (!prepared.record.temporary)
        {
            const auto stored = m_panelRegistry.panelDefinition(prepared.record.panelId);
            if (!stored || *stored != prepared.draft.previousPanel) return fail(QStringLiteral("preview-revision-conflict"));
            QVariantMap changes;
            const auto before = stored->toLegacyMap();
            const auto after = candidate.toLegacyMap();
            const auto frozen = prepared.record.kind == QStringLiteral("scene3d")
                ? PresetApplication::prepareSceneEdit(*stored, prepared.draft.previousGlobals, {}, error)
                : prepared.panelPreset
                ? PresetApplication::preparePanel(*stored, prepared.draft.previousGlobals,
                    *prepared.panelPreset, {}, prepared.recommendedIcons, error)
                : PresetApplication::prepareIcon(*stored, prepared.draft.previousGlobals,
                    *prepared.iconPreset, {}, error);
            if (!frozen) return false;
            const auto frozenValues = frozen->candidatePanel.toLegacyMap();
            const auto effectiveTier = m_panelRegistry.resolvePanelCapabilities(candidate).renderer.effectiveTier;
            for (auto it = after.cbegin(); it != after.cend(); ++it)
            {
                const auto *field = PanelSettingsSchema::panelDescriptor(it.key());
                // A full preset carries normalized values for inactive layouts.
                // Only the frozen source may supply those dormant values;
                // unavailable custom edits still pass through the editor gate.
                const bool inactiveLayout = field && !field->editor.layouts.isEmpty() &&
                    !field->editor.layouts.contains(candidate.layout.pathType);
                const bool inactiveRenderer = field &&
                    ((field->editor.capability == QStringLiteral("procedural-surface") &&
                      effectiveTier != RendererTier::Procedural2D) ||
                     ((field->editor.capability == QStringLiteral("scene3d-quality") ||
                       field->editor.capability == QStringLiteral("scene3d-shape")) &&
                      effectiveTier != RendererTier::True3D));
                const bool dormantPresetValue = (inactiveLayout || inactiveRenderer) &&
                    it.value() == frozenValues.value(it.key());
                if (PanelSettingsSchema::isTransactionPanelField(it.key()) && before.value(it.key()) != it.value() &&
                    !dormantPresetValue)
                    changes.insert(it.key(), it.value());
            }
            PanelSettingsTransactionOutcome outcome;
            const auto validated = preparePanelSettingsDraft(prepared.record.panelId, stored->settingsRevision,
                changes, {}, &outcome);
            if (!validated)
            {
                qWarning() << "Preset draft field validation failed:" << outcome.errorCode << outcome.errorMessage;
                return fail(outcome.errorCode);
            }
            // The full candidate already passed the shared pure transaction,
            // model, renderer and icon validation above. Retain its normalized
            // dormant values and preset metadata after checking active edits.
        }
        else if (candidate.segments != QList<PanelSegmentDefinition>{PanelSegmentDefinition{}})
        {
            const auto capabilities = PanelCapabilityResolver::segmentCapabilities(candidate,
                m_panelRegistry.resolvePanelCapabilities(candidate), !m_systemStatus.availableSources().isEmpty());
            if (!capabilities.value(QStringLiteral("available")).toBool()) return fail(QStringLiteral("unavailable-segment-feature"));
            for (const auto &segment : candidate.segments)
                if (!capabilities.value(QStringLiteral("sources")).toStringList().contains(segment.source) ||
                    !capabilities.value(QStringLiteral("motionProfiles")).toStringList().contains(segment.motionProfile) ||
                    (segment.background != QStringLiteral("solid") && segment.corners != QStringLiteral("inherited")))
                    return fail(QStringLiteral("unavailable-segment-feature"));
            auto entries = candidate.host.kind == PanelHostKind::FreeDesktop ? freePanelEntries(candidate)
                : m_dockModel.panelEntries(candidate.content.type);
            entries.append(statusEntriesFor(candidate, true));
            QString segmentError;
            const auto projected = PanelContentTransaction::segmentEntries(candidate, entries, &segmentError, true);
            Q_UNUSED(projected);
            if (!segmentError.isEmpty()) return fail(QStringLiteral("invalid-segment-ownership"));
        }
        return true;
    };
    operations.editorProjection = [this](const PanelDefinition &candidate) { return presetEditorProjection(candidate); };
    return operations;
}

std::optional<QVariantMap> PanelWindow::presetFreeHostGeometry(
    const ArchDock::PanelDefinition &definition, QString *errorCode) const
{
    const auto fail = [errorCode]() -> std::optional<QVariantMap> {
        if (errorCode) *errorCode = QStringLiteral("preview-free-geometry-unverified");
        return std::nullopt;
    };
    const auto &host = definition.host;
    if (host.kind != ArchDock::PanelHostKind::FreeDesktop || host.freeDesktopContainmentId < 0 ||
        host.freeDockAppletId < 0 || host.freeOwnershipToken.isEmpty()) return fail();
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), QStringLiteral(R"JS(
var value = (function() {
    var desktop = desktopById(%1);
    var widget = desktop ? desktop.widgetById(%2) : null;
    if (!widget || widget.type !== 'org.archdock.dock') return {};
    widget.currentConfigGroup = ['General'];
    if (String(widget.readConfig('panelId', '')) !== %3 ||
        String(widget.readConfig('ownerToken', '')) !== %4 ||
        String(widget.readConfig('panelType', '')) !== 'empty' ||
        !['false', '0'].includes(String(widget.readConfig('bootstrapFreeDock', true)).toLowerCase())) return {};
    var g = widget.geometry;
    return {x: Number(g.x), y: Number(g.y), width: Number(g.width), height: Number(g.height)};
})(); print('ARCHDOCK_PREVIEW_GEOMETRY:' + JSON.stringify(value));
)JS").arg(host.freeDesktopContainmentId).arg(host.freeDockAppletId)
        .arg(plasmaScriptStringLiteral(definition.identity.id)).arg(plasmaScriptStringLiteral(host.freeOwnershipToken)));
    const QString prefix = QStringLiteral("ARCHDOCK_PREVIEW_GEOMETRY:");
    if (!reply.isValid() || reply.value().size() > 4096 || !reply.value().trimmed().startsWith(prefix)) return fail();
    const auto document = QJsonDocument::fromJson(reply.value().trimmed().mid(prefix.size()).toUtf8());
    if (!document.isObject()) return fail();
    const auto geometry = document.object().toVariantMap();
    if (geometry.keys() != QStringList{QStringLiteral("height"), QStringLiteral("width"), QStringLiteral("x"), QStringLiteral("y")}) return fail();
    for (auto it = geometry.cbegin(); it != geometry.cend(); ++it)
    {
        bool ok = false;
        const double value = it.value().toDouble(&ok);
        if (!ok || !std::isfinite(value) || std::abs(value) > 1000000 ||
            ((it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")) && value <= 0)) return fail();
    }
    return geometry;
}

bool PanelWindow::setPresetFreeHostGeometry(const ArchDock::PanelDefinition &definition,
    const QVariantMap &geometry, QString *errorCode) const
{
    const auto current = presetFreeHostGeometry(definition, errorCode);
    if (!current || geometry.keys() != current->keys()) return false;
    for (auto it = geometry.cbegin(); it != geometry.cend(); ++it)
    {
        bool ok = false;
        const double value = it.value().toDouble(&ok);
        if (!ok || !std::isfinite(value) || std::abs(value) > 1000000 ||
            ((it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")) && value <= 0))
        { if (errorCode) *errorCode = QStringLiteral("invalid-preview-free-geometry"); return false; }
    }
    const auto &host = definition.host;
    auto command = geometry;
    command.insert(QStringLiteral("panelId"), definition.identity.id);
    command.insert(QStringLiteral("ownerToken"), host.freeOwnershipToken);
    const QString payload = QString::fromUtf8(QJsonDocument::fromVariant(command).toJson(QJsonDocument::Compact));
    const int applied = evaluatePlasmaScriptResult(QStringLiteral(R"JS(
var result = (function() {
    var desktop = desktopById(%1);
    var widget = desktop ? desktop.widgetById(%2) : null;
    if (!widget || widget.type !== 'org.archdock.dock') return 0;
    widget.currentConfigGroup = ['General'];
    if (String(widget.readConfig('panelId', '')) !== %3 ||
        String(widget.readConfig('ownerToken', '')) !== %4 ||
        String(widget.readConfig('panelType', '')) !== 'empty' ||
        !['false', '0'].includes(String(widget.readConfig('bootstrapFreeDock', true)).toLowerCase())) return 0;
    // Plasma 6's Widget.setGeometry is a no-op. The owned applet handles
    // this bounded command through its own desktop layout container.
    widget.writeConfig('auditionRestoreGeometry', %5);
    return 1;
})(); print('ARCHDOCK_RESULT:' + String(result));
)JS").arg(host.freeDesktopContainmentId).arg(host.freeDockAppletId)
        .arg(plasmaScriptStringLiteral(definition.identity.id)).arg(plasmaScriptStringLiteral(host.freeOwnershipToken))
        .arg(plasmaScriptStringLiteral(payload)));
    std::optional<QVariantMap> observed;
    for (int attempt = 0; applied == 1 && attempt < 80; ++attempt)
    {
        QEventLoop wait;
        QTimer::singleShot(25, &wait, &QEventLoop::quit);
        wait.exec(QEventLoop::ExcludeUserInputEvents);
        observed = presetFreeHostGeometry(definition, errorCode);
        if (observed && *observed == geometry) break;
    }
    // A command is not retained as active configuration. The recovery
    // journal remains authoritative if restoration cannot be verified.
    const int cleared = evaluatePlasmaScriptResult(QStringLiteral(R"JS(
var result = (function() {
    var desktop = desktopById(%1);
    var widget = desktop ? desktop.widgetById(%2) : null;
    if (!widget || widget.type !== 'org.archdock.dock') return 0;
    widget.currentConfigGroup = ['General'];
    if (String(widget.readConfig('panelId', '')) !== %3 ||
        String(widget.readConfig('ownerToken', '')) !== %4) return 0;
    widget.writeConfig('auditionRestoreGeometry', '');
    return String(widget.readConfig('auditionRestoreGeometry', '')) === '' ? 1 : 0;
})(); print('ARCHDOCK_RESULT:' + String(result));
)JS").arg(host.freeDesktopContainmentId).arg(host.freeDockAppletId)
        .arg(plasmaScriptStringLiteral(definition.identity.id)).arg(plasmaScriptStringLiteral(host.freeOwnershipToken)));
    if (cleared != 1 || !observed || *observed != geometry)
    {
        qWarning() << "Preset free geometry read-back failed:" << definition.identity.id
            << "script-result" << applied << "requested" << geometry
            << "observed" << observed.value_or(QVariantMap{});
        if (errorCode) *errorCode = QStringLiteral("preview-free-geometry-readback-failed");
        return false;
    }
    return true;
}

bool PanelWindow::capturePresetPreviewHost(ArchDock::PresetPreviewRecord &record,
    QString *errorCode) const
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-host-not-verified"); return false; };
    if (record.previewToken.isEmpty()) return fail();
    if (record.hostKind == QStringLiteral("free-desktop"))
    {
        if (freePanelHostVerification(record.containmentId, record.appletId, record.panelId,
            record.previewToken) != ArchDock::FreePanelHostVerificationOutcome::Owned) return fail();
        const auto geometry = presetFreeHostGeometry(record.snapshot, errorCode);
        if (!geometry) return false;
        record.hostState = {{QStringLiteral("geometry"), *geometry}};
        return true;
    }
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    if (!shell.isValid()) return fail();
    const QString script = QStringLiteral(R"JS(
var state = (function() {
    var panel = panelById(%1);
    if (!panel) return {};
    panel.currentConfigGroup = ['ArchDock'];
    if (String(panel.readConfig('panelId', '')) !== %2 ||
        String(panel.readConfig('ownerToken', '')) !== %3) return {};
    var state = {};
    var keys = ['location', 'screen', 'alignment', 'offset', 'height',
        'minimumLength', 'maximumLength', 'length', 'lengthMode'];
    for (var i = 0; i < keys.length; ++i) {
        if (typeof panel[keys[i]] === 'undefined') return {};
        state[keys[i]] = panel[keys[i]];
    }
    state.panelType = '';
    if (%4 >= 0) {
        var dock = panel.widgetById(%4);
        if (!dock || dock.type !== 'org.archdock.dock') return {};
        dock.currentConfigGroup = ['General'];
        if (String(dock.readConfig('panelId', '')) !== %2) return {};
        state.panelType = String(dock.readConfig('panelType', ''));
    }
    return state;
})();
print('ARCHDOCK_PREVIEW_HOST:' + JSON.stringify(state));
)JS").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.panelId))
        .arg(plasmaScriptStringLiteral(record.previewToken)).arg(record.appletId);
    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    const QString prefix = QStringLiteral("ARCHDOCK_PREVIEW_HOST:");
    if (!reply.isValid() || reply.value().size() > 8192 || !reply.value().trimmed().startsWith(prefix)) return fail();
    const auto document = QJsonDocument::fromJson(reply.value().trimmed().mid(prefix.size()).toUtf8());
    if (!document.isObject()) return fail();
    const auto state = document.object().toVariantMap();
    if (state.size() != 10 || !QStringList{QStringLiteral("top"), QStringLiteral("bottom"),
        QStringLiteral("left"), QStringLiteral("right")}.contains(state.value(QStringLiteral("location")).toString()) ||
        !QStringList{QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("right")}.contains(
            state.value(QStringLiteral("alignment")).toString()) ||
        !QStringList{QStringLiteral("fit"), QStringLiteral("custom"), QStringLiteral("fill")}.contains(
            state.value(QStringLiteral("lengthMode")).toString())) return fail();
    for (const QString &key : {QStringLiteral("screen"), QStringLiteral("offset"), QStringLiteral("height"),
        QStringLiteral("minimumLength"), QStringLiteral("maximumLength"), QStringLiteral("length")})
        if (!ArchDock::PresetParsing::isInteger(state.value(key)) || state.value(key).toInt() < 0) return fail();
    if (record.appletId >= 0 && !isNativeDockPanelType(state.value(QStringLiteral("panelType")).toString())) return fail();
    record.hostState = state;
    return true;
}

bool PanelWindow::restorePresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode, bool profileTransaction) const
{
    const auto fail = [errorCode](const QString &code) { if (errorCode) *errorCode = code; return false; };
    const auto stored = m_panelRegistry.panelDefinition(record.panelId);
    if (!profileTransaction)
    {
        if (!stored) return fail(QStringLiteral("preview-panel-missing"));
        auto comparable = *stored;
        comparable.settingsRevision = record.snapshot.settingsRevision;
        if (comparable != record.snapshot) return fail(QStringLiteral("preview-revision-conflict"));
    }
    if (record.hostKind == QStringLiteral("free-desktop"))
    {
        if (record.hostState.keys() != QStringList{QStringLiteral("geometry")})
            return fail(QStringLiteral("invalid-preview-host-snapshot"));
        return setPresetFreeHostGeometry(record.snapshot, record.hostState.value(QStringLiteral("geometry")).toMap(), errorCode);
    }
    // Revalidate the journal's bounded property shape before generating any
    // mutation. Property names and write order are fixed in this function.
    ArchDock::PresetPreviewRecord current = record;
    if (!capturePresetPreviewHost(current, errorCode)) return false;
    if (record.hostState.size() != current.hostState.size() || record.hostState.keys() != current.hostState.keys())
        return fail(QStringLiteral("invalid-preview-host-snapshot"));
    for (auto it = record.hostState.cbegin(); it != record.hostState.cend(); ++it)
        if (it.value().metaType() != current.hostState.value(it.key()).metaType())
            return fail(QStringLiteral("invalid-preview-host-snapshot"));
    if (current.hostState == record.hostState) return true;
    const QString snapshotJson = QString::fromUtf8(QJsonDocument::fromVariant(record.hostState).toJson(QJsonDocument::Compact));
    const int restored = evaluatePlasmaScriptResult(QStringLiteral(R"JS(
var result = (function() {
    try {
        var panel = panelById(%1);
        if (!panel) return 0;
        panel.currentConfigGroup = ['ArchDock'];
        if (String(panel.readConfig('panelId', '')) !== %2 ||
            String(panel.readConfig('ownerToken', '')) !== %3) return 0;
        var state = JSON.parse(%4);
        var keys = ['location', 'screen', 'alignment', 'offset', 'height',
            'maximumLength', 'minimumLength', 'length', 'lengthMode'];
        for (var i = 0; i < keys.length; ++i)
            if (panel[keys[i]] !== state[keys[i]]) panel[keys[i]] = state[keys[i]];
        if (%5 >= 0) {
            var dock = panel.widgetById(%5);
            if (!dock || dock.type !== 'org.archdock.dock') return 0;
            dock.currentConfigGroup = ['General'];
            if (String(dock.readConfig('panelId', '')) !== %2) return 0;
            if (String(dock.readConfig('panelType', '')) !== state.panelType) {
                dock.writeConfig('panelType', state.panelType); dock.reloadConfig();
            }
        }
        return 1;
    } catch (error) { return -1; }
})(); print('ARCHDOCK_RESULT:' + String(result));
)JS").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.panelId))
        .arg(plasmaScriptStringLiteral(record.previewToken)).arg(plasmaScriptStringLiteral(snapshotJson)).arg(record.appletId));
    if (restored != 1 || !capturePresetPreviewHost(current, errorCode) || current.hostState != record.hostState)
        return fail(QStringLiteral("preview-host-rollback-readback-failed"));
    return true;
}

bool PanelWindow::removePresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode, bool profileTransaction) const
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-host-cleanup-blocked"); return false; };
    if (!record.temporary || (!profileTransaction && m_panelRegistry.panelDefinition(record.panelId))) return fail();
    for (const QString &token : {record.previewToken, record.managedToken})
    {
        if (token.isEmpty()) continue;
        if (record.hostKind == QStringLiteral("free-desktop"))
        {
            const auto outcome = removeOwnedFreePanelHostByIdentity(record.panelId, token);
            if (outcome == ArchDock::FreePanelRemovalOutcome::QueryFailed) return fail();
            // Plasma can defer applet destruction until evaluateScript returns.
            // Observe disappearance in a separate call, without retrying removal.
            // Count every matching token, including malformed host markers.
            const auto remaining = evaluatePlasmaScriptResultOptional(QStringLiteral(R"JS(
var count = 0;
var all = desktops();
for (var i = 0; i < all.length; ++i) {
    var desktop = desktopById(Number(all[i].id));
    if (!desktop) continue;
    var widgets = desktop.widgets('org.archdock.dock');
    for (var j = 0; j < widgets.length; ++j) {
        var widget = desktop.widgetById(Number(widgets[j].id));
        if (!widget || widget.type !== 'org.archdock.dock') continue;
        widget.currentConfigGroup = ['General'];
        if (String(widget.readConfig('panelId', '')) === %1 &&
            String(widget.readConfig('ownerToken', '')) === %2) ++count;
    }
}
print('ARCHDOCK_RESULT:' + String(count));
)JS").arg(plasmaScriptStringLiteral(record.panelId)).arg(plasmaScriptStringLiteral(token)));
            if (!remaining || *remaining != 0) return fail();
        }
        else
        {
            const auto found = discoverNativePanel(record.panelId, token, record.snapshot.content.type);
            if (found.status == NativePanelDiscoveryStatus::Missing) continue;
            // Renderer mismatch does not change the already-verified unique
            // host identity. A partial preview can still be safely removed.
            if ((found.status != NativePanelDiscoveryStatus::Unique &&
                 found.status != NativePanelDiscoveryStatus::RendererConflict) ||
                !rollbackNativePanelCandidate(record.panelId, found.containmentId, token)) return fail();
            if (discoverNativePanel(record.panelId, token, record.snapshot.content.type).status !=
                NativePanelDiscoveryStatus::Missing) return fail();
        }
    }
    return true;
}

bool PanelWindow::recoverPresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode)
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-recovery-blocked"); return false; };
    const auto stored = m_panelRegistry.panelDefinition(record.panelId);
    if (record.temporary)
    {
        if (!stored) return removePresetPreviewHost(record, errorCode);
        if (record.managedToken.isEmpty() || stored->identity.builtIn || stored->settingsRevision < 1) return fail();
        // Adoption may have committed before the journal's COMMITTED write.
        // Preserve it only when both the durable association and host agree.
        if (stored->host.kind == ArchDock::PanelHostKind::FreeDesktop)
            return stored->host.freeOwnershipToken == record.managedToken &&
                stored->host.freeDesktopContainmentId == record.containmentId && stored->host.freeDockAppletId == record.appletId &&
                freePanelHostVerification(record.containmentId, record.appletId, record.panelId, record.managedToken) ==
                    ArchDock::FreePanelHostVerificationOutcome::Owned ? true : fail();
        return stored->host.nativeOwnershipToken == record.managedToken && stored->host.nativePanelId == record.containmentId &&
            stored->host.nativeDockAppletId == record.appletId && nativePanelIsOwned(record.panelId, record.containmentId,
                record.managedToken) ? true : fail();
    }
    if (!stored) return fail();
    if (*stored == record.snapshot)
    {
        auto verified = record;
        return record.kind == QStringLiteral("icon")
            ? capturePresetPreviewHost(verified, errorCode)
            : restorePresetPreviewHost(record, errorCode);
    }
    if (stored->host != record.snapshot.host) return fail();
    // A committed revision or another valid settings transaction is durable
    // truth. Reapply that definition instead of an older snapshot.
    if (stored->host.kind == ArchDock::PanelHostKind::FreeDesktop)
        return freePanelHostVerification(record.containmentId, record.appletId, record.panelId,
            record.previewToken) == ArchDock::FreePanelHostVerificationOutcome::Owned ? true : fail();
    const ArchDock::PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
    const auto placement = normalizedNativePanelPlacement(*stored);
    if (!placement.isValid() || !placement.isSupported() || !placement.placement ||
        !adapter.applyPlacement(record.containmentId, record.panelId, record.previewToken, *placement.placement).success()) return fail();
    if (record.appletId >= 0 && evaluatePlasmaScriptResult(QStringLiteral(
        "var result = (function() { var panel = panelById(%1); if (!panel) return 0; "
        "panel.currentConfigGroup = ['ArchDock']; if (String(panel.readConfig('ownerToken', '')) !== %2 || "
        "String(panel.readConfig('panelId', '')) !== %3) return 0; var dock = panel.widgetById(%4); "
        "if (!dock || dock.type !== 'org.archdock.dock') return 0; dock.currentConfigGroup = ['General']; "
        "if (String(dock.readConfig('panelId', '')) !== %3) return 0; dock.writeConfig('panelType', %5); "
        "dock.reloadConfig(); return String(dock.readConfig('panelType', '')) === %5 ? 1 : 0; })(); "
        "print('ARCHDOCK_RESULT:' + String(result));").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.previewToken))
        .arg(plasmaScriptStringLiteral(record.panelId)).arg(record.appletId).arg(plasmaScriptStringLiteral(stored->content.type))) != 1) return fail();
    return true;
}

bool PanelWindow::convertPresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    ArchDock::PanelDefinition &candidate, QString *errorCode) const
{
    if (!record.temporary || record.managedToken.isEmpty() ||
        !record.previewToken.startsWith(QLatin1String("archdock-preview-")))
    {
        if (errorCode) *errorCode = QStringLiteral("invalid-preview-conversion");
        return false;
    }
    const bool free = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop;
    const QString host = free
        ? QStringLiteral("var desktop = desktopById(%1); var host = desktop ? desktop.widgetById(%2) : null;")
            .arg(candidate.host.freeDesktopContainmentId).arg(candidate.host.freeDockAppletId)
        : QStringLiteral("var host = panelById(%1);").arg(candidate.host.nativePanelId);
    const QString extra = free
        ? QStringLiteral("if (host.type !== 'org.archdock.dock' || String(host.readConfig('panelType', '')) !== 'empty' || "
            "!['false', '0'].includes(String(host.readConfig('bootstrapFreeDock', true)).toLowerCase())) return 0;")
        : QStringLiteral("var dock = host.widgetById(%1); if (!dock || dock.type !== 'org.archdock.dock') return 0; "
            "dock.currentConfigGroup = ['General']; if (String(dock.readConfig('panelId', '')) !== %2 || "
            "String(dock.readConfig('panelType', '')) !== %3) return 0;")
            .arg(candidate.host.nativeDockAppletId).arg(plasmaScriptStringLiteral(record.panelId))
            .arg(plasmaScriptStringLiteral(candidate.content.type));
    const QString group = free ? QStringLiteral("General") : QStringLiteral("ArchDock");
    const int converted = evaluatePlasmaScriptResult(QStringLiteral(
        "var result = (function() { try { %1 if (!host) return 0; "
        "host.currentConfigGroup = [%2]; "
        "if (String(host.readConfig('panelId', '')) !== %3 || "
        "String(host.readConfig('ownerToken', '')) !== %4) return 0; %5 "
        "host.currentConfigGroup = [%2]; host.writeConfig('ownerToken', %6); host.reloadConfig(); "
        "if (String(host.readConfig('ownerToken', '')) === %6 && "
        "String(host.readConfig('panelId', '')) === %3) return 1; "
        "host.writeConfig('ownerToken', %4); host.reloadConfig(); return 0; "
        "} catch (error) { return -1; } })(); print('ARCHDOCK_RESULT:' + String(result));")
        .arg(host, plasmaScriptStringLiteral(group), plasmaScriptStringLiteral(record.panelId),
            plasmaScriptStringLiteral(record.previewToken), extra, plasmaScriptStringLiteral(record.managedToken)));
    const bool verified = converted == 1 && (free
        ? freePanelHostVerification(candidate.host.freeDesktopContainmentId,
            candidate.host.freeDockAppletId, record.panelId, record.managedToken) ==
            ArchDock::FreePanelHostVerificationOutcome::Owned
        : nativePanelIsOwned(record.panelId, candidate.host.nativePanelId, record.managedToken));
    if (!verified)
    {
        if (errorCode) *errorCode = QStringLiteral("preview-token-conversion-failed");
        return false;
    }
    if (free) candidate.host.freeOwnershipToken = record.managedToken;
    else candidate.host.nativeOwnershipToken = record.managedToken;
    candidate.settingsRevision = 1;
    candidate.identity.builtIn = false;
    candidate = candidate.normalized();
    return true;
}
