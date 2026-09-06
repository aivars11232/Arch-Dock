#include "themes/ThemePackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSet>
#include <QTest>

using ArchDock::ThemePackage;

namespace
{

struct PerspectiveSpec
{
    QString id;
    QString referenceId;
    QString referenceHash;
    QString shape;
    QString tint;
    QString iconStyleId;
    QString presetIntent;
    qreal trackRadiusX;
    qreal trackRadiusY;
    bool closed;
};

const QList<PerspectiveSpec> perspectiveSpecs{
    {QStringLiteral("ring-platform-blue"),
     QStringLiteral("panel-screenshot-20260811-195427"),
     QStringLiteral("20639a8a6f26462e876f357fa8d60d77705ae9aabcb28e3d2b06b93f94d4cd1a"),
     QStringLiteral("ellipse"),
     QStringLiteral("#58c8f0"),
     QStringLiteral("metallic-blue"),
     QStringLiteral("circular-blue-ring"),
     446.0, 163.0, true},
    {QStringLiteral("octagon-platform-steel"),
     QStringLiteral("panel-screenshot-20260811-200136"),
     QStringLiteral("c35ea87ab7332c36649c9683f5777707fcfe2fc637565f7cf530d5f6ef2366e2"),
     QStringLiteral("polygon"),
     QStringLiteral("#9fb0be"),
     QStringLiteral("dark-orb"),
     QStringLiteral("octagonal-platform"),
     444.0, 162.0, true},
    {QStringLiteral("arc-platform-orange"),
     QStringLiteral("panel-screenshot-20260811-200511"),
     QStringLiteral("4f50a2b8f5998f27d9792edeac96a8603225c0ceee0bea7360937e8b62b62a3e"),
     QStringLiteral("arc"),
     QStringLiteral("#ff8f47"),
     QStringLiteral("neon-orange"),
     QStringLiteral("orange-arc-dock"),
     443.0, 162.0, false},
};

const QStringList productionSvgPaths{
    QStringLiteral("assets/platform-rear.svg"),
    QStringLiteral("assets/platform-front.svg"),
    QStringLiteral("assets/platform-shadow.svg"),
    QStringLiteral("assets/platform-reflection.svg"),
    QStringLiteral("assets/glow-mask.svg"),
    QStringLiteral("masks/input.svg"),
};

QString repositoryFile(const QString &relativePath)
{
    return QFINDTESTDATA(QStringLiteral("../") + relativePath);
}

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return file.open(QIODevice::ReadOnly) && hash.addData(&file)
        ? QString::fromLatin1(hash.result().toHex())
        : QString{};
}

QJsonObject readObject(const QString &path)
{
    return QJsonDocument::fromJson(fileBytes(path)).object();
}

QString packagePath(const QString &themeId, const QString &relative)
{
    return repositoryFile(
        QStringLiteral("assets/themes/") + themeId + QLatin1Char('/') + relative);
}

// The drawn layers composited in manifest order, so alpha claims are made
// about what a viewer actually sees rather than one layer in isolation.
QImage compositeDrawnLayers(const QString &themeId)
{
    QImage canvas(1200, 600, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    for (const QString &relative : {QStringLiteral("assets/platform-shadow.svg"),
                                    QStringLiteral("assets/platform-rear.svg"),
                                    QStringLiteral("assets/platform-reflection.svg"),
                                    QStringLiteral("assets/platform-front.svg")})
    {
        QImage layer(packagePath(themeId, relative));
        if (layer.isNull())
        {
            return {};
        }
        painter.drawImage(0, 0, layer.scaled(1200, 600, Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation));
    }
    painter.end();
    return canvas;
}

}

class Baked25DAssetTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanRoomRecipeMatchesReferenceCatalog();
    void productionPackagesValidateAndShareGeometry();
    void tracksDescribeWhereRealIconsStand();
    void vectorLayersHaveCleanAlphaAndNeutralMasks();
    void theInputMaskMatchesTheDrawnSilhouette();
    void visualApprovalAndOutputHashesAreRecorded();
    void familyIsCataloguedWithRuntimeCapabilities();
};

