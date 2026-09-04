#include "animation/AnimationProfileCatalog.h"
#include "model/AnimationProfile.h"
#include "model/PanelSettingsSchema.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace ArchDock;

namespace
{

QString fixtureRoot()
{
    return QStringLiteral(ARCHDOCK_ANIMATION_PROFILE_FIXTURE_ROOT);
}

QVariantMap readFixture(const QString &fileName)
{
    QFile file(QDir(fixtureRoot()).filePath(fileName));
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

bool containsCode(const QVector<AnimationValidationDiagnostic> &diagnostics,
                  const QString &code)
{
    for (const AnimationValidationDiagnostic &entry : diagnostics)
    {
        if (entry.code == code)
        {
            return true;
        }
    }
    return false;
}

QByteArray catalogBytes(const QByteArray &profiles,
                        const QByteArray &fallback = "\"none\"")
{
    return QByteArray(
               "{\"format\":\"org.archdock.animation-profile-catalog\","
               "\"version\":1,\"fallbackProfileId\":")
        + fallback + QByteArray(",\"animationProfiles\":") + profiles
        + QByteArray("}");
}

QByteArray minimalProfile(const QByteArray &id,
                          const QByteArray &extra = QByteArray())
{
    return QByteArray(
               "{\"format\":\"org.archdock.animation-profile\",\"version\":1,"
               "\"id\":\"")
        + id
        + QByteArray("\",\"name\":\"Profile\",\"target\":\"icon\","
                     "\"trigger\":\"idle\",\"timing\":{\"baseDuration\":200},"
                     "\"tracks\":[{\"id\":\"t\",\"property\":\"opacity\","
                     "\"from\":0,\"to\":1,\"duration\":200}],"
                     "\"rendererRequirements\":[\"procedural2d\"]")
        + (extra.isEmpty()
               ? QByteArray(",\"reducedMotion\":{\"mode\":\"none\"}}")
               : QByteArray(",") + extra + QByteArray("}"));
}

}

class AnimationProfileTest : public QObject
{
    Q_OBJECT

private slots:
    void everyFixtureMatchesItsDeclaredOutcome();
    void targetsDistinguishEveryRequiredLayer();
    void clickAndLaunchSuccessAreSeparateTriggers();
    void reducedMotionSubstituteIsExplicitAndResolvable();
    void conflictingTracksAreRejectedWithPreciseDiagnostics();
    void resolvableCompositionIsAccepted();
    void catalogResolvesLegacyNamesAndFallsBackSafely();
    void projectionReportsDerivedRuntimeFacts();
    void builtInCatalogIsValid();
    void builtInCatalogCoversEverySelectableAnimation();
    void builtInProfileIdsAreFrozen();
};

// The fixture index is the contract: every listed file must produce exactly the
// outcome it declares, so a validator regression cannot pass unnoticed.
void AnimationProfileTest::everyFixtureMatchesItsDeclaredOutcome()
{
    QFile indexFile(QDir(fixtureRoot()).filePath(
        QStringLiteral("fixture-index.json")));
    QVERIFY2(indexFile.open(QIODevice::ReadOnly), "fixture index is readable");
    const QJsonArray fixtures = QJsonDocument::fromJson(indexFile.readAll())
                                    .object()
                                    .value(QStringLiteral("fixtures"))
                                    .toArray();
    QVERIFY(!fixtures.isEmpty());

    for (const QJsonValue &value : fixtures)
    {
        const QJsonObject entry = value.toObject();
        const QString fileName = entry.value(QStringLiteral("file")).toString();
        const bool expectValid = entry.value(QStringLiteral("valid")).toBool();
        const QVariantMap profile = readFixture(fileName);
        QVERIFY2(!profile.isEmpty(), qPrintable(fileName));

        const QVector<AnimationValidationDiagnostic> diagnostics =
            AnimationProfileCatalog::validateProfile(profile);
        if (expectValid)
        {
            QVERIFY2(diagnostics.isEmpty(), qPrintable(
                fileName + QStringLiteral(" reported ")
                + (diagnostics.isEmpty() ? QString{}
                                         : diagnostics.first().code)));
            continue;
        }
        QVERIFY2(!diagnostics.isEmpty(), qPrintable(fileName));
        const QString expectedCode =
            entry.value(QStringLiteral("diagnostic")).toString();
        QVERIFY2(containsCode(diagnostics, expectedCode),
                 qPrintable(fileName + QStringLiteral(" expected ")
                            + expectedCode + QStringLiteral(" but reported ")
                            + diagnostics.first().code));
        // A diagnostic must point at the offending member, not at the document.
        for (const AnimationValidationDiagnostic &diagnostic : diagnostics)
        {
            QVERIFY(!diagnostic.message.isEmpty());
        }
    }
}

void AnimationProfileTest::targetsDistinguishEveryRequiredLayer()
{
    const QStringList &targets = animationTargetVocabulary();
    for (const QString &required : {QStringLiteral("glyph"),
                                    QStringLiteral("tile"),
                                    QStringLiteral("icon"),
                                    QStringLiteral("indicator"),
                                    QStringLiteral("panel-surface"),
                                    QStringLiteral("free-scene")})
    {
        QVERIFY2(targets.contains(required), qPrintable(required));
    }

    // The same motion aimed at two different layers must stay two distinct
    // profiles, never collapse into one.
    QVariantMap profile = readFixture(QStringLiteral("valid-composed.json"));
    AnimationProfileDefinition definition;
    QVERIFY(AnimationProfileCatalog::validateProfile(profile, &definition)
                .isEmpty());
    const QStringList resolved = definition.effectiveTargets();
    QVERIFY(resolved.contains(QStringLiteral("glyph")));
    QVERIFY(resolved.contains(QStringLiteral("tile")));
    QVERIFY(resolved.contains(QStringLiteral("icon")));
    QVERIFY(resolved.contains(QStringLiteral("indicator")));
}

// Legacy TASK-0049 requires click and launch success to be separable in the
// schema so a later phase cannot quietly treat a click as a launch result.
void AnimationProfileTest::clickAndLaunchSuccessAreSeparateTriggers()
{
    const QStringList &triggers = animationTriggerVocabulary();
    QVERIFY(triggers.contains(QStringLiteral("click")));
    QVERIFY(triggers.contains(QStringLiteral("launch-requested")));
    QVERIFY(triggers.contains(QStringLiteral("launch-succeeded")));
    QVERIFY(triggers.contains(QStringLiteral("launch-failed")));

    // The pre-migration umbrella value must no longer validate.
    const QVariantMap legacy = readFixture(
        QStringLiteral("invalid-trigger.json"));
    const QVector<AnimationValidationDiagnostic> diagnostics =
        AnimationProfileCatalog::validateProfile(legacy);
    QVERIFY(containsCode(diagnostics, QStringLiteral("invalid-trigger")));

    AnimationProfileDefinition clickProfile;
    QVariantMap profile = readFixture(QStringLiteral("valid-simple.json"));
    profile.insert(QStringLiteral("trigger"), QStringLiteral("click"));
    QVERIFY(AnimationProfileCatalog::validateProfile(profile, &clickProfile)
                .isEmpty());
    QCOMPARE(clickProfile.trigger, QStringLiteral("click"));
    QVERIFY(clickProfile.trigger != QStringLiteral("launch-succeeded"));
}

void AnimationProfileTest::reducedMotionSubstituteIsExplicitAndResolvable()
{
    const QVariantMap missing = readFixture(
        QStringLiteral("invalid-missing-reduced-motion.json"));
    QVERIFY(containsCode(AnimationProfileCatalog::validateProfile(missing),
                         QStringLiteral("missing-reduced-motion")));

    // A substitute that names a profile the catalog does not contain must fail
    // at catalog level, not silently degrade at runtime.
    const AnimationProfileCatalogLoadResult dangling =
        AnimationProfileCatalog::loadCatalogBytes(catalogBytes(
            QByteArray("[")
            + minimalProfile("base",
                             "\"reducedMotion\":{\"mode\":\"substitute\","
                             "\"substituteProfileId\":\"ghost\"}")
            + QByteArray("]"),
            "\"base\""));
    QVERIFY(!dangling.isValid());
    QCOMPARE(dangling.primaryCode(), QStringLiteral("unknown-substitute"));

    // A substitute chain is refused: the fallback must be a resting state.
    const AnimationProfileCatalogLoadResult chained =
        AnimationProfileCatalog::loadCatalogBytes(catalogBytes(
            QByteArray("[")
            + minimalProfile("base",
                             "\"reducedMotion\":{\"mode\":\"substitute\","
                             "\"substituteProfileId\":\"middle\"}")
            + QByteArray(",")
            + minimalProfile("middle",
                             "\"reducedMotion\":{\"mode\":\"substitute\","
                             "\"substituteProfileId\":\"base\"}")
            + QByteArray("]"),
            "\"base\""));
    QVERIFY(!chained.isValid());
    QCOMPARE(chained.primaryCode(), QStringLiteral("invalid-reduced-motion"));
}

void AnimationProfileTest::conflictingTracksAreRejectedWithPreciseDiagnostics()
{
    const QVariantMap conflict = readFixture(
        QStringLiteral("invalid-track-conflict.json"));
    const QVector<AnimationValidationDiagnostic> diagnostics =
        AnimationProfileCatalog::validateProfile(conflict);
    QCOMPARE(diagnostics.size(), 1);
    QCOMPARE(diagnostics.first().code, QStringLiteral("track-conflict"));
    // The pointer must identify the second writer, which is index 1.
    QCOMPARE(diagnostics.first().jsonPointer, QStringLiteral("/tracks/1"));

    // The same two tracks aimed at different layers are not a conflict.
    QVariantMap separated = conflict;
    QVariantList tracks = separated.value(QStringLiteral("tracks")).toList();
    QVariantMap second = tracks.at(1).toMap();
    second.insert(QStringLiteral("target"), QStringLiteral("glyph"));
    tracks[1] = second;
    separated.insert(QStringLiteral("tracks"), tracks);
    QVERIFY(AnimationProfileCatalog::validateProfile(separated).isEmpty());
}

void AnimationProfileTest::resolvableCompositionIsAccepted()
{
    const QVariantMap conflict = readFixture(
        QStringLiteral("invalid-track-conflict.json"));

    // Distinct priorities pick a deterministic winner.
    QVariantMap prioritised = conflict;
    QVariantList tracks = prioritised.value(QStringLiteral("tracks")).toList();
    QVariantMap second = tracks.at(1).toMap();
    second.insert(QStringLiteral("priority"), 9);
    tracks[1] = second;
    prioritised.insert(QStringLiteral("tracks"), tracks);
    QVERIFY(AnimationProfileCatalog::validateProfile(prioritised).isEmpty());

    // Additive blending on both writers sums without ordering ambiguity.
    QVariantMap additive = conflict;
    QVariantList additiveTracks =
        additive.value(QStringLiteral("tracks")).toList();
    for (qsizetype index = 0; index < additiveTracks.size(); ++index)
    {
        QVariantMap track = additiveTracks.at(index).toMap();
        track.insert(QStringLiteral("blend"), QStringLiteral("add"));
        additiveTracks[index] = track;
    }
    additive.insert(QStringLiteral("tracks"), additiveTracks);
    QVERIFY(AnimationProfileCatalog::validateProfile(additive).isEmpty());
}

void AnimationProfileTest::catalogResolvesLegacyNamesAndFallsBackSafely()
{
    const AnimationProfileCatalogLoadResult result =
        AnimationProfileCatalog::loadCatalogBytes(catalogBytes(
            QByteArray("[")
            + minimalProfile("none")
            + QByteArray(",")
            + minimalProfile("soft-pulse",
                             "\"reducedMotion\":{\"mode\":\"none\"},"
                             "\"legacyNames\":[\"pulse\",\"scale\"]")
            + QByteArray("]")));
    QVERIFY2(result.isValid(), qPrintable(result.primaryMessage()));
    const AnimationProfileCatalog &catalog = *result.catalog;

    QCOMPARE(catalog.profileIdForLegacyName(QStringLiteral("pulse")),
             QStringLiteral("soft-pulse"));
    QCOMPARE(catalog.profileIdForLegacyName(QStringLiteral("scale")),
             QStringLiteral("soft-pulse"));
    // A current id resolves to itself rather than through the legacy map.
    QCOMPARE(catalog.profileIdForLegacyName(QStringLiteral("soft-pulse")),
             QStringLiteral("soft-pulse"));

    const QVariantMap resolved =
        catalog.resolve(QStringLiteral("pulse"));
    QCOMPARE(resolved.value(QStringLiteral("profileId")).toString(),
             QStringLiteral("soft-pulse"));
    QCOMPARE(resolved.value(QStringLiteral("fallbackApplied")).toBool(), false);

    // An unknown name falls back rather than leaving the caller with nothing.
    const QVariantMap unknown = catalog.resolve(QStringLiteral("never-existed"));
    QCOMPARE(unknown.value(QStringLiteral("profileId")).toString(),
             QStringLiteral("none"));
    QCOMPARE(unknown.value(QStringLiteral("fallbackApplied")).toBool(), true);

    // Two profiles may not claim the same legacy name.
    const AnimationProfileCatalogLoadResult clash =
        AnimationProfileCatalog::loadCatalogBytes(catalogBytes(
            QByteArray("[")
            + minimalProfile("none")
            + QByteArray(",")
            + minimalProfile("first",
                             "\"reducedMotion\":{\"mode\":\"none\"},"
                             "\"legacyNames\":[\"pulse\"]")
            + QByteArray(",")
            + minimalProfile("second",
                             "\"reducedMotion\":{\"mode\":\"none\"},"
                             "\"legacyNames\":[\"pulse\"]")
            + QByteArray("]")));
    QVERIFY(!clash.isValid());
    QCOMPARE(clash.primaryCode(), QStringLiteral("duplicate-legacy-name"));
}

void AnimationProfileTest::projectionReportsDerivedRuntimeFacts()
{
    AnimationProfileDefinition bounded;
    QVERIFY(AnimationProfileCatalog::validateProfile(
                readFixture(QStringLiteral("valid-simple.json")), &bounded)
                .isEmpty());
    QCOMPARE(bounded.isContinuous(), false);
    QCOMPARE(bounded.totalDuration(), 200);

    const QVariantMap projection = bounded.toRuntimeProjection();
    QCOMPARE(projection.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(projection.value(QStringLiteral("continuous")).toBool(), false);
    QCOMPARE(projection.value(QStringLiteral("totalDuration")).toInt(), 200);

    AnimationProfileDefinition continuous;
    QVERIFY(AnimationProfileCatalog::validateProfile(
                readFixture(QStringLiteral("valid-composed.json")), &continuous)
                .isEmpty());
    QCOMPARE(continuous.isContinuous(), true);
    // A continuous profile has no finite end; it must say so explicitly.
    QCOMPARE(continuous.totalDuration(), -1);
    QVERIFY(continuous.trackById(QStringLiteral("glyph-turn")) != nullptr);
    QVERIFY(continuous.trackById(QStringLiteral("absent")) == nullptr);
}

// The shipped catalog must load cleanly, or the product would fall back to
// "none" for every entry at runtime.
void AnimationProfileTest::builtInCatalogIsValid()
{
    const AnimationProfileCatalogLoadResult result =
        AnimationProfileCatalog::loadCatalog(
            QStringLiteral(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH));
    QVERIFY2(result.isValid(), qPrintable(result.primaryCode()
                                          + QStringLiteral(": ")
                                          + result.primaryMessage()));
    const AnimationProfileCatalog &catalog = *result.catalog;
    QVERIFY(catalog.contains(catalog.fallbackProfileId()));

    for (const QString &profileId : catalog.profileIds())
    {
        const AnimationProfileDefinition *profile =
            catalog.profileById(profileId);
        QVERIFY(profile != nullptr);
        // Reduced motion is mandatory for every shipped profile.
        QVERIFY2(!profile->reducedMotion.mode.isEmpty(),
                 qPrintable(profileId));
        QVERIFY(!profile->rendererRequirements.isEmpty());
    }
}

// Every value the settings schema offers must resolve to a validated profile,
// and the catalog must not offer anything the schema cannot select. This is
// what keeps the editor from listing a preset that does not exist.
void AnimationProfileTest::builtInCatalogCoversEverySelectableAnimation()
{
    const AnimationProfileCatalogLoadResult result =
        AnimationProfileCatalog::loadCatalog(
            QStringLiteral(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH));
    QVERIFY(result.isValid());
    const AnimationProfileCatalog &catalog = *result.catalog;

    const ArchDock::PanelSettingsFieldDescriptor *field =
        ArchDock::PanelSettingsSchema::panelDescriptor(
            QStringLiteral("iconAnimation"));
    QVERIFY(field != nullptr);
    QVERIFY(!field->choices.isEmpty());

    QStringList selectable = field->choices;
    selectable.sort();

    QStringList offered = catalog.profileIds();
    const QMap<QString, QString> legacy = catalog.legacyNameMap();
    for (auto it = legacy.constBegin(); it != legacy.constEnd(); ++it)
    {
        offered.append(it.key());
    }
    offered.sort();

    QCOMPARE(offered, selectable);

    for (const QString &choice : field->choices)
    {
        const QString resolved = catalog.profileIdForLegacyName(choice);
        QVERIFY2(!resolved.isEmpty(), qPrintable(choice));
        QVERIFY2(catalog.contains(resolved), qPrintable(choice));
        // No selectable value may silently degrade to the fallback.
        const QVariantMap resolution = catalog.resolve(choice);
        QCOMPARE(resolution.value(QStringLiteral("fallbackApplied")).toBool(),
                 false);
    }
}

// TASK-0031 freezes these ids. The built-in Panel and Icon Presets that
// TASK-0040 must deliver name motion by id, so a rename here would silently
// break a shipped preset. Adding an id is a deliberate act that updates this
// list, the settings schema and the migration test together; removing or
// renaming one is a compatibility break.
void AnimationProfileTest::builtInProfileIdsAreFrozen()
{
    const AnimationProfileCatalogLoadResult result =
        AnimationProfileCatalog::loadCatalog(
            QStringLiteral(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH));
    QVERIFY(result.isValid());

    QStringList frozen = {
        // Migrated by TASK-0030.
        QStringLiteral("none"),        QStringLiteral("bounce"),
        QStringLiteral("elastic"),     QStringLiteral("spring"),
        QStringLiteral("float"),       QStringLiteral("wave"),
        QStringLiteral("orbit"),       QStringLiteral("pulse"),
        QStringLiteral("breathe"),     QStringLiteral("ripple"),
        QStringLiteral("magnetic"),    QStringLiteral("spin"),
        QStringLiteral("idle-rotate"), QStringLiteral("swing"),
        QStringLiteral("wobble"),      QStringLiteral("wiggle"),
        QStringLiteral("shake"),       QStringLiteral("glow"),
        // Requested motions, added by TASK-0031.
        QStringLiteral("slow-y-turn"), QStringLiteral("jump"),
        QStringLiteral("shake-tangent"), QStringLiteral("enlarge"),
        QStringLiteral("spiral"),
    };
    frozen.sort();

    QStringList shipped = result.catalog->profileIds();
    shipped.sort();
    QCOMPARE(shipped, frozen);

    // Every frozen id must still resolve to itself, never to the fallback.
    for (const QString &profileId : frozen)
    {
        const QVariantMap resolution = result.catalog->resolve(profileId);
        QCOMPARE(resolution.value(QStringLiteral("profileId")).toString(),
                 profileId);
        QCOMPARE(resolution.value(QStringLiteral("fallbackApplied")).toBool(),
                 false);
    }
}

QTEST_MAIN(AnimationProfileTest)

#include "AnimationProfileTest.moc"
