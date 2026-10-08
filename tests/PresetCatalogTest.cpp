#include "model/PanelCapabilityResolver.h"
#include "presets/PresetCapabilityResolver.h"
#include "presets/PresetCatalog.h"
#include "presets/UserPresetStore.h"
#include "persistence/ConfigurationBackup.h"

#include "PresetTestSupport.h"

#include <QTemporaryDir>
#include <QTest>

#include <type_traits>

using ArchDock::AnimationProfileCatalog;
using ArchDock::IconPresetCatalog;
using ArchDock::IconPresetDefinition;
using ArchDock::IconStyleStore;
using ArchDock::PanelCapabilityResolver;
using ArchDock::PanelPresetCatalog;
using ArchDock::PanelPresetDefinition;
using ArchDock::PresetCapabilityResolver;
using ArchDock::PresetCompatibility;
using ArchDock::RendererAvailability;
using ArchDock::RendererTier;
using ArchDock::PresetIdentity;
using ArchDock::PresetReferenceContext;
using ArchDock::PresetValidationDiagnostic;
using ArchDock::UserPresetStore;

using namespace PresetTestSupport;

namespace
{

// Writes `<id>.json` for every definition plus an index listing them in
// order. Returns the index path.
template<typename Catalog>
QString writeCatalog(const QString &directory, const QList<QVariantMap> &definitions)
{
    QVariantList ids;
    for (const QVariantMap &definition : definitions)
    {
        const QString id = definition.value(QStringLiteral("identity")).toMap()
            .value(QStringLiteral("id")).toString();
        ids.append(id);
        if (!writeBytes(QDir(directory).filePath(id + QStringLiteral(".json")),
                        toJson(definition)))
        {
            return {};
        }
    }
    const QString indexPath = QDir(directory).filePath(Catalog::indexFileName());
    return writeBytes(indexPath, toJson({
               {QStringLiteral("format"), Catalog::indexFormat()},
               {QStringLiteral("version"), 1},
               {QStringLiteral("presets"), ids},
           })) ? indexPath : QString{};
}

QVariantMap withId(const QVariantMap &definition, const QString &id)
{
    return withValue(definition, QStringLiteral("/identity/id"), id);
}

// A baked 2.5D free-ring preset, built from the chassis fixture: the second
// renderer family the reference checks need.
QVariantMap bakedRingPresetMap()
{
    QVariantMap map = withId(panelPresetMap(), QStringLiteral("fixture-ring"));
    map = withValue(map, QStringLiteral("/preview"), QVariantMap{
        {QStringLiteral("previewMode"), QStringLiteral("free")},
        {QStringLiteral("rendererTier"), QStringLiteral("baked2.5d")},
        {QStringLiteral("fallbackTier"), QStringLiteral("procedural2d")},
        {QStringLiteral("deterministicPreviewSeed"), QStringLiteral("fixture-ring-v1")},
    });
    map = withValue(map, QStringLiteral("/compatibility"), QVariantMap{
        {QStringLiteral("hostKinds"), QStringList{QStringLiteral("free-desktop")}},
        {QStringLiteral("orientations"), QStringList{QStringLiteral("free")}},
        {QStringLiteral("layouts"), QStringList{QStringLiteral("ring")}},
        {QStringLiteral("requiredCapabilities"), QStringList{}},
        {QStringLiteral("optionalCapabilities"),
         QStringList{QStringLiteral("baked2.5d")}},
    });
    QVariantMap panel{
        {QStringLiteral("host"), QVariantMap{{QStringLiteral("edge"),
                                               QStringLiteral("free")}}},
        {QStringLiteral("content"), QVariantMap{{QStringLiteral("type"),
                                                  QStringLiteral("launcher")}}},
        {QStringLiteral("layout"), QVariantMap{
             {QStringLiteral("layout"), QStringLiteral("ring")},
             {QStringLiteral("layoutRadius"), 300},
             {QStringLiteral("iconShape"), QStringLiteral("circle")},
         }},
        {QStringLiteral("theme"), QVariantMap{
             {QStringLiteral("completeThemeId"), QStringLiteral("ring-platform-blue")},
             {QStringLiteral("rendererTier"), QStringLiteral("baked2.5d")},
         }},
        {QStringLiteral("motion"), QVariantMap{
             {QStringLiteral("iconAnimation"), QStringLiteral("glow")},
         }},
        {QStringLiteral("recommendedIconPresetId"),
         QStringLiteral("fixture-pedestal")},
    };
    // A ring cannot fall back to a horizontal chassis; the procedural ring
    // theme is the fallback that keeps this preset's layout.
    map = withValue(map, QStringLiteral("/fallback/themeId"),
                    QStringLiteral("holographic-ring"));
    return withValue(map, QStringLiteral("/panel"), panel);
}

// Finds a layer by id in a style projection, whichever role it sits in.
QVariantMap layerById(const QVariantMap &projection, const QString &id)
{
    const QVariantMap layers = projection.value(QStringLiteral("layers")).toMap();
    for (auto role = layers.cbegin(); role != layers.cend(); ++role)
    {
        QVariantList candidates = role.value().toList();
        if (candidates.isEmpty())
        {
            candidates.append(role.value());
        }
        for (const QVariant &candidate : std::as_const(candidates))
        {
            if (candidate.toMap().value(QStringLiteral("id")).toString() == id)
            {
                return candidate.toMap();
            }
        }
    }
    return {};
}

QVariantMap stateById(const QVariantMap &projection, const QString &id)
{
    const QVariantList states = projection.value(QStringLiteral("states")).toList();
    for (const QVariant &state : states)
    {
        if (state.toMap().value(QStringLiteral("id")).toString() == id)
        {
            return state.toMap();
        }
    }
    return {};
}

QVector<RendererAvailability> renderersWithout(RendererTier tier)
{
    QVector<RendererAvailability> renderers =
        PresetCapabilityResolver::idealRenderers();
    for (RendererAvailability &renderer : renderers)
    {
        if (renderer.tier == tier)
        {
            renderer.installed = false;
        }
    }
    return renderers;
}

}

class PresetCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void catalogLoadsListedDefinitionsInOrder();
    void catalogRejects_data();
    void catalogRejects();
    void iconReferencesMustResolve_data();
    void iconReferencesMustResolve();
    void panelReferencesMustResolve_data();
    void panelReferencesMustResolve();
    void panelAndIconCatalogsAreSeparate();
    void userStoreStartsEmptyAndReadingWritesNothing();
    void unmarkedStoreAdoptionIsRecoverable();
    void derivedPresetGetsANewUserIdAndRoundTrips();
    void savingAgainKeepsTheIdAndAdvancesTheRevision();
    void builtInDefinitionsAreNeverWritten();
    void removeOnlyTouchesUserPresets();
    void damagedUserFilesAreReportedAndSkipped();
    void newerStoreVersionIsLeftUntouched();
    void invalidDefinitionsAreNotStored();
    void untouchedStyleIsTheStoreProjection();
    void mergingKeepsEverythingTheOverrideDoesNotName();
    void overridesChangeOnlyWhatTheyName();
    void overridesTheStyleValidatorRejects_data();
    void overridesTheStyleValidatorRejects();
    void panelPresetResolvesAsDeclared();
    void missingRendererTierSelectsTheThemeFallbackTier();
    void unusableThemeSelectsTheDeclaredFallbackTheme();
    void presetWithoutAUsableFallbackIsIncompatible();
    void iconPresetFallsBackToItsDeclaredSafeReferences();
    void structuralValidationRejects_data();
    void structuralValidationRejects();
    void everyBuiltInDefinitionFileIsValid();
    void builtInCatalogsAreExactAndValid();
    void builtInPresetsAreDistinctAndCoverTheirFamilies();
    void builtInFilesSurviveEveryStoreOperation();

private:
    [[nodiscard]] PresetReferenceContext context(
        const IconPresetCatalog *icons = nullptr) const;
    [[nodiscard]] IconPresetDefinition iconFixture() const;
    [[nodiscard]] PanelPresetDefinition panelFixture() const;

    QVariantList m_themes;
    std::optional<IconStyleStore> m_styles;
    std::optional<AnimationProfileCatalog> m_profiles;
};

PresetReferenceContext PresetCatalogTest::context(
    const IconPresetCatalog *icons) const
{
    PresetReferenceContext result;
    result.themeCatalog = m_themes;
    result.iconStyles = &*m_styles;
    result.animationProfiles = &*m_profiles;
    if (icons)
    {
        result.iconPresetById = [icons](const QString &id)
        {
            return icons->presetById(id);
        };
    }
    return result;
}

IconPresetDefinition PresetCatalogTest::iconFixture() const
{
    return *IconPresetDefinition::fromVariantMap(iconPresetMap());
}

PanelPresetDefinition PresetCatalogTest::panelFixture() const
{
    return *PanelPresetDefinition::fromVariantMap(panelPresetMap());
}

void PresetCatalogTest::initTestCase()
{
    // The real shipped resources: presets are validated against these, never
    // against stand-ins.
    m_themes = parseJson(readBytes(
        QStringLiteral(ARCHDOCK_SOURCE_THEME_CATALOG_PATH)))
        .value(QStringLiteral("themes")).toList();
    QVERIFY(!m_themes.isEmpty());

    auto styles = IconStyleStore::loadCatalog(
        QStringLiteral(ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH),
        QStringLiteral(ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT));
    QVERIFY2(styles.isValid(), qPrintable(styles.primaryMessage()));
    m_styles = std::move(styles.store);

    auto profiles = AnimationProfileCatalog::loadCatalog(
        QStringLiteral(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH));
    QVERIFY2(profiles.isValid(), qPrintable(profiles.primaryMessage()));
    m_profiles = std::move(profiles.catalog);
}

void PresetCatalogTest::catalogLoadsListedDefinitionsInOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QVariantMap second = withId(iconPresetMap(), QStringLiteral("zeta-first"));
    const QVariantMap first = withId(iconPresetMap(), QStringLiteral("alpha-second"));
    const QString iconIndex = writeCatalog<IconPresetCatalog>(
        directory.filePath(QStringLiteral("icons")), {second, first, iconPresetMap()});
    QVERIFY(!iconIndex.isEmpty());

    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndex, context());
    QVERIFY2(icons.isValid(), qPrintable(describe(icons.diagnostics)));
    // Catalog order is the index order, not file-name order.
    QCOMPARE(icons.catalog->presetIds(),
             (QStringList{QStringLiteral("zeta-first"),
                          QStringLiteral("alpha-second"),
                          QStringLiteral("fixture-pedestal")}));
    QVERIFY(icons.catalog->contains(QStringLiteral("alpha-second")));
    QVERIFY(!icons.catalog->contains(QStringLiteral("ghost")));
    QVERIFY(icons.catalog->presetById(QStringLiteral("ghost")) == nullptr);
    QCOMPARE(icons.catalog->presetById(QStringLiteral("zeta-first"))->icon.iconStyleId,
             QStringLiteral("dark-orb"));
    QCOMPARE(icons.catalog->indexPath(), QFileInfo(iconIndex).absoluteFilePath());
    QCOMPARE(QFileInfo(icons.catalog->definitionPath(
                           QStringLiteral("zeta-first"))).fileName(),
             QStringLiteral("zeta-first.json"));

    const QString panelIndex = writeCatalog<PanelPresetCatalog>(
        directory.filePath(QStringLiteral("panels")),
        {panelPresetMap(), bakedRingPresetMap()});
    const auto panels = PanelPresetCatalog::loadBuiltIn(
        panelIndex, context(&*icons.catalog));
    QVERIFY2(panels.isValid(), qPrintable(describe(panels.diagnostics)));
    QCOMPARE(panels.catalog->presetIds(),
             (QStringList{QStringLiteral("fixture-chassis"),
                          QStringLiteral("fixture-ring")}));
    QVERIFY(*panels.catalog->presetById(QStringLiteral("fixture-chassis")) ==
            panelFixture());
}