void Baked25DAssetTest::cleanRoomRecipeMatchesReferenceCatalog()
{
    const QJsonObject recipe = readObject(repositoryFile(
        QStringLiteral("assets/source-samples/perspective/original-artwork-recipe.json")));
    QCOMPARE(recipe.value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.clean-room-artwork-recipe"));
    QCOMPARE(recipe.value(QStringLiteral("task")).toString(),
             QStringLiteral("TASK-0034"));
    QVERIFY(!recipe.value(QStringLiteral("pixelInput")).toBool(true));
    QVERIFY(!recipe.value(QStringLiteral("sourceDerivative")).toBool(true));
    QVERIFY(recipe.value(QStringLiteral("prohibitedElements")).toArray()
                .contains(QJsonValue(QStringLiteral("placeholder-icons"))));

    const QJsonObject catalog = readObject(repositoryFile(
        QStringLiteral("data/source-assets/source-asset-catalog.json")));
    const QJsonArray catalogAssets =
        catalog.value(QStringLiteral("assets")).toArray();

    const QJsonArray references =
        recipe.value(QStringLiteral("inspirationReferences")).toArray();
    QCOMPARE(references.size(), perspectiveSpecs.size());
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        bool seen = false;
        for (const QJsonValue &value : references)
        {
            const QJsonObject reference = value.toObject();
            if (reference.value(QStringLiteral("catalogId")).toString() !=
                spec.referenceId)
            {
                continue;
            }
            seen = true;
            QCOMPARE(reference.value(QStringLiteral("sha256")).toString(),
                     spec.referenceHash);
            QCOMPARE(reference.value(QStringLiteral("variant")).toString(),
                     spec.id);
            QCOMPARE(reference.value(QStringLiteral("permittedUse")).toString(),
                     QStringLiteral("broad-concept-only"));
            QVERIFY(!reference.value(QStringLiteral("pixelInput")).toBool(true));
        }
        QVERIFY2(seen, qPrintable(spec.referenceId));

        // Every reference must still be an unusable screenshot in the source
        // catalog. A production package may never quietly promote one.
        bool found = false;
        for (const QJsonValue &value : catalogAssets)
        {
            const QJsonObject record = value.toObject();
            if (record.value(QStringLiteral("id")).toString() != spec.referenceId)
            {
                continue;
            }
            found = true;
            QCOMPARE(record.value(QStringLiteral("sha256")).toString(),
                     spec.referenceHash);
            QCOMPARE(record.value(QStringLiteral("status")).toString(),
                     QStringLiteral("reference-only"));
            QVERIFY(record.value(QStringLiteral("opaqueScreenshot")).toBool());
            QVERIFY(!record.value(QStringLiteral("isolatedCleanAsset")).toBool(true));
            QCOMPARE(record.value(QStringLiteral("provenance")).toObject()
                         .value(QStringLiteral("redistribution")).toString(),
                     QStringLiteral("unknown"));
        }
        QVERIFY2(found, qPrintable(spec.referenceId));
    }
}

void Baked25DAssetTest::productionPackagesValidateAndShareGeometry()
{
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        const QString manifest = packagePath(
            spec.id, QStringLiteral("archdock-theme.json"));
        QVERIFY2(!manifest.isEmpty(), qPrintable(spec.id));
        const auto loaded = ThemePackage::load(manifest);
        QVERIFY2(loaded.isValid(),
                 qPrintable(spec.id + QLatin1Char(' ') + loaded.primaryCode()));

        const ArchDock::ThemeDefinition &definition = loaded.package->definition();
        QCOMPARE(definition.id, spec.id);
        QCOMPARE(definition.license.spdx, QStringLiteral("NOASSERTION"));
        QCOMPARE(definition.license.redistribution, QStringLiteral("allowed"));
        QCOMPARE(definition.capabilities.hosts,
                 QStringList{QStringLiteral("free-desktop")});
        QCOMPARE(definition.capabilities.orientations,
                 QStringList{QStringLiteral("free")});
        QCOMPARE(definition.capabilities.preferredRendererTier,
                 QStringLiteral("baked2.5d"));
        QCOMPARE(definition.capabilities.fallbackRendererTiers,
                 QStringList{QStringLiteral("procedural2d")});
        QVERIFY(definition.capabilities.features.contains(
            QStringLiteral("dynamic-glow")));
        QVERIFY(definition.capabilities.features.contains(
            QStringLiteral("dynamic-tint")));
        // The radial mechanism is not implemented in this task, so no
        // perspective package may advertise it.
        QCOMPARE(definition.capabilities.presentationMechanisms,
                 QStringList{QStringLiteral("open")});
        QCOMPARE(definition.capabilities.rotation.mode,
                 spec.closed ? QStringLiteral("free") : QStringLiteral("none"));

        QCOMPARE(definition.assets.size(), 8);
        QCOMPARE(definition.states.size(), 4);
        QCOMPARE(definition.layers.size(), 8);
        QCOMPARE(definition.tracks.size(), 1);
        QCOMPARE(definition.inputMasks.size(), 4);
        QCOMPARE(definition.effectMargins.left, 16.0);
        QCOMPARE(definition.effectMargins.top, 16.0);
        QCOMPARE(definition.effectMargins.right, 16.0);
        QCOMPARE(definition.effectMargins.bottom, 16.0);

        // A baked package needs one rear platform and one foreground rim; the
        // rim is what a real icon passes behind.
        int rearLayers = 0;
        int foregroundLayers = 0;
        for (const auto &layer : definition.layers)
        {
            if (layer.role == QStringLiteral("rear"))
            {
                ++rearLayers;
            }
            if (layer.role == QStringLiteral("foreground"))
            {
                ++foregroundLayers;
            }
        }
        QCOMPARE(rearLayers, 1);
        QCOMPARE(foregroundLayers, 1);

        for (const auto &asset : definition.assets)
        {
            QVERIFY2(!asset.sha256.isEmpty(), qPrintable(asset.id));
            QCOMPARE(asset.sha256,
                     fileSha256(packagePath(spec.id, asset.path)));
        }
    }
}

