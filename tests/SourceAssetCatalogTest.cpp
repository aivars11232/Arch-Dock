#include "themes/SourceAssetCatalog.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::SourceAssetCatalog;
using ArchDock::SourceAssetRecord;

namespace
{

QString catalogPath()
{
    return QFINDTESTDATA(
        QStringLiteral("../data/source-assets/source-asset-catalog.json"));
}

QByteArray catalogBytes()
{
    QFile file(catalogPath());
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

}

class SourceAssetCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void completeCatalogLoadsWithNoInstallableScreenshots();
    void duplicateIdentifiersAreRejected();
    void unsafeArchiveMembersAreRejected();
    void missingSourceFilesAreDetected();
    void installationRequiresAllEvidence();
    void incompleteVisualReviewIsRejected();
    void liveArchiveAndExtractedRootMatchCatalog();
};

void SourceAssetCatalogTest::completeCatalogLoadsWithNoInstallableScreenshots()
{
    QVERIFY(!catalogPath().isEmpty());
    const auto result = SourceAssetCatalog::load(catalogPath());
    QVERIFY2(result.isValid(), qPrintable(result.primaryCode()));
    const SourceAssetCatalog &catalog = *result.catalog;
    QCOMPARE(catalog.assets.size(), SourceAssetCatalog::ExpectedAssetCount);
    QCOMPARE(catalog.expectedPanelCount, SourceAssetCatalog::ExpectedPanelCount);
    QCOMPARE(catalog.expectedIconReferenceCount,
             SourceAssetCatalog::ExpectedIconReferenceCount);
    QVERIFY(catalog.sourceOnly);
    QVERIFY(!catalog.installByDefault);
    QVERIFY(catalog.installEligibleRecords().isEmpty());

    qsizetype panelCount = 0;
    qsizetype iconCount = 0;
    QHash<QString, qsizetype> classCounts;
    QSet<QString> visualGroups;
    const QSet<QString> noAssertionReferences{
        QStringLiteral("panel-screenshot-20260802-010007"),
        QStringLiteral("panel-screenshot-20260802-010039"),
        QStringLiteral("panel-screenshot-20260802-010048"),
    };
    for (const SourceAssetRecord &record : catalog.assets)
    {
        panelCount += record.sampleKind == QStringLiteral("panel");
        iconCount += record.sampleKind == QStringLiteral("icon-reference");
        ++classCounts[record.assetClass];
        for (const QString &visualGroup : record.visualGroups)
        {
            visualGroups.insert(visualGroup);
        }
        QCOMPARE(record.status, QStringLiteral("reference-only"));
        QCOMPARE(record.provenance.redistribution, QStringLiteral("unknown"));
        if (noAssertionReferences.contains(record.id))
        {
            QCOMPARE(record.provenance.licenseSpdx,
                     QStringLiteral("NOASSERTION"));
            QCOMPARE(record.provenance.creator,
                     QStringLiteral("unknown third-party creator(s)"));
            QVERIFY(record.provenance.evidence.contains(
                QStringLiteral("Google Images")));
        }
        else
        {
            QVERIFY(record.provenance.licenseSpdx.isEmpty());
        }
        QVERIFY(record.opaqueScreenshot);
        QVERIFY(!record.isolatedCleanAsset);
        QVERIFY(!record.isInstallEligible());
        QVERIFY(!record.assetClass.startsWith(QStringLiteral("unreviewed")));
        QVERIFY(record.reviewNotes.contains(
            QStringLiteral("visually reviewed"), Qt::CaseInsensitive));
        QVERIFY(record.statePairAvailability != QStringLiteral("unknown"));
        QVERIFY(record.cleanupRequirements.contains(
            QStringLiteral("manual-review")));
        QVERIFY(record.cleanupRequirements.contains(
            QStringLiteral("isolate-clean-assets")));
        QVERIFY(record.cleanupRequirements.contains(
            QStringLiteral("alpha-mask-review")));
    }
    QCOMPARE(panelCount, SourceAssetCatalog::ExpectedPanelCount);
    QCOMPARE(iconCount, SourceAssetCatalog::ExpectedIconReferenceCount);
    QCOMPARE(noAssertionReferences.size(), 3);
    QCOMPARE(classCounts.value(QStringLiteral("A-desktop-product-reference")), 9);
    QCOMPARE(classCounts.value(QStringLiteral("B-strong-horizontal-2d-candidate")), 8);
    QCOMPARE(classCounts.value(QStringLiteral("C-legacy-dock-shelf-reference")), 27);
    QCOMPARE(classCounts.value(QStringLiteral("D-energy-glow-frame-candidate")), 28);
    QCOMPARE(classCounts.value(QStringLiteral("E-circular-launcher-reference")), 8);
    QCOMPARE(classCounts.value(QStringLiteral("F-ring-polygon-platform-candidate")), 30);
    QCOMPARE(classCounts.value(QStringLiteral("G-arc-freeform-platform-candidate")), 12);
    QCOMPARE(classCounts.value(QStringLiteral("H-icon-style-reference-sheet")), 16);
    QCOMPARE(classCounts.size(), 8);
    for (const QString &requiredGroup : {
             QStringLiteral("strong-2d"), QStringLiteral("energy"),
             QStringLiteral("ring-polygon"), QStringLiteral("arc"),
             QStringLiteral("icon-reference")})
    {
        QVERIFY2(visualGroups.contains(requiredGroup), qPrintable(requiredGroup));
    }
    QCOMPARE(catalog.recordById(catalog.assets.constFirst().id),
             &catalog.assets.constFirst());
    QVERIFY(!catalog.recordById(QStringLiteral("not-present")));
}