void PresetCatalogTest::catalogRejects_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");

    QTest::newRow("missing index") << "missing-index" << "missing-file";
    QTest::newRow("index is not JSON") << "index-not-json" << "invalid-json";
    QTest::newRow("wrong index format") << "wrong-format" << "unsupported-format";
    QTest::newRow("future index version") << "future-version" << "unsupported-version";
    QTest::newRow("unknown index field") << "unknown-index-key" << "unknown-field";
    QTest::newRow("presets is not an array") << "presets-not-array" << "invalid-type";
    QTest::newRow("id listed twice") << "duplicate-id" << "duplicate-id";
    QTest::newRow("user id listed") << "user-id" << "invalid-id";
    QTest::newRow("malformed id listed") << "malformed-id" << "invalid-id";
    QTest::newRow("listed definition missing") << "missing-definition" << "missing-file";
    QTest::newRow("unlisted definition present") << "unlisted-definition"
                                                 << "unlisted-definition";
    QTest::newRow("definition id differs") << "identity-mismatch"
                                           << "catalog-identity-mismatch";
    QTest::newRow("user definition in catalog") << "user-definition" << "invalid-id";
    QTest::newRow("malformed definition") << "invalid-definition" << "unknown-field";
    QTest::newRow("panel definition in icon catalog") << "panel-definition"
                                                      << "unsupported-format";
    QTest::newRow("definition escapes by symlink") << "symlink-escape" << "unsafe-path";
    QTest::newRow("oversized definition") << "oversized-definition" << "file-too-large";
    QTest::newRow("definition is a directory") << "directory-definition"
                                               << "file-not-regular";
    QTest::newRow("unresolved reference") << "unresolved-reference"
                                          << "invalid-reference";
}

void PresetCatalogTest::catalogRejects()
{
    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString directory = root.filePath(QStringLiteral("icons"));
    const QString indexPath = QDir(directory).filePath(
        IconPresetCatalog::indexFileName());
    const QString id = QStringLiteral("fixture-pedestal");
    const QString definitionPath = QDir(directory).filePath(id + QStringLiteral(".json"));
    QVERIFY(!writeCatalog<IconPresetCatalog>(directory, {iconPresetMap()}).isEmpty());

    const auto writeIndex = [&](const QVariantMap &index)
    {
        return writeBytes(indexPath, toJson(index));
    };
    const QVariantMap validIndex{
        {QStringLiteral("format"), IconPresetCatalog::indexFormat()},
        {QStringLiteral("version"), 1},
        {QStringLiteral("presets"), QVariantList{id}},
    };

    if (scenario == QStringLiteral("missing-index"))
    {
        QVERIFY(QFile::remove(indexPath));
    }
    else if (scenario == QStringLiteral("index-not-json"))
    {
        QVERIFY(writeBytes(indexPath, "not json"));
    }
    else if (scenario == QStringLiteral("wrong-format"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/format"),
                                     PanelPresetCatalog::indexFormat())));
    }
    else if (scenario == QStringLiteral("future-version"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/version"), 2)));
    }
    else if (scenario == QStringLiteral("unknown-index-key"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/extra"), 1)));
    }
    else if (scenario == QStringLiteral("presets-not-array"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/presets"),
                                     QVariantMap{})));
    }
    else if (scenario == QStringLiteral("duplicate-id"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/presets"),
                                     QVariantList{id, id})));
    }
    else if (scenario == QStringLiteral("user-id"))
    {
        QVERIFY(writeIndex(withValue(
            validIndex, QStringLiteral("/presets"),
            QVariantList{id, QStringLiteral("user-1a2b3c4d5e6f")})));
    }
    else if (scenario == QStringLiteral("malformed-id"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/presets"),
                                     QVariantList{id, QStringLiteral("Bad Id")})));
    }
    else if (scenario == QStringLiteral("missing-definition"))
    {
        QVERIFY(writeIndex(withValue(validIndex, QStringLiteral("/presets"),
                                     QVariantList{id, QStringLiteral("ghost")})));
    }
    else if (scenario == QStringLiteral("unlisted-definition"))
    {
        QVERIFY(writeBytes(QDir(directory).filePath(QStringLiteral("stray.json")),
                           toJson(withId(iconPresetMap(), QStringLiteral("stray")))));
    }
    else if (scenario == QStringLiteral("identity-mismatch"))
    {
        QVERIFY(writeBytes(definitionPath,
                           toJson(withId(iconPresetMap(), QStringLiteral("another")))));
    }
    else if (scenario == QStringLiteral("user-definition"))
    {
        QVERIFY(writeBytes(definitionPath, toJson(withValue(
            iconPresetMap(), QStringLiteral("/identity/builtIn"), false))));
    }
    else if (scenario == QStringLiteral("invalid-definition"))
    {
        QVERIFY(writeBytes(definitionPath, toJson(withValue(
            iconPresetMap(), QStringLiteral("/surprise"), 1))));
    }
    else if (scenario == QStringLiteral("panel-definition"))
    {
        QVERIFY(writeBytes(definitionPath, toJson(withId(panelPresetMap(), id))));
    }
    else if (scenario == QStringLiteral("symlink-escape"))
    {
        const QString outside = root.filePath(QStringLiteral("outside.json"));
        QVERIFY(writeBytes(outside, toJson(iconPresetMap())));
        QVERIFY(QFile::remove(definitionPath));
        QVERIFY(QFile::link(outside, definitionPath));
    }
    else if (scenario == QStringLiteral("oversized-definition"))
    {
        QVERIFY(writeBytes(definitionPath,
                           QByteArray(IconPresetCatalog::MaximumFileBytes + 1, ' ')));
    }
    else if (scenario == QStringLiteral("directory-definition"))
    {
        QVERIFY(QFile::remove(definitionPath));
        QVERIFY(QDir().mkpath(definitionPath));
    }
    else if (scenario == QStringLiteral("unresolved-reference"))
    {
        QVERIFY(writeBytes(definitionPath, toJson(withValue(
            iconPresetMap(), QStringLiteral("/icon/iconStyleId"),
            QStringLiteral("ghost-style")))));
    }
    else
    {
        QFAIL("unknown scenario");
    }

    const auto result = IconPresetCatalog::loadBuiltIn(indexPath, context());
    // An installed catalog is all or nothing: one bad entry leaves no catalog.
    QVERIFY2(!result.isValid(), "the damaged catalog was accepted");
    QVERIFY(!result.catalog.has_value());
    QVERIFY(!result.primaryCode().isEmpty());
    QVERIFY2(hasDiagnostic(result.diagnostics, expectedCode, QString{}),
             qPrintable(describe(result.diagnostics)));
}

void PresetCatalogTest::iconReferencesMustResolve_data()
{
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedCode");

    QTest::newRow("unknown icon style")
        << "/icon/iconStyleId" << QVariant(QStringLiteral("ghost-style"))
        << "invalid-reference";
    QTest::newRow("unknown fallback style")
        << "/fallback/iconStyleId" << QVariant(QStringLiteral("ghost-style"))
        << "invalid-reference";
    QTest::newRow("unknown motion profile")
        << "/icon/motion/profileId" << QVariant(QStringLiteral("ghost-motion"))
        << "invalid-reference";
    // A legacy `iconAnimation` name is not a motion profile id.
    QTest::newRow("legacy motion name")
        << "/icon/motion/profileId" << QVariant(QStringLiteral("scale"))
        << "invalid-reference";
    QTest::newRow("unknown fallback motion")
        << "/fallback/motionProfileId" << QVariant(QStringLiteral("ghost-motion"))
        << "invalid-reference";
    QTest::newRow("unknown per-state motion")
        << "/icon/perStateAnimationOverrides/urgent"
        << QVariant(QStringLiteral("ghost-motion")) << "invalid-reference";
    QTest::newRow("capability the style lacks")
        << "/compatibility/requiredStyleCapabilities"
        << QVariant(QStringList{QStringLiteral("mapped-replacement")})
        << "invalid-reference";
    QTest::newRow("tier the style lacks")
        << "/compatibility/rendererTiers"
        << QVariant(QStringList{QStringLiteral("procedural2d"),
                                QStringLiteral("skinned2d")})
        << "inconsistent-declaration";
}

void PresetCatalogTest::iconReferencesMustResolve()
{
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedCode);

    QVERIFY(ArchDock::validateIconPresetReferences(iconFixture(), context()).isEmpty());

    QVector<PresetValidationDiagnostic> parseDiagnostics;
    const auto preset = IconPresetDefinition::fromVariantMap(
        throughJson(withValue(iconPresetMap(), pointer, value)), &parseDiagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(parseDiagnostics)));
    const auto diagnostics = ArchDock::validateIconPresetReferences(*preset, context());
    QVERIFY2(hasDiagnostic(diagnostics, expectedCode, pointer),
             qPrintable(describe(diagnostics)));
    QVERIFY(ArchDock::presetDiagnosticsHaveErrors(diagnostics));
}

void PresetCatalogTest::panelReferencesMustResolve_data()
{
    QTest::addColumn<bool>("baked");
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedCode");

    QTest::newRow("unknown theme")
        << false << "/panel/theme/completeThemeId"
        << QVariant(QStringLiteral("ghost-theme")) << "invalid-reference";
    QTest::newRow("unknown fallback theme")
        << false << "/fallback/themeId" << QVariant(QStringLiteral("ghost-theme"))
        << "invalid-reference";
    QTest::newRow("unknown recommended icon preset")
        << false << "/panel/recommendedIconPresetId"
        << QVariant(QStringLiteral("ghost-preset")) << "invalid-reference";
    QTest::newRow("unknown fallback icon preset")
        << false << "/fallback/iconPresetId"
        << QVariant(QStringLiteral("ghost-preset")) << "invalid-reference";
    QTest::newRow("legacy motion name")
        << false << "/panel/motion/iconAnimation"
        << QVariant(QStringLiteral("scale")) << "invalid-reference";
    // `pulse` declares only the 2D tiers, so it cannot back a baked 2.5D preset.
    QTest::newRow("motion without the baked tier")
        << true << "/panel/motion/iconAnimation"
        << QVariant(QStringLiteral("pulse")) << "inconsistent-declaration";
    QTest::newRow("recommended icons without the baked tier")
        << true << "/panel/recommendedIconPresetId"
        << QVariant(QStringLiteral("fixture-spiral")) << "inconsistent-declaration";
}

