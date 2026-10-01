#pragma once

#include "PanelDefinition.h"
#include "PresetIdentity.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct PanelPresetPreview
{
    QString previewMode = QStringLiteral("horizontal");
    QString rendererTier = QStringLiteral("procedural2d");
    QString fallbackTier = QStringLiteral("procedural2d");
    QString deterministicPreviewSeed;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const PanelPresetPreview &) const = default;
};

struct PanelPresetCompatibility
{
    QStringList hostKinds;
    QStringList orientations;
    QStringList layouts;
    QStringList requiredCapabilities;
    QStringList optionalCapabilities;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const PanelPresetCompatibility &) const = default;
};

struct PanelPresetPanel
{
    // The complete normalized single-panel configuration this preset
    // describes: host kind, content type, placement, visibility,
    // presentation, layout, theme reference, surface overrides and motion
    // profile references. It never carries a host association, a screen, an
    // ownership token or a content list, so it can be loaded as a draft on
    // any machine.
    PanelDefinition configuration;
    // A panel preset may recommend an icon preset; the two stay independently
    // selectable.
    QString recommendedIconPresetId;

    [[nodiscard]] QString themeId() const;
    bool operator==(const PanelPresetPanel &) const = default;
};

struct PanelPresetFallback
{
    // Empty names the built-in procedural surface, which supports every
    // layout on both hosts.
    QString themeId;
    QString iconPresetId;
    // `reject`: a field this version does not support makes the preset
    // invalid. `drop`: it is ignored and reported as a warning.
    QString unsupportedFieldPolicy = QStringLiteral("reject");

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const PanelPresetFallback &) const = default;
};

// A loadable one-panel starting configuration. It is a separate resource type
// from a panel theme, an icon preset and a multi-panel profile.
struct PanelPresetDefinition
{
    static constexpr int CurrentSchemaVersion = 1;

    QString format = formatName();
    int schemaVersion = CurrentSchemaVersion;
    PresetIdentity identity;
    PanelPresetPreview preview;
    PanelPresetCompatibility compatibility;
    PanelPresetPanel panel;
    PanelPresetFallback fallback;

    [[nodiscard]] static QString formatName();
    // The panel settings a Panel Preset may carry, in the groups the preset
    // specification names. Every key is a user-editable settings-schema field.
    [[nodiscard]] static QStringList panelValueGroups();
    [[nodiscard]] static QStringList panelValueKeys(const QString &group);
    [[nodiscard]] static QStringList panelValueKeys();
    [[nodiscard]] QVariantMap panelValues() const;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] static std::optional<PanelPresetDefinition> fromVariantMap(
        const QVariantMap &definition,
        QVector<PresetValidationDiagnostic> *diagnostics = nullptr);

    bool operator==(const PanelPresetDefinition &) const = default;
};

}