void Baked25DAssetTest::tracksDescribeWhereRealIconsStand()
{
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        const auto loaded = ThemePackage::load(
            packagePath(spec.id, QStringLiteral("archdock-theme.json")));
        QVERIFY(loaded.isValid());
        const auto &track = loaded.package->definition().tracks.constFirst();

        QCOMPARE(track.shape, spec.shape);
        QCOMPARE(track.center.x, 600.0);
        QCOMPARE(track.center.y, 300.0);
        QCOMPARE(track.radiusX, spec.trackRadiusX);
        QCOMPARE(track.radiusY, spec.trackRadiusY);
        // The track sits inside the drawn platform, between its radii.
        QVERIFY(track.radiusX > 0.0 && track.radiusY > 0.0);
        QVERIFY(track.radiusX < 520.0 && track.radiusX > 366.0);

        QCOMPARE(track.depth.farScale, 0.62);
        QCOMPARE(track.depth.nearScale, 1.0);
        QCOMPARE(track.depth.occlusionDepth, 0.62);
        QVERIFY(track.depth.farScale < track.depth.nearScale);

        QVERIFY(track.tilt.has_value());
        QCOMPARE(track.tilt->minimumDegrees, -10.0);
        QCOMPARE(track.tilt->maximumDegrees, 10.0);
        QCOMPARE(track.tilt->defaultDegrees, 0.0);

        if (spec.closed)
        {
            QCOMPARE(track.sweepDegrees, 360.0);
        }
        else
        {
            QCOMPARE(track.startDegrees, 110.0);
            QCOMPARE(track.sweepDegrees, 140.0);
        }
        if (spec.shape == QStringLiteral("polygon"))
        {
            QCOMPARE(track.sides, 8);
        }
    }
}

