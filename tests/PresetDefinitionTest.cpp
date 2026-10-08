#include "model/IconPresetDefinition.h"
#include "model/PanelPresetDefinition.h"
#include "model/PanelSettingsSchema.h"

#include "PresetTestSupport.h"

#include <QTest>

using ArchDock::IconPresetDefinition;
using ArchDock::PanelHostKind;
using ArchDock::PanelPresetDefinition;
using ArchDock::PanelSettingsFieldAccess;
using ArchDock::PanelSettingsSchema;
using ArchDock::PresetIdentity;
using ArchDock::PresetValidationDiagnostic;

using namespace PresetTestSupport;

class PresetDefinitionTest final : public QObject
{
    Q_OBJECT

private slots:
    void identifiersFollowTheVersionOneGrammar();
    void panelPresetExposesNormativeFields();
    void panelPresetConfigurationCarriesNoHostAssociation();
    void panelPresetRoundTrips();
    void panelPresetRejects_data();
    void panelPresetRejects();
    void panelPresetDropPolicyReportsAndDropsUnsupportedFields();
    void panelPresetKeysAreEditableSchemaFields();
    void iconPresetExposesNormativeFields();
    void iconPresetRoundTrips();
    void iconPresetParametersAreBoundedAndRoundTrip();
    void iconPresetRejects_data();
    void iconPresetRejects();
    void iconPresetPanelValuesStayInsideTheIconLayer();
    void userPresetsCarryLineage();
};

void PresetDefinitionTest::identifiersFollowTheVersionOneGrammar()
{
    for (const QString &id : {QStringLiteral("a"),
                              QStringLiteral("obsidian-glass-dock"),
                              QStringLiteral("user-1a2b3c4d5e6f"),
                              QStringLiteral("v1.2-preset")})
    {
        QVERIFY2(PresetIdentity::isValidId(id), qPrintable(id));
    }
    for (const QString &id : {QString{}, QStringLiteral("-leading"),
                              QStringLiteral("Upper"), QStringLiteral("has space"),
                              QStringLiteral("newline\n"),
                              QStringLiteral("../escape"),
                              QStringLiteral("a/b"), QString(65, QLatin1Char('a'))})
    {
        QVERIFY2(!PresetIdentity::isValidId(id), qPrintable(id));
    }
    QVERIFY(PresetIdentity::isValidId(QString(64, QLatin1Char('a'))));
    QVERIFY(PresetIdentity::isUserId(QStringLiteral("user-1a2b3c4d5e6f")));
    QVERIFY(!PresetIdentity::isUserId(QStringLiteral("obsidian-glass-dock")));
}

