#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"

#include <QSet>
#include <QTest>

#include <algorithm>

using ArchDock::PanelDefinition;
using ArchDock::PanelSettingsFieldAccess;
using ArchDock::PanelSettingsFieldDescriptor;
using ArchDock::PanelSettingsFieldScope;
using ArchDock::PanelSettingsSchema;

class PanelSettingsSchemaTest final : public QObject
{
    Q_OBJECT

private slots:
    void overlayPreferencesUseTheSettingsContract()
    {
        auto panel = ArchDock::PanelDefinition::defaults("overlays", "Overlays", "free", false);
        auto record = panel.toPersistedMap();
        for (const auto &key : {"showBadges", "showProgress", "showTemporaryStatus"}) {
            QVERIFY(ArchDock::PanelSettingsSchema::panelDescriptor(QString::fromLatin1(key)));
            record.insert(QString::fromLatin1(key), false);
        }
        const auto restored = ArchDock::PanelDefinition::fromLegacyMap(record);
        QVERIFY(restored);
        QVERIFY(!restored->content.showBadges);
        QVERIFY(!restored->content.showProgress);
        QVERIFY(!restored->content.showTemporaryStatus);
    }
    void descriptorsAreUniqueAndComplete();
    void descriptorMapsContainOnlyValidValues();
    void persistedPanelFieldsAreClassified();
    void editorCandidatesExcludeProtectedAndHiddenState();
    void runtimeProjectionContainsOnlyDeclaredConsumerValues();
    void transactionAccessPreservesLegacyPlacementWithoutExposingIt();
    void legacyMutationInterfacesAreSchemaBounded();
    void panelNormalizationMatchesTheDurableModelContract();
    void globalNormalizationIsSchemaDrivenAndStrictForChoices();
    void consumerProjectionCannotBroadenTransactionAuthority();
    void sceneQualityIsBoundedAndReversible();
    void materialsAndFlatLookRoundTrip();
    void tiltScalarsPreserveParameterMaps();
    void sceneTransformFieldsAreBoundedAndReversible();
    void folderSettingsPreserveLegacyValues();
    void everyEditorCapabilityHasAnAvailabilityRule();
};

// ADREP-TASK-001: Panel Studio offers a field only through the availability
// rule of its capability, and the rules switch over EditorCapability, which
// the compiler checks for completeness. A presented field naming a capability
// with no rule would never be shown, so it fails here instead; a name the rule
// set does not know is never offered (fail closed).
void PanelSettingsSchemaTest::everyEditorCapabilityHasAnAvailabilityRule()
{
    QSet<int> used;
    for (const PanelSettingsFieldDescriptor &field : PanelSettingsSchema::fields())
    {
        if (!field.editor.isPresented())
            continue;
        const auto capability = ArchDock::editorCapabilityFromName(field.editor.capability);
        QVERIFY2(capability.has_value(), qPrintable(field.key + QStringLiteral(": capability '")
            + field.editor.capability + QStringLiteral("' has no availability rule")));
        used.insert(static_cast<int>(*capability));
    }
    for (int value = 0; value < static_cast<int>(ArchDock::EditorCapability::Count); ++value)
    {
        const auto capability = static_cast<ArchDock::EditorCapability>(value);
        const QString name = ArchDock::editorCapabilityName(capability);
        QVERIFY2(!name.isEmpty(), qPrintable(QString::number(value)));
        QVERIFY2(ArchDock::editorCapabilityFromName(name) == capability, qPrintable(name));
        QVERIFY2(used.contains(value), qPrintable(name + QStringLiteral(" is a rule no field uses")));
    }
    for (const QString &unknown : {QString{}, QStringLiteral("native-edge-placement"),
                                   QStringLiteral("invented-capability")})
        QVERIFY2(!ArchDock::editorCapabilityFromName(unknown).has_value(), qPrintable(unknown));
}

void PanelSettingsSchemaTest::descriptorMapsContainOnlyValidValues()
{
    for (const auto &field : PanelSettingsSchema::fields())
    {
        const auto map = field.toVariantMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            QVERIFY2(it.value().isValid(), qPrintable(field.key + QLatin1Char(':') + it.key()));
        QCOMPARE(map.contains(QStringLiteral("minimumValue")), field.minimumValue.isValid());
        QCOMPARE(map.contains(QStringLiteral("maximumValue")), field.maximumValue.isValid());
    }
}