void PresetCatalogTest::panelReferencesMustResolve()
{
    QFETCH(bool, baked);
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedCode);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVariantMap spiral = withId(iconPresetMap(), QStringLiteral("fixture-spiral"));
    spiral = withValue(spiral, QStringLiteral("/icon/motion"), QVariantMap{
        {QStringLiteral("profileId"), QStringLiteral("spiral")}});
    const auto icons = IconPresetCatalog::loadBuiltIn(
        writeCatalog<IconPresetCatalog>(directory.path(), {iconPresetMap(), spiral}),
        context());
    QVERIFY2(icons.isValid(), qPrintable(describe(icons.diagnostics)));
    const PresetReferenceContext references = context(&*icons.catalog);

    const QVariantMap base = baked ? bakedRingPresetMap() : panelPresetMap();
    QVector<PresetValidationDiagnostic> parseDiagnostics;
    const auto valid = PanelPresetDefinition::fromVariantMap(base, &parseDiagnostics);
    QVERIFY2(valid.has_value(), qPrintable(describe(parseDiagnostics)));
    const auto baseline = ArchDock::validatePanelPresetReferences(*valid, references);
    QVERIFY2(baseline.isEmpty(), qPrintable(describe(baseline)));

    const auto preset = PanelPresetDefinition::fromVariantMap(
        throughJson(withValue(base, pointer, value)), &parseDiagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(parseDiagnostics)));
    const auto diagnostics =
        ArchDock::validatePanelPresetReferences(*preset, references);
    QVERIFY2(hasDiagnostic(diagnostics, expectedCode, pointer),
             qPrintable(describe(diagnostics)));

    // Without an icon catalog a panel preset cannot prove its icon pairing.
    const auto unresolved = ArchDock::validatePanelPresetReferences(*valid, context());
    QVERIFY(hasDiagnostic(unresolved, QStringLiteral("invalid-reference"),
                          QStringLiteral("/panel/recommendedIconPresetId")));
}

void PresetCatalogTest::panelAndIconCatalogsAreSeparate()
{
    static_assert(!std::is_same_v<PanelPresetCatalog, IconPresetCatalog>);
    static_assert(!std::is_same_v<PanelPresetDefinition, IconPresetDefinition>);
    QVERIFY(PanelPresetCatalog::indexFileName() != IconPresetCatalog::indexFileName());
    QVERIFY(PanelPresetCatalog::indexFormat() != IconPresetCatalog::indexFormat());
    QVERIFY(PanelPresetDefinition::formatName() != IconPresetDefinition::formatName());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString iconIndex = writeCatalog<IconPresetCatalog>(
        directory.filePath(QStringLiteral("icons")), {iconPresetMap()});
    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndex, context());
    QVERIFY(icons.isValid());
    const QString panelIndex = writeCatalog<PanelPresetCatalog>(
        directory.filePath(QStringLiteral("panels")), {panelPresetMap()});

    // Neither loader accepts the other catalog, by index or by definition.
    const auto iconsAsPanels = PanelPresetCatalog::loadBuiltIn(
        iconIndex, context(&*icons.catalog));
    QVERIFY(!iconsAsPanels.isValid());
    QVERIFY(hasDiagnostic(iconsAsPanels.diagnostics,
                          QStringLiteral("unsupported-format"), QString{}));
    const auto panelsAsIcons = IconPresetCatalog::loadBuiltIn(panelIndex, context());
    QVERIFY(!panelsAsIcons.isValid());
    QVERIFY(hasDiagnostic(panelsAsIcons.diagnostics,
                          QStringLiteral("unsupported-format"), QString{}));
}

void PresetCatalogTest::userStoreStartsEmptyAndReadingWritesNothing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("presets"));
    const UserPresetStore store(root);
    QCOMPARE(store.rootDirectory(), root);

    QVector<PresetValidationDiagnostic> diagnostics;
    QVERIFY(store.panelPresets(&diagnostics).isEmpty());
    QVERIFY(store.iconPresets(&diagnostics).isEmpty());
    QVERIFY(diagnostics.isEmpty());
    // Browsing an empty library must not even create its directory.
    QVERIFY(!QFileInfo::exists(root));
}

void PresetCatalogTest::unmarkedStoreAdoptionIsRecoverable()
{
    QTemporaryDir directory;
    const QString root = directory.filePath(QStringLiteral("presets"));
    UserPresetStore store(root);
    QString error;
    const auto first = store.save(UserPresetStore::derivedFrom(panelFixture(), QStringLiteral("First")), &error);
    QVERIFY2(first.has_value(), qPrintable(error));
    const QString marker = QDir(root).filePath(QStringLiteral("user-presets.json"));
    QVERIFY(QFile::remove(marker));
    const QString original = QDir(root).filePath(QStringLiteral("panels/") + first->identity.id + QStringLiteral(".json"));
    const auto originalBytes = readBytes(original);
    const QString defaults = QDir(root).filePath(QStringLiteral("defaults.json"));
    const auto defaultsBytes = toJson({{QStringLiteral("panelPresetId"), first->identity.id}});
    QVERIFY(writeBytes(defaults, defaultsBytes));
    ArchDock::ConfigurationBackup backup(QDir(root).filePath(QStringLiteral(".config-backups")),
        {{QStringLiteral("presets"), {root, true}}});
    QVERIFY(backup.backups().isEmpty());
    const auto second = store.save(UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Second")), &error);
    QVERIFY2(second.has_value(), qPrintable(error));
    QCOMPARE(backup.backups().size(), 1);
    QCOMPARE(readBytes(original), originalBytes);
    QCOMPARE(readBytes(defaults), defaultsBytes);
    QVERIFY2(backup.restore(backup.backups().first(), &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(marker));
    QCOMPARE(store.panelPresets().size(), 1);
    QVERIFY(store.iconPresets().isEmpty());
    QCOMPARE(readBytes(original), originalBytes);
    QCOMPARE(readBytes(defaults), defaultsBytes);

    QVERIFY(QDir(QDir(root).filePath(QStringLiteral(".config-backups"))).removeRecursively());
    QVERIFY(writeBytes(QDir(root).filePath(QStringLiteral(".config-backups")), QByteArray("blocked")));
    QVERIFY(!store.save(UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Refused")), &error));
    QCOMPARE(error, QStringLiteral("backup-root-unwritable"));
    QVERIFY(!QFileInfo::exists(marker));
    QCOMPARE(readBytes(original), originalBytes);
    QCOMPARE(readBytes(defaults), defaultsBytes);
    QVERIFY(store.iconPresets().isEmpty());
}

void PresetCatalogTest::derivedPresetGetsANewUserIdAndRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));

    const PanelPresetDefinition builtIn = panelFixture();
    PanelPresetDefinition draft = UserPresetStore::derivedFrom(
        builtIn, QStringLiteral("  My Rail  "));
    QVERIFY(draft.identity.id.isEmpty());
    QVERIFY(!draft.identity.builtIn);
    QCOMPARE(draft.identity.name, QStringLiteral("My Rail"));
    // Customize the derivative before it is stored.
    draft.panel.configuration.surface.color = QStringLiteral("#ff8800");
    draft.panel.configuration.iconStyle.size = 64;
    draft.panel.configuration.motion.iconProfile = QStringLiteral("bounce");
    draft.panel.configuration.presentation.mode = QStringLiteral("open");

    QString error;
    const auto saved = store.save(draft, &error);
    QVERIFY2(saved.has_value(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QVERIFY(PresetIdentity::isUserId(saved->identity.id));
    QVERIFY(PresetIdentity::isValidId(saved->identity.id));
    QVERIFY(saved->identity.id != builtIn.identity.id);
    QVERIFY(!saved->identity.builtIn);
    QCOMPARE(saved->identity.revision, 1);
    QCOMPARE(saved->identity.derivedFromPresetId, builtIn.identity.id);
    QCOMPARE(saved->identity.sourceRevision, builtIn.identity.revision);
    QCOMPARE(saved->panel.configuration.identity.id, saved->identity.id);
    QCOMPARE(saved->panel.configuration.surface.color, QStringLiteral("#ff8800"));
    QCOMPARE(saved->panel.configuration.iconStyle.size, 64);
    QCOMPARE(saved->panel.configuration.motion.iconProfile, QStringLiteral("bounce"));
    QCOMPARE(saved->panel.configuration.presentation.mode, QStringLiteral("open"));
    // Everything the derivative did not change is carried over complete.
    QCOMPARE(saved->panel.configuration.placement.width,
             builtIn.panel.configuration.placement.width);
    QCOMPARE(saved->panel.themeId(), builtIn.panel.themeId());

    const QString path = QDir(store.rootDirectory()).filePath(
        QStringLiteral("panels/") + saved->identity.id + QStringLiteral(".json"));
    QVERIFY(QFileInfo(path).isFile());
    const QVariantMap marker = parseJson(readBytes(
        QDir(store.rootDirectory()).filePath(QStringLiteral("user-presets.json"))));
    QCOMPARE(marker.value(QStringLiteral("format")).toString(),
             UserPresetStore::storeFormat());
    QCOMPARE(marker.value(QStringLiteral("version")).toInt(),
             UserPresetStore::CurrentStoreVersion);

    // A second store object - a later session - reads back the same preset.
    QVector<PresetValidationDiagnostic> diagnostics;
    const auto reloaded = UserPresetStore(store.rootDirectory()).panelPresets(
        &diagnostics);
    QVERIFY2(diagnostics.isEmpty(), qPrintable(describe(diagnostics)));
    QCOMPARE(reloaded.size(), 1);
    QVERIFY(reloaded.first() == *saved);
    // The stored user preset stands on its own against the real resources.
    QVERIFY(ArchDock::validatePanelPresetReferences(
                reloaded.first(), context()).size() ==
            ArchDock::validatePanelPresetReferences(builtIn, context()).size());

    // The same holds for an icon preset, in its own directory.
    IconPresetDefinition iconDraft = UserPresetStore::derivedFrom(
        iconFixture(), QString{});
    QCOMPARE(iconDraft.identity.name, iconFixture().identity.name);
    iconDraft.icon.motion.profileId = QStringLiteral("glow");
    iconDraft.icon.visualOverrides.insert(QStringLiteral("orb-glass"), QVariantMap{
        {QStringLiteral("borderColor"), QStringLiteral("#ffffff")}});
    const auto savedIcon = store.save(iconDraft, &error);
    QVERIFY2(savedIcon.has_value(), qPrintable(error));
    QVERIFY(PresetIdentity::isUserId(savedIcon->identity.id));
    QVERIFY(savedIcon->identity.id != saved->identity.id);
    QCOMPARE(savedIcon->identity.derivedFromPresetId, QStringLiteral("fixture-pedestal"));
    QCOMPARE(savedIcon->identity.sourceRevision, 2);
    QVERIFY(QFileInfo(QDir(store.rootDirectory()).filePath(
        QStringLiteral("icons/") + savedIcon->identity.id + QStringLiteral(".json")))
                .isFile());
    const auto reloadedIcons = store.iconPresets(&diagnostics);
    QCOMPARE(reloadedIcons.size(), 1);
    QVERIFY(reloadedIcons.first() == *savedIcon);
    QCOMPARE(reloadedIcons.first().icon.motion.profileId, QStringLiteral("glow"));
    QVERIFY(ArchDock::validateIconPresetReferences(
                reloadedIcons.first(), context()).isEmpty());
    // The two kinds never mix.
    QCOMPARE(store.panelPresets().size(), 1);
}

void PresetCatalogTest::savingAgainKeepsTheIdAndAdvancesTheRevision()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));

    const auto first = store.save(
        UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Copy")));
    QVERIFY(first.has_value());

    IconPresetDefinition renamed = *first;
    renamed.identity.name = QStringLiteral("Renamed Copy");
    renamed.identity.description = QStringLiteral("Edited by the user.");
    // The caller's revision is irrelevant; the stored one is what advances.
    renamed.identity.revision = 40;
    const auto second = store.save(renamed);
    QVERIFY(second.has_value());
    QCOMPARE(second->identity.id, first->identity.id);
    QCOMPARE(second->identity.revision, 2);
    QCOMPARE(second->identity.name, QStringLiteral("Renamed Copy"));
    QCOMPARE(second->identity.derivedFromPresetId, first->identity.derivedFromPresetId);

    const auto listed = store.iconPresets();
    QCOMPARE(listed.size(), 1);
    QVERIFY(listed.first() == *second);

    // A copy of a user preset is a new preset with lineage to that copy.
    const auto copy = store.save(
        UserPresetStore::derivedFrom(*second, QStringLiteral("Copy of copy")));
    QVERIFY(copy.has_value());
    QVERIFY(copy->identity.id != second->identity.id);
    QCOMPARE(copy->identity.revision, 1);
    QCOMPARE(copy->identity.derivedFromPresetId, second->identity.id);
    QCOMPARE(copy->identity.sourceRevision, 2);
    QCOMPARE(store.iconPresets().size(), 2);
}

void PresetCatalogTest::builtInDefinitionsAreNeverWritten()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString catalogRoot = directory.filePath(QStringLiteral("installed"));
    const QString iconIndex = writeCatalog<IconPresetCatalog>(
        QDir(catalogRoot).filePath(QStringLiteral("icons")), {iconPresetMap()});
    const QString panelIndex = writeCatalog<PanelPresetCatalog>(
        QDir(catalogRoot).filePath(QStringLiteral("panels")), {panelPresetMap()});
    const QByteArray before = directoryDigest(catalogRoot);
    QVERIFY(!before.isEmpty());

    const UserPresetStore store(directory.filePath(QStringLiteral("user")));
    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndex, context());
    QVERIFY(icons.isValid());
    const auto panels = PanelPresetCatalog::loadBuiltIn(
        panelIndex, context(&*icons.catalog));
    QVERIFY(panels.isValid());
    const PanelPresetDefinition builtInPanel =
        *panels.catalog->presetById(QStringLiteral("fixture-chassis"));
    const IconPresetDefinition builtInIcon =
        *icons.catalog->presetById(QStringLiteral("fixture-pedestal"));

    // Handing the store a built-in as it is cannot write that built-in: it
    // becomes a new user-owned preset inside the user store.
    QString error;
    const auto forcedPanel = store.save(builtInPanel, &error);
    QVERIFY2(forcedPanel.has_value(), qPrintable(error));
    QVERIFY(PresetIdentity::isUserId(forcedPanel->identity.id));
    QVERIFY(!forcedPanel->identity.builtIn);
    const auto forcedIcon = store.save(builtInIcon, &error);
    QVERIFY2(forcedIcon.has_value(), qPrintable(error));
    QVERIFY(PresetIdentity::isUserId(forcedIcon->identity.id));

    // Derive, customize, rename and delete - the installed files never change.
    PanelPresetDefinition derived = UserPresetStore::derivedFrom(
        builtInPanel, QStringLiteral("Mine"));
    derived.panel.configuration.surface.opacity = 0.5;
    const auto saved = store.save(derived, &error);
    QVERIFY2(saved.has_value(), qPrintable(error));
    PanelPresetDefinition renamed = *saved;
    renamed.identity.name = QStringLiteral("Mine, renamed");
    QVERIFY(store.save(renamed, &error).has_value());
    QVERIFY(store.removePanelPreset(saved->identity.id, &error));
    QVERIFY(!store.removePanelPreset(QStringLiteral("fixture-chassis"), &error));
    QCOMPARE(error, QStringLiteral("not-user-preset"));

    QCOMPARE(directoryDigest(catalogRoot), before);
    QVERIFY(!QFileInfo::exists(QDir(store.rootDirectory()).filePath(
        QStringLiteral("panels/fixture-chassis.json"))));
    const auto reloaded = PanelPresetCatalog::loadBuiltIn(
        panelIndex, context(&*icons.catalog));
    QVERIFY(reloaded.isValid());
    QVERIFY(*reloaded.catalog->presetById(QStringLiteral("fixture-chassis")) ==
            builtInPanel);
    QCOMPARE(readBytes(reloaded.catalog->definitionPath(
                 QStringLiteral("fixture-chassis"))),
             toJson(panelPresetMap()));
}

