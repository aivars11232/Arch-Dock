#include "model/PanelDefinition.h"
#include "model/PanelRuntimeState.h"
#include "model/PanelSettingsSchema.h"
#include "model/SettingsMigration.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <limits>

using ArchDock::PanelContent;
using ArchDock::PanelDefinition;
using ArchDock::PanelHostKind;
using ArchDock::PanelSettingsSchema;
using ArchDock::PanelPresetOrigin;
using ArchDock::PanelMigrationStatus;
using ArchDock::PanelRuntimeState;
using ArchDock::PanelTransitionState;
using ArchDock::SettingsMigration;

namespace
{

QJsonObject fixtureRecord(const QString &fileName)
{
    const QString path = QFINDTESTDATA(
        QStringLiteral("fixtures/panel-migration/") + fileName);
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QByteArray fixtureArray(const QStringList &fileNames)
{
    QJsonArray records;
    for (const QString &fileName : fileNames)
    {
        const QJsonObject record = fixtureRecord(fileName);
        if (record.isEmpty())
        {
            return {};
        }
        records.append(record);
    }
    return QJsonDocument(records).toJson(QJsonDocument::Compact);
}

}

class PanelModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void statusIdentitiesAreReservedAndPreferencesPersist()
    {
        auto panel = PanelDefinition::defaults("content-status", "Status", "free", false);
        panel.content.showBadges = false;
        panel.content.showProgress = false;
        panel.content.showTemporaryStatus = false;
        panel.segments.first().source = "status";
        panel.segments.first().entryIds = {"status:cpu", "status:memory"};
        const auto restored = PanelDefinition::fromLegacyMap(panel.toPersistedMap());
        QVERIFY(restored.has_value());
        QVERIFY(restored->content == panel.content);
        QVERIFY(restored->segments == panel.segments);
        panel.segments.first().entryIds = {"foreign.desktop"};
        QVERIFY(!panel.isValid());
        panel.segments.first().entryIds = {"status:unknown"};
        QVERIFY(!panel.isValid());
        panel.segments.first().source = "custom";
        panel.segments.first().entryIds = {"status:cpu"};
        QVERIFY(!panel.isValid());
    }
    void defaultsExposeEveryVersionTwoSection();
    void legacyRecordConvertsWithDeterministicDefaults();
    void contentOrderIsCanonicalAndDerived();
    void settingsRevisionRoundTripsWithoutSchemaBump();
    void currentFieldsAndExtensionsRoundTripWithoutLoss();
    void iconStyleSelectionOwnsItsLegacyMirror();
    void iconOverridesRoundTripAsTypedRecords();
    void invalidIconOverridesFailClosed();
    void provenanceIsOptionalAndSelfContained();
    void runtimeStateStartsFromSafeDefaults();
    void serializationExcludesTransientRuntimeState();
    void legacyFixturesMigrateWithoutDataLoss();
    void migrationIsIdempotent();
    void existingPanelsAcquireOneInheritedSegment();
    void corruptAndUnsupportedSourcesFailSafely();
    void invalidRecordsAreRejected();
    void segmentsRoundTripWithIndependentSettings();
    void invalidSegmentsFailClosed();
};

