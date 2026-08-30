#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"

#include <QSet>
#include <QTest>

using ArchDock::PanelDefinition;
using ArchDock::PanelSettingsFieldAccess;
using ArchDock::PanelSettingsFieldScope;
using ArchDock::PanelSettingsSchema;

class PanelSettingsSchemaTest final : public QObject
{
    Q_OBJECT

private slots:
    void descriptorsAreUniqueAndComplete();
    void persistedPanelFieldsAreClassified();
    void editorCandidatesExcludeProtectedAndHiddenState();
    void runtimeProjectionContainsOnlyDeclaredConsumerValues();
    void transactionAccessPreservesLegacyPlacementWithoutExposingIt();
    void legacyMutationInterfacesAreSchemaBounded();
    void panelNormalizationMatchesTheDurableModelContract();
    void globalNormalizationIsSchemaDrivenAndStrictForChoices();
    void consumerProjectionCannotBroadenTransactionAuthority();
};

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
    QVERIFY(!editor.contains(QStringLiteral("id")));
    QVERIFY(!editor.contains(QStringLiteral("builtIn")));
    QVERIFY(!editor.contains(QStringLiteral("nativeOwnershipToken")));
    QVERIFY(!editor.contains(QStringLiteral("screenId")));
    QVERIFY(!editor.contains(QStringLiteral("physicsEnabled")));
    QVERIFY(!editor.contains(QStringLiteral("folderLayout")));
    QVERIFY(!editor.contains(QStringLiteral("pathAnchor")));
    QVERIFY(!editor.contains(QStringLiteral("surface3D")));

    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("physicsEnabled")));
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("surface3D")));
    QVERIFY(!PanelSettingsSchema::isTransactionPanelField(
        QStringLiteral("presentationMode")));
}

void PanelSettingsSchemaTest::runtimeProjectionContainsOnlyDeclaredConsumerValues()
{
    const QVariantMap record{
        {QStringLiteral("id"), QStringLiteral("bottom")},
        {QStringLiteral("nativeOwnershipToken"), QStringLiteral("secret")},
        {QStringLiteral("visible"), true},
        {QStringLiteral("opacity"), 0.74},
        {QStringLiteral("glowIntensity"), 1.25},
        {QStringLiteral("themeAsset"), QStringLiteral("file:///managed.png")},
        {QStringLiteral("surface3D"), QVariantMap{{QStringLiteral("depth"), 12}}},
        {QStringLiteral("themeStatus"), QStringLiteral("diagnostic")},
    };

    const QVariantMap runtime = PanelSettingsSchema::runtimeValues(
        PanelSettingsFieldScope::Panel, record);
    QCOMPARE(runtime.value(QStringLiteral("visible")).toBool(), true);
    QCOMPARE(runtime.value(QStringLiteral("opacity")).toReal(), 0.74);
    QCOMPARE(runtime.value(QStringLiteral("glowIntensity")).toReal(), 1.25);
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