void Baked25DAssetTest::vectorLayersHaveCleanAlphaAndNeutralMasks()
{
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        for (const QString &relative : productionSvgPaths)
        {
            const QString path = packagePath(spec.id, relative);
            QVERIFY2(!path.isEmpty(), qPrintable(spec.id + relative));
            const QFileInfo info(path);
            QVERIFY2(!info.fileName().startsWith(QStringLiteral("Screenshot_")),
                     qPrintable(path));

            const QByteArray markup = fileBytes(path);
            QVERIFY2(!markup.isEmpty(), qPrintable(path));
            // No text, no embedded raster, no filter, no foreign content: an
            // installed package must be original vector geometry only.
            QVERIFY2(!markup.contains("<text"), qPrintable(path));
            QVERIFY2(!markup.contains("<image"), qPrintable(path));
            QVERIFY2(!markup.contains("data:image"), qPrintable(path));
            QVERIFY2(!markup.contains("<filter"), qPrintable(path));
            QVERIFY2(!markup.contains("foreignobject"), qPrintable(path));

            const QImage image(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            QCOMPARE(image.size(), QSize(1200, 600));
            QVERIFY(image.hasAlphaChannel());
            QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
            QCOMPARE(qAlpha(image.pixel(1199, 599)), 0);

            // A tintable mask must be colour-neutral, or dynamic tint would
            // fight the artwork's own colour instead of replacing it.
            if (relative.contains(QStringLiteral("glow-mask"))
                || relative.contains(QStringLiteral("input.svg")))
            {
                for (int y = 0; y < image.height(); y += 40)
                {
                    for (int x = 0; x < image.width(); x += 40)
                    {
                        const QRgb pixel = image.pixel(x, y);
                        if (qAlpha(pixel) == 0)
                        {
                            continue;
                        }
                        QVERIFY2(qRed(pixel) == qGreen(pixel)
                                     && qGreen(pixel) == qBlue(pixel),
                                 qPrintable(path));
                    }
                }
            }
        }

        const QImage drawn = compositeDrawnLayers(spec.id);
        QVERIFY2(!drawn.isNull(), qPrintable(spec.id));
        // The platform is drawn, its exterior is not, and a closed family
        // leaves its middle transparent so the desktop shows through.
        QCOMPARE(qAlpha(drawn.pixel(2, 2)), 0);
        QCOMPARE(qAlpha(drawn.pixel(1197, 2)), 0);
        if (spec.closed)
        {
            QCOMPARE(qAlpha(drawn.pixel(600, 300)), 0);
            QVERIFY(qAlpha(drawn.pixel(600, 115)) > 200);
        }
        QVERIFY(qAlpha(drawn.pixel(600, 462)) > 200);
    }
}

void Baked25DAssetTest::theInputMaskMatchesTheDrawnSilhouette()
{
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        const QImage drawn = compositeDrawnLayers(spec.id);
        QVERIFY(!drawn.isNull());
        const QImage mask(packagePath(spec.id, QStringLiteral("masks/input.svg")));
        QVERIFY(!mask.isNull());
        QCOMPARE(mask.size(), drawn.size());

        // Every pixel the package paints must be inside its input mask.
        // Otherwise the visible edge of the platform would pass clicks
        // through to the desktop behind it.
        int uncovered = 0;
        for (int y = 0; y < drawn.height(); y += 3)
        {
            for (int x = 0; x < drawn.width(); x += 3)
            {
                if (qAlpha(drawn.pixel(x, y)) > 160
                    && qAlpha(mask.pixel(x, y)) < 96)
                {
                    ++uncovered;
                }
            }
        }
        QVERIFY2(uncovered == 0,
                 qPrintable(spec.id + QStringLiteral(" uncovered=")
                            + QString::number(uncovered)));
    }
}

void Baked25DAssetTest::visualApprovalAndOutputHashesAreRecorded()
{
    QSet<QString> contactSheets;
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        const QJsonObject production = readObject(packagePath(
            spec.id, QStringLiteral("metadata/production-record.json")));
        QCOMPARE(production.value(QStringLiteral("themeId")).toString(), spec.id);
        QCOMPARE(production.value(QStringLiteral("task")).toString(),
                 QStringLiteral("TASK-0034"));
        QVERIFY(!production.value(QStringLiteral("pixelInput")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("sourceDerivative")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("parameters")).toObject()
                     .value(QStringLiteral("placeholderIcons")).toBool(true));

        const QJsonArray excluded =
            production.value(QStringLiteral("excludedReferences")).toArray();
        QCOMPARE(excluded.size(), 1);
        QCOMPARE(excluded.first().toObject()
                     .value(QStringLiteral("sha256")).toString(),
                 spec.referenceHash);
        QVERIFY(!excluded.first().toObject()
                     .value(QStringLiteral("pixelInput")).toBool(true));

        const QJsonArray outputs =
            production.value(QStringLiteral("outputs")).toArray();
        QCOMPARE(outputs.size(), productionSvgPaths.size());
        for (const QJsonValue &value : outputs)
        {
            const QJsonObject output = value.toObject();
            const QString relative =
                output.value(QStringLiteral("path")).toString();
            QVERIFY2(productionSvgPaths.contains(relative), qPrintable(relative));
            QCOMPARE(output.value(QStringLiteral("sha256")).toString(),
                     fileSha256(packagePath(spec.id, relative)));
        }

        const QJsonObject review = readObject(packagePath(
            spec.id, QStringLiteral("metadata/visual-review.json")));
        QCOMPARE(review.value(QStringLiteral("themeId")).toString(), spec.id);
        QCOMPARE(review.value(QStringLiteral("status")).toString(),
                 QStringLiteral("production-approved"));
        const QJsonObject checks =
            review.value(QStringLiteral("checks")).toObject();
        QCOMPARE(checks.value(QStringLiteral("transparentExterior")).toString(),
                 QStringLiteral("pass"));
        QCOMPARE(checks.value(QStringLiteral("placeholderIcons")).toString(),
                 QStringLiteral("absent"));
        QCOMPARE(checks.value(QStringLiteral("sourcePixels")).toString(),
                 QStringLiteral("absent"));
        QCOMPARE(checks.value(
                     QStringLiteral("inputMaskMatchesDrawnSilhouette")).toString(),
                 QStringLiteral("pass"));
        QCOMPARE(review.value(QStringLiteral("contactSheetSizes")).toArray().size(),
                 4);
        contactSheets.insert(
            review.value(QStringLiteral("contactSheetSha256")).toString());
    }
    // One review surface covered all three families at four sizes.
    QCOMPARE(contactSheets.size(), 1);
    QVERIFY(!contactSheets.constBegin()->isEmpty());
}