void PresetCatalogTest::removeOnlyTouchesUserPresets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));
    const auto panel = store.save(
        UserPresetStore::derivedFrom(panelFixture(), QStringLiteral("Panel")));
    const auto icon = store.save(
        UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Icon")));
    QVERIFY(panel.has_value() && icon.has_value());

    // A file outside the store that a traversal would reach.
    const QString outside = directory.filePath(QStringLiteral("outside.json"));
    QVERIFY(writeBytes(outside, "{}"));

    QString error;
    for (const QString &id : {QStringLiteral("fixture-chassis"),
                              QStringLiteral("../outside"),
                              QStringLiteral("user-../../outside"),
                              QString{}})
    {
        QVERIFY2(!store.removePanelPreset(id, &error), qPrintable(id));
        QCOMPARE(error, QStringLiteral("not-user-preset"));
    }
    QVERIFY(!store.removePanelPreset(QStringLiteral("user-000000000000"), &error));
    QCOMPARE(error, QStringLiteral("not-found"));
    // An icon preset id is not a panel preset.
    QVERIFY(!store.removePanelPreset(icon->identity.id, &error));
    QCOMPARE(error, QStringLiteral("not-found"));
    QVERIFY(QFileInfo::exists(outside));
    QCOMPARE(store.panelPresets().size(), 1);
    QCOMPARE(store.iconPresets().size(), 1);

    QVERIFY(store.removePanelPreset(panel->identity.id, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(store.panelPresets().isEmpty());
    QCOMPARE(store.iconPresets().size(), 1);
    QVERIFY(store.removeIconPreset(icon->identity.id, &error));
    QVERIFY(store.iconPresets().isEmpty());
}

void PresetCatalogTest::damagedUserFilesAreReportedAndSkipped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));
    const auto valid = store.save(
        UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Valid")));
    QVERIFY(valid.has_value());

    const QDir icons(QDir(store.rootDirectory()).filePath(QStringLiteral("icons")));
    QVERIFY(writeBytes(icons.filePath(QStringLiteral("user-aaaaaaaaaaaa.json")),
                       "not json"));
    // A valid definition stored under another preset's file name.
    QVERIFY(writeBytes(icons.filePath(QStringLiteral("user-bbbbbbbbbbbb.json")),
                       toJson(valid->toVariantMap())));
    // A built-in definition copied into the user store by hand.
    QVERIFY(writeBytes(icons.filePath(QStringLiteral("fixture-pedestal.json")),
                       toJson(iconPresetMap())));
    // A panel preset in the icon directory.
    QVERIFY(writeBytes(icons.filePath(QStringLiteral("user-cccccccccccc.json")),
                       toJson(panelPresetMap())));

    QVector<PresetValidationDiagnostic> diagnostics;
    const auto listed = store.iconPresets(&diagnostics);
    QCOMPARE(listed.size(), 1);
    QVERIFY(listed.first() == *valid);
    QVERIFY(hasDiagnostic(diagnostics, QStringLiteral("invalid-json"), QString{}));
    QVERIFY(hasDiagnostic(diagnostics, QStringLiteral("store-identity-mismatch"),
                          QStringLiteral("/identity/id")));
    QVERIFY(hasDiagnostic(diagnostics, QStringLiteral("unsupported-format"),
                          QStringLiteral("/format")));
    QVERIFY2(std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                         [](const PresetValidationDiagnostic &entry)
                         {
                             return entry.jsonPointer.startsWith(
                                 QStringLiteral("fixture-pedestal.json#"));
                         }),
             qPrintable(describe(diagnostics)));
}

void PresetCatalogTest::newerStoreVersionIsLeftUntouched()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));
    const auto saved = store.save(
        UserPresetStore::derivedFrom(iconFixture(), QStringLiteral("Kept")));
    QVERIFY(saved.has_value());

    QVERIFY(writeBytes(
        QDir(store.rootDirectory()).filePath(QStringLiteral("user-presets.json")),
        toJson({{QStringLiteral("format"), UserPresetStore::storeFormat()},
                {QStringLiteral("version"), 2}})));
    const QByteArray before = directoryDigest(store.rootDirectory());

    QVector<PresetValidationDiagnostic> diagnostics;
    QVERIFY(store.iconPresets(&diagnostics).isEmpty());
    QVERIFY(hasDiagnostic(diagnostics, QStringLiteral("unsupported-version"),
                          QStringLiteral("user-presets.json")));
    QString error;
    QVERIFY(!store.save(UserPresetStore::derivedFrom(
                            iconFixture(), QStringLiteral("Refused")), &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("unsupported-store-version"));
    QCOMPARE(directoryDigest(store.rootDirectory()), before);
}

void PresetCatalogTest::invalidDefinitionsAreNotStored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));

    QString error;
    PanelPresetDefinition unnamed = UserPresetStore::derivedFrom(
        panelFixture(), QStringLiteral("Draft"));
    unnamed.identity.name = QStringLiteral("   ");
    QVERIFY(!store.save(unnamed, &error).has_value());
    QVERIFY(!error.isEmpty());

    PanelPresetDefinition outOfRange = UserPresetStore::derivedFrom(
        panelFixture(), QStringLiteral("Draft"));
    outOfRange.panel.configuration.iconStyle.size = 4000;
    QVERIFY(!store.save(outOfRange, &error).has_value());
    QCOMPARE(error, QStringLiteral("invalid-value"));

    IconPresetDefinition badColor = UserPresetStore::derivedFrom(
        iconFixture(), QStringLiteral("Draft"));
    badColor.icon.visualOverrides.insert(QStringLiteral("orb-glass"), QVariantMap{
        {QStringLiteral("color"), QStringLiteral("blue")}});
    QVERIFY(!store.save(badColor, &error).has_value());
    QCOMPARE(error, QStringLiteral("invalid-color"));

    // Nothing reached the disk, not even the store directory.
    QVERIFY(!QFileInfo::exists(store.rootDirectory()));
}

void PresetCatalogTest::untouchedStyleIsTheStoreProjection()
{
    QCOMPARE(m_styles->styleIds().size(), 6);
    for (const QString &styleId : m_styles->styleIds())
    {
        IconPresetDefinition preset = iconFixture();
        preset.icon.iconStyleId = styleId;
        preset.icon.visualOverrides.clear();
        preset.icon.stateOverrides.clear();
        const auto resolved = PresetCapabilityResolver::resolveIconStyle(
            *m_styles, preset);
        QVERIFY2(resolved.valid, qPrintable(styleId + QLatin1Char('\n') +
                                            describe(resolved.diagnostics)));
        QVERIFY(!resolved.overridesApplied);
        QCOMPARE(resolved.projection, m_styles->resolve(styleId));
    }
}

void PresetCatalogTest::mergingKeepsEverythingTheOverrideDoesNotName()
{
    // An override that restates a value the style already has must reproduce
    // the style exactly. This is what proves the merge path loses nothing.
    for (const QString &styleId : m_styles->styleIds())
    {
        const QVariantMap original = m_styles->resolve(styleId);
        IconPresetDefinition preset = iconFixture();
        preset.icon.iconStyleId = styleId;
        preset.icon.visualOverrides.clear();
        preset.icon.stateOverrides = {{QStringLiteral("normal"), QVariantMap{
            {QStringLiteral("glyphOpacity"),
             stateById(original, QStringLiteral("normal"))
                 .value(QStringLiteral("glyphOpacity")).toDouble()}}}};
        const auto resolved = PresetCapabilityResolver::resolveIconStyle(
            *m_styles, preset);
        QVERIFY2(resolved.valid, qPrintable(styleId + QLatin1Char('\n') +
                                            describe(resolved.diagnostics)));
        QVERIFY(resolved.overridesApplied);
        for (const QString &key : {QStringLiteral("id"), QStringLiteral("name"),
                                   QStringLiteral("revision"),
                                   QStringLiteral("layers"),
                                   QStringLiteral("states"),
                                   QStringLiteral("glyphPolicy"),
                                   QStringLiteral("safeGlyphInset"),
                                   QStringLiteral("capabilities"),
                                   QStringLiteral("preview"),
                                   QStringLiteral("license"),
                                   QStringLiteral("assetPaths"),
                                   QStringLiteral("valid"),
                                   QStringLiteral("loadable")})
        {
            QVERIFY2(resolved.projection.value(key) == original.value(key),
                     qPrintable(styleId + QStringLiteral(": ") + key));
        }
    }
}

