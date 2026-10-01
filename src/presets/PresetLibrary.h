#pragma once

#include "PresetCatalog.h"
#include "UserPresetStore.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

class PanelRegistry;

namespace ArchDock
{

// The preset catalogs as Panel Studio sees them: the two immutable installed
// catalogs, the user's own presets, and for every preset whether it can be
// used here together with the data its shared-renderer preview is drawn from.
//
// It reads panel resources and writes only the user preset store. Listing,
// selecting and previewing a preset never changes a panel, a default or the
// desktop.
class PresetLibrary final : public QObject
{
    Q_OBJECT

    // Advances whenever the user's presets change.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit PresetLibrary(const PanelRegistry &registry, QObject *parent = nullptr);
    // Explicit locations, for a staged install and for tests.
    PresetLibrary(const PanelRegistry &registry,
                  QString builtInRoot,
                  QString userRoot,
                  QObject *parent = nullptr);

    // The installed preset directory, or the source tree's when Arch Dock
    // runs uninstalled.
    [[nodiscard]] static QString locateBuiltInRoot();
    [[nodiscard]] static QString defaultUserRoot();

    [[nodiscard]] int revision() const;
    [[nodiscard]] QString builtInRoot() const;
    [[nodiscard]] QString userRoot() const;

    // `valid`, `errorCode`, `diagnostics`, the two built-in counts and the
    // two roots.
    Q_INVOKABLE QVariantMap catalogStatus() const;
    // One card per preset. `scope` is `builtin` or `user`.
    Q_INVOKABLE QVariantList panelPresets(const QString &scope) const;
    Q_INVOKABLE QVariantList iconPresets(const QString &scope) const;

    // User-store actions. `kind` is `panel` or `icon`. Each returns `success`,
    // `errorCode` and, when it succeeded, the affected `presetId`.
    Q_INVOKABLE QVariantMap duplicatePreset(const QString &kind,
                                            const QString &presetId,
                                            const QString &name);
    Q_INVOKABLE QVariantMap renamePreset(const QString &kind,
                                         const QString &presetId,
                                         const QString &name);
    Q_INVOKABLE QVariantMap removePreset(const QString &kind,
                                         const QString &presetId);

signals:
    void revisionChanged();

private:
    [[nodiscard]] bool ensureLoaded() const;
    [[nodiscard]] std::optional<IconPresetDefinition> iconPreset(
        const QString &presetId) const;
    [[nodiscard]] std::optional<PanelPresetDefinition> panelPreset(
        const QString &presetId) const;
    [[nodiscard]] bool themeUsable(const QString &themeId) const;
    [[nodiscard]] QVariantMap resolvedTheme(const QString &themeId) const;
    [[nodiscard]] QVariantMap previewCandidate(
        const PanelDefinition &definition,
        const QVariantMap &iconStyleProjection) const;
    [[nodiscard]] QVariantMap panelCard(const PanelPresetDefinition &preset) const;
    [[nodiscard]] QVariantMap iconCard(const IconPresetDefinition &preset) const;
    [[nodiscard]] QVariantMap finished(const QString &errorCode,
                                       const QString &presetId);

    const PanelRegistry &m_registry;
    QString m_builtInRoot;
    UserPresetStore m_userStore;
    // The installed catalogs never change while Arch Dock runs, so they and
    // their cards are built once, on first use.
    mutable bool m_loadAttempted = false;
    mutable QString m_errorCode;
    mutable QVector<PresetValidationDiagnostic> m_diagnostics;
    mutable std::optional<IconPresetCatalog> m_iconCatalog;
    mutable std::optional<PanelPresetCatalog> m_panelCatalog;
    mutable std::optional<QVariantList> m_builtInIconCards;
    mutable std::optional<QVariantList> m_builtInPanelCards;
    int m_revision = 0;
};

}