void PanelSettingsSchemaTest::folderSettingsPreserveLegacyValues()
{
    const auto legacy = PanelDefinition::fromLegacyMap({
        {QStringLiteral("id"), QStringLiteral("legacy-folder")}});
    QVERIFY(legacy.has_value());
    QVERIFY(legacy->content.folderShowNames);
    for (const QString &layout : {QStringLiteral("fan"), QStringLiteral("grid"),
         QStringLiteral("stack"), QStringLiteral("arc"), QStringLiteral("ring"),
         QStringLiteral("spiral"), QStringLiteral("physics")})
    {
        const auto definition = PanelDefinition::fromLegacyMap({
            {QStringLiteral("id"), QStringLiteral("folder-test")},
            {QStringLiteral("folderLayout"), layout},
            {QStringLiteral("folderSpeed"), 9999},
            {QStringLiteral("folderEasing"), QStringLiteral("spring")},
            {QStringLiteral("folderExpandOnClick"), false},
            {QStringLiteral("folderShowNames"), false}});
        QVERIFY(definition.has_value());
        const auto roundTrip = PanelDefinition::fromLegacyMap(definition->toPersistedMap());
        QVERIFY(roundTrip.has_value());
        QCOMPARE(roundTrip->content.folderLayout, layout);
        QCOMPARE(roundTrip->content.folderSpeed, 1200);
        QCOMPARE(roundTrip->content.folderEasing, QStringLiteral("spring"));
        QVERIFY(!roundTrip->content.folderExpandOnClick);
        QVERIFY(!roundTrip->content.folderShowNames);
    }
    for (const QString &key : {QStringLiteral("folderLayout"), QStringLiteral("folderSpeed"),
         QStringLiteral("folderEasing"), QStringLiteral("folderExpandOnClick"),
         QStringLiteral("folderShowNames"), QStringLiteral("folderFanOpening"),
         QStringLiteral("folderStackLength"), QStringLiteral("folderRingSize")})
        QVERIFY(PanelSettingsSchema::isTransactionPanelField(key));

    // ADREP-TASK-003: a free panel's folder shapes. A saved panel without
    // them opens a 90 degree fan, a stack of five and a small ring.
    QCOMPARE(legacy->content.folderFanOpening, 90);
    QCOMPARE(legacy->content.folderStackLength, 5);
    QCOMPARE(legacy->content.folderRingSize, QStringLiteral("small"));
    const auto shapes = PanelDefinition::fromLegacyMap({
        {QStringLiteral("id"), QStringLiteral("folder-shapes")},
        {QStringLiteral("folderFanOpening"), 400},
        {QStringLiteral("folderStackLength"), 1},
        {QStringLiteral("folderRingSize"), QStringLiteral("PANEL")}});
    QVERIFY(shapes.has_value());
    QCOMPARE(shapes->content.folderFanOpening, 160);
    QCOMPARE(shapes->content.folderStackLength, 2);
    QCOMPARE(shapes->content.folderRingSize, QStringLiteral("panel"));
    const auto shapesBack = PanelDefinition::fromLegacyMap(shapes->toPersistedMap());
    QVERIFY(shapesBack.has_value());
    QCOMPARE(shapesBack->content.folderFanOpening, 160);
    QCOMPARE(shapesBack->content.folderStackLength, 2);
    QCOMPARE(shapesBack->content.folderRingSize, QStringLiteral("panel"));
    const auto narrow = PanelDefinition::fromLegacyMap({
        {QStringLiteral("id"), QStringLiteral("folder-narrow")},
        {QStringLiteral("folderFanOpening"), 10},
        {QStringLiteral("folderStackLength"), 99},
        {QStringLiteral("folderRingSize"), QStringLiteral("huge")}});
    QVERIFY(narrow.has_value());
    QCOMPARE(narrow->content.folderFanOpening, 40);
    QCOMPARE(narrow->content.folderStackLength, 12);
    QCOMPARE(narrow->content.folderRingSize, QStringLiteral("small"));
}