void PresetCatalogTest::overridesChangeOnlyWhatTheyName()
{
    const QVariantMap original = m_styles->resolve(QStringLiteral("dark-orb"));
    const auto resolved = PresetCapabilityResolver::resolveIconStyle(
        *m_styles, iconFixture());
    QVERIFY2(resolved.valid, qPrintable(describe(resolved.diagnostics)));
    QVERIFY(resolved.overridesApplied);
    QVERIFY(resolved.diagnostics.isEmpty());

    // The projection is still the referenced style, usable by IconScene.
    QCOMPARE(resolved.projection.value(QStringLiteral("id")).toString(),
             QStringLiteral("dark-orb"));
    QCOMPARE(resolved.projection.value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.icon-style"));
    QVERIFY(resolved.projection.value(QStringLiteral("valid")).toBool());
    QVERIFY(resolved.projection.value(QStringLiteral("loadable")).toBool());
    QCOMPARE(resolved.projection.value(QStringLiteral("selectionStatus")).toString(),
             QStringLiteral("selected"));
    QVERIFY(!resolved.projection.value(QStringLiteral("fellBack")).toBool());

    const QVariantMap pedestal = layerById(resolved.projection,
                                           QStringLiteral("orb-pedestal"));
    const QVariantMap originalPedestal = layerById(original,
                                                   QStringLiteral("orb-pedestal"));
    QVERIFY(!originalPedestal.isEmpty());
    QCOMPARE(pedestal.value(QStringLiteral("color")).toString(),
             QStringLiteral("#08101c"));
    QCOMPARE(pedestal.value(QStringLiteral("borderColor")).toString(),
             QStringLiteral("#4a9dff"));
    QCOMPARE(pedestal.value(QStringLiteral("opacity")).toDouble(), 0.9);
    for (const QString &untouched : {QStringLiteral("secondaryColor"),
                                     QStringLiteral("shape"),
                                     QStringLiteral("inset"),
                                     QStringLiteral("radius"),
                                     QStringLiteral("borderWidth"),
                                     QStringLiteral("kind")})
    {
        QCOMPARE(pedestal.value(untouched), originalPedestal.value(untouched));
    }
    for (const QString &layer : {QStringLiteral("orb-shadow"),
                                 QStringLiteral("orb-glass"),
                                 QStringLiteral("orb-energy-ring"),
                                 QStringLiteral("orb-reflection"),
                                 QStringLiteral("pedestal-shadow"),
                                 QStringLiteral("orb-glow")})
    {
        QVERIFY2(!layerById(original, layer).isEmpty(), qPrintable(layer));
        QCOMPARE(layerById(resolved.projection, layer), layerById(original, layer));
    }

    const QVariantMap normal = stateById(resolved.projection, QStringLiteral("normal"));
    const QVariantMap originalNormal = stateById(original, QStringLiteral("normal"));
    QCOMPARE(normal.value(QStringLiteral("glowColor")).toString(),
             QStringLiteral("#4a9dff"));
    QCOMPARE(normal.value(QStringLiteral("glowOpacity")).toDouble(), 0.2);
    QCOMPARE(normal.value(QStringLiteral("borderColor")),
             originalNormal.value(QStringLiteral("borderColor")));
    for (const QString &state : IconPresetDefinition::stateIds())
    {
        if (state != QStringLiteral("normal"))
        {
            QCOMPARE(stateById(resolved.projection, state), stateById(original, state));
        }
    }
    // Resolving a preset never changes the installed style.
    QCOMPARE(m_styles->resolve(QStringLiteral("dark-orb")), original);
}

void PresetCatalogTest::overridesTheStyleValidatorRejects_data()
{
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedPointerSuffix");

    QTest::newRow("layer the style does not have")
        << "/icon/visualOverrides/ghost-layer"
        << QVariant(QVariantMap{{QStringLiteral("opacity"), 1}})
        << "invalid-reference" << "/icon/visualOverrides/ghost-layer";
    QTest::newRow("layer opacity above one")
        << "/icon/visualOverrides/orb-pedestal/opacity" << QVariant(2)
        << "invalid-value" << "/opacity";
    QTest::newRow("layer inset too large")
        << "/icon/visualOverrides/orb-pedestal/inset" << QVariant(0.9)
        << "invalid-value" << "/inset";
    QTest::newRow("layer border too wide")
        << "/icon/visualOverrides/orb-pedestal/borderWidth" << QVariant(1)
        << "invalid-value" << "/borderWidth";
    QTest::newRow("shape the renderer cannot draw")
        << "/icon/visualOverrides/orb-pedestal/shape"
        // PD-20 adds Hexagon; Triangle remains outside the icon declaration.
        << QVariant(QStringLiteral("triangle")) << "invalid-enum" << "/shape";
    QTest::newRow("glyph scale out of range")
        << "/icon/stateOverrides/normal/glyphScale" << QVariant(3)
        << "invalid-value" << "/glyphScale";
    QTest::newRow("negative glow opacity")
        << "/icon/stateOverrides/normal/glowOpacity" << QVariant(-1)
        << "invalid-value" << "/glowOpacity";
    // The real application glyph stays unless a complete mapping exists.
    QTest::newRow("mapped replacement without a mapping")
        << "/icon/glyphPolicy"
        << QVariant(QVariantMap{{QStringLiteral("mode"),
                                 QStringLiteral("mapped-replacement")}})
        << "missing-mapping" << "/mappedReplacements";
}

void PresetCatalogTest::overridesTheStyleValidatorRejects()
{
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedPointerSuffix);

    QVector<PresetValidationDiagnostic> parseDiagnostics;
    const auto preset = IconPresetDefinition::fromVariantMap(
        throughJson(withValue(iconPresetMap(), pointer, value)), &parseDiagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(parseDiagnostics)));

    const auto resolved = PresetCapabilityResolver::resolveIconStyle(*m_styles, *preset);
    QVERIFY(!resolved.valid);
    QVERIFY(resolved.projection.isEmpty());
    QVERIFY2(hasDiagnostic(resolved.diagnostics, expectedCode, expectedPointerSuffix),
             qPrintable(describe(resolved.diagnostics)));
    // The catalog refuses the preset for the same reason.
    const auto references = ArchDock::validateIconPresetReferences(*preset, context());
    QVERIFY2(hasDiagnostic(references, expectedCode, expectedPointerSuffix),
             qPrintable(describe(references)));
}

void PresetCatalogTest::panelPresetResolvesAsDeclared()
{
    const PanelPresetDefinition preset = panelFixture();
    const auto platform = PanelCapabilityResolver::productionPlatform();
    const PresetCompatibility result = PresetCapabilityResolver::resolvePanelPreset(
        preset, m_themes, PresetCapabilityResolver::idealRenderers(), platform);
    QVERIFY2(result.available, qPrintable(result.reasonCode));
    QVERIFY(!result.fallbackApplied);
    QVERIFY(result.reasonCode.isEmpty());
    QCOMPARE(result.hostKind, QStringLiteral("native-edge"));
    QCOMPARE(result.requestedRendererTier, QStringLiteral("skinned2d"));
    QCOMPARE(result.effectiveRendererTier, QStringLiteral("skinned2d"));
    QCOMPARE(result.effectiveThemeId, QStringLiteral("sci-fi-chassis-dark"));
    QVERIFY(result.missingRequiredCapabilities.isEmpty());
    QVERIFY(result.missingOptionalCapabilities.isEmpty());
    QVERIFY(result.effectiveConfiguration.has_value());
    QVERIFY(*result.effectiveConfiguration == preset.panel.configuration);

    // The answer is the panel capability resolver's own, not a second opinion.
    QVariantMap chassis;
    for (const QVariant &theme : std::as_const(m_themes))
    {
        if (theme.toMap().value(QStringLiteral("id")).toString() ==
            QStringLiteral("sci-fi-chassis-dark"))
        {
            chassis = theme.toMap();
        }
    }
    const auto profile = PanelCapabilityResolver::themeProfileFromVariantMap(chassis);
    QVERIFY(profile.has_value());
    const auto direct = PanelCapabilityResolver::resolve(
        preset.panel.configuration,
        PanelCapabilityResolver::productionHostProfile(
            preset.panel.configuration.host.kind),
        *profile, PresetCapabilityResolver::idealRenderers(), platform);
    QCOMPARE(result.capabilityResolution, direct.toVariantMap());

    const QVariantMap summary = result.toVariantMap();
    QCOMPARE(summary.value(QStringLiteral("available")).toBool(), true);
    QCOMPARE(summary.value(QStringLiteral("effectiveRendererTier")).toString(),
             QStringLiteral("skinned2d"));
    QCOMPARE(summary.value(QStringLiteral("hostKind")).toString(),
             QStringLiteral("native-edge"));

    // The renderers this machine really has give the same result here: the
    // 2D tiers are always part of a build.
    const PresetCompatibility production = PresetCapabilityResolver::resolvePanelPreset(
        preset, m_themes, PanelCapabilityResolver::productionRenderers(), platform);
    QVERIFY(production.available);
    QVERIFY(!production.fallbackApplied);
}

void PresetCatalogTest::missingRendererTierSelectsTheThemeFallbackTier()
{
    const auto preset = PanelPresetDefinition::fromVariantMap(bakedRingPresetMap());
    QVERIFY(preset.has_value());
    const auto platform = PanelCapabilityResolver::productionPlatform();

    const PresetCompatibility ideal = PresetCapabilityResolver::resolvePanelPreset(
        *preset, m_themes, PresetCapabilityResolver::idealRenderers(), platform);
    QVERIFY(ideal.available);
    QVERIFY(!ideal.fallbackApplied);
    QCOMPARE(ideal.effectiveRendererTier, QStringLiteral("baked2.5d"));
    QCOMPARE(ideal.hostKind, QStringLiteral("free-desktop"));

    const PresetCompatibility degraded = PresetCapabilityResolver::resolvePanelPreset(
        *preset, m_themes, renderersWithout(RendererTier::Baked2_5D), platform);
    QVERIFY(degraded.available);
    QVERIFY(degraded.fallbackApplied);
    QCOMPARE(degraded.reasonCode, QStringLiteral("renderer-not-installed"));
    QCOMPARE(degraded.requestedRendererTier, QStringLiteral("baked2.5d"));
    QCOMPARE(degraded.effectiveRendererTier, QStringLiteral("procedural2d"));
    // Same theme, drawn by the tier the theme itself declares as its fallback.
    QCOMPARE(degraded.effectiveThemeId, QStringLiteral("ring-platform-blue"));
    QCOMPARE(degraded.missingOptionalCapabilities,
             QStringList{QStringLiteral("baked2.5d")});
    QVERIFY(*degraded.effectiveConfiguration == preset->panel.configuration);
    QCOMPARE(degraded.capabilityResolution.value(QStringLiteral("renderer")).toMap()
                 .value(QStringLiteral("effectiveTier")).toString(),
             QStringLiteral("procedural2d"));
}

