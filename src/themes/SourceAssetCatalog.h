#pragma once

#include "../model/ThemeDefinition.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct SourceAssetProvenance
{
    QString sourceKind;
    QString creator;
    QString licenseSpdx;
    QString redistribution = QStringLiteral("unknown");
    QString evidence;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const SourceAssetProvenance &) const = default;
};

struct SourceAssetRecord
{
    QString id;
    QString archiveMember;
    QString fileName;
    QString sha256;
    qint64 byteSize = 0;
    QString sampleKind;
    QString assetClass;
    QString intendedUse;
    SourceAssetProvenance provenance;
    QString targetRendererTier = QStringLiteral("unknown");
    QStringList orientations;
    QStringList cleanupRequirements;
    QString statePairAvailability = QStringLiteral("unknown");
    QString status = QStringLiteral("reference-only");
    QStringList visualGroups;
    QString reviewNotes;
    bool isolatedCleanAsset = false;
    bool opaqueScreenshot = true;

    [[nodiscard]] bool isInstallEligible() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const SourceAssetRecord &) const = default;
};

struct SourceAssetCatalogLoadResult;

class SourceAssetCatalog
{
public:
    static constexpr int CurrentVersion = 1;
    static constexpr qsizetype ExpectedAssetCount = 138;
    static constexpr qsizetype ExpectedPanelCount = 122;
    static constexpr qsizetype ExpectedIconReferenceCount = 16;

    [[nodiscard]] static SourceAssetCatalogLoadResult load(
        const QString &catalogPath);
    [[nodiscard]] static SourceAssetCatalogLoadResult loadBytes(
        const QByteArray &catalogBytes,
        const QString &catalogPath = {});

    [[nodiscard]] QVector<ThemeValidationDiagnostic> validateSourceRoot(
        const QString &sourceRoot) const;
    [[nodiscard]] const SourceAssetRecord *recordById(const QString &id) const;
    [[nodiscard]] QVector<SourceAssetRecord> installEligibleRecords() const;
    [[nodiscard]] QVariantMap toVariantMap() const;

    QString format = QStringLiteral("org.archdock.source-asset-catalog");
    int version = CurrentVersion;
    QString archiveFileName;
    QString archiveSha256;
    QString archiveContentRoot;
    int expectedPanelCount = 0;
    int expectedIconReferenceCount = 0;
    int expectedAssetCount = 0;
    bool sourceOnly = true;
    bool installByDefault = false;
    QString installEligibleStatus = QStringLiteral("production-approved");
    QVector<SourceAssetRecord> assets;
};

struct SourceAssetCatalogLoadResult
{
    std::optional<SourceAssetCatalog> catalog;
    QVector<ThemeValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

}
