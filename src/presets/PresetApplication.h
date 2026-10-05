#pragma once

#include "../model/IconPresetDefinition.h"
#include "../model/PanelPresetDefinition.h"
#include "../panel/PanelSettingsTransaction.h"

namespace ArchDock
{

// Shared preparation for audition, active application and future defaults.
// These functions neither persist nor touch a desktop host.
class PresetApplication final
{
public:
    [[nodiscard]] static std::optional<PanelSettingsTransactionDraft> preparePanel(
        const PanelDefinition &snapshot,
        const QVariantMap &globals,
        const PanelPresetDefinition &preset,
        const QVariantMap &customizations,
        const std::optional<IconPresetDefinition> &recommendedIcons,
        QString *errorCode = nullptr);
    [[nodiscard]] static std::optional<PanelSettingsTransactionDraft> prepareIcon(
        const PanelDefinition &snapshot,
        const QVariantMap &globals,
        const IconPresetDefinition &preset,
        const QVariantMap &customizations,
        QString *errorCode = nullptr);
    // A desktop 3D edit of an existing panel: only the settings on the
    // Panels > 3D page may change. Empty customizations give the panel itself.
    [[nodiscard]] static std::optional<PanelSettingsTransactionDraft> prepareSceneEdit(
        const PanelDefinition &snapshot,
        const QVariantMap &globals,
        const QVariantMap &customizations,
        QString *errorCode = nullptr);
    [[nodiscard]] static bool isSceneEditKey(const QString &key);
    [[nodiscard]] static bool iconOnlyChange(
        const PanelDefinition &snapshot, const PanelDefinition &candidate);
    [[nodiscard]] static PanelPresetDefinition panelSnapshot(
        const PanelPresetDefinition &source,
        const PanelDefinition &draft,
        const QString &name);
    [[nodiscard]] static IconPresetDefinition iconSnapshot(
        const IconPresetDefinition &source,
        const PanelDefinition &draft,
        const QString &name);
    static void markCustomized(
        const PanelDefinition &previous, PanelDefinition *candidate);
};

}
