#pragma once

#include "IconStyleDefinition.h"
#include "PresetIdentity.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

// Icon Presets: a loadable look for a panel's icons. One names an icon
// style and may adjust its layers and states, its glyph policy and its
// motion. Like Panel Presets, it is parsed strictly (PresetParsing).
namespace ArchDock
{

struct IconPresetCompatibility
{
    QStringList rendererTiers;
    QStringList requiredStyleCapabilities;
    bool reducedMotionSupport = true;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconPresetCompatibility &) const = default;
};

// The motion profile reference and the motion defaults that travel with it.
// Each member is an icon-layer panel setting that already exists.
struct IconPresetMotion
{
    QString profileId = QStringLiteral("none");
    QString trigger = QStringLiteral("hover");
    qreal speed = 1.0;
    qreal intensity = 1.0;
    qreal magnificationRadius = 2.4;
    QString magnificationFalloff = QStringLiteral("linear");

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconPresetMotion &) const = default;
};

struct IconPresetIcon
{
    // The icon style this preset is built on. An icon preset is not an icon
    // style: it references one and may adjust how it is drawn.
    QString iconStyleId;
    // Layer id -> the layer fields this preset replaces.
    QVariantMap visualOverrides;
    // Icon state id -> the state fields this preset replaces.
    QVariantMap stateOverrides;
    IconStyleGlyphPolicyDefinition glyphPolicy;
    IconPresetMotion motion;
    // Icon state id -> motion profile id.
    QMap<QString, QString> perStateAnimationOverrides;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconPresetIcon &) const = default;
};

struct IconPresetFallback
{
    QString iconStyleId = QStringLiteral("plain-original");
    QString motionProfileId = QStringLiteral("none");

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconPresetFallback &) const = default;
};

// A reusable icon appearance and motion configuration. It is a separate
// resource type from an icon style, a panel preset and a profile.
struct IconPresetDefinition
{
    static constexpr int CurrentSchemaVersion = 1;
    static constexpr qsizetype MaximumOverrideEntries = 64;

    QString format = formatName();
    int schemaVersion = CurrentSchemaVersion;
    PresetIdentity identity;
    IconPresetCompatibility compatibility;
    IconPresetIcon icon;
    IconPresetFallback fallback;

    [[nodiscard]] static QString formatName();
    // The fields an override may replace. The icon-style validator re-checks
    // every merged value, so these lists only bound what may be named.
    [[nodiscard]] static const QStringList &layerOverrideKeys();
    [[nodiscard]] static const QStringList &stateOverrideKeys();
    [[nodiscard]] static const QStringList &stateIds();

    // The panel settings this preset resolves to. Icon layer only: no theme,
    // layout, placement, visibility, presentation or content key appears here,
    // which is what lets an icon preset be applied without touching a panel.
    [[nodiscard]] static const QStringList &panelValueKeys();
    [[nodiscard]] QVariantMap panelValues() const;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] static std::optional<IconPresetDefinition> fromVariantMap(
        const QVariantMap &definition,
        QVector<PresetValidationDiagnostic> *diagnostics = nullptr);

    bool operator==(const IconPresetDefinition &) const = default;
};

}