void PresetCatalogTest::unusableThemeSelectsTheDeclaredFallbackTheme()
{
    const auto preset = PanelPresetDefinition::fromVariantMap(bakedRingPresetMap());
    QVERIFY(preset.has_value());
    const PresetCompatibility result = PresetCapabilityResolver::resolvePanelPreset(
        *preset, m_themes, PresetCapabilityResolver::idealRenderers(),
        PanelCapabilityResolver::productionPlatform(),
        [](const QString &themeId)
        {
            return themeId != QStringLiteral("ring-platform-blue");
        });
    QVERIFY(result.available);
    QVERIFY(result.fallbackApplied);
    QCOMPARE(result.reasonCode, QStringLiteral("theme-package-unavailable"));
    QCOMPARE(result.effectiveThemeId, QStringLiteral("holographic-ring"));
    QCOMPARE(result.effectiveRendererTier, QStringLiteral("procedural2d"));
    QCOMPARE(result.capabilityResolution.value(QStringLiteral("themeId")).toString(),
             QStringLiteral("holographic-ring"));

    // Only the theme and tier change; the preset's layout and everything else
    // is still the preset's.
    ArchDock::PanelDefinition expected = preset->panel.configuration;
    expected.surface.completeThemeId = QStringLiteral("holographic-ring");
    expected.surface.panelThemeId = QStringLiteral("holographic-ring");
    expected.surface.rendererTier.clear();
    QVERIFY(result.effectiveConfiguration.has_value());
    QVERIFY(*result.effectiveConfiguration == expected);
    QCOMPARE(result.effectiveConfiguration->layout.pathType, QStringLiteral("ring"));
}

void PresetCatalogTest::presetWithoutAUsableFallbackIsIncompatible()
{
    const auto platform = PanelCapabilityResolver::productionPlatform();
    const auto nothingLoads = [](const QString &) { return false; };

    // The collapsible chassis has no theme other than its own that can split.
    const PanelPresetDefinition chassis = panelFixture();
    const PresetCompatibility lost = PresetCapabilityResolver::resolvePanelPreset(
        chassis, m_themes, PresetCapabilityResolver::idealRenderers(), platform,
        nothingLoads);
    QVERIFY(!lost.available);
    QVERIFY(!lost.fallbackApplied);
    QCOMPARE(lost.reasonCode, QStringLiteral("theme-package-unavailable"));
    QVERIFY(lost.effectiveRendererTier.isEmpty());
    QVERIFY(!lost.effectiveConfiguration.has_value());
    QVERIFY(!lost.toVariantMap().value(QStringLiteral("available")).toBool());

    // A required capability that is missing is not papered over by a tier
    // fallback: the preset is reported incompatible.
    PanelPresetDefinition demanding = chassis;
    demanding.compatibility.requiredCapabilities = {QStringLiteral("skinned2d")};
    demanding.compatibility.optionalCapabilities.clear();
    const PresetCompatibility missing = PresetCapabilityResolver::resolvePanelPreset(
        demanding, m_themes, renderersWithout(RendererTier::Skinned2D), platform);
    QVERIFY(!missing.available);
    QVERIFY(!missing.fallbackApplied);
    QCOMPARE(missing.reasonCode, QStringLiteral("required-capability-unavailable"));
    QCOMPARE(missing.missingRequiredCapabilities,
             QStringList{QStringLiteral("skinned2d")});
    QVERIFY(!missing.effectiveConfiguration.has_value());

    // When neither the theme nor its fallback theme can be loaded there is
    // nothing truthful left to draw.
    const auto ring = PanelPresetDefinition::fromVariantMap(bakedRingPresetMap());
    QVERIFY(ring.has_value());
    QVERIFY(!PresetCapabilityResolver::resolvePanelPreset(
                 *ring, m_themes, PresetCapabilityResolver::idealRenderers(),
                 platform, nothingLoads).available);
}

void PresetCatalogTest::iconPresetFallsBackToItsDeclaredSafeReferences()
{
    const IconPresetDefinition preset = iconFixture();
    const PresetCompatibility result = PresetCapabilityResolver::resolveIconPreset(
        preset, *m_styles, *m_profiles);
    QVERIFY(result.available);
    QVERIFY(!result.fallbackApplied);
    QVERIFY(result.reasonCode.isEmpty());
    QCOMPARE(result.effectiveIconStyleId, QStringLiteral("dark-orb"));
    QCOMPARE(result.effectiveMotionProfileId, QStringLiteral("slow-y-turn"));
    QCOMPARE(result.requestedRendererTier, QStringLiteral("procedural2d"));
    QCOMPARE(result.effectiveRendererTier, QStringLiteral("procedural2d"));

    IconPresetDefinition lostStyle = preset;
    lostStyle.icon.iconStyleId = QStringLiteral("ghost-style");
    const PresetCompatibility styleFallback =
        PresetCapabilityResolver::resolveIconPreset(lostStyle, *m_styles, *m_profiles);
    QVERIFY(styleFallback.available);
    QVERIFY(styleFallback.fallbackApplied);
    QCOMPARE(styleFallback.reasonCode, QStringLiteral("icon-style-unavailable"));
    QCOMPARE(styleFallback.effectiveIconStyleId, QStringLiteral("plain-original"));
    QCOMPARE(styleFallback.effectiveMotionProfileId, QStringLiteral("slow-y-turn"));

    IconPresetDefinition lostMotion = preset;
    lostMotion.icon.motion.profileId = QStringLiteral("ghost-motion");
    const PresetCompatibility motionFallback =
        PresetCapabilityResolver::resolveIconPreset(lostMotion, *m_styles, *m_profiles);
    QVERIFY(motionFallback.available);
    QVERIFY(motionFallback.fallbackApplied);
    QCOMPARE(motionFallback.reasonCode, QStringLiteral("motion-profile-unavailable"));
    QCOMPARE(motionFallback.effectiveIconStyleId, QStringLiteral("dark-orb"));
    QCOMPARE(motionFallback.effectiveMotionProfileId, QStringLiteral("none"));

    IconPresetDefinition lost = lostStyle;
    lost.fallback.iconStyleId = QStringLiteral("ghost-style");
    const PresetCompatibility unavailable =
        PresetCapabilityResolver::resolveIconPreset(lost, *m_styles, *m_profiles);
    QVERIFY(!unavailable.available);
    QVERIFY(!unavailable.fallbackApplied);
    QCOMPARE(unavailable.reasonCode, QStringLiteral("icon-style-unavailable"));
    QVERIFY(unavailable.effectiveIconStyleId.isEmpty());
    QVERIFY(unavailable.effectiveMotionProfileId.isEmpty());
}

void PresetCatalogTest::structuralValidationRejects_data()
{
    QTest::addColumn<bool>("baked");
    QTest::addColumn<QString>("pointer");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("expectedPointer");

    QTest::newRow("capability the theme never offers")
        << false << "/compatibility/optionalCapabilities"
        << QVariant(QStringList{QStringLiteral("skinned2d"),
                                QStringLiteral("dynamic-glow")})
        << "/compatibility/optionalCapabilities";
    QTest::newRow("fallback tier the theme does not use")
        << false << "/preview/fallbackTier" << QVariant(QStringLiteral("skinned2d"))
        << "/preview/fallbackTier";
    // The procedural surface cannot split, so it is not a fallback for a
    // preset whose mechanism is split.
    QTest::newRow("procedural fallback for a split preset")
        << false << "/fallback/themeId" << QVariant(QString{}) << "/fallback/themeId";
    QTest::newRow("mechanism the theme does not declare")
        << false << "/panel/presentation/collapseMechanism"
        << QVariant(QStringLiteral("collapse-radial")) << "/panel";
    QTest::newRow("fallback theme without the layout")
        << true << "/fallback/themeId" << QVariant(QStringLiteral("obsidian-glass"))
        << "/fallback/themeId";
    QTest::newRow("theme without the layout")
        << true << "/panel/theme/completeThemeId"
        << QVariant(QStringLiteral("octagon-platform-steel")) << "/panel";
    QTest::newRow("baked preset declaring no fallback tier")
        << true << "/preview/fallbackTier" << QVariant(QStringLiteral("baked2.5d"))
        << "/preview/fallbackTier";
}

void PresetCatalogTest::structuralValidationRejects()
{
    QFETCH(bool, baked);
    QFETCH(QString, pointer);
    QFETCH(QVariant, value);
    QFETCH(QString, expectedPointer);

    const QVariantMap base = baked ? bakedRingPresetMap() : panelPresetMap();
    const auto valid = PanelPresetDefinition::fromVariantMap(base);
    QVERIFY(valid.has_value());
    const auto baseline = PresetCapabilityResolver::validatePanelPreset(
        *valid, m_themes);
    QVERIFY2(baseline.isEmpty(), qPrintable(describe(baseline)));

    QVector<PresetValidationDiagnostic> parseDiagnostics;
    const auto preset = PanelPresetDefinition::fromVariantMap(
        throughJson(withValue(base, pointer, value)), &parseDiagnostics);
    QVERIFY2(preset.has_value(), qPrintable(describe(parseDiagnostics)));
    const auto diagnostics = PresetCapabilityResolver::validatePanelPreset(
        *preset, m_themes);
    QVERIFY2(hasDiagnostic(diagnostics, QStringLiteral("inconsistent-declaration"),
                           expectedPointer),
             qPrintable(describe(diagnostics)));
    QVERIFY(ArchDock::presetDiagnosticsHaveErrors(diagnostics));
}

namespace
{

QString presetRoot()
{
    return QStringLiteral(ARCHDOCK_SOURCE_PRESET_ROOT);
}

QString iconIndexPath()
{
    return QDir(presetRoot()).filePath(
        QStringLiteral("icons/") + IconPresetCatalog::indexFileName());
}

QString panelIndexPath()
{
    return QDir(presetRoot()).filePath(
        QStringLiteral("panels/") + PanelPresetCatalog::indexFileName());
}

struct ExpectedPreset
{
    const char *id;
    const char *name;
};

// The exact built-in libraries, in catalog order, from PRESET_SYSTEM_SPEC
// sections 2.1 and 2.2.
const ExpectedPreset expectedPanelPresets[] = {
    {"obsidian-glass-dock", "Obsidian Glass Dock"},
    {"metallic-shelf-dock", "Metallic Shelf Dock"},
    {"sci-fi-chassis-blue", "Blue Sci-Fi Chassis"},
    {"sci-fi-chassis-red", "Red Sci-Fi Chassis"},
    {"sci-fi-chassis-dark", "Dark Sci-Fi Chassis"},
    {"energy-frame-cyan", "Cyan Energy Frame"},
    {"energy-frame-green", "Green Energy Frame"},
    {"energy-frame-orange", "Orange Energy Frame"},
    {"energy-frame-purple", "Purple Energy Frame"},
    {"minimal-neon-rail", "Minimal Neon Rail"},
    {"mechanical-collapsible-rail", "Collapsible Mechanical Rail"},
    {"circular-blue-ring", "Circular Blue Ring"},
    {"octagonal-platform", "Octagonal Platform"},
    {"orange-arc-dock", "Orange Arc Dock"},
    {"holographic-semicircle", "Holographic Semicircle"},
};

const ExpectedPreset expectedIconPresets[] = {
    {"original-clean", "Original Clean"},
    {"glass-tile", "Glass Tile"},
    {"metallic-blue", "Metallic Blue"},
    {"metallic-red", "Metallic Red"},
    {"neon-green", "Neon Green"},
    {"neon-orange", "Neon Orange"},
    {"dark-orb", "Dark Orb"},
    {"blue-pedestal", "Blue Pedestal"},
    {"red-pedestal", "Red Pedestal"},
    {"holographic-tile", "Holographic Tile"},
    {"minimal-glow", "Minimal Glow"},
    {"beveled-sci-fi", "Beveled Sci-Fi"},
    {"metallic-blue-slow-turn", "Metallic Blue — Slow Turn"},
    {"neon-green-enlarge", "Neon Green — Enlarge"},
    {"dark-orb-spiral", "Dark Orb — Spiral"},
};

QString themeCategory(const QVariantList &themes, const QString &themeId)
{
    for (const QVariant &theme : themes)
    {
        if (theme.toMap().value(QStringLiteral("id")).toString() == themeId)
        {
            return theme.toMap().value(QStringLiteral("category")).toString();
        }
    }
    return {};
}

}