void PanelModelTest::defaultsExposeEveryVersionTwoSection()
{
    const PanelDefinition definition = PanelDefinition::defaults(
        QStringLiteral("free-1"),
        QStringLiteral("Free panel 1"),
        QStringLiteral("free"),
        false);

    QCOMPARE(definition.schemaVersion, PanelDefinition::CurrentSchemaVersion);
    QCOMPARE(definition.identity.id, QStringLiteral("free-1"));
    QCOMPARE(definition.identity.name, QStringLiteral("Free panel 1"));
    QVERIFY(!definition.identity.builtIn);
    QCOMPARE(definition.host.kind, PanelHostKind::FreeDesktop);
    QCOMPARE(definition.content.type, QStringLiteral("empty"));
    QCOMPARE(definition.placement.edge, QStringLiteral("free"));
    QCOMPARE(definition.placement.width, 420);
    QCOMPARE(definition.placement.height, 420);
    QVERIFY(definition.visibility.visible);
    QCOMPARE(definition.visibility.hostMode, QStringLiteral("always"));
    // Presentation carries real defaults now that a motion engine can honour
    // them. Pinning the values is stronger than the previous emptiness check:
    // an empty mechanism would leave a collapsed panel undrawable.
    QCOMPARE(definition.presentation.mode, QStringLiteral("open"));
    QCOMPARE(definition.presentation.trigger, QStringLiteral("hover"));
    QCOMPARE(definition.presentation.collapseMechanism, QStringLiteral("open"));
    QCOMPARE(definition.presentation.collapseAxis, QStringLiteral("horizontal"));
    QCOMPARE(definition.presentation.revealHandle, QStringLiteral("edge-strip"));
    QCOMPARE(definition.layout.pathType, QStringLiteral("circular"));
    QCOMPARE(definition.surface.appearance, QStringLiteral("glass"));
    QCOMPARE(definition.surface.glowIntensity, 1.0);
    QCOMPARE(definition.iconStyle.shape, QStringLiteral("rounded"));
    QCOMPARE(definition.iconStyle.styleReference,
             QStringLiteral("plain-original"));
    QCOMPARE(definition.toLegacyMap().value(QStringLiteral("iconThemeId")).toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(definition.motion.iconProfile, QStringLiteral("scale"));
    QVERIFY(!definition.presetOrigin.has_value());
    QVERIFY(definition.extensions.isEmpty());

    QString errorMessage;
    QVERIFY2(definition.isValid(&errorMessage), qPrintable(errorMessage));
}

void PanelModelTest::iconStyleSelectionOwnsItsLegacyMirror()
{
    const QVariantMap legacyOnly{
        {QStringLiteral("id"), QStringLiteral("legacy-icons")},
        {QStringLiteral("name"), QStringLiteral("Legacy icons")},
        {QStringLiteral("edge"), QStringLiteral("bottom")},
        {QStringLiteral("iconThemeId"), QStringLiteral("metallic-blue")},
    };
    const auto migrated = PanelDefinition::fromLegacyMap(legacyOnly);
    QVERIFY(migrated.has_value());
    QCOMPARE(migrated->iconStyle.styleReference,
             QStringLiteral("metallic-blue"));
    QCOMPARE(migrated->iconStyle.themeId, QStringLiteral("metallic-blue"));

    QVariantMap explicitSelection = legacyOnly;
    explicitSelection.insert(QStringLiteral("iconStyle"),
                             QStringLiteral("neon-green"));
    explicitSelection.insert(QStringLiteral("iconThemeId"),
                             QStringLiteral("obsolete-panel-theme"));
    const auto explicitDefinition = PanelDefinition::fromLegacyMap(
        explicitSelection);
    QVERIFY(explicitDefinition.has_value());
    QCOMPARE(explicitDefinition->iconStyle.styleReference,
             QStringLiteral("neon-green"));
    QCOMPARE(explicitDefinition->iconStyle.themeId,
             QStringLiteral("neon-green"));
    const QVariantMap persisted = explicitDefinition->toPersistedMap();
    QCOMPARE(persisted.value(QStringLiteral("iconStyle")).toString(),
             QStringLiteral("neon-green"));
    QCOMPARE(persisted.value(QStringLiteral("iconThemeId")).toString(),
             QStringLiteral("neon-green"));
}

void PanelModelTest::iconOverridesRoundTripAsTypedRecords()
{
    const QVariantMap firstOverride{
        {QStringLiteral("customGlyph"), QStringLiteral("file:///tmp/custom.svg")},
        {QStringLiteral("customLabel"), QStringLiteral("Custom editor")},
        {QStringLiteral("tileEnabled"), false},
        {QStringLiteral("styleReference"), QStringLiteral("metallic-red")},
        {QStringLiteral("animationProfileReference"),
         QStringLiteral("future-bounce")},
    };
    const QVariantMap secondOverride{
        {QStringLiteral("styleReference"), QStringLiteral("dark-orb")},
    };
    const QVariantMap record{
        {QStringLiteral("id"), QStringLiteral("typed-overrides")},
        {QStringLiteral("name"), QStringLiteral("Typed overrides")},
        {QStringLiteral("edge"), QStringLiteral("bottom")},
        {QStringLiteral("iconOverrides"), QVariantMap{
            {QStringLiteral("desktop.org.example.editor.desktop"),
             firstOverride},
            {QStringLiteral("application.org.example.helper"),
             secondOverride},
        }},
    };

    const auto definition = PanelDefinition::fromLegacyMap(record);
    QVERIFY(definition.has_value());
    QCOMPARE(definition->iconStyle.perEntryOverrides.size(), 2);
    const auto first = definition->iconStyle.perEntryOverrides.value(
        QStringLiteral("desktop.org.example.editor.desktop"));
    QCOMPARE(first.customGlyph, QStringLiteral("file:///tmp/custom.svg"));
    QCOMPARE(first.customLabel, QStringLiteral("Custom editor"));
    QVERIFY(first.tileEnabled.has_value());
    QVERIFY(!*first.tileEnabled);
    QCOMPARE(first.styleReference, QStringLiteral("metallic-red"));
    QCOMPARE(first.animationProfileReference,
             QStringLiteral("future-bounce"));

    const QVariantMap persistedOverrides = definition->toPersistedMap()
        .value(QStringLiteral("iconOverrides")).toMap();
    QCOMPARE(persistedOverrides.value(
                 QStringLiteral("desktop.org.example.editor.desktop")).toMap(),
             firstOverride);
    const auto reparsed = PanelDefinition::fromLegacyMap(
        definition->toPersistedMap());
    QVERIFY(reparsed.has_value());
    QCOMPARE(*reparsed, *definition);
}

void PanelModelTest::invalidIconOverridesFailClosed()
{
    const auto reject = [](const QString &identity,
                           const QVariant &overrideValue,
                           const QString &expectedError)
    {
        const QVariantMap record{
            {QStringLiteral("id"), QStringLiteral("invalid-overrides")},
            {QStringLiteral("name"), QStringLiteral("Invalid overrides")},
            {QStringLiteral("edge"), QStringLiteral("bottom")},
            {QStringLiteral("iconOverrides"),
             QVariantMap{{identity, overrideValue}}},
        };
        QString errorMessage;
        QVERIFY(!PanelDefinition::fromLegacyMap(
            record, &errorMessage).has_value());
        QVERIFY2(errorMessage.contains(expectedError), qPrintable(errorMessage));
    };

    reject(QStringLiteral("raw unsafe key"),
           QVariantMap{{QStringLiteral("customLabel"), QStringLiteral("x")}},
           QStringLiteral("invalid icon override identity"));
    reject(QStringLiteral("application.example"),
           QVariantMap{{QStringLiteral("tileEnabled"), QStringLiteral("false")}},
           QStringLiteral("must be a boolean"));
    reject(QStringLiteral("application.example"),
           QVariantMap{{QStringLiteral("unknown"), true}},
           QStringLiteral("unknown icon override field"));
}

void PanelModelTest::legacyRecordConvertsWithDeterministicDefaults()
{
    const QVariantMap legacy{
        {QStringLiteral("id"), QStringLiteral("side")},
        {QStringLiteral("name"), QStringLiteral("Side panel")},
        {QStringLiteral("builtIn"), true},
        {QStringLiteral("edge"), QStringLiteral("RIGHT")},
        {QStringLiteral("iconSize"), 999},
        {QStringLiteral("layoutScale"), 9.0},
        {QStringLiteral("contentAppIds"),
         QStringList{QStringLiteral("org.kde.kate"),
                     QStringLiteral(" org.kde.kate "),
                     QString{}}},
    };

    QString firstError;
    const auto first = PanelDefinition::fromLegacyMap(legacy, &firstError);
    QVERIFY2(first.has_value(), qPrintable(firstError));
    const auto second = PanelDefinition::fromLegacyMap(legacy);
    QVERIFY(second.has_value());
    QCOMPARE(*first, *second);
    QCOMPARE(first->schemaVersion, PanelDefinition::CurrentSchemaVersion);
    QCOMPARE(first->placement.edge, QStringLiteral("right"));
    QCOMPARE(first->host.kind, PanelHostKind::NativeEdge);
    QCOMPARE(first->placement.alignment, QStringLiteral("center"));
    QCOMPARE(first->host.screenIndex, 0);
    QCOMPARE(first->iconStyle.size, 128);
    QCOMPARE(first->layout.scale, 2.5);
    QCOMPARE(
        first->content.applicationIds,
        QStringList{QStringLiteral("org.kde.kate")});
    QCOMPARE(first->normalized(), *first);
}

// TASK-0033 Phase A: a free panel's content is one ordered list across its
// application and URL entries. Legacy records carry no order, so it is derived
// deterministically; a stored order that names unknown or repeated ids is
// repaired rather than trusted.
void PanelModelTest::contentOrderIsCanonicalAndDerived()
{
    const QString folder = PanelContent::urlEntryId(QStringLiteral("file:///tmp/Folder/"));
    const QString example = PanelContent::urlEntryId(
        QStringLiteral("file:///tmp/example.desktop"));
    const QVariantMap legacy{
        {QStringLiteral("id"), QStringLiteral("free-4")},
        {QStringLiteral("edge"), QStringLiteral("free")},
        {QStringLiteral("type"), QStringLiteral("launcher")},
        {QStringLiteral("contentAppIds"), QStringList{QStringLiteral("org.kde.kate")}},
        {QStringLiteral("contentUrls"),
         QStringList{QStringLiteral("file:///tmp/example.desktop"),
                     QStringLiteral("file:///tmp/Folder/")}},
    };

    QString errorMessage;
    const auto derived = PanelDefinition::fromLegacyMap(legacy, &errorMessage);
    QVERIFY2(derived.has_value(), qPrintable(errorMessage));
    QCOMPARE(derived->content.entryOrder,
             QStringList({QStringLiteral("org.kde.kate"), example, folder}));
    QCOMPARE(derived->toLegacyMap().value(QStringLiteral("contentOrder")).toStringList(),
             derived->content.entryOrder);

    QVariantMap reordered = legacy;
    reordered.insert(QStringLiteral("contentOrder"),
                     QStringList{folder, QStringLiteral("bogus"), folder,
                                 QStringLiteral("org.kde.kate")});
    const auto repaired = PanelDefinition::fromLegacyMap(reordered, &errorMessage);
    QVERIFY2(repaired.has_value(), qPrintable(errorMessage));
    QCOMPARE(repaired->content.entryOrder,
             QStringList({folder, QStringLiteral("org.kde.kate"), example}));
    QCOMPARE(repaired->normalized(), *repaired);
    QVERIFY(PanelSettingsSchema::isKnownPanelField(QStringLiteral("contentOrder")));

    // TASK-0033 Phase C: rotation is configuration; its running angle is not.
    QVariantMap rotating = legacy;
    rotating.insert(QStringLiteral("panelRotationMode"), QStringLiteral("Clockwise"));
    rotating.insert(QStringLiteral("panelRotationSpeed"), 999);
    rotating.insert(QStringLiteral("panelRotationTrigger"), QStringLiteral("whenever"));
    rotating.insert(QStringLiteral("sceneRotation"), 123.0);
    const auto turned = PanelDefinition::fromLegacyMap(rotating, &errorMessage);
    QVERIFY2(turned.has_value(), qPrintable(errorMessage));
    QCOMPARE(turned->layout.rotationMode, QStringLiteral("clockwise"));
    QCOMPARE(turned->layout.rotationSpeed, 180.0);
    QCOMPARE(turned->layout.rotationTrigger, QStringLiteral("idle"));
    QVERIFY(!turned->toPersistedMap().contains(QStringLiteral("sceneRotation")));
    QVERIFY(PanelRuntimeState::isTransientLegacyKey(QStringLiteral("sceneRotation")));
    QCOMPARE(derived->layout.rotationMode, QStringLiteral("none"));
    QCOMPARE(derived->layout.rotationSpeed, 12.0);

    // ADREP-TASK-002 (PD-25): a record saved before "Continuous motion
    // moves" existed keeps the motion it had - one that rotated turns as a
    // whole, any other moves its items - and a saved choice is kept. How far
    // the items have travelled is never stored.
    QVERIFY(!legacy.contains(QStringLiteral("panelMotionTarget")));
    QCOMPARE(turned->layout.motionTarget, QStringLiteral("panel"));
    QCOMPARE(derived->layout.motionTarget, QStringLiteral("items"));
    QVariantMap counter = legacy;
    counter.insert(QStringLiteral("panelRotationMode"), QStringLiteral("counter-clockwise"));
    QCOMPARE(PanelDefinition::fromLegacyMap(counter, &errorMessage)->layout.motionTarget,
             QStringLiteral("panel"));
    QVariantMap chosen = rotating;
    chosen.insert(QStringLiteral("panelMotionTarget"), QStringLiteral("Items"));
    chosen.insert(QStringLiteral("panelTravelSpeed"), 99);
    chosen.insert(QStringLiteral("scrollSensitivity"), 0.01);
    const auto kept = PanelDefinition::fromLegacyMap(chosen, &errorMessage);
    QVERIFY2(kept.has_value(), qPrintable(errorMessage));
    QCOMPARE(kept->layout.motionTarget, QStringLiteral("items"));
    QCOMPARE(kept->layout.travelSpeed, 5.0);
    QCOMPARE(kept->layout.scrollSensitivity, 0.25);
    const QVariantMap persisted = kept->toPersistedMap();
    QCOMPARE(persisted.value(QStringLiteral("panelMotionTarget")).toString(), QStringLiteral("items"));
    for (auto it = persisted.cbegin(); it != persisted.cend(); ++it)
        QVERIFY2(!it.key().contains(QStringLiteral("ravel")) || it.key() == QStringLiteral("panelTravelSpeed"),
                 qPrintable(it.key()));
    QCOMPARE(PanelDefinition::fromLegacyMap(persisted, &errorMessage)->layout, kept->layout);
    QCOMPARE(derived->layout.travelSpeed, 0.5);
    QCOMPARE(derived->layout.scrollSensitivity, 1.0);
}

void PanelModelTest::settingsRevisionRoundTripsWithoutSchemaBump()
{
    const QVariantMap oldVersionTwoRecord{
        {QStringLiteral("schemaVersion"), PanelDefinition::CurrentSchemaVersion},
        {QStringLiteral("id"), QStringLiteral("bottom")},
        {QStringLiteral("name"), QStringLiteral("Bottom panel")},
        {QStringLiteral("edge"), QStringLiteral("bottom")},
    };
    const auto oldRecord = PanelDefinition::fromLegacyMap(oldVersionTwoRecord);
    QVERIFY(oldRecord.has_value());
    QCOMPARE(oldRecord->settingsRevision, quint64{0});

    PanelDefinition revised = *oldRecord;
    revised.settingsRevision = std::numeric_limits<quint64>::max();
    const QVariantMap persisted = revised.toPersistedMap();
    QCOMPARE(persisted.value(QStringLiteral("schemaVersion")).toInt(),
             PanelDefinition::CurrentSchemaVersion);
    QCOMPARE(persisted.value(QStringLiteral("settingsRevision")).toString(),
             QStringLiteral("18446744073709551615"));
    const auto reparsed = PanelDefinition::fromLegacyMap(persisted);
    QVERIFY(reparsed.has_value());
    QCOMPARE(reparsed->settingsRevision, revised.settingsRevision);

    QVariantMap malformed = oldVersionTwoRecord;
    malformed.insert(QStringLiteral("settingsRevision"), QStringLiteral("not-a-number"));
    QString errorMessage;
    QVERIFY(!PanelDefinition::fromLegacyMap(malformed, &errorMessage).has_value());
    QCOMPARE(errorMessage, QStringLiteral("panel settings revision is invalid"));
}

void PanelModelTest::currentFieldsAndExtensionsRoundTripWithoutLoss()
{
    const QVariantMap legacy{
        {QStringLiteral("id"), QStringLiteral("native-custom")},
        {QStringLiteral("name"), QStringLiteral("Native custom")},
        {QStringLiteral("builtIn"), false},
        {QStringLiteral("edge"), QStringLiteral("left")},
        {QStringLiteral("alignment"), QStringLiteral("end")},
        {QStringLiteral("screen"), 2},
        {QStringLiteral("screenId"), QStringLiteral("DP-2")},
        {QStringLiteral("visible"), true},
        {QStringLiteral("visibilityMode"), QStringLiteral("dodge")},
        {QStringLiteral("revealZone"), 12},
        {QStringLiteral("dynamic"), true},
        {QStringLiteral("width"), 84},
        {QStringLiteral("height"), 800},
        {QStringLiteral("x"), 21},
        {QStringLiteral("y"), 34},
        {QStringLiteral("floatingMargin"), 0},
        {QStringLiteral("type"), QStringLiteral("hybrid")},
        {QStringLiteral("contentAppIds"),
         QStringList{QStringLiteral("org.kde.kate"), QStringLiteral("org.kde.konsole")}},
        {QStringLiteral("contentUrls"),
         QStringList{QStringLiteral("file:///tmp/example.desktop")}},
        {QStringLiteral("kdeWidgets"), QStringList{QStringLiteral("org.kde.plasma.clock")}},
        {QStringLiteral("acceptDrops"), false},
        {QStringLiteral("folderLayout"), QStringLiteral("grid")},
        {QStringLiteral("folderSpeed"), 310},
        {QStringLiteral("folderEasing"), QStringLiteral("outCubic")},
        {QStringLiteral("folderExpandOnClick"), false},
        {QStringLiteral("layout"), QStringLiteral("vertical")},
        {QStringLiteral("layoutScale"), 1.2},
        {QStringLiteral("layoutAngle"), -12.0},
        {QStringLiteral("layoutRadius"), 220},
        {QStringLiteral("layoutRows"), 3},
        {QStringLiteral("layoutPadding"), 24},
        {QStringLiteral("pathSides"), 7},
        {QStringLiteral("pathOrientation"), QStringLiteral("tangent")},
        {QStringLiteral("pathAnchor"), QStringLiteral("bottom-right")},
        {QStringLiteral("appearance"), QStringLiteral("metallic")},
        {QStringLiteral("shape"), QStringLiteral("rounded")},
        {QStringLiteral("opacity"), 0.82},
        {QStringLiteral("color"), QStringLiteral("#112233")},
        {QStringLiteral("glowIntensity"), 1.35},
        {QStringLiteral("panelThemeId"), QStringLiteral("metallic-shelf")},
        {QStringLiteral("completeThemeId"), QStringLiteral("metallic-shelf")},
        {QStringLiteral("themeAsset"), QStringLiteral("/tmp/surface.png")},
        {QStringLiteral("themeSource"), QStringLiteral("file:///tmp/source.svg")},
        {QStringLiteral("themeFit"), QStringLiteral("contain")},
        {QStringLiteral("themeSourceKind"), QStringLiteral("vector")},
        {QStringLiteral("themeSourceFormat"), QStringLiteral("svg")},
        {QStringLiteral("themeSourceWidth"), 640},
        {QStringLiteral("themeSourceHeight"), 96},
        {QStringLiteral("themeSourceHasAlpha"), true},
        {QStringLiteral("themeSuggestedFit"), QStringLiteral("contain")},
        {QStringLiteral("themePreview"), QStringLiteral("/tmp/preview.png")},
        {QStringLiteral("themeAnalysisStatus"), QStringLiteral("Ready")},
        {QStringLiteral("themeConversionTool"), QStringLiteral("convert")},
        {QStringLiteral("themeConversionAvailable"), true},
        {QStringLiteral("themePackageFormat"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("themePackageVersion"), 1},
        {QStringLiteral("themePackageId"), QStringLiteral("custom-theme")},
        {QStringLiteral("themePackageName"), QStringLiteral("Custom theme")},
        {QStringLiteral("themePackageAuthor"), QStringLiteral("Tester")},
        {QStringLiteral("themePackageManifest"), QStringLiteral("/tmp/manifest.json")},
        {QStringLiteral("themeStatus"), QStringLiteral("Rendered")},
        {QStringLiteral("themeRenderWidth"), 800},
        {QStringLiteral("themeRenderHeight"), 84},
        {QStringLiteral("themeRenderFit"), QStringLiteral("contain")},
        {QStringLiteral("themeRenderOutcome"), QStringLiteral("success")},
        {QStringLiteral("iconThemeId"), QStringLiteral("metallic-shelf")},
        {QStringLiteral("iconShape"), QStringLiteral("circle")},
        {QStringLiteral("iconSize"), 56},
        {QStringLiteral("spacing"), 9.0},
        {QStringLiteral("iconAnimation"), QStringLiteral("bounce")},
        {QStringLiteral("animationTrigger"), QStringLiteral("launch")},
        {QStringLiteral("animationSpeed"), 0.9},
        {QStringLiteral("animationIntensity"), 0.8},
        {QStringLiteral("physicsEnabled"), true},
        {QStringLiteral("nativePanelId"), 41},
        {QStringLiteral("nativeControlAppletId"), 42},
        {QStringLiteral("nativeDockAppletId"), 43},
        {QStringLiteral("nativeOwnershipToken"), QStringLiteral("native-token")},
        {QStringLiteral("nativeRecoveryState"), QStringLiteral("verified")},
        {QStringLiteral("nativeRecoveryError"), QString{}},
        {QStringLiteral("legacyExtensionData"), QStringLiteral("keep-me")},
    };

    QString errorMessage;
    const auto parsed = PanelDefinition::fromLegacyMap(legacy, &errorMessage);
    QVERIFY2(parsed.has_value(), qPrintable(errorMessage));
    const QVariantMap flat = parsed->toLegacyMap();
    for (auto it = legacy.cbegin(); it != legacy.cend(); ++it)
    {
        QCOMPARE(flat.value(it.key()), it.value());
    }

    QCOMPARE(
        parsed->extensions.value(QStringLiteral("legacyExtensionData")).toString(),
        QStringLiteral("keep-me"));
    const QVariantMap persisted = parsed->toPersistedMap();
    QVERIFY(!persisted.contains(QStringLiteral("legacyExtensionData")));
    QCOMPARE(
        persisted.value(QStringLiteral("extensions")).toMap().value(
            QStringLiteral("legacyExtensionData")).toString(),
        QStringLiteral("keep-me"));
    QCOMPARE(
        persisted.value(QStringLiteral("schemaVersion")).toInt(),
        PanelDefinition::CurrentSchemaVersion);
}

void PanelModelTest::provenanceIsOptionalAndSelfContained()
{
    PanelDefinition definition = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom panel"),
        QStringLiteral("bottom"),
        true);
    QVERIFY(!definition.toPersistedMap().contains(QStringLiteral("presetOrigin")));

    definition.presetOrigin = PanelPresetOrigin{
        .panelPresetId = QStringLiteral("builtin.glass"),
        .panelPresetRevision = 4,
        .iconPresetId = QStringLiteral("builtin.rounded"),
        .iconPresetRevision = 2,
        .customizedAfterApply = true,
        .detachedFromPreset = false,
    };

    const QVariantMap persisted = definition.toPersistedMap();
    const QVariantMap origin = persisted.value(QStringLiteral("presetOrigin")).toMap();
    QCOMPARE(origin.value(QStringLiteral("panelPresetId")).toString(),
             QStringLiteral("builtin.glass"));
    QCOMPARE(origin.value(QStringLiteral("panelPresetRevision")).toInt(), 4);
    QCOMPARE(origin.value(QStringLiteral("iconPresetId")).toString(),
             QStringLiteral("builtin.rounded"));
    QCOMPARE(origin.value(QStringLiteral("iconPresetRevision")).toInt(), 2);
    QVERIFY(origin.value(QStringLiteral("customizedAfterApply")).toBool());

    const auto reparsed = PanelDefinition::fromLegacyMap(persisted);
    QVERIFY(reparsed.has_value());
    QCOMPARE(reparsed->presetOrigin, definition.presetOrigin);
}

void PanelModelTest::runtimeStateStartsFromSafeDefaults()
{
    const PanelRuntimeState first = PanelRuntimeState::defaults();
    const PanelRuntimeState restarted = PanelRuntimeState::defaults();
    QCOMPARE(first, restarted);
    QVERIFY(!first.hovered);
    QCOMPARE(first.hoveredEntry, -1);
    QVERIFY(!first.editMode);
    QVERIFY(!first.popupOpen);
    QVERIFY(!first.dragInProgress);
    QCOMPARE(first.transition, PanelTransitionState::Idle);
    QCOMPARE(first.surfaceState, ArchDock::PanelSurfaceState::Open);
    QVERIFY(!first.hostConcealed);
    QVERIFY(!first.windowOverlap);
    QVERIFY(first.rendererFallback.isEmpty());
    QCOMPARE(first.frameQuality, QStringLiteral("normal"));
    QCOMPARE(first.presentationProgress, -1.0);
    QVERIFY(first.currentScreenGeometry.isNull());

    const QVariantMap runtime = first.toRuntimeMap();
    QCOMPARE(runtime.value(QStringLiteral("transitionState")).toString(),
             QStringLiteral("idle"));
    QCOMPARE(runtime.value(QStringLiteral("hoveredEntry")).toInt(), -1);
    QCOMPARE(runtime.value(QStringLiteral("presentationProgress")).toReal(), -1.0);
    QVERIFY(runtime.contains(QStringLiteral("currentScreenGeometry")));

    // The surface state the renderer consumes must actually be published, and
    // host concealment must be reported beside it rather than folded into it.
    QCOMPARE(runtime.value(QStringLiteral("presentationState")).toString(),
             QStringLiteral("open"));
    QVERIFY(!runtime.value(QStringLiteral("hostConcealed")).toBool());

    PanelRuntimeState collapsedAndConcealed;
    collapsedAndConcealed.surfaceState = ArchDock::PanelSurfaceState::Collapsed;
    collapsedAndConcealed.hostConcealed = true;
    collapsedAndConcealed.transition = PanelTransitionState::Revealing;
    const QVariantMap concealedRuntime = collapsedAndConcealed.toRuntimeMap();
    QCOMPARE(concealedRuntime.value(QStringLiteral("presentationState")).toString(),
             QStringLiteral("collapsed"));
    QVERIFY(concealedRuntime.value(QStringLiteral("hostConcealed")).toBool());
    // A host reveal is never reported to a surface renderer as an open or
    // close interpolation.
    QCOMPARE(concealedRuntime.value(QStringLiteral("transitionState")).toString(),
             QStringLiteral("revealing"));
}

void PanelModelTest::serializationExcludesTransientRuntimeState()
{
    const QVariantMap legacy{
        {QStringLiteral("id"), QStringLiteral("bottom")},
        {QStringLiteral("name"), QStringLiteral("Bottom panel")},
        {QStringLiteral("edge"), QStringLiteral("bottom")},
        {QStringLiteral("hovered"), true},
        {QStringLiteral("hoveredIndex"), 4},
        {QStringLiteral("editMode"), true},
        {QStringLiteral("popupOpen"), true},
        {QStringLiteral("dragging"), true},
        {QStringLiteral("transitionState"), QStringLiteral("opening")},
        {QStringLiteral("presentationState"), QStringLiteral("collapsed")},
        {QStringLiteral("hostConcealed"), true},
        {QStringLiteral("presentationProgress"), 0.4},
        {QStringLiteral("windowOverlap"), true},
        {QStringLiteral("rendererFallback"), QStringLiteral("software")},
        {QStringLiteral("frameQuality"), QStringLiteral("reduced")},
        {QStringLiteral("currentScreenGeometry"), QRect(0, 0, 1920, 1080)},
        {QStringLiteral("nativeRecoveryState"), QStringLiteral("verified")},
        {QStringLiteral("durableExtension"), QStringLiteral("keep-me")},
        {QStringLiteral("extensions"),
         QVariantMap{
             {QStringLiteral("moving"), true},
             {QStringLiteral("screenGeometry"), QRect(0, 0, 1280, 720)},
             {QStringLiteral("nestedDurableExtension"), QStringLiteral("keep-too")},
         }},
    };

    const auto parsed = PanelDefinition::fromLegacyMap(legacy);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->host.nativeRecoveryState, QStringLiteral("verified"));
    QCOMPARE(
        parsed->extensions.value(QStringLiteral("durableExtension")).toString(),
        QStringLiteral("keep-me"));
    QCOMPARE(
        parsed->extensions.value(QStringLiteral("nestedDurableExtension")).toString(),
        QStringLiteral("keep-too"));

    const QStringList transientKeys{
        QStringLiteral("hovered"),
        QStringLiteral("hoveredIndex"),
        QStringLiteral("editMode"),
        QStringLiteral("popupOpen"),
        QStringLiteral("dragging"),
        QStringLiteral("moving"),
        QStringLiteral("transitionState"),
        QStringLiteral("presentationState"),
        QStringLiteral("hostConcealed"),
        QStringLiteral("presentationProgress"),
        QStringLiteral("windowOverlap"),
        QStringLiteral("rendererFallback"),
        QStringLiteral("frameQuality"),
        QStringLiteral("currentScreenGeometry"),
        QStringLiteral("screenGeometry"),
    };
    const QVariantMap flat = parsed->toLegacyMap();
    const QVariantMap persisted = parsed->toPersistedMap();
    const QVariantMap extensions = persisted.value(QStringLiteral("extensions")).toMap();
    for (const QString &key : transientKeys)
    {
        QVERIFY2(!flat.contains(key), qPrintable(key));
        QVERIFY2(!persisted.contains(key), qPrintable(key));
        QVERIFY2(!extensions.contains(key), qPrintable(key));
    }
    QCOMPARE(flat.value(QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("verified"));
    QCOMPARE(
        extensions.value(QStringLiteral("durableExtension")).toString(),
        QStringLiteral("keep-me"));
    QCOMPARE(parsed->toPersistedMap(), parsed->toPersistedMap());
}

void PanelModelTest::legacyFixturesMigrateWithoutDataLoss()
{
    const QStringList fixtureNames{
        QStringLiteral("builtin-native-v1.json"),
        QStringLiteral("custom-native-v1.json"),
        QStringLiteral("custom-free-v1.json"),
    };
    const QByteArray source = fixtureArray(fixtureNames);
    QVERIFY(!source.isEmpty());

    const auto migration = SettingsMigration::migratePanelRecords(source);
    QVERIFY2(migration.ok(), qPrintable(migration.diagnostic));
    QCOMPARE(migration.status, PanelMigrationStatus::Success);
    QVERIFY(migration.sourceWasLegacy);
    QVERIFY(migration.rewriteRequired);
    QCOMPARE(migration.definitions.size(), fixtureNames.size());
    QCOMPARE(migration.definitions.at(0).identity.id, QStringLiteral("bottom"));
    QCOMPARE(migration.definitions.at(0).host.kind, PanelHostKind::NativeEdge);
    QVERIFY(migration.definitions.at(0).identity.builtIn);
    QCOMPARE(migration.definitions.at(1).identity.id, QStringLiteral("panel-7"));
    QCOMPARE(migration.definitions.at(1).host.nativeOwnershipToken,
             QStringLiteral("native-custom-token"));
    QCOMPARE(migration.definitions.at(2).identity.id, QStringLiteral("free-3"));
    QCOMPARE(migration.definitions.at(2).host.kind, PanelHostKind::FreeDesktop);
    QCOMPARE(migration.definitions.at(2).host.freeCreationState,
             QStringLiteral("rollback-pending"));
    QCOMPARE(migration.definitions.at(2).host.freeRollbackError,
             QStringLiteral("containment-removal-deferred"));

    const QJsonArray sourceRecords = QJsonDocument::fromJson(source).array();
    for (qsizetype index = 0; index < sourceRecords.size(); ++index)
    {
        const QJsonObject original = sourceRecords.at(index).toObject();
        const QJsonObject flat = QJsonObject::fromVariantMap(
            migration.definitions.at(index).toLegacyMap());
        for (auto it = original.begin(); it != original.end(); ++it)
        {
            QCOMPARE(flat.value(it.key()), it.value());
        }
    }

    const QJsonArray migratedRecords = QJsonDocument::fromJson(
        migration.serializedVersionTwo).array();
    QCOMPARE(migratedRecords.size(), fixtureNames.size());
    for (const QJsonValue &value : migratedRecords)
    {
        QCOMPARE(
            value.toObject().value(QStringLiteral("schemaVersion")).toInt(),
            PanelDefinition::CurrentSchemaVersion);
    }
    QCOMPARE(
        migratedRecords.at(0).toObject().value(QStringLiteral("extensions"))
            .toObject().value(QStringLiteral("legacyExtensionData")).toString(),
        QStringLiteral("preserve-built-in"));
    QCOMPARE(
        migratedRecords.at(2).toObject().value(QStringLiteral("extensions"))
            .toObject().value(QStringLiteral("legacyFreeExtension"))
            .toObject().value(QStringLiteral("keep")).toBool(),
        true);
}

void PanelModelTest::migrationIsIdempotent()
{
    const QByteArray legacy = fixtureArray({
        QStringLiteral("builtin-native-v1.json"),
        QStringLiteral("custom-native-v1.json"),
        QStringLiteral("custom-free-v1.json"),
    });
    QVERIFY(!legacy.isEmpty());

    const auto first = SettingsMigration::migratePanelRecords(legacy);
    QVERIFY2(first.ok(), qPrintable(first.diagnostic));
    QVERIFY(first.rewriteRequired);
    const auto second = SettingsMigration::migratePanelRecords(
        first.serializedVersionTwo);
    QVERIFY2(second.ok(), qPrintable(second.diagnostic));
    QVERIFY(!second.sourceWasLegacy);
    QVERIFY(!second.rewriteRequired);
    QCOMPARE(second.definitions, first.definitions);
    QCOMPARE(second.serializedVersionTwo, first.serializedVersionTwo);
}

void PanelModelTest::existingPanelsAcquireOneInheritedSegment()
{
    auto original = PanelDefinition::defaults(QStringLiteral("segment-migration"),
        QStringLiteral("Existing"), QStringLiteral("free"), false);
    original.settingsRevision = 17;
    original.content.applicationIds = {QStringLiteral("org.kde.dolphin.desktop")};
    QVariantMap record = original.toPersistedMap();
    record.remove(QStringLiteral("segments"));
    const QByteArray source = QJsonDocument(QJsonArray{
        QJsonObject::fromVariantMap(record)}).toJson(QJsonDocument::Compact);
    const auto first = SettingsMigration::migratePanelRecords(source);
    QVERIFY2(first.ok(), qPrintable(first.diagnostic));
    QVERIFY(!first.sourceWasLegacy);
    QVERIFY(first.rewriteRequired);
    QCOMPARE(first.definitions.size(), 1);
    QCOMPARE(first.definitions.first(), original.normalized());
    QCOMPARE(first.definitions.first().segments.size(), 1);
    QCOMPARE(first.definitions.first().segments.first().source, QStringLiteral("inherited"));
    const auto second = SettingsMigration::migratePanelRecords(first.serializedVersionTwo);
    QVERIFY2(second.ok(), qPrintable(second.diagnostic));
    QVERIFY(!second.rewriteRequired);
    QCOMPARE(second.serializedVersionTwo, first.serializedVersionTwo);
    QCOMPARE(second.definitions, first.definitions);
}

void PanelModelTest::corruptAndUnsupportedSourcesFailSafely()
{
    const QByteArray corrupt = fixtureArray({QStringLiteral("corrupt-v1.json")});
    QVERIFY(!corrupt.isEmpty());
    const QByteArray corruptBefore = corrupt;
    const auto corruptResult = SettingsMigration::migratePanelRecords(corrupt);
    QVERIFY(!corruptResult.ok());
    QCOMPARE(corruptResult.status, PanelMigrationStatus::InvalidRecord);
    QVERIFY(corruptResult.definitions.isEmpty());
    QVERIFY(corruptResult.serializedVersionTwo.isEmpty());
    QVERIFY(corruptResult.diagnostic.contains(QStringLiteral("has no id")));
    QCOMPARE(corrupt, corruptBefore);

    const QByteArray unsupported = fixtureArray({
        QStringLiteral("unsupported-v99.json"),
    });
    QVERIFY(!unsupported.isEmpty());
    const QByteArray unsupportedBefore = unsupported;
    const auto unsupportedResult = SettingsMigration::migratePanelRecords(unsupported);
    QVERIFY(!unsupportedResult.ok());
    QCOMPARE(unsupportedResult.status, PanelMigrationStatus::UnsupportedVersion);
    QVERIFY(unsupportedResult.definitions.isEmpty());
    QVERIFY(unsupportedResult.serializedVersionTwo.isEmpty());
    QVERIFY(unsupportedResult.diagnostic.contains(QStringLiteral("version 99")));
    QCOMPARE(unsupported, unsupportedBefore);

    const auto invalidJson = SettingsMigration::migratePanelRecords("not-json");
    QCOMPARE(invalidJson.status, PanelMigrationStatus::InvalidJson);
    const auto invalidRoot = SettingsMigration::migratePanelRecords("{}");
    QCOMPARE(invalidRoot.status, PanelMigrationStatus::InvalidRoot);
    const auto noSource = SettingsMigration::migratePanelRecords({});
    QVERIFY(noSource.ok());
    QCOMPARE(noSource.status, PanelMigrationStatus::NoSource);
}

void PanelModelTest::invalidRecordsAreRejected()
{
    QString errorMessage;
    QVERIFY(!PanelDefinition::fromLegacyMap({}, &errorMessage).has_value());
    QCOMPARE(errorMessage, QStringLiteral("panel record is empty"));

    QVERIFY(!PanelDefinition::fromLegacyMap(
        {{QStringLiteral("name"), QStringLiteral("Missing id")}},
        &errorMessage).has_value());
    QCOMPARE(errorMessage, QStringLiteral("panel record has no id"));

    QVERIFY(!PanelDefinition::fromLegacyMap(
        {{QStringLiteral("schemaVersion"), 99},
         {QStringLiteral("id"), QStringLiteral("future")}},
        &errorMessage).has_value());
    QCOMPARE(errorMessage, QStringLiteral("unsupported panel schema version: 99"));
}

void PanelModelTest::segmentsRoundTripWithIndependentSettings()
{
    auto panel = PanelDefinition::defaults(QStringLiteral("free-segments"),
        QStringLiteral("Segments"), QStringLiteral("free"), false);
    QCOMPARE(panel.segments.size(), 1);
    QCOMPARE(panel.segments.first().source, QStringLiteral("inherited"));
    QCOMPARE(panel.segments.first().padding, -1);
    ArchDock::PanelSegmentDefinition files;
    files.id = QStringLiteral("files");
    files.source = QStringLiteral("custom");
    files.order = 1;
    files.entryIds = {QStringLiteral("free-url:file:///tmp/document.txt")};
    files.background = QStringLiteral("solid");
    files.color = QStringLiteral("#123456");
    files.padding = 14;
    files.spacing = 6;
    files.corners = QStringLiteral("capsule");
    files.presentation = QStringLiteral("closed");
    files.motionProfile = QStringLiteral("panel-slide");
    panel.segments.append(files);
    QString error;
    QVERIFY2(panel.isValid(&error), qPrintable(error));
    const QVariantMap record = panel.toPersistedMap();
    QCOMPARE(record.value(QStringLiteral("segments")).toList().size(), 2);
    const auto restored = PanelDefinition::fromLegacyMap(record, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(restored->segments == panel.segments);
    QVERIFY(!restored->extensions.contains(QStringLiteral("segments")));
    auto reversedRecord = record;
    auto rows = record.value(QStringLiteral("segments")).toList();
    std::reverse(rows.begin(), rows.end());
    reversedRecord.insert(QStringLiteral("segments"), rows);
    const auto reordered = PanelDefinition::fromLegacyMap(reversedRecord, &error);
    QVERIFY2(reordered.has_value(), qPrintable(error));
    QVERIFY(reordered->segments == panel.segments);
}

void PanelModelTest::invalidSegmentsFailClosed()
{
    auto panel = PanelDefinition::defaults(QStringLiteral("invalid-segments"),
        QStringLiteral("Segments"), QStringLiteral("bottom"), false);
    const auto main = panel.segments.first().toVariantMap();
    const auto rejects = [&panel](const QVariant &segments) {
        auto record = panel.toPersistedMap();
        record.insert(QStringLiteral("segments"), segments);
        return !PanelDefinition::fromLegacyMap(record).has_value();
    };
    QVERIFY(rejects(QStringLiteral("not-a-list")));
    QVERIFY(rejects(QVariantList{}));
    QVERIFY(rejects(QVariantList{QStringLiteral("not-a-map")}));
    QVERIFY(rejects(QVariantList{main, main}));
    for (const auto &change : QList<QPair<QString, QVariant>>{
        {QStringLiteral("id"), QStringLiteral("../invalid")},
        {QStringLiteral("source"), QStringLiteral("plugin")},
        {QStringLiteral("padding"), 65}, {QStringLiteral("spacing"), -2},
        {QStringLiteral("order"), 0.5}, {QStringLiteral("order"), true},
        {QStringLiteral("order"), 1}, {QStringLiteral("background"), QStringLiteral("url")},
        {QStringLiteral("color"), QStringLiteral("invalid")},
        {QStringLiteral("corners"), QStringLiteral("invalid")},
        {QStringLiteral("presentation"), QStringLiteral("invalid")},
        {QStringLiteral("entryIds"), QVariantList{12}},
        {QStringLiteral("entryIds"), QStringList{QStringLiteral("same"), QStringLiteral("same")}}})
    {
        auto invalid = main;
        invalid.insert(change.first, change.second);
        QVERIFY2(rejects(QVariantList{invalid}), qPrintable(change.first));
    }
    auto first = main;
    first.insert(QStringLiteral("entryIds"), QStringList{QStringLiteral("same-entry")});
    auto second = first;
    second.insert(QStringLiteral("id"), QStringLiteral("second"));
    second.insert(QStringLiteral("order"), 1);
    QVERIFY(rejects(QVariantList{first, second}));
    QVariantList excessive;
    for (int i = 0; i < 17; ++i) {
        auto value = main;
        value.insert(QStringLiteral("id"), QStringLiteral("segment-%1").arg(i));
        value.insert(QStringLiteral("order"), i);
        excessive.append(value);
    }
    QVERIFY(rejects(excessive));
}

QTEST_APPLESS_MAIN(PanelModelTest)

#include "PanelModelTest.moc"