void PanelSettingsSchemaTest::sceneQualityIsBoundedAndReversible()
{
    QVariantMap values{{QStringLiteral("id"), QStringLiteral("quality-test")},
        {QStringLiteral("surface3D"), QVariantMap{{QStringLiteral("futureData"), 7}}}};
    for (const QString &quality : {QStringLiteral("low"), QStringLiteral("high"), QStringLiteral("low")})
    {
        values.insert(QStringLiteral("scene3DQuality"), quality);
        const auto definition = PanelDefinition::fromLegacyMap(values);
        QVERIFY(definition.has_value());
        QCOMPARE(definition->surface.parameters3D.value(QStringLiteral("quality")).toString(), quality);
        QCOMPARE(definition->surface.parameters3D.value(QStringLiteral("futureData")).toInt(), 7);
        values = definition->toPersistedMap();
        QCOMPARE(values.value(QStringLiteral("scene3DQuality")).toString(), quality);
    }
    values.insert(QStringLiteral("scene3DQuality"), QStringLiteral("unbounded"));
    const auto normalized = PanelDefinition::fromLegacyMap(values);
    QVERIFY(normalized.has_value());
    QCOMPARE(normalized->surface.parameters3D
                 .value(QStringLiteral("quality")).toString(), QStringLiteral("medium"));
}

void PanelSettingsSchemaTest::materialsAndFlatLookRoundTrip()
{
    const QVariantMap flat{{"rendererTier", "procedural2d"}, {"appearance", "floating-glass"},
        {"color", "#345678"}, {"opacity", 0.43}, {"layout", "circular"}, {"layoutRadius", 137.0}};
    const auto panel = PanelDefinition::fromLegacyMap({{"id", "materials"},
        {"previousFlatLook", flat}, {"sparkleIntensity", 0.6},
        {"scene3DColor", "#987654"}, {"scene3DMaterial", "METALLIC"},
        {"scene3DTexture", "organic"}, {"surface2D", QVariantMap{{"futureData", 7}}}});
    QVERIFY(panel);
    const auto restored = PanelDefinition::fromLegacyMap(panel->toPersistedMap());
    QVERIFY(restored);
    QCOMPARE(restored->surface.parameters2D.value("previousFlatLook").toMap(), flat);
    QCOMPARE(restored->surface.parameters2D.value("sparkleIntensity").toReal(), 0.6);
    QCOMPARE(restored->surface.parameters2D.value("futureData").toInt(), 7);
    QCOMPARE(restored->surface.parameters3D.value("color").toString(), QStringLiteral("#987654"));
    QCOMPARE(restored->surface.parameters3D.value("material").toString(), QStringLiteral("metallic"));
    QCOMPARE(restored->surface.parameters3D.value("texture").toString(), QStringLiteral("organic"));
    const auto bounded = PanelDefinition::fromLegacyMap({{"id", "bounded-material"},
        {"sparkleIntensity", 8.0}, {"scene3DTexture", "../../outside.svg"}});
    QVERIFY(bounded);
    QCOMPARE(bounded->surface.parameters2D.value("sparkleIntensity").toReal(), 1.0);
    QCOMPARE(bounded->surface.parameters3D.value("texture").toString(), QStringLiteral("theme"));
    const auto snapshot = PanelSettingsSchema::flatLookValues(panel->toLegacyMap());
    QVERIFY(snapshot.contains("appearance") && snapshot.contains("layout"));
    for (const auto *key : {"id", "host", "visible", "previousFlatLook", "launchers", "surface2D"})
        QVERIFY(!snapshot.contains(QLatin1String(key)));
}

void PanelSettingsSchemaTest::tiltScalarsPreserveParameterMaps()
{
    auto definition = PanelDefinition::fromLegacyMap({
        {"id", "tilt-test"}, {"scene3DCameraPitch", 900.0}, {"bakedTilt", -900.0},
        {"scene3DCameraYaw", 900.0}, {"scene3DThickness", 900.0}, {"scene3DIconElevation", -900.0},
        {"surface3D", QVariantMap{{"futureData", 7}}},
        {"surface2_5D", QVariantMap{{"futureData", 9}}}});
    QVERIFY(definition);
    QCOMPARE(definition->surface.parameters3D.value("cameraPitch").toDouble(), 60.0);
    QCOMPARE(definition->surface.parameters3D.value("cameraYaw").toDouble(), 180.0);
    QCOMPARE(definition->surface.parameters3D.value("thickness").toDouble(), 4.0);
    QCOMPARE(definition->surface.parameters3D.value("iconElevation").toDouble(), 0.0);
    QCOMPARE(definition->surface.parameters2_5D.value("tilt").toDouble(), -60.0);
    auto values = definition->toPersistedMap();
    values.insert("scene3DCameraPitch", -35.0);
    values.insert("scene3DCameraYaw", -20.0);
    values.insert("scene3DThickness", 1.6);
    values.insert("scene3DIconElevation", 0.5);
    values.insert("bakedTilt", 8.0);
    definition = PanelDefinition::fromLegacyMap(values);
    QVERIFY(definition);
    const auto restored = PanelDefinition::fromLegacyMap(definition->toPersistedMap());
    QVERIFY(restored);
    QCOMPARE(restored->surface.parameters3D.value("cameraPitch").toDouble(), -35.0);
    QCOMPARE(restored->surface.parameters3D.value("cameraYaw").toDouble(), -20.0);
    QCOMPARE(restored->surface.parameters3D.value("thickness").toDouble(), 1.6);
    QCOMPARE(restored->surface.parameters3D.value("iconElevation").toDouble(), 0.5);
    QCOMPARE(restored->surface.parameters2_5D.value("tilt").toDouble(), 8.0);
    QCOMPARE(restored->surface.parameters3D.value("futureData").toInt(), 7);
    QCOMPARE(restored->surface.parameters2_5D.value("futureData").toInt(), 9);
    for (const QString &key : {QStringLiteral("x"), QStringLiteral("y"),
         QStringLiteral("openDelay"), QStringLiteral("closeDelay")})
        QVERIFY(PanelSettingsSchema::isEditorField(PanelSettingsFieldScope::Panel, key));
}