void PresetCatalogTest::everyBuiltInDefinitionFileIsValid()
{
    // Checks each definition file on its own, without the index, so a preset
    // can be proved the moment it is written.
    const QDir icons(QDir(presetRoot()).filePath(QStringLiteral("icons")));
    const QDir panels(QDir(presetRoot()).filePath(QStringLiteral("panels")));

    QHash<QString, IconPresetDefinition> iconPresets;
    for (const QString &fileName : icons.entryList(
             {QStringLiteral("*.json")}, QDir::Files, QDir::Name))
    {
        if (fileName == IconPresetCatalog::indexFileName())
        {
            continue;
        }
        QVector<PresetValidationDiagnostic> diagnostics;
        const auto object = ArchDock::readPresetDefinitionFile(
            icons.filePath(fileName), &diagnostics);
        QVERIFY2(object.has_value(), qPrintable(fileName + describe(diagnostics)));
        const auto preset = IconPresetDefinition::fromVariantMap(*object, &diagnostics);
        QVERIFY2(preset.has_value(),
                 qPrintable(fileName + QLatin1Char('\n') + describe(diagnostics)));
        QVERIFY2(preset->identity.builtIn &&
                     preset->identity.id + QStringLiteral(".json") == fileName,
                 qPrintable(fileName));
        const auto references =
            ArchDock::validateIconPresetReferences(*preset, context());
        QVERIFY2(references.isEmpty(),
                 qPrintable(fileName + QLatin1Char('\n') + describe(references)));
        // A definition file is canonical: loading and re-saving changes nothing.
        QVERIFY2(*IconPresetDefinition::fromVariantMap(
                     throughJson(preset->toVariantMap())) == *preset,
                 qPrintable(fileName));
        iconPresets.insert(preset->identity.id, *preset);
    }

    PresetReferenceContext panelContext = context();
    panelContext.iconPresetById = [&iconPresets](const QString &id)
        -> const IconPresetDefinition *
    {
        const auto match = iconPresets.constFind(id);
        return match == iconPresets.cend() ? nullptr : &match.value();
    };
    int panelCount = 0;
    for (const QString &fileName : panels.entryList(
             {QStringLiteral("*.json")}, QDir::Files, QDir::Name))
    {
        if (fileName == PanelPresetCatalog::indexFileName())
        {
            continue;
        }
        QVector<PresetValidationDiagnostic> diagnostics;
        const auto object = ArchDock::readPresetDefinitionFile(
            panels.filePath(fileName), &diagnostics);
        QVERIFY2(object.has_value(), qPrintable(fileName + describe(diagnostics)));
        const auto preset = PanelPresetDefinition::fromVariantMap(*object, &diagnostics);
        QVERIFY2(preset.has_value(),
                 qPrintable(fileName + QLatin1Char('\n') + describe(diagnostics)));
        QVERIFY2(preset->identity.builtIn &&
                     preset->identity.id + QStringLiteral(".json") == fileName,
                 qPrintable(fileName));
        const auto references =
            ArchDock::validatePanelPresetReferences(*preset, panelContext);
        QVERIFY2(references.isEmpty(),
                 qPrintable(fileName + QLatin1Char('\n') + describe(references)));
        QVERIFY2(*PanelPresetDefinition::fromVariantMap(
                     throughJson(preset->toVariantMap())) == *preset,
                 qPrintable(fileName));
        ++panelCount;
    }
    qInfo("Valid built-in definition files: %d panel, %d icon",
          panelCount, int(iconPresets.size()));
}

void PresetCatalogTest::builtInCatalogsAreExactAndValid()
{
    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndexPath(), context());
    QVERIFY2(icons.isValid(), qPrintable(describe(icons.diagnostics)));
    QVERIFY(icons.diagnostics.isEmpty());
    const auto panels = PanelPresetCatalog::loadBuiltIn(
        panelIndexPath(), context(&*icons.catalog));
    QVERIFY2(panels.isValid(), qPrintable(describe(panels.diagnostics)));
    QVERIFY(panels.diagnostics.isEmpty());

    // Exactly fifteen of each, with the specified stable ids and names.
    QCOMPARE(int(std::size(expectedPanelPresets)), 15);
    QCOMPARE(int(std::size(expectedIconPresets)), 15);
    QCOMPARE(panels.catalog->presetIds().size(), 15);
    QCOMPARE(icons.catalog->presetIds().size(), 15);
    // One definition file per preset, plus the index, and nothing else.
    QCOMPARE(QFileInfo(panelIndexPath()).dir().entryList(QDir::Files).size(), 16);
    QCOMPARE(QFileInfo(iconIndexPath()).dir().entryList(QDir::Files).size(), 16);

    const auto platform = PanelCapabilityResolver::productionPlatform();
    QSet<QString> seeds;
    for (int index = 0; index < 15; ++index)
    {
        const QString id = QString::fromLatin1(expectedPanelPresets[index].id);
        QCOMPARE(panels.catalog->presetIds().at(index), id);
        const PanelPresetDefinition *preset = panels.catalog->presetById(id);
        QVERIFY(preset != nullptr);
        QCOMPARE(preset->identity.name,
                 QString::fromUtf8(expectedPanelPresets[index].name));
        QVERIFY2(!preset->identity.description.isEmpty(), qPrintable(id));
        QVERIFY(preset->identity.builtIn);
        QCOMPARE(preset->identity.revision, 1);
        QVERIFY(preset->identity.derivedFromPresetId.isEmpty());
        QCOMPARE(preset->fallback.unsupportedFieldPolicy, QStringLiteral("reject"));
        QVERIFY2(!seeds.contains(preset->preview.deterministicPreviewSeed),
                 qPrintable(id));
        seeds.insert(preset->preview.deterministicPreviewSeed);

        // Each preset pairs with a real icon preset and has a real fallback.
        QVERIFY2(icons.catalog->contains(preset->panel.recommendedIconPresetId),
                 qPrintable(id));
        QVERIFY2(icons.catalog->contains(preset->fallback.iconPresetId),
                 qPrintable(id));

        // Drawable exactly as declared when every renderer exists, and usable
        // on this build's actual renderers.
        const PresetCompatibility ideal = PresetCapabilityResolver::resolvePanelPreset(
            *preset, m_themes, PresetCapabilityResolver::idealRenderers(), platform);
        QVERIFY2(ideal.available && !ideal.fallbackApplied,
                 qPrintable(id + QStringLiteral(": ") + ideal.reasonCode));
        QCOMPARE(ideal.effectiveRendererTier, preset->preview.rendererTier);
        QVERIFY(ideal.missingOptionalCapabilities.isEmpty());
        const PresetCompatibility production =
            PresetCapabilityResolver::resolvePanelPreset(
                *preset, m_themes, PanelCapabilityResolver::productionRenderers(),
                platform);
        QVERIFY2(production.available,
                 qPrintable(id + QStringLiteral(": ") + production.reasonCode));

        // The safe fallback tier is always available without optional modules.
        const PresetCompatibility minimal =
            PresetCapabilityResolver::resolvePanelPreset(
                *preset, m_themes,
                renderersWithout(*ArchDock::rendererTierFromName(
                    preset->preview.rendererTier)),
                platform);
        if (preset->preview.rendererTier != preset->preview.fallbackTier)
        {
            QVERIFY2(minimal.available && minimal.fallbackApplied,
                     qPrintable(id + QStringLiteral(": ") + minimal.reasonCode));
            QCOMPARE(minimal.effectiveRendererTier, preset->preview.fallbackTier);
        }
    }

    for (int index = 0; index < 15; ++index)
    {
        const QString id = QString::fromLatin1(expectedIconPresets[index].id);
        QCOMPARE(icons.catalog->presetIds().at(index), id);
        const IconPresetDefinition *preset = icons.catalog->presetById(id);
        QVERIFY(preset != nullptr);
        QCOMPARE(preset->identity.name,
                 QString::fromUtf8(expectedIconPresets[index].name));
        QVERIFY2(!preset->identity.description.isEmpty(), qPrintable(id));
        QVERIFY(preset->identity.builtIn);
        // PD-21: the revised Pedestal preset explicitly enables its plate.
        const bool pedestalPreset = id == QStringLiteral("blue-pedestal") || id == QStringLiteral("red-pedestal");
        QCOMPARE(preset->identity.revision, pedestalPreset ? 2 : 1);
        QCOMPARE(preset->panelValues().value(QStringLiteral("iconPedestalEnabled")).toBool(),
                 pedestalPreset);
        QVERIFY(preset->identity.derivedFromPresetId.isEmpty());
        // The real application glyph is the default glyph policy.
        QCOMPARE(preset->icon.glyphPolicy.mode, QStringLiteral("original"));
        QVERIFY(preset->compatibility.reducedMotionSupport);

        const PresetCompatibility resolved = PresetCapabilityResolver::resolveIconPreset(
            *preset, *m_styles, *m_profiles);
        QVERIFY2(resolved.available && !resolved.fallbackApplied,
                 qPrintable(id + QStringLiteral(": ") + resolved.reasonCode));
        const auto style = PresetCapabilityResolver::resolveIconStyle(*m_styles, *preset);
        QVERIFY2(style.valid, qPrintable(id + QLatin1Char('\n') +
                                         describe(style.diagnostics)));
        // The safe fallback references are implemented resources too.
        QVERIFY(m_styles->contains(preset->fallback.iconStyleId));
        QVERIFY(m_profiles->contains(preset->fallback.motionProfileId));
    }

    // The lineage TASK-0034 declared on its themes is now closed: each named
    // preset exists and is built on the theme that named it.
    int declaredIntents = 0;
    for (const QVariant &candidate : std::as_const(m_themes))
    {
        const QVariantMap theme = candidate.toMap();
        const QString presetId = theme.value(QStringLiteral("presetIntent")).toMap()
            .value(QStringLiteral("panelPresetId")).toString();
        if (presetId.isEmpty())
        {
            continue;
        }
        ++declaredIntents;
        const PanelPresetDefinition *preset = panels.catalog->presetById(presetId);
        QVERIFY2(preset != nullptr, qPrintable(presetId));
        QCOMPARE(preset->panel.themeId(), theme.value(QStringLiteral("id")).toString());
    }
    QCOMPARE(declaredIntents, 3);
}