void SourceAssetCatalogTest::duplicateIdentifiersAreRejected()
{
    QJsonDocument document = QJsonDocument::fromJson(catalogBytes());
    QJsonObject root = document.object();
    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    QJsonObject duplicate = assets.at(1).toObject();
    duplicate.insert(QStringLiteral("id"),
                     assets.at(0).toObject().value(QStringLiteral("id")));
    assets.replace(1, duplicate);
    root.insert(QStringLiteral("assets"), assets);

    const auto result = SourceAssetCatalog::loadBytes(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("duplicate-id"));
}

void SourceAssetCatalogTest::unsafeArchiveMembersAreRejected()
{
    QJsonDocument document = QJsonDocument::fromJson(catalogBytes());
    QJsonObject root = document.object();
    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    QJsonObject unsafe = assets.at(0).toObject();
    unsafe.insert(QStringLiteral("archiveMember"), QStringLiteral("../outside.png"));
    unsafe.insert(QStringLiteral("fileName"), QStringLiteral("outside.png"));
    assets.replace(0, unsafe);
    root.insert(QStringLiteral("assets"), assets);

    const auto result = SourceAssetCatalog::loadBytes(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("unsafe-path"));
}

void SourceAssetCatalogTest::missingSourceFilesAreDetected()
{
    const auto result = SourceAssetCatalog::load(catalogPath());
    QVERIFY(result.isValid());
    QTemporaryDir emptyRoot;
    QVERIFY(emptyRoot.isValid());

    const auto diagnostics = result.catalog->validateSourceRoot(emptyRoot.path());
    QVERIFY(!diagnostics.isEmpty());
    QCOMPARE(diagnostics.constFirst().code, QStringLiteral("missing-asset"));
}

void SourceAssetCatalogTest::installationRequiresAllEvidence()
{
    const auto result = SourceAssetCatalog::load(catalogPath());
    QVERIFY(result.isValid());
    SourceAssetRecord candidate = result.catalog->assets.constFirst();
    QVERIFY(!candidate.isInstallEligible());

    candidate.status = QStringLiteral("production-approved");
    candidate.provenance.redistribution = QStringLiteral("allowed");
    candidate.isolatedCleanAsset = true;
    candidate.opaqueScreenshot = false;
    candidate.cleanupRequirements = {QStringLiteral("none")};
    QVERIFY(candidate.isInstallEligible());

    candidate.cleanupRequirements = {QStringLiteral("remove-logo")};
    QVERIFY(!candidate.isInstallEligible());
}

void SourceAssetCatalogTest::incompleteVisualReviewIsRejected()
{
    QJsonDocument document = QJsonDocument::fromJson(catalogBytes());
    QJsonObject root = document.object();
    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    QJsonObject incomplete = assets.at(0).toObject();
    incomplete.insert(
        QStringLiteral("reviewNotes"),
        QStringLiteral("Pending visual classification in TASK-0026 Phase E"));
    incomplete.insert(QStringLiteral("statePairAvailability"),
                      QStringLiteral("unknown"));
    assets.replace(0, incomplete);
    root.insert(QStringLiteral("assets"), assets);

    const auto result = SourceAssetCatalog::loadBytes(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("invalid-value"));
}

void SourceAssetCatalogTest::liveArchiveAndExtractedRootMatchCatalog()
{
    const QString archivePath = qEnvironmentVariable("ARCHDOCK_SAMPLE_ARCHIVE");
    const QString sourceRoot = qEnvironmentVariable("ARCHDOCK_SAMPLE_ROOT");
    if (archivePath.isEmpty() || sourceRoot.isEmpty())
    {
        QSKIP("live source archive/root not supplied to this test invocation");
    }

    const auto result = SourceAssetCatalog::load(catalogPath());
    QVERIFY(result.isValid());
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::ReadOnly));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QVERIFY(hash.addData(&archive));
    QCOMPARE(QString::fromLatin1(hash.result().toHex()),
             result.catalog->archiveSha256);
    const auto diagnostics = result.catalog->validateSourceRoot(sourceRoot);
    QVERIFY2(diagnostics.isEmpty(),
             qPrintable(diagnostics.isEmpty()
                            ? QString{}
                            : diagnostics.constFirst().toVariantMap().value(
                                  QStringLiteral("message")).toString()));
}

QTEST_GUILESS_MAIN(SourceAssetCatalogTest)

#include "SourceAssetCatalogTest.moc"