// AD3D-TASK-002: the 3D page's transform, view, light and motion settings are
// saved in the same 3D parameter map, bounded, and read back unchanged. Every
// 3D setting belongs to the one 3D page.
void PanelSettingsSchemaTest::sceneTransformFieldsAreBoundedAndReversible()
{
    const QList<std::tuple<QString, QString, double, double, double>> ranges{
        {"scene3DRoll", "roll", -180.0, 180.0, 0.0},
        {"scene3DPositionX", "positionX", -1.0, 1.0, 0.0},
        {"scene3DPositionY", "positionY", -1.0, 1.0, 0.0},
        {"scene3DPositionZ", "positionZ", -1.0, 1.0, 0.0},
        {"scene3DScale", "scale", 0.5, 1.25, 1.0},
        {"scene3DFieldOfView", "fieldOfView", 20.0, 70.0, 40.0},
        {"scene3DKeyLight", "keyLightBrightness", 0.0, 4.0, 1.0},
        {"scene3DFillLight", "fillLightBrightness", 0.0, 2.0, 0.4}};
    QVariantMap tooLarge{{"id", "transform-test"}, {"surface3D", QVariantMap{{"futureData", 7}}}};
    QVariantMap tooSmall = tooLarge;
    for (const auto &[key, parameter, minimum, maximum, fallback] : ranges)
    {
        const auto *descriptor = PanelSettingsSchema::panelDescriptor(key);
        QVERIFY2(descriptor, qPrintable(key));
        QCOMPARE(descriptor->editor.section, QStringLiteral("panels-3d"));
        QCOMPARE(descriptor->editor.capability, QStringLiteral("scene3d-quality"));
        QCOMPARE(descriptor->defaultValue.toDouble(), fallback);
        QVERIFY(PanelSettingsSchema::isTransactionPanelField(key));
        tooLarge.insert(key, maximum + 1000.0);
        tooSmall.insert(key, minimum - 1000.0);
    }
    const auto large = PanelDefinition::fromLegacyMap(tooLarge);
    const auto small = PanelDefinition::fromLegacyMap(tooSmall);
    QVERIFY(large && small);
    for (const auto &[key, parameter, minimum, maximum, fallback] : ranges)
    {
        QCOMPARE(large->surface.parameters3D.value(parameter).toDouble(), maximum);
        QCOMPARE(small->surface.parameters3D.value(parameter).toDouble(), minimum);
    }
    QCOMPARE(large->surface.parameters3D.value("futureData").toInt(), 7);

    // Values inside the ranges and both switches survive a save and reload.
    QVariantMap values{{"id", "transform-test"}, {"scene3DRoll", -12.5},
        {"scene3DPositionX", 0.25}, {"scene3DPositionY", -0.5}, {"scene3DPositionZ", 0.1},
        {"scene3DScale", 0.85}, {"scene3DFieldOfView", 55.0}, {"scene3DKeyLight", 2.5},
        {"scene3DFillLight", 0.75}, {"scene3DTransitions", false}, {"scene3DFloat", true}};
    const auto saved = PanelDefinition::fromLegacyMap(values);
    QVERIFY(saved);
    const auto restored = PanelDefinition::fromLegacyMap(saved->toPersistedMap());
    QVERIFY(restored);
    const QVariantMap persisted = restored->toPersistedMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if (it.key() != QStringLiteral("id"))
            QVERIFY2(persisted.value(it.key()) == it.value(), qPrintable(it.key()));
    QCOMPARE(restored->surface.parameters3D.value("transitions").toBool(), false);
    QCOMPARE(restored->surface.parameters3D.value("float").toBool(), true);

    // A panel saved before these settings existed keeps its look: none of them
    // is written until the person changes it.
    const auto legacy = PanelDefinition::fromLegacyMap({{"id", "legacy-3d"},
        {"scene3DCameraPitch", 30.0}});
    QVERIFY(legacy);
    for (const auto &[key, parameter, minimum, maximum, fallback] : ranges)
        QVERIFY2(!legacy->surface.parameters3D.contains(parameter), qPrintable(parameter));

    for (const QString &key : {QStringLiteral("scene3DQuality"), QStringLiteral("scene3DCameraPitch"),
             QStringLiteral("scene3DCameraYaw"), QStringLiteral("scene3DThickness"),
             QStringLiteral("scene3DIconElevation"), QStringLiteral("scene3DTransitions"),
             QStringLiteral("scene3DFloat")})
        QCOMPARE(PanelSettingsSchema::panelDescriptor(key)->editor.section, QStringLiteral("panels-3d"));
}

