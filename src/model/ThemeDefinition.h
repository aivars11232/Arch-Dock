#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct ThemeValidationDiagnostic
{
    QString code;
    QString jsonPointer;
    QString severity = QStringLiteral("error");
    QString message;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeValidationDiagnostic &) const = default;
};

[[nodiscard]] QVariantList themeDiagnosticsToVariantList(
    const QVector<ThemeValidationDiagnostic> &diagnostics);

struct ThemeSize
{
    int width = 0;
    int height = 0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeSize &) const = default;
};

struct ThemeRect
{
    qreal x = 0.0;
    qreal y = 0.0;
    qreal width = 0.0;
    qreal height = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeRect &) const = default;
};

struct ThemeRotationDefinition
{
    QString mode = QStringLiteral("none");
    qreal minimumDegrees = 0.0;
    qreal maximumDegrees = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeRotationDefinition &) const = default;
};

struct ThemeCapabilityDefinition
{
    QStringList hosts;
    QStringList rendererTiers;
    QString preferredRendererTier;
    QStringList fallbackRendererTiers;
    QStringList layouts;
    QStringList orientations;
    QStringList features;
    QStringList presentationMechanisms;
    ThemeRotationDefinition rotation;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeCapabilityDefinition &) const = default;
};

struct ThemeLicenseDefinition
{
    QString spdx;
    QString redistribution = QStringLiteral("unknown");
    QString evidence;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeLicenseDefinition &) const = default;
};

struct ThemeAssetDefinition
{
    QString id;
    QString path;
    QString kind;
    QString mimeType;
    QString sha256;
    std::optional<ThemeSize> naturalSize;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeAssetDefinition &) const = default;
};

struct ThemeStateDefinition
{
    QString id;
    QString inherits;
    QStringList layers;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeStateDefinition &) const = default;
};

struct ThemeLayerDefinition
{
    QString id;
    QString asset;
    QString role;
    std::optional<ThemeRect> sourceRect;
    qreal opacity = 1.0;
    QString blendMode = QStringLiteral("source-over");

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeLayerDefinition &) const = default;
};

struct ThemeSliceDefinition
{
    QString id;
    QString asset;
    QString state;
    QString orientation;
    ThemeRect sourceRect;
    qreal fixedStart = 0.0;
    qreal fixedEnd = 0.0;
    QString centerMode = QStringLiteral("stretch");

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeSliceDefinition &) const = default;
};

struct ThemeContentRegionDefinition
{
    QString id;
    QString state;
    QString orientation;
    QString shape = QStringLiteral("rect");
    std::optional<ThemeRect> rect;
    QString maskAsset;
    std::optional<qreal> baseline;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeContentRegionDefinition &) const = default;
};

struct ThemeEffectMargins
{
    qreal left = 0.0;
    qreal top = 0.0;
    qreal right = 0.0;
    qreal bottom = 0.0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeEffectMargins &) const = default;
};

struct ThemeInputMaskDefinition
{
    QString id;
    QString asset;
    QString state;
    QString orientation;
    qreal threshold = 0.5;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeInputMaskDefinition &) const = default;
};

struct ThemeResourceReference
{
    QString id;
    QString manifest;
    QStringList states;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeResourceReference &) const = default;
};

struct ThemeDefinition
{
    static constexpr int CurrentVersion = 2;

    QString format = QStringLiteral("org.archdock.theme");
    int version = CurrentVersion;
    QString id;
    QString name;
    int packageRevision = 1;
    QString author;
    QString description;
    ThemeLicenseDefinition license;
    ThemeCapabilityDefinition capabilities;
    QVector<ThemeAssetDefinition> assets;
    QVector<ThemeStateDefinition> states;
    QVector<ThemeLayerDefinition> layers;
    QVector<ThemeSliceDefinition> slices;
    QVector<ThemeContentRegionDefinition> contentRegions;
    ThemeEffectMargins effectMargins;
    QVector<ThemeInputMaskDefinition> inputMasks;
    std::optional<ThemeResourceReference> iconStyleRef;
    QVector<ThemeResourceReference> animationProfileRefs;

    int sourceVersion = CurrentVersion;
    bool adaptedFromVersion1 = false;

    [[nodiscard]] const ThemeAssetDefinition *assetById(const QString &assetId) const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QVariantMap toRuntimeProjection(
        const QString &validationStatus = QStringLiteral("valid"),
        const QVector<ThemeValidationDiagnostic> &diagnostics = {}) const;
    bool operator==(const ThemeDefinition &) const = default;
};

}