void PresetCatalogTest::builtInPresetsAreDistinctAndCoverTheirFamilies()
{
    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndexPath(), context());
    QVERIFY2(icons.isValid(), qPrintable(describe(icons.diagnostics)));
    const auto panels = PanelPresetCatalog::loadBuiltIn(
        panelIndexPath(), context(&*icons.catalog));
    QVERIFY2(panels.isValid(), qPrintable(describe(panels.diagnostics)));

    // No preset is another preset under a different name.
    QSet<QByteArray> iconSignatures;
    for (const QString &id : icons.catalog->presetIds())
    {
        const IconPresetDefinition *preset = icons.catalog->presetById(id);
        const auto style = PresetCapabilityResolver::resolveIconStyle(*m_styles, *preset);
        QVERIFY(style.valid);
        iconSignatures.insert(toJson({
            {QStringLiteral("layers"), style.projection.value(QStringLiteral("layers"))},
            {QStringLiteral("states"), style.projection.value(QStringLiteral("states"))},
            {QStringLiteral("motion"), preset->icon.motion.toVariantMap()},
        }));
    }
    QCOMPARE(iconSignatures.size(), 15);
    QSet<QByteArray> panelSignatures;
    for (const QString &id : panels.catalog->presetIds())
    {
        const PanelPresetDefinition *preset = panels.catalog->presetById(id);
        QVariantMap signature = preset->panelValues();
        signature.insert(QStringLiteral("recommendedIconPresetId"),
                         preset->panel.recommendedIconPresetId);
        panelSignatures.insert(toJson(signature));
    }
    QCOMPARE(panelSignatures.size(), 15);

    // Each preset is built on the resource family the specification names.
    const auto panel = [&](const char *id) -> const PanelPresetDefinition &
    {
        return *panels.catalog->presetById(QString::fromLatin1(id));
    };
    const auto category = [&](const char *id)
    {
        return themeCategory(m_themes, panel(id).panel.themeId());
    };
    const auto tier = [&](const char *id)
    {
        return panel(id).preview.rendererTier;
    };
    const auto freeHost = [&](const char *id)
    {
        return panel(id).panel.configuration.host.kind ==
            ArchDock::PanelHostKind::FreeDesktop;
    };
    const auto layout = [&](const char *id)
    {
        return panel(id).panel.configuration.layout.pathType;
    };

    QCOMPARE(category("obsidian-glass-dock"), QStringLiteral("glass"));
    QCOMPARE(tier("obsidian-glass-dock"), QStringLiteral("procedural2d"));
    QCOMPARE(category("metallic-shelf-dock"), QStringLiteral("metallic"));
    QCOMPARE(tier("metallic-shelf-dock"), QStringLiteral("procedural2d"));
    for (const char *id : {"sci-fi-chassis-blue", "sci-fi-chassis-red",
                           "sci-fi-chassis-dark"})
    {
        QCOMPARE(panel(id).panel.themeId(), QString::fromLatin1(id));
        QCOMPARE(category(id), QStringLiteral("chassis"));
        QCOMPARE(tier(id), QStringLiteral("skinned2d"));
        QVERIFY(!freeHost(id));
    }
    for (const char *id : {"energy-frame-cyan", "energy-frame-green",
                           "energy-frame-orange", "energy-frame-purple"})
    {
        QCOMPARE(panel(id).panel.themeId(), QString::fromLatin1(id));
        QCOMPARE(category(id), QStringLiteral("energy"));
        QCOMPARE(tier(id), QStringLiteral("skinned2d"));
    }
    // A compact procedural rail: smaller than the default dock.
    QCOMPARE(tier("minimal-neon-rail"), QStringLiteral("procedural2d"));
    QCOMPARE(category("minimal-neon-rail"), QStringLiteral("neon"));
    QVERIFY(panel("minimal-neon-rail").panel.configuration.placement.height < 76);
    QVERIFY(panel("minimal-neon-rail").panel.configuration.iconStyle.size < 52);
    // A visible closed shell that opens on hover.
    const ArchDock::PanelDefinition &rail =
        panel("mechanical-collapsible-rail").panel.configuration;
    QCOMPARE(rail.presentation.mode, QStringLiteral("collapsed"));
    QCOMPARE(rail.presentation.trigger, QStringLiteral("hover"));
    QVERIFY(rail.presentation.collapseMechanism != QStringLiteral("open"));
    QCOMPARE(category("mechanical-collapsible-rail"), QStringLiteral("chassis"));
    QVERIFY(panel("mechanical-collapsible-rail").compatibility.requiredCapabilities
                .contains(rail.presentation.collapseMechanism));
    // The free-panel families, each with its safe procedural fallback tier.
    QCOMPARE(layout("circular-blue-ring"), QStringLiteral("ring"));
    QCOMPARE(layout("octagonal-platform"), QStringLiteral("octagon"));
    QCOMPARE(layout("orange-arc-dock"), QStringLiteral("arc"));
    for (const char *id : {"circular-blue-ring", "octagonal-platform",
                           "orange-arc-dock"})
    {
        QVERIFY(freeHost(id));
        QCOMPARE(category(id), QStringLiteral("perspective"));
        QCOMPARE(tier(id), QStringLiteral("baked2.5d"));
        QCOMPARE(panel(id).preview.fallbackTier, QStringLiteral("procedural2d"));
        QCOMPARE(panel(id).preview.previewMode, QStringLiteral("free"));
    }
    // Approved adaptation D-2: no semicircle-capable 2.5D or 3D resource
    // exists, so this preset is the procedural free semicircle and says so.
    QVERIFY(freeHost("holographic-semicircle"));
    QCOMPARE(layout("holographic-semicircle"), QStringLiteral("semicircle"));
    QCOMPARE(tier("holographic-semicircle"), QStringLiteral("procedural2d"));
    QVERIFY(panel("holographic-semicircle").panel.themeId().isEmpty());
    // No built-in preset claims a renderer tier it cannot be built on.
    for (const QString &id : panels.catalog->presetIds())
    {
        QVERIFY2(panels.catalog->presetById(id)->preview.rendererTier !=
                     QStringLiteral("true3d"), qPrintable(id));
    }

    const auto icon = [&](const char *id) -> const IconPresetDefinition &
    {
        return *icons.catalog->presetById(QString::fromLatin1(id));
    };
    const auto overridden = [&](const char *id)
    {
        return !icon(id).icon.visualOverrides.isEmpty() &&
            !icon(id).icon.stateOverrides.isEmpty();
    };
    QCOMPARE(icon("original-clean").icon.iconStyleId, QStringLiteral("plain-original"));
    QCOMPARE(icon("original-clean").icon.motion.profileId, QStringLiteral("none"));
    QCOMPARE(icon("minimal-glow").icon.iconStyleId, QStringLiteral("plain-original"));
    QCOMPARE(icon("minimal-glow").icon.motion.profileId, QStringLiteral("glow"));
    QVERIFY(icon("minimal-glow").icon.motion.intensity < 1.0);
    // The style families that ship as packages are used as they are.
    for (const char *id : {"metallic-blue", "metallic-red", "neon-green",
                           "neon-orange", "dark-orb"})
    {
        QCOMPARE(icon(id).icon.iconStyleId, QString::fromLatin1(id));
        QVERIFY(!overridden(id));
    }
    // Approved adaptation D-1: these five are a shipped style plus declared
    // overrides, in the family the specification names.
    QCOMPARE(icon("glass-tile").icon.iconStyleId, QStringLiteral("metallic-blue"));
    QCOMPARE(icon("holographic-tile").icon.iconStyleId, QStringLiteral("metallic-blue"));
    QCOMPARE(icon("beveled-sci-fi").icon.iconStyleId, QStringLiteral("metallic-red"));
    QCOMPARE(icon("blue-pedestal").icon.iconStyleId, QStringLiteral("dark-orb"));
    QCOMPARE(icon("red-pedestal").icon.iconStyleId, QStringLiteral("dark-orb"));
    for (const char *id : {"glass-tile", "holographic-tile", "beveled-sci-fi",
                           "blue-pedestal", "red-pedestal"})
    {
        QVERIFY2(overridden(id), id);
    }
    // The three motion presets are a base preset plus its named motion.
    QCOMPARE(icon("metallic-blue-slow-turn").icon.iconStyleId,
             QStringLiteral("metallic-blue"));
    QCOMPARE(icon("metallic-blue-slow-turn").icon.motion.profileId,
             QStringLiteral("slow-y-turn"));
    QCOMPARE(icon("neon-green-enlarge").icon.iconStyleId, QStringLiteral("neon-green"));
    QCOMPARE(icon("neon-green-enlarge").icon.motion.profileId,
             QStringLiteral("enlarge"));
    QVERIFY(icon("neon-green-enlarge").icon.motion.magnificationRadius >
            icon("neon-green").icon.motion.magnificationRadius);
    QCOMPARE(icon("dark-orb-spiral").icon.iconStyleId, QStringLiteral("dark-orb"));
    QCOMPARE(icon("dark-orb-spiral").icon.motion.profileId, QStringLiteral("spiral"));
}

void PresetCatalogTest::builtInFilesSurviveEveryStoreOperation()
{
    const QByteArray before = directoryDigest(presetRoot());
    const auto icons = IconPresetCatalog::loadBuiltIn(iconIndexPath(), context());
    QVERIFY2(icons.isValid(), qPrintable(describe(icons.diagnostics)));
    const auto panels = PanelPresetCatalog::loadBuiltIn(
        panelIndexPath(), context(&*icons.catalog));
    QVERIFY2(panels.isValid(), qPrintable(describe(panels.diagnostics)));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const UserPresetStore store(directory.filePath(QStringLiteral("presets")));
    QString error;
    for (const QString &id : panels.catalog->presetIds())
    {
        const PanelPresetDefinition *builtIn = panels.catalog->presetById(id);
        PanelPresetDefinition draft = UserPresetStore::derivedFrom(
            *builtIn, builtIn->identity.name + QStringLiteral(" (mine)"));
        draft.panel.configuration.iconStyle.size = 60;
        const auto saved = store.save(draft, &error);
        QVERIFY2(saved.has_value(), qPrintable(id + QStringLiteral(": ") + error));
        QVERIFY(PresetIdentity::isUserId(saved->identity.id));
        QCOMPARE(saved->identity.derivedFromPresetId, id);
        QCOMPARE(saved->panel.configuration.iconStyle.size, 60);
        // Even the built-in itself, handed to the store, lands as a user copy.
        QVERIFY(store.save(*builtIn, &error).has_value());
    }
    for (const QString &id : icons.catalog->presetIds())
    {
        const auto saved = store.save(UserPresetStore::derivedFrom(
            *icons.catalog->presetById(id), QString{}), &error);
        QVERIFY2(saved.has_value(), qPrintable(id + QStringLiteral(": ") + error));
        QVERIFY(!store.removeIconPreset(id, &error));
        QVERIFY(store.removeIconPreset(saved->identity.id, &error));
    }
    QCOMPARE(store.panelPresets().size(), 30);
    QVERIFY(store.iconPresets().isEmpty());

    // Every derivative reloads and still validates against the real resources.
    PresetReferenceContext references = context(&*icons.catalog);
    for (const PanelPresetDefinition &preset : store.panelPresets())
    {
        const auto diagnostics =
            ArchDock::validatePanelPresetReferences(preset, references);
        QVERIFY2(diagnostics.isEmpty(), qPrintable(describe(diagnostics)));
    }
    QCOMPARE(directoryDigest(presetRoot()), before);
}

QTEST_GUILESS_MAIN(PresetCatalogTest)

#include "PresetCatalogTest.moc"