void PanelSettingsSchemaTest::descriptorsAreUniqueAndComplete()
{
    QSet<QString> identities;
    for (const auto &field : PanelSettingsSchema::fields())
    {
        const QString identity = QString::number(static_cast<int>(field.scope)) +
            QLatin1Char(':') + field.key;
        QVERIFY2(!field.key.isEmpty(), "schema field key must not be empty");
        QVERIFY2(!field.persistencePath.isEmpty(),
                 qPrintable(QStringLiteral("missing persistence path for %1")
                                .arg(identity)));
        QVERIFY2(!identities.contains(identity),
                 qPrintable(QStringLiteral("duplicate schema field %1").arg(identity)));
        identities.insert(identity);

        if (field.access == PanelSettingsFieldAccess::Editor &&
            field.editor.isPresented())
        {
            QVERIFY2(field.runtimeConsumer,
                     qPrintable(QStringLiteral("presented field has no runtime consumer: %1")
                                    .arg(identity)));
            QVERIFY(!field.editor.consumers.isEmpty());
            QVERIFY(!field.editor.control.isEmpty());
            QVERIFY(!field.editor.section.isEmpty());
        }
    }

    QVERIFY(PanelSettingsSchema::keys(PanelSettingsFieldScope::Panel).size() > 100);
    QCOMPARE(PanelSettingsSchema::keys(PanelSettingsFieldScope::Global).size(), 29);
}

void PanelSettingsSchemaTest::persistedPanelFieldsAreClassified()
{
    PanelDefinition definition = PanelDefinition::defaults(
        QStringLiteral("schema-test"),
        QStringLiteral("Schema test"),
        QStringLiteral("free"),
        false);
    definition.placement.offset = 4;
    definition.placement.floatingMargin = 8;
    definition.placement.thickness = 72;
    definition.placement.lengthMode = QStringLiteral("fixed");
    definition.placement.minimumLength = 48;
    definition.placement.maximumLength = 800;
    definition.presetOrigin = ArchDock::PanelPresetOrigin{};
    definition.extensions.insert(QStringLiteral("futureField"), 7);

    const QVariantMap persisted = definition.toPersistedMap();
    for (auto it = persisted.cbegin(); it != persisted.cend(); ++it)
    {
        if (it.key() == QStringLiteral("futureField"))
        {
            continue;
        }
        QVERIFY2(PanelSettingsSchema::isKnownPanelField(it.key()),
                 qPrintable(QStringLiteral("unclassified persisted field: %1")
                                .arg(it.key())));
    }
}

