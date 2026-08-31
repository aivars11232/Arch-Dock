#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct IconStyleValidationDiagnostic
{
    QString code;
    QString jsonPointer;
    QString severity = QStringLiteral("error");
    QString message;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleValidationDiagnostic &) const = default;
};

[[nodiscard]] QVariantList iconStyleDiagnosticsToVariantList(
    const QVector<IconStyleValidationDiagnostic> &diagnostics);

struct IconStyleLicenseDefinition
{
    QString spdx;
    QString redistribution = QStringLiteral("unknown");
    QString evidence;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleLicenseDefinition &) const = default;
};

struct IconStyleGlyphPolicyDefinition
{
    QString mode = QStringLiteral("original");
    QString tint;
    bool compatibleOnly = true;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleGlyphPolicyDefinition &) const = default;
};

struct IconStyleSafeInset
{
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleSafeInset &) const = default;
};

struct IconStyleLayerDefinition
{
    QString id;
    QString kind = QStringLiteral("procedural");
    QString asset;
    QString shape = QStringLiteral("rounded-rect");
    QString color = QStringLiteral("transparent");
    QString secondaryColor;
    QString borderColor = QStringLiteral("transparent");
    qreal opacity = 1.0;
    qreal inset = 0.0;
    qreal radius = 0.22;
    qreal borderWidth = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleLayerDefinition &) const = default;
};

struct IconStyleLayerSet
{
    QVector<IconStyleLayerDefinition> rear;
    QVector<IconStyleLayerDefinition> base;
    QVector<IconStyleLayerDefinition> front;
    std::optional<IconStyleLayerDefinition> mask;
    std::optional<IconStyleLayerDefinition> reflection;
    std::optional<IconStyleLayerDefinition> shadow;
    std::optional<IconStyleLayerDefinition> glow;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QStringList assetPaths() const;
    bool operator==(const IconStyleLayerSet &) const = default;
};

struct IconStyleStateDefinition
{
    QString id;
    qreal rearOpacity = 1.0;
    qreal baseOpacity = 1.0;
    qreal frontOpacity = 1.0;
    qreal glyphOpacity = 1.0;
    qreal glyphScale = 1.0;
    QString borderColor = QStringLiteral("transparent");
    QString glowColor = QStringLiteral("transparent");
    qreal glowOpacity = 0.0;
    qreal reflectionOpacity = 0.0;
    QString indicatorColor = QStringLiteral("transparent");
    qreal indicatorOpacity = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleStateDefinition &) const = default;
};

struct IconStyleCapabilityDefinition
{
    QStringList rendererTiers;
    QStringList features;
    QStringList animationCapabilities;
    bool supports3D = false;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStyleCapabilityDefinition &) const = default;
};

struct IconStylePreviewDefinition
{
    QString seed;
    QString iconName = QStringLiteral("applications-system");
    QString state = QStringLiteral("normal");
    int tileSize = 64;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const IconStylePreviewDefinition &) const = default;
};

struct IconStyle3DReference
{
    QString mesh;
    QString material;
    QString fallbackStyleId = QStringLiteral("plain-original");

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QStringList assetPaths() const;
    bool operator==(const IconStyle3DReference &) const = default;
};

struct IconStyleDefinition
{
    static constexpr int CurrentVersion = 1;

    QString format = QStringLiteral("org.archdock.icon-style");
    int version = CurrentVersion;
    QString id;
    QString name;
    int revision = 1;
    QString author;
    QString description;
    IconStyleLicenseDefinition license;
    IconStyleGlyphPolicyDefinition glyphPolicy;
    IconStyleSafeInset safeGlyphInset;
    IconStyleLayerSet layers;
    QVector<IconStyleStateDefinition> states;
    IconStyleCapabilityDefinition capabilities;
    IconStylePreviewDefinition preview;
    QMap<QString, QString> mappedReplacements;
    std::optional<IconStyle3DReference> threeD;
    QVariantMap extensions;

    [[nodiscard]] const IconStyleStateDefinition *stateById(
        const QString &stateId) const;
    [[nodiscard]] QStringList assetPaths() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QVariantMap toRuntimeProjection(
        const QString &validationStatus = QStringLiteral("valid"),
        const QVector<IconStyleValidationDiagnostic> &diagnostics = {}) const;
    bool operator==(const IconStyleDefinition &) const = default;
};

}