void PresetDefinitionTest::panelPresetExposesNormativeFields()
{
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = PanelPresetDefinition::fromVariantMap(
        panelPresetMap(), &diagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(diagnostics.isEmpty());

    QCOMPARE(preset->format, QStringLiteral("org.archdock.panel-preset"));
    QCOMPARE(preset->schemaVersion, 1);
    QCOMPARE(preset->identity.id, QStringLiteral("fixture-chassis"));
    QCOMPARE(preset->identity.name, QStringLiteral("Fixture Chassis"));
    QCOMPARE(preset->identity.description,
             QStringLiteral("A collapsible chassis fixture."));
    QVERIFY(preset->identity.builtIn);
    QCOMPARE(preset->identity.revision, 3);
    QVERIFY(preset->identity.derivedFromPresetId.isEmpty());
    QCOMPARE(preset->identity.sourceRevision, 0);

    QCOMPARE(preset->preview.previewMode, QStringLiteral("horizontal"));
    QCOMPARE(preset->preview.rendererTier, QStringLiteral("skinned2d"));
    QCOMPARE(preset->preview.fallbackTier, QStringLiteral("procedural2d"));
    QCOMPARE(preset->preview.deterministicPreviewSeed,
             QStringLiteral("fixture-chassis-v1"));

    QCOMPARE(preset->compatibility.hostKinds,
             (QStringList{QStringLiteral("native-edge"),
                          QStringLiteral("free-desktop")}));
    QCOMPARE(preset->compatibility.orientations,
             QStringList{QStringLiteral("horizontal")});
    QCOMPARE(preset->compatibility.layouts,
             QStringList{QStringLiteral("horizontal")});
    QCOMPARE(preset->compatibility.requiredCapabilities,
             QStringList{QStringLiteral("split")});
    QCOMPARE(preset->compatibility.optionalCapabilities,
             QStringList{QStringLiteral("skinned2d")});

    const ArchDock::PanelDefinition &panel = preset->panel.configuration;
    QCOMPARE(panel.host.kind, PanelHostKind::NativeEdge);
    QCOMPARE(panel.content.type, QStringLiteral("hybrid"));
    QCOMPARE(panel.placement.edge, QStringLiteral("bottom"));
    QCOMPARE(panel.placement.alignment, QStringLiteral("center"));
    QVERIFY(panel.placement.dynamic);
    QCOMPARE(panel.placement.width, 860);
    QCOMPARE(panel.placement.height, 88);
    QCOMPARE(panel.visibility.hostMode, QStringLiteral("auto-hide"));
    QCOMPARE(panel.presentation.mode, QStringLiteral("collapsed"));
    QCOMPARE(panel.presentation.trigger, QStringLiteral("hover"));
    QCOMPARE(panel.presentation.collapseMechanism, QStringLiteral("split"));
    QCOMPARE(panel.presentation.revealHandle, QStringLiteral("bar"));
    QCOMPARE(panel.layout.pathType, QStringLiteral("horizontal"));
    QCOMPARE(panel.layout.padding, 20);
    QCOMPARE(panel.iconStyle.size, 48);
    QCOMPARE(panel.iconStyle.spacing, 9.5);
    QCOMPARE(preset->panel.themeId(), QStringLiteral("sci-fi-chassis-dark"));
    QCOMPARE(panel.surface.panelThemeId, QStringLiteral("sci-fi-chassis-dark"));
    QCOMPARE(panel.surface.rendererTier, QStringLiteral("skinned2d"));
    QCOMPARE(panel.surface.color, QStringLiteral("#44ddea"));
    QCOMPARE(panel.surface.opacity, 0.92);
    QCOMPARE(panel.surface.glowIntensity, 1.15);
    QCOMPARE(panel.motion.iconProfile, QStringLiteral("pulse"));
    QCOMPARE(panel.motion.speed, 0.9);
    QCOMPARE(panel.motion.magnifyRadius, 3.0);
    QCOMPARE(panel.motion.magnifyFalloff, QStringLiteral("cosine"));
    QCOMPARE(preset->panel.recommendedIconPresetId,
             QStringLiteral("fixture-pedestal"));

    QCOMPARE(preset->fallback.themeId, QStringLiteral("sci-fi-chassis-dark"));
    QCOMPARE(preset->fallback.iconPresetId, QStringLiteral("fixture-pedestal"));
    QCOMPARE(preset->fallback.unsupportedFieldPolicy, QStringLiteral("reject"));
}

void PresetDefinitionTest::panelPresetConfigurationCarriesNoHostAssociation()
{
    const auto preset = PanelPresetDefinition::fromVariantMap(panelPresetMap());
    QVERIFY(preset.has_value());
    const ArchDock::PanelDefinition &panel = preset->panel.configuration;

    QString error;
    QVERIFY2(panel.isValid(&error), qPrintable(error));
    QCOMPARE(panel.identity.id, preset->identity.id);
    QVERIFY(!panel.identity.builtIn);
    QCOMPARE(panel.host.nativePanelId, -1);
    QCOMPARE(panel.host.nativeDockAppletId, -1);
    QCOMPARE(panel.host.nativeControlAppletId, -1);
    QCOMPARE(panel.host.freeDesktopContainmentId, -1);
    QCOMPARE(panel.host.freeDockAppletId, -1);
    QVERIFY(panel.host.nativeOwnershipToken.isEmpty());
    QVERIFY(panel.host.freeOwnershipToken.isEmpty());
    QVERIFY(panel.host.screenId.isEmpty());
    QCOMPARE(panel.host.screenIndex, 0);
    QVERIFY(panel.content.applicationIds.isEmpty());
    QVERIFY(panel.content.urls.isEmpty());
    QVERIFY(panel.content.kdeWidgets.isEmpty());
    QVERIFY(panel.iconStyle.perEntryOverrides.isEmpty());
    QVERIFY(!panel.presetOrigin.has_value());
    QCOMPARE(panel.settingsRevision, quint64{0});

    // The serialized preset names no instance state at all.
    const QByteArray text = toJson(preset->toVariantMap());
    for (const char *forbidden : {"OwnershipToken", "nativePanelId", "AppletId",
                                  "ContainmentId", "screen", "contentAppIds",
                                  "contentUrls", "settingsRevision"})
    {
        QVERIFY2(!text.contains(forbidden), forbidden);
    }
}

void PresetDefinitionTest::panelPresetRoundTrips()
{
    const auto preset = PanelPresetDefinition::fromVariantMap(panelPresetMap());
    QVERIFY(preset.has_value());

    QVector<PresetValidationDiagnostic> diagnostics;
    const auto reloaded = PanelPresetDefinition::fromVariantMap(
        throughJson(preset->toVariantMap()), &diagnostics);
    QVERIFY2(reloaded.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(*reloaded == *preset);

    // Serialization writes the complete snapshot, not only what was declared.
    const QVariantMap panel = preset->toVariantMap()
        .value(QStringLiteral("panel")).toMap();
    for (const QString &group : PanelPresetDefinition::panelValueGroups())
    {
        QVERIFY2(panel.contains(group), qPrintable(group));
    }
    QCOMPARE(panel.value(QStringLiteral("layout")).toMap()
                 .value(QStringLiteral("layoutRadius")).toInt(), 150);
    QCOMPARE(preset->panelValues().value(QStringLiteral("completeThemeId")).toString(),
             QStringLiteral("sci-fi-chassis-dark"));
    QVERIFY(!preset->panelValues().contains(QStringLiteral("panelThemeId")));
}

void PresetDefinitionTest::panelPresetRejects_data()
{
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedPointer");

    const QVariant removed;
    const auto row = [](const char *name, const QString &pointer,
                        const QVariant &value, const QString &code,
                        const QString &expectedPointer = {})
    {
        QTest::newRow(name) << pointer << value << code
                            << (expectedPointer.isEmpty() ? pointer : expectedPointer);
    };

    row("unknown root field", "/surprise", 1, "unknown-field");
    row("wrong format", "/format", QStringLiteral("org.archdock.icon-preset"),
        "unsupported-format");
    row("future schema", "/schemaVersion", 2, "unsupported-version");
    row("malformed id", "/identity/id", QStringLiteral("Bad Id"), "invalid-id");
    row("built-in in user namespace", "/identity/id",
        QStringLiteral("user-abc"), "invalid-id");
    row("built-in with lineage", "/identity/derivedFromPresetId",
        QStringLiteral("other-preset"), "invalid-value");
    row("missing name", "/identity/name", removed, "missing-field");
    row("zero revision", "/identity/revision", 0, "invalid-value");
    row("unknown identity field", "/identity/owner", QStringLiteral("me"),
        "unknown-field");
    row("missing host edge", "/panel/host/edge", removed, "missing-field");
    row("missing content type", "/panel/content/type", removed, "missing-field");
    row("missing layout", "/panel/layout/layout", removed, "missing-field");
    row("ownership token", "/panel/host/nativeOwnershipToken",
        QStringLiteral("token"), "unknown-field");
    row("host id", "/panel/host/nativePanelId", 7, "unknown-field");
    row("screen assignment", "/panel/placement/screen", 1, "unknown-field");
    row("content list", "/panel/content/contentUrls",
        QVariantList{QStringLiteral("file:///tmp")}, "unknown-field");
    row("key in the wrong group", "/panel/surface/layout",
        QStringLiteral("ring"), "unknown-field");
    row("unknown panel group", "/panel/extras", QVariantMap{}, "unknown-field");
    row("unknown layout", "/panel/layout/layout", QStringLiteral("semi-circle"),
        "invalid-value");
    row("upper-case choice", "/panel/content/type", QStringLiteral("Hybrid"),
        "invalid-value");
    row("icon size out of range", "/panel/layout/iconSize", 500, "invalid-value");
    row("opacity out of range", "/panel/surface/opacity", 1.5, "invalid-value");
    row("width as text", "/panel/placement/width", QStringLiteral("720"),
        "invalid-type");
    row("fractional integer", "/panel/placement/width", 720.5, "invalid-type");
    row("dynamic as number", "/panel/placement/dynamic", 1, "invalid-type");
    row("named color", "/panel/surface/color", QStringLiteral("blue"),
        "invalid-color");
    row("transparent panel color", "/panel/surface/color",
        QStringLiteral("transparent"), "invalid-color");
    row("unknown tier", "/panel/theme/rendererTier", QStringLiteral("voxel"),
        "invalid-enum");
    row("malformed theme reference", "/panel/theme/completeThemeId",
        QStringLiteral("Bad Theme"), "invalid-id");
    row("malformed icon preset reference", "/panel/recommendedIconPresetId",
        QStringLiteral("Bad Preset"), "invalid-id");
    row("host not listed", "/compatibility/hostKinds",
        QVariantList{QStringLiteral("free-desktop")}, "inconsistent-declaration");
    row("layout not listed", "/compatibility/layouts",
        QVariantList{QStringLiteral("ring")}, "inconsistent-declaration");
    row("orientation not listed", "/compatibility/orientations",
        QVariantList{QStringLiteral("vertical")}, "inconsistent-declaration");
    row("empty host list", "/compatibility/hostKinds", QVariantList{},
        "missing-field");
    row("unknown capability", "/compatibility/requiredCapabilities",
        QVariantList{QStringLiteral("teleport")}, "invalid-enum",
        "/compatibility/requiredCapabilities/0");
    row("capability required and optional", "/compatibility/optionalCapabilities",
        QVariantList{QStringLiteral("split")}, "duplicate-value");
    row("preview mode for the wrong host", "/preview/previewMode",
        QStringLiteral("free"), "inconsistent-declaration");
    row("preview tier differs", "/preview/rendererTier",
        QStringLiteral("procedural2d"), "inconsistent-declaration");
    row("unknown preview tier", "/preview/fallbackTier",
        QStringLiteral("voxel"), "invalid-enum");
    row("missing seed", "/preview/deterministicPreviewSeed", removed,
        "missing-field");
    row("unknown policy", "/fallback/unsupportedFieldPolicy",
        QStringLiteral("ignore"), "invalid-enum");
    row("missing fallback", "/fallback", removed, "missing-field");
    row("missing panel", "/panel", removed, "missing-field");
}

void PresetDefinitionTest::panelPresetRejects()
{
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedPointer);

    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = PanelPresetDefinition::fromVariantMap(
        throughJson(withValue(panelPresetMap(), pointer, value)), &diagnostics);
    QVERIFY2(!preset.has_value(), "the mutated definition was accepted");
    QVERIFY2(hasDiagnostic(diagnostics, expectedCode, expectedPointer),
             qPrintable(describe(diagnostics)));
}

void PresetDefinitionTest::panelPresetDropPolicyReportsAndDropsUnsupportedFields()
{
    QVariantMap tolerant = withValue(
        panelPresetMap(), QStringLiteral("/fallback/unsupportedFieldPolicy"),
        QStringLiteral("drop"));
    const auto baseline = PanelPresetDefinition::fromVariantMap(tolerant);
    QVERIFY(baseline.has_value());

    tolerant = withValue(tolerant, QStringLiteral("/panel/surface/futureFinish"),
                         QStringLiteral("brushed"));
    tolerant = withValue(tolerant, QStringLiteral("/panel/futureGroup"),
                         QVariantMap{{QStringLiteral("depth"), 3}});
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = PanelPresetDefinition::fromVariantMap(tolerant, &diagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(*preset == *baseline);
    QCOMPARE(diagnostics.size(), 2);
    for (const PresetValidationDiagnostic &entry : std::as_const(diagnostics))
    {
        QCOMPARE(entry.severity, QStringLiteral("warning"));
        QCOMPARE(entry.code, QStringLiteral("unsupported-field"));
    }
    QVERIFY(!ArchDock::presetDiagnosticsHaveErrors(diagnostics));

    // An invalid value of a known field is never "dropped".
    QVERIFY(!PanelPresetDefinition::fromVariantMap(
                 withValue(tolerant, QStringLiteral("/panel/layout/iconSize"), 500))
                 .has_value());
}

void PresetDefinitionTest::panelPresetKeysAreEditableSchemaFields()
{
    const QStringList keys = PanelPresetDefinition::panelValueKeys();
    QVERIFY(!keys.isEmpty());
    QCOMPARE(QSet<QString>(keys.cbegin(), keys.cend()).size(), keys.size());
    for (const QString &key : keys)
    {
        const auto *field = PanelSettingsSchema::panelDescriptor(key);
        QVERIFY2(field != nullptr, qPrintable(key));
        QVERIFY2(field->access == PanelSettingsFieldAccess::Editor, qPrintable(key));
    }
    // Instance state is user-editable in Panel Studio but is not a preset.
    for (const QString &instanceKey : {QStringLiteral("screen"),
                                       QStringLiteral("visible"),
                                       QStringLiteral("segments"),
                                       QStringLiteral("iconStyle"),
                                       QStringLiteral("panelThemeId")})
    {
        QVERIFY2(!keys.contains(instanceKey), qPrintable(instanceKey));
    }
}

void PresetDefinitionTest::iconPresetExposesNormativeFields()
{
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = IconPresetDefinition::fromVariantMap(
        iconPresetMap(), &diagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(diagnostics.isEmpty());

    QCOMPARE(preset->format, QStringLiteral("org.archdock.icon-preset"));
    QCOMPARE(preset->schemaVersion, 1);
    QCOMPARE(preset->identity.id, QStringLiteral("fixture-pedestal"));
    QCOMPARE(preset->identity.revision, 2);
    QVERIFY(preset->identity.builtIn);

    QCOMPARE(preset->compatibility.rendererTiers,
             QStringList{QStringLiteral("procedural2d")});
    QCOMPARE(preset->compatibility.requiredStyleCapabilities,
             (QStringList{QStringLiteral("tile"), QStringLiteral("glow")}));
    QVERIFY(preset->compatibility.reducedMotionSupport);

    QCOMPARE(preset->icon.iconStyleId, QStringLiteral("dark-orb"));
    const QVariantMap pedestal = preset->icon.visualOverrides
        .value(QStringLiteral("orb-pedestal")).toMap();
    QCOMPARE(pedestal.value(QStringLiteral("color")).toString(),
             QStringLiteral("#08101c"));
    QCOMPARE(pedestal.value(QStringLiteral("opacity")).toDouble(), 0.9);
    const QVariantMap normal = preset->icon.stateOverrides
        .value(QStringLiteral("normal")).toMap();
    QCOMPARE(normal.value(QStringLiteral("glowColor")).toString(),
             QStringLiteral("#4a9dff"));
    QCOMPARE(normal.value(QStringLiteral("glowOpacity")).toDouble(), 0.2);
    QCOMPARE(preset->icon.glyphPolicy.mode, QStringLiteral("original"));
    QVERIFY(preset->icon.glyphPolicy.compatibleOnly);
    QCOMPARE(preset->icon.motion.profileId, QStringLiteral("slow-y-turn"));
    QCOMPARE(preset->icon.motion.trigger, QStringLiteral("idle"));
    QCOMPARE(preset->icon.motion.speed, 0.8);
    QCOMPARE(preset->icon.motion.intensity, 1.2);
    QCOMPARE(preset->icon.motion.magnificationRadius, 3.0);
    QCOMPARE(preset->icon.motion.magnificationFalloff, QStringLiteral("gaussian"));
    QCOMPARE(preset->icon.perStateAnimationOverrides.value(QStringLiteral("urgent")),
             QStringLiteral("glow"));

    QCOMPARE(preset->fallback.iconStyleId, QStringLiteral("plain-original"));
    QCOMPARE(preset->fallback.motionProfileId, QStringLiteral("none"));
}

void PresetDefinitionTest::iconPresetRoundTrips()
{
    const auto preset = IconPresetDefinition::fromVariantMap(iconPresetMap());
    QVERIFY(preset.has_value());
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto reloaded = IconPresetDefinition::fromVariantMap(
        throughJson(preset->toVariantMap()), &diagnostics);
    QVERIFY2(reloaded.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(*reloaded == *preset);

    // Motion defaults are optional; a preset naming only a profile is valid.
    QVariantMap minimal = withValue(
        iconPresetMap(), QStringLiteral("/icon/motion"),
        QVariantMap{{QStringLiteral("profileId"), QStringLiteral("glow")}});
    minimal = withValue(minimal, QStringLiteral("/icon/visualOverrides"), QVariant{});
    minimal = withValue(minimal, QStringLiteral("/icon/stateOverrides"), QVariant{});
    minimal = withValue(minimal, QStringLiteral("/icon/perStateAnimationOverrides"),
                        QVariant{});
    const auto defaults = IconPresetDefinition::fromVariantMap(minimal, &diagnostics);
    QVERIFY2(defaults.has_value(), qPrintable(describe(diagnostics)));
    QCOMPARE(defaults->icon.motion.trigger, QStringLiteral("hover"));
    QCOMPARE(defaults->icon.motion.speed, 1.0);
    QCOMPARE(defaults->icon.motion.magnificationFalloff, QStringLiteral("linear"));
    QVERIFY(defaults->icon.visualOverrides.isEmpty());
    QVERIFY(defaults->icon.stateOverrides.isEmpty());
    QVERIFY(defaults->icon.perStateAnimationOverrides.isEmpty());
}

void PresetDefinitionTest::iconPresetParametersAreBoundedAndRoundTrip()
{
    // ADREP-TASK-005, PD-20/21/22: a preset carries icon/tile appearance,
    // while its parameter block cannot mutate any panel or global setting.
    const QVariantMap parameters{{"iconShape", "hexagon"}, {"iconDiameter", 70},
        {"iconLogoSize", 88}, {"iconOutlineWidth", 4}, {"iconBodyColor", "#114477"},
        {"iconPedestalEnabled", true}, {"iconPedestalHeight", 33},
        {"iconTileMode", "custom"}, {"iconTileTexture", "organic"}, {"iconTileThickness", 8.0},
        {"iconTileIconOffsetX", -8.0}, {"iconTileIconOffsetY", 6.0}, {"iconTileIconScale", 85},
        {"iconTileBevel", 2.0}, {"iconTileMaterial", "metallic"}, {"iconTileElevation", 12.0}};
    const auto object = withValue(iconPresetMap(), "/icon/parameters", parameters);
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = IconPresetDefinition::fromVariantMap(object, &diagnostics);
    QVERIFY2(preset, qPrintable(describe(diagnostics)));
    QCOMPARE(preset->toVariantMap().value("icon").toMap().value("parameters").toMap(), parameters);
    const auto roundTrip = IconPresetDefinition::fromVariantMap(preset->toVariantMap(), &diagnostics);
    QVERIFY(roundTrip); QCOMPARE(*roundTrip, *preset);
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        QCOMPARE(preset->panelValues().value(it.key()), it.value());
    for (const auto &invalid : {QVariantMap{{"layout", "circular"}}, QVariantMap{{"scene3DCameraPitch", 12}},
             QVariantMap{{"iconLogoSize", 101}}, QVariantMap{{"iconPedestalEnabled", "yes"}},
             QVariantMap{{"iconShape", "Hexagon"}}, QVariantMap{{"iconBodyColor", "#broken"}}}) {
        diagnostics.clear();
        QVERIFY(!IconPresetDefinition::fromVariantMap(withValue(object, "/icon/parameters", invalid), &diagnostics));
        QVERIFY2(!diagnostics.isEmpty(), qPrintable(describe(diagnostics)));
        QVERIFY(diagnostics.first().jsonPointer.startsWith("/icon/parameters/"));
    }
}

void PresetDefinitionTest::iconPresetRejects_data()
{
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedPointer");

    const QVariant removed;
    const auto row = [](const char *name, const QString &pointer,
                        const QVariant &value, const QString &code,
                        const QString &expectedPointer = {})
    {
        QTest::newRow(name) << pointer << value << code
                            << (expectedPointer.isEmpty() ? pointer : expectedPointer);
    };

    row("unknown root field", "/panel", QVariantMap{}, "unknown-field");
    row("wrong format", "/format", QStringLiteral("org.archdock.panel-preset"),
        "unsupported-format");
    row("future schema", "/schemaVersion", 2, "unsupported-version");
    row("malformed style reference", "/icon/iconStyleId",
        QStringLiteral("Dark Orb"), "invalid-id");
    row("missing style reference", "/icon/iconStyleId", removed, "missing-field");
    row("layer override not an object", "/icon/visualOverrides/orb-pedestal",
        QStringLiteral("blue"), "invalid-type");
    row("empty layer override", "/icon/visualOverrides/orb-pedestal",
        QVariantMap{}, "invalid-value");
    row("layer asset override", "/icon/visualOverrides/orb-pedestal/asset",
        QStringLiteral("assets/x.svg"), "unknown-field");
    row("layer kind override", "/icon/visualOverrides/orb-pedestal/kind",
        QStringLiteral("asset"), "unknown-field");
    row("layer id override", "/icon/visualOverrides/orb-pedestal/id",
        QStringLiteral("other"), "unknown-field");
    row("named layer color", "/icon/visualOverrides/orb-pedestal/color",
        QStringLiteral("blue"), "invalid-color");
    row("layer opacity as text", "/icon/visualOverrides/orb-pedestal/opacity",
        QStringLiteral("0.5"), "invalid-type");
    row("layer color as number", "/icon/visualOverrides/orb-pedestal/color", 3,
        "invalid-type");
    row("malformed layer id", "/icon/visualOverrides/Bad Layer",
        QVariantMap{{QStringLiteral("opacity"), 1}}, "invalid-reference");
    row("unknown state", "/icon/stateOverrides/sleeping",
        QVariantMap{{QStringLiteral("glowOpacity"), 1}}, "invalid-reference");
    row("unknown state field", "/icon/stateOverrides/normal/tileOpacity", 1,
        "unknown-field");
    row("state id override", "/icon/stateOverrides/normal/id",
        QStringLiteral("hover"), "unknown-field");
    row("unknown glyph policy", "/icon/glyphPolicy/mode",
        QStringLiteral("invisible"), "invalid-enum");
    row("tinted without a tint", "/icon/glyphPolicy/mode",
        QStringLiteral("tinted"), "missing-field", "/icon/glyphPolicy/tint");
    row("missing glyph policy", "/icon/glyphPolicy", removed, "missing-field");
    row("missing motion profile", "/icon/motion/profileId", removed,
        "missing-field");
    row("malformed motion profile", "/icon/motion/profileId",
        QStringLiteral("Slow Turn"), "invalid-id");
    row("speed out of range", "/icon/motion/animationSpeed", 9, "invalid-value");
    row("upper-case trigger", "/icon/motion/animationTrigger",
        QStringLiteral("Hover"), "invalid-value");
    row("unknown motion field", "/icon/motion/iconSize", 64, "unknown-field");
    row("animation override for unknown state",
        "/icon/perStateAnimationOverrides/sleeping", QStringLiteral("glow"),
        "invalid-reference");
    row("malformed animation override",
        "/icon/perStateAnimationOverrides/urgent", QStringLiteral("Big Glow"),
        "invalid-id");
    row("no renderer tier", "/compatibility/rendererTiers", QVariantList{},
        "missing-field");
    row("unknown renderer tier", "/compatibility/rendererTiers",
        QVariantList{QStringLiteral("voxel")}, "invalid-enum",
        "/compatibility/rendererTiers/0");
    row("malformed style capability", "/compatibility/requiredStyleCapabilities",
        QVariantList{QStringLiteral("Bad Cap")}, "invalid-id");
    row("reduced motion as text", "/compatibility/reducedMotionSupport",
        QStringLiteral("yes"), "invalid-type");
    row("missing fallback style", "/fallback/iconStyleId", removed,
        "missing-field");
    row("unknown fallback field", "/fallback/themeId", QStringLiteral("x"),
        "unknown-field");
    row("panel theme smuggled into icon", "/icon/completeThemeId",
        QStringLiteral("obsidian-glass"), "unknown-field");
}

void PresetDefinitionTest::iconPresetRejects()
{
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedPointer);

    QVector<PresetValidationDiagnostic> diagnostics;
    const auto preset = IconPresetDefinition::fromVariantMap(
        throughJson(withValue(iconPresetMap(), pointer, value)), &diagnostics);
    QVERIFY2(!preset.has_value(), "the mutated definition was accepted");
    QVERIFY2(hasDiagnostic(diagnostics, expectedCode, expectedPointer),
             qPrintable(describe(diagnostics)));
}

void PresetDefinitionTest::iconPresetPanelValuesStayInsideTheIconLayer()
{
    const auto preset = IconPresetDefinition::fromVariantMap(iconPresetMap());
    QVERIFY(preset.has_value());
    const QVariantMap values = preset->panelValues();
    const QStringList keys = IconPresetDefinition::panelValueKeys();
    QCOMPARE(values.keys().size(), keys.size());
    for (const QString &key : keys)
    {
        QVERIFY2(values.contains(key), qPrintable(key));
        QVERIFY2(PanelSettingsSchema::isEditorField(
                     ArchDock::PanelSettingsFieldScope::Panel, key),
                 qPrintable(key));
    }
    QCOMPARE(values.value(QStringLiteral("iconStyle")).toString(),
             QStringLiteral("dark-orb"));
    QCOMPARE(values.value(QStringLiteral("iconAnimation")).toString(),
             QStringLiteral("slow-y-turn"));
    QCOMPARE(values.value(QStringLiteral("magnificationRadius")).toDouble(), 3.0);

    // An icon preset can never reach the panel's theme, layout, placement,
    // visibility, presentation or content.
    for (const QString &group : {QStringLiteral("host"), QStringLiteral("content"),
                                 QStringLiteral("placement"),
                                 QStringLiteral("visibility"),
                                 QStringLiteral("presentation"),
                                 QStringLiteral("layout"), QStringLiteral("theme"),
                                 QStringLiteral("surface")})
    {
        for (const QString &panelKey : PanelPresetDefinition::panelValueKeys(group))
        {
            // PD-20 moves icon Shape into the style's saved parameters. The
            // version-one panel preset format still carries its legacy copy
            // in "layout"; that alias must not forbid an icon preset's Shape.
            if (panelKey == QStringLiteral("iconShape")) {
                QVERIFY(keys.contains(panelKey));
                continue;
            }
            QVERIFY2(!keys.contains(panelKey), qPrintable(panelKey));
        }
    }
}

void PresetDefinitionTest::userPresetsCarryLineage()
{
    QVariantMap panel = panelPresetMap();
    panel = withValue(panel, QStringLiteral("/identity"), QVariantMap{
        {QStringLiteral("id"), QStringLiteral("user-1a2b3c4d5e6f")},
        {QStringLiteral("name"), QStringLiteral("My Chassis")},
        {QStringLiteral("builtIn"), false},
        {QStringLiteral("revision"), 4},
        {QStringLiteral("derivedFromPresetId"), QStringLiteral("fixture-chassis")},
        {QStringLiteral("sourceRevision"), 3},
    });
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto userPanel = PanelPresetDefinition::fromVariantMap(panel, &diagnostics);
    QVERIFY2(userPanel.has_value(), qPrintable(describe(diagnostics)));
    QVERIFY(!userPanel->identity.builtIn);
    QCOMPARE(userPanel->identity.derivedFromPresetId,
             QStringLiteral("fixture-chassis"));
    QCOMPARE(userPanel->identity.sourceRevision, 3);
    QCOMPARE(userPanel->panel.configuration.identity.id,
             QStringLiteral("user-1a2b3c4d5e6f"));
    const auto reloadedPanel = PanelPresetDefinition::fromVariantMap(
        throughJson(userPanel->toVariantMap()));
    QVERIFY(reloadedPanel.has_value());
    QVERIFY(*reloadedPanel == *userPanel);

    QVariantMap icon = withValue(iconPresetMap(), QStringLiteral("/identity"),
                                 QVariantMap{
        {QStringLiteral("id"), QStringLiteral("user-0f0e0d0c0b0a")},
        {QStringLiteral("name"), QStringLiteral("My Pedestal")},
        {QStringLiteral("builtIn"), false},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("derivedFromPresetId"), QStringLiteral("fixture-pedestal")},
        {QStringLiteral("sourceRevision"), 2},
    });
    const auto userIcon = IconPresetDefinition::fromVariantMap(icon, &diagnostics);
    QVERIFY2(userIcon.has_value(), qPrintable(describe(diagnostics)));
    QCOMPARE(userIcon->identity.derivedFromPresetId,
             QStringLiteral("fixture-pedestal"));
    QCOMPARE(userIcon->identity.sourceRevision, 2);
    const auto reloadedIcon = IconPresetDefinition::fromVariantMap(
        throughJson(userIcon->toVariantMap()));
    QVERIFY(reloadedIcon.has_value());
    QVERIFY(*reloadedIcon == *userIcon);

    // A user preset outside the user namespace, or a source revision without
    // a source, is malformed.
    QVERIFY(!PanelPresetDefinition::fromVariantMap(
                 withValue(panel, QStringLiteral("/identity/id"),
                           QStringLiteral("fixture-chassis"))).has_value());
    QVERIFY(!IconPresetDefinition::fromVariantMap(
                 withValue(icon, QStringLiteral("/identity/derivedFromPresetId"),
                           QVariant{})).has_value());
}

QTEST_APPLESS_MAIN(PresetDefinitionTest)

#include "PresetDefinitionTest.moc"