void PanelSettingsSchemaTest::editorCandidatesExcludeProtectedAndHiddenState()
{
    QVariantMap record{
        {QStringLiteral("id"), QStringLiteral("bottom")},
        {QStringLiteral("builtIn"), true},
        {QStringLiteral("nativeOwnershipToken"), QStringLiteral("secret")},
        {QStringLiteral("screenId"), QStringLiteral("forged")},
        {QStringLiteral("visible"), true},
        {QStringLiteral("opacity"), 0.75},
        {QStringLiteral("iconStyle"), QStringLiteral("plain-original")},
        {QStringLiteral("iconThemeId"), QStringLiteral("forged-theme")},
        {QStringLiteral("glowIntensity"), 1.4},
        {QStringLiteral("physicsEnabled"), true},
        {QStringLiteral("folderLayout"), QStringLiteral("fan")},
        {QStringLiteral("pathAnchor"), QStringLiteral("center")},
        {QStringLiteral("surface3D"), QVariantMap{{QStringLiteral("depth"), 12}}},
    };

    const QVariantMap editor = PanelSettingsSchema::editorValues(
        PanelSettingsFieldScope::Panel, record);
    QVERIFY(editor.contains(QStringLiteral("visible")));
    QVERIFY(editor.contains(QStringLiteral("opacity")));
    QVERIFY(editor.contains(QStringLiteral("glowIntensity")));
    QCOMPARE(editor.value(QStringLiteral("iconStyle")).toString(),
             QStringLiteral("plain-original"));
    QVERIFY(!editor.contains(QStringLiteral("iconThemeId")));
    QVERIFY(!editor.contains(QStringLiteral("id")));
    QVERIFY(!editor.contains(QStringLiteral("builtIn")));
    QVERIFY(!editor.contains(QStringLiteral("nativeOwnershipToken")));
    QVERIFY(!editor.contains(QStringLiteral("screenId")));
    QVERIFY(!editor.contains(QStringLiteral("physicsEnabled")));
    QCOMPARE(editor.value(QStringLiteral("folderLayout")).toString(), QStringLiteral("fan"));
    QVERIFY(!editor.contains(QStringLiteral("pathAnchor")));
    QVERIFY(!editor.contains(QStringLiteral("surface3D")));

    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("physicsEnabled")));
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("surface3D")));

    // Presentation left the hidden-state group when TASK-0032 Phase C gave it
    // a renderer. physicsEnabled and surface3D stay hidden because the features
    // behind them still do not exist; presentation is now a real editor field,
    // and it is gated by capability rather than by concealment.
    for (const QString &key : {QStringLiteral("presentationMode"),
                               QStringLiteral("presentationTrigger"),
                               QStringLiteral("collapseMechanism"),
                               QStringLiteral("collapseAxis"),
                               QStringLiteral("revealHandle")})
    {
        QVERIFY2(PanelSettingsSchema::isTransactionPanelField(key),
                 qPrintable(key));
        QVERIFY2(PanelSettingsSchema::isEditorField(
                     PanelSettingsFieldScope::Panel, key),
                 qPrintable(key));
        const PanelSettingsFieldDescriptor *descriptor =
            PanelSettingsSchema::panelDescriptor(key);
        QVERIFY2(descriptor, qPrintable(key));
        // Contract change, ADREP-TASK-001 (OF-07, CF-05): opening and
        // closing is shown on one page, Animations > Opening and closing.
        QCOMPARE(descriptor->editor.section, QStringLiteral("panels-animations"));
        QCOMPARE(descriptor->editor.capability,
                 QStringLiteral("presentation-mechanism"));
        QVERIFY2(!descriptor->choices.isEmpty(), qPrintable(key));
        QVERIFY2(descriptor->choices.contains(
                     descriptor->defaultValue.toString()),
                 qPrintable(key));
    }
}

void PanelSettingsSchemaTest::runtimeProjectionContainsOnlyDeclaredConsumerValues()
{
    const QVariantMap record{
        {QStringLiteral("id"), QStringLiteral("bottom")},
        {QStringLiteral("nativeOwnershipToken"), QStringLiteral("secret")},
        {QStringLiteral("visible"), true},
        {QStringLiteral("opacity"), 0.74},
        {QStringLiteral("glowIntensity"), 1.25},
        {QStringLiteral("iconStyle"), QStringLiteral("plain-original")},
        {QStringLiteral("iconThemeId"), QStringLiteral("plain-original")},
        {QStringLiteral("themeAsset"), QStringLiteral("file:///managed.png")},
        {QStringLiteral("surface3D"), QVariantMap{{QStringLiteral("depth"), 12}}},
        {QStringLiteral("themeStatus"), QStringLiteral("diagnostic")},
    };

    const QVariantMap runtime = PanelSettingsSchema::runtimeValues(
        PanelSettingsFieldScope::Panel, record);
    QCOMPARE(runtime.value(QStringLiteral("visible")).toBool(), true);
    QCOMPARE(runtime.value(QStringLiteral("opacity")).toReal(), 0.74);
    QCOMPARE(runtime.value(QStringLiteral("glowIntensity")).toReal(), 1.25);
    QCOMPARE(runtime.value(QStringLiteral("iconStyle")).toString(),
             QStringLiteral("plain-original"));
    QVERIFY(!runtime.contains(QStringLiteral("iconThemeId")));
    QCOMPARE(runtime.value(QStringLiteral("themeAsset")).toString(),
             QStringLiteral("file:///managed.png"));
    QVERIFY(!runtime.contains(QStringLiteral("id")));
    QVERIFY(!runtime.contains(QStringLiteral("nativeOwnershipToken")));
    QVERIFY(!runtime.contains(QStringLiteral("surface3D")));
    QVERIFY(!runtime.contains(QStringLiteral("themeStatus")));
}