void Baked25DAssetTest::familyIsCataloguedWithRuntimeCapabilities()
{
    const QJsonObject catalog = readObject(
        repositoryFile(QStringLiteral("data/themes/builtin-themes.json")));
    const QJsonArray themes = catalog.value(QStringLiteral("themes")).toArray();
    QCOMPARE(themes.size(), 15);

    int observed = 0;
    for (const PerspectiveSpec &spec : perspectiveSpecs)
    {
        const QJsonObject package = readObject(packagePath(
            spec.id, QStringLiteral("archdock-theme.json")));
        const QJsonObject packageCapabilities =
            package.value(QStringLiteral("capabilities")).toObject();

        for (const QJsonValue &value : themes)
        {
            const QJsonObject theme = value.toObject();
            if (theme.value(QStringLiteral("id")).toString() != spec.id)
            {
                continue;
            }
            ++observed;
            QVERIFY(theme.value(QStringLiteral("builtIn")).toBool());
            QCOMPARE(theme.value(QStringLiteral("version")).toInt(), 2);
            QCOMPARE(theme.value(QStringLiteral("category")).toString(),
                     QStringLiteral("perspective"));
            QCOMPARE(theme.value(QStringLiteral("packageManifest")).toString(),
                     spec.id + QStringLiteral("/archdock-theme.json"));
            // The catalog must state exactly what the package states, or the
            // registry rejects it as a capability mismatch at load time.
            QCOMPARE(theme.value(QStringLiteral("capabilities")).toObject(),
                     packageCapabilities);
            QCOMPARE(theme.value(QStringLiteral("iconStyleRef")).toObject()
                         .value(QStringLiteral("id")).toString(),
                     spec.iconStyleId);

            const QJsonObject panelStyle =
                theme.value(QStringLiteral("panelStyle")).toObject();
            QCOMPARE(panelStyle.value(QStringLiteral("rendererTier")).toString(),
                     QStringLiteral("baked2.5d"));
            QCOMPARE(panelStyle.value(QStringLiteral("color")).toString(),
                     spec.tint);

            const QJsonObject preview =
                theme.value(QStringLiteral("previewConfiguration")).toObject();
            QVERIFY(preview.value(QStringLiteral("active")).toBool());
            QCOMPARE(preview.value(QStringLiteral("mode")).toString(),
                     QStringLiteral("free"));

            // Stable preset lineage and a declared safe fallback, so the
            // built-in Panel Preset catalog can reference these families.
            const QJsonObject intent =
                theme.value(QStringLiteral("presetIntent")).toObject();
            QCOMPARE(intent.value(QStringLiteral("panelPresetId")).toString(),
                     spec.presetIntent);
            QCOMPARE(intent.value(
                         QStringLiteral("fallbackRendererTier")).toString(),
                     QStringLiteral("procedural2d"));
            QVERIFY(!intent.value(
                QStringLiteral("fallbackThemeId")).toString().isEmpty());
        }
    }
    QCOMPARE(observed, perspectiveSpecs.size());
}

QTEST_MAIN(Baked25DAssetTest)

#include "Baked25DAssetTest.moc"