void PanelSettingsSchemaTest::transactionAccessPreservesLegacyPlacementWithoutExposingIt()
{
    QVERIFY(PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("floatingMargin")));
    QVERIFY(!PanelSettingsSchema::isEditorField(
        PanelSettingsFieldScope::Panel, QStringLiteral("floatingMargin")));
    QVERIFY(!PanelSettingsSchema::editorValues(
        PanelSettingsFieldScope::Panel,
        {{QStringLiteral("floatingMargin"), 12}})
                 .contains(QStringLiteral("floatingMargin")));
}

void PanelSettingsSchemaTest::legacyMutationInterfacesAreSchemaBounded()
{
    const auto supportsPanel = [](const QString &key, const QString &interfaceName)
    {
        return PanelSettingsSchema::supportsMutationInterface(
            PanelSettingsFieldScope::Panel, key, interfaceName);
    };
    const auto supportsGlobal = [](const QString &key, const QString &interfaceName)
    {
        return PanelSettingsSchema::supportsMutationInterface(
            PanelSettingsFieldScope::Global, key, interfaceName);
    };

    QVERIFY(supportsPanel(QStringLiteral("opacity"),
                          QStringLiteral("dock-configuration")));
    QVERIFY(supportsPanel(QStringLiteral("acceptDrops"),
                          QStringLiteral("dock-configuration")));
    QVERIFY(!supportsPanel(QStringLiteral("layout"),
                           QStringLiteral("dock-configuration")));
    QVERIFY(!supportsPanel(QStringLiteral("physicsEnabled"),
                           QStringLiteral("dock-configuration")));
    QVERIFY(!supportsPanel(QStringLiteral("id"),
                           QStringLiteral("dock-configuration")));

    QVERIFY(supportsPanel(QStringLiteral("screen"),
                          QStringLiteral("native-placement")));
    QVERIFY(supportsPanel(QStringLiteral("floatingMargin"),
                          QStringLiteral("native-placement")));
    QVERIFY(!supportsPanel(QStringLiteral("screenId"),
                           QStringLiteral("native-placement")));
    QVERIFY(!supportsPanel(QStringLiteral("pathAnchor"),
                           QStringLiteral("native-placement")));
    QVERIFY(supportsPanel(QStringLiteral("screen"),
                          QStringLiteral("panel-screen")));
    QVERIFY(supportsPanel(QStringLiteral("type"),
                          QStringLiteral("native-type")));
    QVERIFY(supportsPanel(QStringLiteral("visible"),
                          QStringLiteral("native-visibility")));
    QVERIFY(supportsPanel(QStringLiteral("visibilityMode"),
                          QStringLiteral("native-visibility")));

    QVERIFY(supportsGlobal(QStringLiteral("showTooltips"),
                           QStringLiteral("dock-configuration")));
    QVERIFY(!supportsGlobal(QStringLiteral("alignment"),
                            QStringLiteral("dock-configuration")));
}

void PanelSettingsSchemaTest::panelNormalizationMatchesTheDurableModelContract()
{
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("edge"), QStringLiteral(" RIGHT ")).toString(),
             QStringLiteral("right"));
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("edge"), QStringLiteral("unknown")).toString(),
             QStringLiteral("bottom"));
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("iconSize"), 999).toInt(),
             128);
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("iconStyle"),
                 QStringLiteral("not-installed")).toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("iconStyle"),
                 QStringLiteral("neon-orange")).toString(),
             QStringLiteral("neon-orange"));
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("layoutAngle"), -900.0).toReal(),
             -180.0);
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("glowIntensity"), 9.0).toReal(),
             2.0);
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("glowIntensity"), -1.0).toReal(),
             0.0);
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("nativePanelId"), -8).toInt(),
             -1);
    QCOMPARE(PanelSettingsSchema::normalizePanelValue(
                 QStringLiteral("contentAppIds"),
                 QStringList{QStringLiteral(" app "), QStringLiteral("app"), {}})
                 .toStringList(),
             QStringList{QStringLiteral("app")});

    const auto *glow = PanelSettingsSchema::panelDescriptor(
        QStringLiteral("glowIntensity"));
    QVERIFY(glow);
    QCOMPARE(glow->editor.control, QStringLiteral("slider"));
    QCOMPARE(glow->editor.capability, QStringLiteral("dynamic-glow"));
    QCOMPARE(glow->minimumValue.toReal(), 0.0);
    QCOMPARE(glow->maximumValue.toReal(), 2.0);

    const auto *iconStyle = PanelSettingsSchema::panelDescriptor(
        QStringLiteral("iconStyle"));
    QVERIFY(iconStyle);
    QCOMPARE(iconStyle->access, PanelSettingsFieldAccess::Editor);
    QCOMPARE(iconStyle->defaultValue.toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(iconStyle->editor.control, QStringLiteral("combo"));
    QVERIFY(iconStyle->editor.consumers.contains(QStringLiteral("studio")));
    QVERIFY(iconStyle->editor.consumers.contains(QStringLiteral("native")));
    QVERIFY(PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("iconStyle")));
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("iconThemeId")));
}

void PanelSettingsSchemaTest::globalNormalizationIsSchemaDrivenAndStrictForChoices()
{
    QVariantMap current;
    for (const QString &key : PanelSettingsSchema::keys(
             PanelSettingsFieldScope::Global))
    {
        const auto *field = PanelSettingsSchema::globalDescriptor(key);
        QVERIFY(field);
        current.insert(key, field->defaultValue);
    }

    QVariantMap candidate;
    QString error;
    QVERIFY(PanelSettingsSchema::normalizeGlobalValues(
        current,
        {{QStringLiteral("magnification"), 9.0},
         {QStringLiteral("showTooltips"), false},
         {QStringLiteral("monitorIndex"), 99}},
        2,
        &candidate,
        &error));
    QCOMPARE(candidate.value(QStringLiteral("magnification")).toReal(), 2.4);
    QCOMPARE(candidate.value(QStringLiteral("showTooltips")).toBool(), false);
    QCOMPARE(candidate.value(QStringLiteral("monitorIndex")).toInt(), 2);
    QCOMPARE(candidate.size(), 29);

    QVERIFY(!PanelSettingsSchema::normalizeGlobalValues(
        current,
        {{QStringLiteral("alignment"), QStringLiteral("forged")}},
        2,
        &candidate,
        &error));
    QVERIFY(error.contains(QStringLiteral("alignment")));

    QVERIFY(!PanelSettingsSchema::normalizeGlobalValues(
        current,
        {{QStringLiteral("notASetting"), true}},
        2,
        &candidate,
        &error));
    QVERIFY(error.contains(QStringLiteral("notASetting")));
}

void PanelSettingsSchemaTest::consumerProjectionCannotBroadenTransactionAuthority()
{
    const QVariantList studio = PanelSettingsSchema::editorDescriptors(
        PanelSettingsFieldScope::Panel, QStringLiteral("studio"));
    const QVariantList native = PanelSettingsSchema::editorDescriptors(
        PanelSettingsFieldScope::Panel, QStringLiteral("native"));
    QVERIFY(studio.size() > native.size());
    const auto containsKey = [](const QVariantList &fields, const QString &key)
    {
        return std::any_of(fields.cbegin(), fields.cend(), [&key](const QVariant &value)
        {
            return value.toMap().value(QStringLiteral("key")).toString() == key;
        });
    };
    QVERIFY(containsKey(studio, QStringLiteral("iconStyle")));
    QVERIFY(containsKey(native, QStringLiteral("iconStyle")));

    for (const QVariantList &projection : {studio, native})
    {
        for (const QVariant &value : projection)
        {
            const QString key = value.toMap().value(QStringLiteral("key")).toString();
            QVERIFY(PanelSettingsSchema::isEditorField(
                PanelSettingsFieldScope::Panel, key));
            QVERIFY(key != QStringLiteral("id"));
            QVERIFY(key != QStringLiteral("screenId"));
            QVERIFY(key != QStringLiteral("nativeOwnershipToken"));
        }
    }
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(QStringLiteral("id")));
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(QStringLiteral("screenId")));
}

QTEST_MAIN(PanelSettingsSchemaTest)

#include "PanelSettingsSchemaTest.moc"
