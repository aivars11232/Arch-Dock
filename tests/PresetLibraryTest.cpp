#include "PanelRegistry.h"
#include "presets/PresetLibrary.h"

#include "PresetTestSupport.h"

#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using ArchDock::PresetIdentity;
using ArchDock::PresetLibrary;

using namespace PresetTestSupport;

namespace
{

// Set by the staged-install smoke to the prefix Arch Dock was installed into.
// Every resource this program reads must then come from that prefix.
QString stagePrefix()
{
    return qEnvironmentVariable("ARCHDOCK_PRESET_STAGE_PREFIX");
}

QString stagedOrSource(const QString &stagedRelativePath, const char *sourcePath)
{
    return stagePrefix().isEmpty()
        ? QString::fromUtf8(sourcePath)
        : QDir(stagePrefix()).filePath(stagedRelativePath);
}

QString builtInRoot()
{
    return stagedOrSource(QStringLiteral("share/arch-dock/presets"),
                          ARCHDOCK_SOURCE_PRESET_ROOT);
}

QVariantList themeCatalog()
{
    return parseJson(readBytes(stagedOrSource(
                         QStringLiteral("share/arch-dock/themes/builtin-themes.json"),
                         ARCHDOCK_SOURCE_THEME_CATALOG_PATH)))
        .value(QStringLiteral("themes")).toList();
}

QStringList cardIds(const QVariantList &cards)
{
    QStringList ids;
    for (const QVariant &card : cards)
    {
        ids.append(card.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

QVariantMap cardById(const QVariantList &cards, const QString &id)
{
    for (const QVariant &card : cards)
    {
        if (card.toMap().value(QStringLiteral("id")).toString() == id)
        {
            return card.toMap();
        }
    }
    return {};
}

QVariantMap compatibilityOf(const QVariantMap &card)
{
    return card.value(QStringLiteral("compatibility")).toMap();
}

// The record a card hands the shared renderer as its panel definition.
QVariantMap previewDefinition(const QVariantMap &card)
{
    return card.value(QStringLiteral("preview")).toMap()
        .value(QStringLiteral("panelDefinition")).toMap();
}

QVariantMap previewTheme(const QVariantMap &card)
{
    return card.value(QStringLiteral("preview")).toMap()
        .value(QStringLiteral("themeDefinition")).toMap();
}

// Everything a preset action could conceivably disturb in the registry.
QByteArray registryState(const PanelRegistry &registry)
{
    QVariantMap state{{QStringLiteral("revision"), registry.revision()}};
    for (const QString &panelId : registry.panelIds())
    {
        state.insert(panelId, registry.panelSnapshot(panelId));
    }
    return toJson(state);
}

// The shared renderer module: the build tree's, or a staged install's.
QString renderingImportRoot()
{
    return qEnvironmentVariable("ARCHDOCK_RENDERING_IMPORT_ROOT",
                                QStringLiteral(ARCHDOCK_RENDERING_IMPORT_ROOT));
}

QUrl runtimeComponent(const QString &fileName)
{
    return QUrl::fromLocalFile(
        QDir(qEnvironmentVariable("ARCHDOCK_TEST_RUNTIME_QML_DIR",
             QStringLiteral(ARCHDOCK_SOURCE_RUNTIME_QML_DIR))).filePath(fileName));
}

// Visual-tree lookup: list delegates are visual children, not QObject ones.
QQuickItem *findItem(QQuickItem *root, const QString &objectName)
{
    QList<QQuickItem *> pending{root};
    while (!pending.isEmpty())
    {
        QQuickItem *item = pending.takeLast();
        if (item->objectName() == objectName)
        {
            return item;
        }
        pending.append(item->childItems());
    }
    return nullptr;
}

int distinctColors(const QImage &image)
{
    QSet<QRgb> colors;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            colors.insert(image.pixel(x, y));
        }
    }
    return int(colors.size());
}

// Collects every warning a QML engine reports, so a test can require none.
class QmlWarnings final : public QObject
{
public:
    explicit QmlWarnings(QQmlEngine *engine)
    {
        connect(engine, &QQmlEngine::warnings, this,
                [this](const QList<QQmlError> &warnings)
                {
                    for (const QQmlError &warning : warnings)
                    {
                        messages.append(warning.toString());
                    }
                });
    }

    QStringList messages;
};

}

class PresetLibraryTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void locatesTheCatalogItShipsWith();
    void listsExactlyTheBuiltInCatalogs();
    void cardsDescribeHostRendererBehaviourAndMotion();
    void everyBuiltInCardCarriesSharedRendererPreviewData();
    void unusableThemeSelectsTheFallbackOrMarksThePresetIncompatible();
    void userPresetActionsNeverTouchABuiltIn();
    void listingAndPreviewingChangeNothing();
    void rejectedCatalogIsReportedAndOffersNothing();
    void everyBuiltInCardRendersThroughTheSharedRenderer();
    void browsingEveryPresetPageChangesNoPanel();

private:
    QTemporaryDir m_settingsDirectory;
};

void PresetLibraryTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QStandardPaths::setTestModeEnabled(false);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       m_settingsDirectory.path());
    QCOMPARE(themeCatalog().size(), 16);
}

void PresetLibraryTest::locatesTheCatalogItShipsWith()
{
    const QString located = PresetLibrary::locateBuiltInRoot();
    QVERIFY(QFileInfo(QDir(located).filePath(
                          QStringLiteral("panels/builtin-panel-presets.json")))
                .isFile());
    QVERIFY(QFileInfo(QDir(located).filePath(
                          QStringLiteral("icons/builtin-icon-presets.json")))
                .isFile());
    if (!stagePrefix().isEmpty())
    {
        // A staged run reads the installed catalog, never the source tree.
        QCOMPARE(QDir(located).canonicalPath(), QDir(builtInRoot()).canonicalPath());
        QVERIFY(QDir(located).canonicalPath() !=
                QDir(QString::fromUtf8(ARCHDOCK_SOURCE_PRESET_ROOT)).canonicalPath());
    }
    // The user's presets live under the application's own data directory.
    QCOMPARE(PresetLibrary::defaultUserRoot(),
             QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                 QStringLiteral("/presets"));
}

void PresetLibraryTest::listsExactlyTheBuiltInCatalogs()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const PanelRegistry registry(themeCatalog());
    const PresetLibrary library(registry, builtInRoot(),
                                userDirectory.filePath(QStringLiteral("presets")));

    const QVariantMap status = library.catalogStatus();
    QVERIFY2(status.value(QStringLiteral("valid")).toBool(),
             qPrintable(QString::fromUtf8(toJson(status))));
    QCOMPARE(status.value(QStringLiteral("errorCode")).toString(), QString{});
    QCOMPARE(status.value(QStringLiteral("panelPresetCount")).toInt(), 15);
    QCOMPARE(status.value(QStringLiteral("iconPresetCount")).toInt(), 15);
    QCOMPARE(status.value(QStringLiteral("builtInRoot")).toString(), builtInRoot());
    QCOMPARE(status.value(QStringLiteral("userRoot")).toString(),
             library.userRoot());

    QCOMPARE(cardIds(library.panelPresets(QStringLiteral("builtin"))),
             QStringList({
                 QStringLiteral("obsidian-glass-dock"),
                 QStringLiteral("metallic-shelf-dock"),
                 QStringLiteral("sci-fi-chassis-blue"),
                 QStringLiteral("sci-fi-chassis-red"),
                 QStringLiteral("sci-fi-chassis-dark"),
                 QStringLiteral("energy-frame-cyan"),
                 QStringLiteral("energy-frame-green"),
                 QStringLiteral("energy-frame-orange"),
                 QStringLiteral("energy-frame-purple"),
                 QStringLiteral("minimal-neon-rail"),
                 QStringLiteral("mechanical-collapsible-rail"),
                 QStringLiteral("circular-blue-ring"),
                 QStringLiteral("octagonal-platform"),
                 QStringLiteral("orange-arc-dock"),
                 QStringLiteral("holographic-semicircle"),
             }));
    QCOMPARE(cardIds(library.iconPresets(QStringLiteral("builtin"))),
             QStringList({
                 QStringLiteral("original-clean"),
                 QStringLiteral("glass-tile"),
                 QStringLiteral("metallic-blue"),
                 QStringLiteral("metallic-red"),
                 QStringLiteral("neon-green"),
                 QStringLiteral("neon-orange"),
                 QStringLiteral("dark-orb"),
                 QStringLiteral("blue-pedestal"),
                 QStringLiteral("red-pedestal"),
                 QStringLiteral("holographic-tile"),
                 QStringLiteral("minimal-glow"),
                 QStringLiteral("beveled-sci-fi"),
                 QStringLiteral("metallic-blue-slow-turn"),
                 QStringLiteral("neon-green-enlarge"),
                 QStringLiteral("dark-orb-spiral"),
             }));

    // The two catalogs are separate: neither scope of one lists the other.
    for (const QVariant &card : library.panelPresets(QStringLiteral("builtin")))
    {
        QCOMPARE(card.toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("panel"));
        QVERIFY(card.toMap().value(QStringLiteral("builtIn")).toBool());
    }
    for (const QVariant &card : library.iconPresets(QStringLiteral("builtin")))
    {
        QCOMPARE(card.toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("icon"));
        QVERIFY(card.toMap().value(QStringLiteral("builtIn")).toBool());
    }
    QVERIFY(library.panelPresets(QStringLiteral("user")).isEmpty());
    QVERIFY(library.iconPresets(QStringLiteral("user")).isEmpty());
    QVERIFY(library.panelPresets(QStringLiteral("everything")).isEmpty());
    QVERIFY(library.iconPresets(QString{}).isEmpty());
}

void PresetLibraryTest::cardsDescribeHostRendererBehaviourAndMotion()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const PanelRegistry registry(themeCatalog());
    const PresetLibrary library(registry, builtInRoot(),
                                userDirectory.filePath(QStringLiteral("presets")));
    const QVariantList panels = library.panelPresets(QStringLiteral("builtin"));
    const QVariantList icons = library.iconPresets(QStringLiteral("builtin"));

    const QVariantMap ring = cardById(panels, QStringLiteral("circular-blue-ring"));
    QCOMPARE(ring.value(QStringLiteral("name")).toString(),
             QStringLiteral("Circular Blue Ring"));
    QVERIFY(!ring.value(QStringLiteral("description")).toString().isEmpty());
    QCOMPARE(ring.value(QStringLiteral("revision")).toInt(), 1);
    QCOMPARE(ring.value(QStringLiteral("derivedFromPresetId")).toString(), QString{});
    QCOMPARE(ring.value(QStringLiteral("hostKind")).toString(),
             QStringLiteral("free-desktop"));
    QCOMPARE(ring.value(QStringLiteral("hostKinds")).toStringList(),
             QStringList{QStringLiteral("free-desktop")});
    QCOMPARE(ring.value(QStringLiteral("orientations")).toStringList(),
             QStringList{QStringLiteral("free")});
    QCOMPARE(ring.value(QStringLiteral("layout")).toString(), QStringLiteral("ring"));
    QCOMPARE(ring.value(QStringLiteral("layouts")).toStringList(),
             QStringList({QStringLiteral("ring"), QStringLiteral("circular")}));
    QCOMPARE(ring.value(QStringLiteral("rendererTier")).toString(),
             QStringLiteral("baked2.5d"));
    QCOMPARE(ring.value(QStringLiteral("fallbackTier")).toString(),
             QStringLiteral("procedural2d"));
    QCOMPARE(ring.value(QStringLiteral("themeId")).toString(),
             QStringLiteral("ring-platform-blue"));
    QCOMPARE(ring.value(QStringLiteral("themeName")).toString(),
             QStringLiteral("Blue Ring Platform"));
    QCOMPARE(ring.value(QStringLiteral("visibilityMode")).toString(),
             QStringLiteral("always"));
    QCOMPARE(ring.value(QStringLiteral("presentationMode")).toString(),
             QStringLiteral("open"));
    QCOMPARE(ring.value(QStringLiteral("motionProfileId")).toString(),
             QStringLiteral("glow"));
    QCOMPARE(ring.value(QStringLiteral("motionProfileName")).toString(),
             QStringLiteral("Glow"));
    QCOMPARE(ring.value(QStringLiteral("motionTrigger")).toString(),
             QStringLiteral("hover"));
    QCOMPARE(ring.value(QStringLiteral("recommendedIconPresetId")).toString(),
             QStringLiteral("blue-pedestal"));
    QCOMPARE(ring.value(QStringLiteral("recommendedIconPresetName")).toString(),
             QStringLiteral("Blue Pedestal"));

    const QVariantMap rail =
        cardById(panels, QStringLiteral("mechanical-collapsible-rail"));
    QCOMPARE(rail.value(QStringLiteral("hostKind")).toString(),
             QStringLiteral("native-edge"));
    QCOMPARE(rail.value(QStringLiteral("presentationMode")).toString(),
             QStringLiteral("collapsed"));
    QCOMPARE(rail.value(QStringLiteral("presentationTrigger")).toString(),
             QStringLiteral("hover"));
    QCOMPARE(rail.value(QStringLiteral("collapseMechanism")).toString(),
             QStringLiteral("split"));

    // The procedural semicircle names no theme and says so.
    const QVariantMap semicircle =
        cardById(panels, QStringLiteral("holographic-semicircle"));
    QCOMPARE(semicircle.value(QStringLiteral("themeId")).toString(), QString{});
    QCOMPARE(semicircle.value(QStringLiteral("themeName")).toString(), QString{});
    QCOMPARE(semicircle.value(QStringLiteral("rendererTier")).toString(),
             QStringLiteral("procedural2d"));

    const QVariantMap pedestal = cardById(icons, QStringLiteral("blue-pedestal"));
    QCOMPARE(pedestal.value(QStringLiteral("name")).toString(),
             QStringLiteral("Blue Pedestal"));
    QCOMPARE(pedestal.value(QStringLiteral("iconStyleId")).toString(),
             QStringLiteral("dark-orb"));
    QCOMPARE(pedestal.value(QStringLiteral("iconStyleName")).toString(),
             QStringLiteral("Dark Orb"));
    QVERIFY(pedestal.value(QStringLiteral("customized")).toBool());
    QCOMPARE(pedestal.value(QStringLiteral("rendererTiers")).toStringList(),
             QStringList{QStringLiteral("procedural2d")});
    QVERIFY(pedestal.value(QStringLiteral("reducedMotionSupport")).toBool());
    QCOMPARE(pedestal.value(QStringLiteral("glyphMode")).toString(),
             QStringLiteral("original"));
    QCOMPARE(pedestal.value(QStringLiteral("motionProfileName")).toString(),
             QStringLiteral("Glow"));

    const QVariantMap clean = cardById(icons, QStringLiteral("original-clean"));
    QVERIFY(!clean.value(QStringLiteral("customized")).toBool());
    QCOMPARE(clean.value(QStringLiteral("motionProfileId")).toString(),
             QStringLiteral("none"));
    const QVariantMap turn =
        cardById(icons, QStringLiteral("metallic-blue-slow-turn"));
    QCOMPARE(turn.value(QStringLiteral("motionProfileId")).toString(),
             QStringLiteral("slow-y-turn"));
    QCOMPARE(turn.value(QStringLiteral("motionTrigger")).toString(),
             QStringLiteral("idle"));
}

void PresetLibraryTest::everyBuiltInCardCarriesSharedRendererPreviewData()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const PanelRegistry registry(themeCatalog());
    const PresetLibrary library(registry, builtInRoot(),
                                userDirectory.filePath(QStringLiteral("presets")));
    const QVariantList icons = library.iconPresets(QStringLiteral("builtin"));

    for (const QVariant &value : library.panelPresets(QStringLiteral("builtin")))
    {
        const QVariantMap card = value.toMap();
        const QByteArray id = card.value(QStringLiteral("id")).toString().toUtf8();
        const QVariantMap compatibility = compatibilityOf(card);
        // Every renderer tier a built-in uses is part of this build, so each
        // preset is drawn exactly as it declares.
        QVERIFY2(compatibility.value(QStringLiteral("available")).toBool(), id);
        QVERIFY2(!compatibility.value(QStringLiteral("fallbackApplied")).toBool(), id);
        QCOMPARE(compatibility.value(QStringLiteral("effectiveRendererTier")),
                 card.value(QStringLiteral("rendererTier")));
        QCOMPARE(compatibility.value(QStringLiteral("effectiveThemeId")),
                 card.value(QStringLiteral("themeId")));
        QCOMPARE(compatibility.value(QStringLiteral("hostKind")),
                 card.value(QStringLiteral("hostKind")));
        QVERIFY2(card.value(QStringLiteral("hostKinds")).toStringList().contains(
                     card.value(QStringLiteral("hostKind")).toString()), id);

        const QVariantMap definition = previewDefinition(card);
        QVERIFY2(!definition.isEmpty(), id);
        QCOMPARE(definition.value(QStringLiteral("layout")),
                 card.value(QStringLiteral("layout")));
        QCOMPARE(definition.value(QStringLiteral("completeThemeId")),
                 card.value(QStringLiteral("themeId")));
        QCOMPARE(definition.value(QStringLiteral("iconAnimation")),
                 card.value(QStringLiteral("motionProfileId")));
        QCOMPARE(definition.value(QStringLiteral("effectiveRendererTier")),
                 card.value(QStringLiteral("rendererTier")));
        const QVariantMap resolution =
            definition.value(QStringLiteral("capabilityResolution")).toMap();
        QVERIFY2(resolution.value(QStringLiteral("available")).toBool(), id);
        QCOMPARE(resolution.value(QStringLiteral("renderer")).toMap()
                     .value(QStringLiteral("effectiveTier")),
                 card.value(QStringLiteral("rendererTier")));
        QCOMPARE(definition.value(QStringLiteral("animationProfiles")).toList().size(),
                 23);
        QCOMPARE(card.value(QStringLiteral("preview")).toMap()
                     .value(QStringLiteral("previewMode")).toString(),
                 card.value(QStringLiteral("hostKind")).toString() ==
                         QStringLiteral("free-desktop")
                     ? QStringLiteral("free") : QStringLiteral("horizontal"));

        // The paired icon preset supplies the style the icons are drawn with.
        const QVariantMap paired = cardById(
            icons, card.value(QStringLiteral("recommendedIconPresetId")).toString());
        QVERIFY2(!paired.isEmpty(), id);
        const QVariantMap style =
            definition.value(QStringLiteral("iconStyleDefinition")).toMap();
        QVERIFY2(style.value(QStringLiteral("valid")).toBool(), id);
        QCOMPARE(style.value(QStringLiteral("resolvedStyleId")),
                 paired.value(QStringLiteral("iconStyleId")));
        QCOMPARE(definition.value(QStringLiteral("iconStyle")),
                 paired.value(QStringLiteral("iconStyleId")));
        QCOMPARE(style, previewDefinition(paired)
                            .value(QStringLiteral("iconStyleDefinition")).toMap());

        // A themed preset hands the renderer that theme; a packaged theme
        // arrives with its package's runtime projection merged in.
        const QVariantMap theme = previewTheme(card);
        QCOMPARE(theme.value(QStringLiteral("id")).toString(),
                 card.value(QStringLiteral("themeId")).toString());
        if (theme.contains(QStringLiteral("packageManifest")))
        {
            const std::optional<QVariantMap> projection =
                registry.builtInThemeRuntimeProjection(
                    theme.value(QStringLiteral("id")).toString());
            QVERIFY2(projection.has_value() && !projection->isEmpty(), id);
            for (auto entry = projection->cbegin(); entry != projection->cend();
                 ++entry)
            {
                QVERIFY2(theme.value(entry.key()) == entry.value(),
                         qPrintable(QString::fromUtf8(id) + QLatin1Char(' ') +
                                    entry.key()));
            }
        }

        // A preview never carries the identity of a real panel: the host
        // association and the content list are not part of the record at all.
        for (const char *key : {"id", "screenId", "nativePanelId",
                                "nativeOwnershipToken", "freeOwnershipToken",
                                "freeDesktopContainmentId", "contentUrls",
                                "contentAppIds"})
        {
            QVERIFY2(!definition.contains(QString::fromLatin1(key)),
                     qPrintable(QString::fromUtf8(id) + QLatin1Char(' ') +
                                QString::fromLatin1(key)));
        }
    }

    for (const QVariant &value : icons)
    {
        const QVariantMap card = value.toMap();
        const QByteArray id = card.value(QStringLiteral("id")).toString().toUtf8();
        const QVariantMap compatibility = compatibilityOf(card);
        QVERIFY2(compatibility.value(QStringLiteral("available")).toBool(), id);
        QVERIFY2(!compatibility.value(QStringLiteral("fallbackApplied")).toBool(), id);
        QCOMPARE(compatibility.value(QStringLiteral("effectiveIconStyleId")),
                 card.value(QStringLiteral("iconStyleId")));
        QCOMPARE(compatibility.value(QStringLiteral("effectiveMotionProfileId")),
                 card.value(QStringLiteral("motionProfileId")));

        // A plain default dock that takes only this preset's icon layer.
        const QVariantMap definition = previewDefinition(card);
        QVERIFY2(!definition.isEmpty(), id);
        QCOMPARE(definition.value(QStringLiteral("iconStyle")),
                 card.value(QStringLiteral("iconStyleId")));
        QCOMPARE(definition.value(QStringLiteral("iconAnimation")),
                 card.value(QStringLiteral("motionProfileId")));
        QCOMPARE(definition.value(QStringLiteral("animationTrigger")),
                 card.value(QStringLiteral("motionTrigger")));
        QCOMPARE(definition.value(QStringLiteral("completeThemeId")).toString(),
                 QString{});
        QCOMPARE(definition.value(QStringLiteral("edge")).toString(),
                 QStringLiteral("bottom"));
        QCOMPARE(definition.value(QStringLiteral("effectiveRendererTier")).toString(),
                 QStringLiteral("procedural2d"));
        QVERIFY2(previewTheme(card).isEmpty(), id);
        const QVariantMap style =
            definition.value(QStringLiteral("iconStyleDefinition")).toMap();
        QVERIFY2(style.value(QStringLiteral("valid")).toBool(), id);
        QCOMPARE(style.value(QStringLiteral("resolvedStyleId")),
                 card.value(QStringLiteral("iconStyleId")));
    }

    // An overridden preset is drawn from its merged style, not from the style
    // it started as.
    const QVariantMap pedestalStyle = previewDefinition(
        cardById(icons, QStringLiteral("blue-pedestal")))
        .value(QStringLiteral("iconStyleDefinition")).toMap();
    const QVariantMap orbStyle = previewDefinition(
        cardById(icons, QStringLiteral("dark-orb")))
        .value(QStringLiteral("iconStyleDefinition")).toMap();
    QCOMPARE(pedestalStyle.value(QStringLiteral("resolvedStyleId")),
             orbStyle.value(QStringLiteral("resolvedStyleId")));
    QVERIFY(pedestalStyle.value(QStringLiteral("layers")) !=
            orbStyle.value(QStringLiteral("layers")));
}

void PresetLibraryTest::unusableThemeSelectsTheFallbackOrMarksThePresetIncompatible()
{
    // The catalog expects package versions that are not the installed ones,
    // which is how an out-of-date or damaged theme package shows up.
    QVariantList themes = themeCatalog();
    for (QVariant &entry : themes)
    {
        QVariantMap theme = entry.toMap();
        const QString id = theme.value(QStringLiteral("id")).toString();
        if (id == QStringLiteral("ring-platform-blue") ||
            id == QStringLiteral("sci-fi-chassis-dark"))
        {
            theme.insert(QStringLiteral("version"), 99);
            entry = theme;
        }
    }
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const PanelRegistry registry(themes);
    QVERIFY(!registry.builtInThemeRuntimeProjection(
                 QStringLiteral("ring-platform-blue")).has_value());
    const PresetLibrary library(registry, builtInRoot(),
                                userDirectory.filePath(QStringLiteral("presets")));
    QVERIFY(library.catalogStatus().value(QStringLiteral("valid")).toBool());
    const QVariantList panels = library.panelPresets(QStringLiteral("builtin"));
    QCOMPARE(panels.size(), 15);

    // A declared fallback theme takes over, and the preview is that theme's.
    const QVariantMap ring = cardById(panels, QStringLiteral("circular-blue-ring"));
    QVERIFY(compatibilityOf(ring).value(QStringLiteral("available")).toBool());
    QVERIFY(compatibilityOf(ring).value(QStringLiteral("fallbackApplied")).toBool());
    QCOMPARE(compatibilityOf(ring).value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("theme-package-unavailable"));
    QCOMPARE(compatibilityOf(ring).value(QStringLiteral("requestedRendererTier"))
                 .toString(), QStringLiteral("baked2.5d"));
    QCOMPARE(compatibilityOf(ring).value(QStringLiteral("effectiveRendererTier"))
                 .toString(), QStringLiteral("procedural2d"));
    QCOMPARE(compatibilityOf(ring).value(QStringLiteral("effectiveThemeId"))
                 .toString(), QStringLiteral("holographic-ring"));
    QCOMPARE(previewTheme(ring).value(QStringLiteral("id")).toString(),
             QStringLiteral("holographic-ring"));
    QCOMPARE(previewDefinition(ring).value(QStringLiteral("completeThemeId"))
                 .toString(), QStringLiteral("holographic-ring"));
    QCOMPARE(previewDefinition(ring).value(QStringLiteral("effectiveRendererTier"))
                 .toString(), QStringLiteral("procedural2d"));
    QCOMPARE(previewDefinition(ring).value(QStringLiteral("layout")).toString(),
             QStringLiteral("ring"));

    // No fallback theme named: the built-in procedural surface draws it.
    const QVariantMap dark = cardById(panels, QStringLiteral("sci-fi-chassis-dark"));
    QVERIFY(compatibilityOf(dark).value(QStringLiteral("available")).toBool());
    QVERIFY(compatibilityOf(dark).value(QStringLiteral("fallbackApplied")).toBool());
    QCOMPARE(compatibilityOf(dark).value(QStringLiteral("effectiveThemeId"))
                 .toString(), QString{});
    QCOMPARE(compatibilityOf(dark).value(QStringLiteral("effectiveRendererTier"))
                 .toString(), QStringLiteral("procedural2d"));
    QVERIFY(previewTheme(dark).isEmpty());
    QVERIFY(!previewDefinition(dark).isEmpty());

    // The collapsible rail needs the split mechanism only that theme has, so
    // it is incompatible and offers nothing to draw or apply.
    const QVariantMap rail =
        cardById(panels, QStringLiteral("mechanical-collapsible-rail"));
    QVERIFY(!compatibilityOf(rail).value(QStringLiteral("available")).toBool());
    QVERIFY(!compatibilityOf(rail).value(QStringLiteral("fallbackApplied")).toBool());
    QCOMPARE(compatibilityOf(rail).value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("theme-package-unavailable"));
    QVERIFY(!rail.contains(QStringLiteral("preview")));

    // Presets on other themes are not affected.
    int untouched = 0;
    for (const QVariant &value : panels)
    {
        const QVariantMap compatibility = compatibilityOf(value.toMap());
        untouched += compatibility.value(QStringLiteral("available")).toBool() &&
                !compatibility.value(QStringLiteral("fallbackApplied")).toBool()
            ? 1 : 0;
    }
    QCOMPARE(untouched, 12);
}

void PresetLibraryTest::userPresetActionsNeverTouchABuiltIn()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const QString userRoot = userDirectory.filePath(QStringLiteral("presets"));
    const PanelRegistry registry(themeCatalog());
    PresetLibrary library(registry, builtInRoot(), userRoot);
    QSignalSpy revisions(&library, &PresetLibrary::revisionChanged);
    const QByteArray builtInFiles = directoryDigest(builtInRoot());
    const QVariantList builtInPanels = library.panelPresets(QStringLiteral("builtin"));
    const QVariantList builtInIcons = library.iconPresets(QStringLiteral("builtin"));
    const auto succeeded = [](const QVariantMap &result)
    {
        return result.value(QStringLiteral("success")).toBool() &&
            result.value(QStringLiteral("errorCode")).toString().isEmpty();
    };
    const auto failedWith = [](const QVariantMap &result, const char *code)
    {
        return !result.value(QStringLiteral("success")).toBool() &&
            result.value(QStringLiteral("presetId")).toString().isEmpty() &&
            result.value(QStringLiteral("errorCode")).toString() ==
                QString::fromLatin1(code);
    };

    // Duplicating a built-in makes a user-owned preset with a new stable id.
    const QVariantMap copied = library.duplicatePreset(
        QStringLiteral("panel"), QStringLiteral("circular-blue-ring"),
        QStringLiteral("My Ring"));
    QVERIFY2(succeeded(copied), qPrintable(QString::fromUtf8(toJson(copied))));
    const QString panelId = copied.value(QStringLiteral("presetId")).toString();
    QVERIFY(PresetIdentity::isUserId(panelId));
    QCOMPARE(library.revision(), 1);
    QCOMPARE(revisions.count(), 1);
    QVariantList mine = library.panelPresets(QStringLiteral("user"));
    QCOMPARE(cardIds(mine), QStringList{panelId});
    QVariantMap card = mine.first().toMap();
    QCOMPARE(card.value(QStringLiteral("name")).toString(), QStringLiteral("My Ring"));
    QVERIFY(!card.value(QStringLiteral("builtIn")).toBool());
    QCOMPARE(card.value(QStringLiteral("revision")).toInt(), 1);
    QCOMPARE(card.value(QStringLiteral("derivedFromPresetId")).toString(),
             QStringLiteral("circular-blue-ring"));
    QCOMPARE(card.value(QStringLiteral("sourceRevision")).toInt(), 1);
    // It is a complete snapshot: it previews exactly as its source does.
    const QVariantMap source =
        cardById(builtInPanels, QStringLiteral("circular-blue-ring"));
    QCOMPARE(card.value(QStringLiteral("preview")),
             source.value(QStringLiteral("preview")));
    QCOMPARE(compatibilityOf(card), compatibilityOf(source));

    // Renaming replaces the user preset in place and advances its revision.
    const QVariantMap renamed = library.renamePreset(
        QStringLiteral("panel"), panelId, QStringLiteral("  Desk Ring  "));
    QVERIFY2(succeeded(renamed), qPrintable(QString::fromUtf8(toJson(renamed))));
    QCOMPARE(renamed.value(QStringLiteral("presetId")).toString(), panelId);
    card = library.panelPresets(QStringLiteral("user")).first().toMap();
    QCOMPARE(card.value(QStringLiteral("name")).toString(),
             QStringLiteral("Desk Ring"));
    QCOMPARE(card.value(QStringLiteral("revision")).toInt(), 2);
    QCOMPARE(card.value(QStringLiteral("derivedFromPresetId")).toString(),
             QStringLiteral("circular-blue-ring"));

    // A copy of a copy records the user preset and revision it came from.
    const QVariantMap again = library.duplicatePreset(
        QStringLiteral("panel"), panelId, QString{});
    QVERIFY(succeeded(again));
    const QString secondId = again.value(QStringLiteral("presetId")).toString();
    QVERIFY(secondId != panelId);
    const QVariantMap second = cardById(
        library.panelPresets(QStringLiteral("user")), secondId);
    QCOMPARE(second.value(QStringLiteral("name")).toString(),
             QStringLiteral("Desk Ring"));
    QCOMPARE(second.value(QStringLiteral("derivedFromPresetId")).toString(), panelId);
    QCOMPARE(second.value(QStringLiteral("sourceRevision")).toInt(), 2);

    // The icon catalog has its own store directory and its own actions.
    const QVariantMap iconCopy = library.duplicatePreset(
        QStringLiteral("icon"), QStringLiteral("blue-pedestal"),
        QStringLiteral("My Pedestal"));
    QVERIFY(succeeded(iconCopy));
    const QString iconId = iconCopy.value(QStringLiteral("presetId")).toString();
    QVERIFY(PresetIdentity::isUserId(iconId));
    const QVariantMap iconCard =
        library.iconPresets(QStringLiteral("user")).first().toMap();
    QCOMPARE(iconCard.value(QStringLiteral("name")).toString(),
             QStringLiteral("My Pedestal"));
    QCOMPARE(iconCard.value(QStringLiteral("derivedFromPresetId")).toString(),
             QStringLiteral("blue-pedestal"));
    QVERIFY(iconCard.value(QStringLiteral("customized")).toBool());
    QCOMPARE(iconCard.value(QStringLiteral("preview")),
             cardById(builtInIcons, QStringLiteral("blue-pedestal"))
                 .value(QStringLiteral("preview")));
    QCOMPARE(library.panelPresets(QStringLiteral("user")).size(), 2);
    QCOMPARE(library.iconPresets(QStringLiteral("user")).size(), 1);
    QVERIFY(succeeded(library.renamePreset(
        QStringLiteral("icon"), iconId, QStringLiteral("Desk Pedestal"))));
    QCOMPARE(library.revision(), 5);
    QCOMPARE(revisions.count(), 5);

    // Nothing reaches a built-in, a missing preset or the other catalog, and
    // a refused action is not announced as a change.
    QVERIFY(failedWith(library.renamePreset(
        QStringLiteral("panel"), QStringLiteral("circular-blue-ring"),
        QStringLiteral("Mine")), "not-user-preset"));
    QVERIFY(failedWith(library.removePreset(
        QStringLiteral("panel"), QStringLiteral("circular-blue-ring")),
        "not-user-preset"));
    QVERIFY(failedWith(library.removePreset(
        QStringLiteral("icon"), QStringLiteral("blue-pedestal")),
        "not-user-preset"));
    QVERIFY(failedWith(library.renamePreset(
        QStringLiteral("panel"), panelId, QStringLiteral("   ")),
        "missing-field"));
    QVERIFY(failedWith(library.duplicatePreset(
        QStringLiteral("panel"), QStringLiteral("no-such-preset"), QString{}),
        "preset-not-found"));
    QVERIFY(failedWith(library.duplicatePreset(
        QStringLiteral("icon"), QStringLiteral("circular-blue-ring"), QString{}),
        "preset-not-found"));
    QVERIFY(failedWith(library.removePreset(QStringLiteral("icon"), panelId),
                       "not-found"));
    QVERIFY(failedWith(library.duplicatePreset(
        QStringLiteral("profile"), QStringLiteral("circular-blue-ring"), QString{}),
        "invalid-kind"));
    QVERIFY(failedWith(library.renamePreset(QString{}, panelId, QStringLiteral("X")),
                       "invalid-kind"));
    QVERIFY(failedWith(library.removePreset(QStringLiteral("theme"), panelId),
                       "invalid-kind"));
    QCOMPARE(library.revision(), 5);
    QCOMPARE(revisions.count(), 5);
    QCOMPARE(library.panelPresets(QStringLiteral("user")).first().toMap()
                 .value(QStringLiteral("name")).toString(),
             QStringLiteral("Desk Ring"));

    // Deleting removes only the user's own file.
    QVERIFY(succeeded(library.removePreset(QStringLiteral("panel"), panelId)));
    QVERIFY(succeeded(library.removePreset(QStringLiteral("panel"), secondId)));
    QVERIFY(succeeded(library.removePreset(QStringLiteral("icon"), iconId)));
    QVERIFY(failedWith(library.removePreset(QStringLiteral("icon"), iconId),
                       "not-found"));
    QCOMPARE(library.revision(), 8);
    QVERIFY(library.panelPresets(QStringLiteral("user")).isEmpty());
    QVERIFY(library.iconPresets(QStringLiteral("user")).isEmpty());

    // The installed catalog is byte-for-byte what it was, and still complete.
    QCOMPARE(directoryDigest(builtInRoot()), builtInFiles);
    QCOMPARE(library.panelPresets(QStringLiteral("builtin")), builtInPanels);
    QCOMPARE(library.iconPresets(QStringLiteral("builtin")), builtInIcons);

    // A second library on the same store sees what the first one saved.
    QVERIFY(succeeded(library.duplicatePreset(
        QStringLiteral("icon"), QStringLiteral("dark-orb"), QStringLiteral("Kept"))));
    const PresetLibrary reopened(registry, builtInRoot(), userRoot);
    QCOMPARE(reopened.iconPresets(QStringLiteral("user")).size(), 1);
    QCOMPARE(reopened.iconPresets(QStringLiteral("user")).first().toMap()
                 .value(QStringLiteral("name")).toString(), QStringLiteral("Kept"));
}

void PresetLibraryTest::listingAndPreviewingChangeNothing()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const QString userRoot = userDirectory.filePath(QStringLiteral("presets"));
    PanelRegistry registry(themeCatalog());
    QSettings().sync();
    const QByteArray panelsBefore = registryState(registry);
    const QByteArray settingsBefore = directoryDigest(m_settingsDirectory.path());
    QSignalSpy panelChanges(&registry, &PanelRegistry::panelsChanged);
    QSignalSpy panelRevisions(&registry, &PanelRegistry::revisionChanged);

    PresetLibrary library(registry, builtInRoot(), userRoot);
    QSignalSpy revisions(&library, &PresetLibrary::revisionChanged);
    for (int pass = 0; pass < 2; ++pass)
    {
        QVERIFY(library.catalogStatus().value(QStringLiteral("valid")).toBool());
        for (const char *scope : {"builtin", "user"})
        {
            QVERIFY(library.panelPresets(QString::fromLatin1(scope)).size() <= 15);
            QVERIFY(library.iconPresets(QString::fromLatin1(scope)).size() <= 15);
        }
    }

    // No panel, setting or library revision moved, and merely looking does
    // not even create the user store.
    QSettings().sync();
    QCOMPARE(registryState(registry), panelsBefore);
    QCOMPARE(directoryDigest(m_settingsDirectory.path()), settingsBefore);
    QCOMPARE(panelChanges.count(), 0);
    QCOMPARE(panelRevisions.count(), 0);
    QCOMPARE(revisions.count(), 0);
    QCOMPARE(library.revision(), 0);
    QVERIFY(!QFileInfo::exists(userRoot));

    // Saving a user preset writes the store and still changes no panel.
    QVERIFY(library.duplicatePreset(
                QStringLiteral("panel"), QStringLiteral("obsidian-glass-dock"),
                QString{}).value(QStringLiteral("success")).toBool());
    QVERIFY(QFileInfo(userRoot).isDir());
    QSettings().sync();
    QCOMPARE(registryState(registry), panelsBefore);
    QCOMPARE(directoryDigest(m_settingsDirectory.path()), settingsBefore);
    QCOMPARE(panelChanges.count(), 0);
    QCOMPARE(panelRevisions.count(), 0);
}

void PresetLibraryTest::rejectedCatalogIsReportedAndOffersNothing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString userRoot = directory.filePath(QStringLiteral("user"));
    const PanelRegistry registry(themeCatalog());
    const auto offersNothing = [&](PresetLibrary &library, const char *code)
    {
        const QVariantMap status = library.catalogStatus();
        return !status.value(QStringLiteral("valid")).toBool() &&
            status.value(QStringLiteral("errorCode")).toString() ==
                QString::fromLatin1(code) &&
            status.value(QStringLiteral("panelPresetCount")).toInt() == 0 &&
            status.value(QStringLiteral("iconPresetCount")).toInt() == 0 &&
            library.panelPresets(QStringLiteral("builtin")).isEmpty() &&
            library.iconPresets(QStringLiteral("builtin")).isEmpty() &&
            library.panelPresets(QStringLiteral("user")).isEmpty() &&
            !library.duplicatePreset(QStringLiteral("panel"),
                                     QStringLiteral("obsidian-glass-dock"),
                                     QString{})
                 .value(QStringLiteral("success")).toBool() &&
            library.revision() == 0 && !QFileInfo::exists(userRoot);
    };

    // No catalog anywhere.
    PresetLibrary missing(registry, QString{}, userRoot);
    QVERIFY(offersNothing(missing, "preset-catalog-unavailable"));
    PresetLibrary absent(registry, directory.filePath(QStringLiteral("absent")),
                         userRoot);
    QVERIFY(offersNothing(absent, "missing-file"));

    // A catalog with one damaged definition is rejected whole: fourteen
    // presets are never presented as the library.
    const QString damagedRoot = directory.filePath(QStringLiteral("damaged"));
    for (const char *kind : {"icons", "panels"})
    {
        const QDir source(QDir(builtInRoot()).filePath(QString::fromLatin1(kind)));
        for (const QString &name : source.entryList(QDir::Files))
        {
            QVERIFY(writeBytes(
                QDir(damagedRoot).filePath(QString::fromLatin1(kind) +
                                           QLatin1Char('/') + name),
                readBytes(source.filePath(name))));
        }
    }
    {
        PresetLibrary copied(registry, damagedRoot, userRoot);
        QVERIFY(copied.catalogStatus().value(QStringLiteral("valid")).toBool());
        QCOMPARE(copied.panelPresets(QStringLiteral("builtin")).size(), 15);
    }
    QVERIFY(writeBytes(
        QDir(damagedRoot).filePath(QStringLiteral("panels/orange-arc-dock.json")),
        QByteArrayLiteral("{ not json")));
    PresetLibrary damaged(registry, damagedRoot, userRoot);
    QVERIFY(offersNothing(damaged, "invalid-json"));
    const QVariantList diagnostics =
        damaged.catalogStatus().value(QStringLiteral("diagnostics")).toList();
    QVERIFY(!diagnostics.isEmpty());
    QVERIFY(diagnostics.first().toMap().value(QStringLiteral("jsonPointer"))
                .toString().startsWith(QStringLiteral("orange-arc-dock.json")));
}

void PresetLibraryTest::everyBuiltInCardRendersThroughTheSharedRenderer()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const PanelRegistry registry(themeCatalog());
    const PresetLibrary library(registry, builtInRoot(),
                                userDirectory.filePath(QStringLiteral("presets")));
    QVariantList cards = library.panelPresets(QStringLiteral("builtin"));
    cards.append(library.iconPresets(QStringLiteral("builtin")));
    QCOMPARE(cards.size(), 30);

    QQmlEngine engine;
    QmlWarnings warnings(&engine);
    engine.addImportPath(renderingImportRoot());
    QQmlComponent component(&engine,
                            runtimeComponent(QStringLiteral("PresetCard.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(800, 280);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    struct Rendered
    {
        QImage picture;
        QString tier;
        bool fallbackApplied = true;
        QString previewName;
    };
    // Instantiates the real card for one preset and returns what its preview
    // shows once two frames in a row are the same.
    const auto render = [&](const QVariantMap &card) -> Rendered
    {
        Rendered result;
        const std::unique_ptr<QObject> object(component.createWithInitialProperties(
            {{QStringLiteral("preset"), card}}));
        auto *item = qobject_cast<QQuickItem *>(object.get());
        if (!item)
        {
            return result;
        }
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(780, 260));
        auto *preview = item->property("livePreview").value<QQuickItem *>();
        if (!preview)
        {
            return result;
        }
        QImage previous;
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            QTest::qWait(20);
            const QImage frame = window.grabWindow().copy(
                preview->mapRectToScene(preview->boundingRect()).toAlignedRect());
            if (!frame.isNull() && frame == previous)
            {
                result.picture = frame;
                break;
            }
            previous = frame;
        }
        result.tier = preview->property("activeRendererTier").toString();
        result.fallbackApplied = preview->property("fallbackApplied").toBool();
        result.previewName = preview->objectName();
        return result;
    };

    QHash<QString, QImage> pictures;
    for (const QVariant &value : std::as_const(cards))
    {
        const QVariantMap card = value.toMap();
        const QString id = card.value(QStringLiteral("id")).toString();
        const QByteArray label = (card.value(QStringLiteral("kind")).toString() +
                                  QLatin1Char(' ') + id).toUtf8();
        const Rendered first = render(card);
        QVERIFY2(!first.picture.isNull(), label);
        QCOMPARE(first.previewName, QStringLiteral("preset-live-preview-") + id);
        // The shared scene draws at the tier the library promised, without a
        // fallback of its own.
        QVERIFY2(first.tier == compatibilityOf(card)
                                   .value(QStringLiteral("effectiveRendererTier"))
                                   .toString(),
                 qPrintable(QString::fromUtf8(label) + QStringLiteral(": ") +
                            first.tier));
        QVERIFY2(!first.fallbackApplied, label);
        // Real output: a preview that drew nothing is one flat colour.
        QVERIFY2(distinctColors(first.picture) >= 8,
                 qPrintable(QString::fromUtf8(label) + QStringLiteral(": ") +
                            QString::number(distinctColors(first.picture))));
        // Deterministic: a second, separate card draws the same picture.
        const Rendered second = render(card);
        QVERIFY2(second.picture == first.picture, label);
        pictures.insert(card.value(QStringLiteral("kind")).toString() +
                            QLatin1Char('/') + id,
                        first.picture);
    }
    QCOMPARE(pictures.size(), 30);

    // Different presets are different pictures, including the presets that
    // differ from their base style only by overrides.
    const auto differ = [&pictures](const char *left, const char *right)
    {
        return pictures.value(QString::fromLatin1(left)) !=
            pictures.value(QString::fromLatin1(right));
    };
    QVERIFY(differ("panel/obsidian-glass-dock", "panel/minimal-neon-rail"));
    QVERIFY(differ("panel/obsidian-glass-dock", "panel/sci-fi-chassis-dark"));
    QVERIFY(differ("panel/circular-blue-ring", "panel/octagonal-platform"));
    QVERIFY(differ("panel/orange-arc-dock", "panel/holographic-semicircle"));
    QVERIFY(differ("icon/original-clean", "icon/metallic-blue"));
    QVERIFY(differ("icon/metallic-blue", "icon/metallic-red"));
    QVERIFY(differ("icon/dark-orb", "icon/blue-pedestal"));
    QVERIFY(differ("icon/blue-pedestal", "icon/red-pedestal"));
    QVERIFY(differ("icon/metallic-blue", "icon/glass-tile"));

    QVERIFY2(warnings.messages.isEmpty(),
             qPrintable(warnings.messages.join(QLatin1Char('\n'))));
}

void PresetLibraryTest::browsingEveryPresetPageChangesNoPanel()
{
    QTemporaryDir userDirectory;
    QVERIFY(userDirectory.isValid());
    const QString userRoot = userDirectory.filePath(QStringLiteral("presets"));
    PanelRegistry registry(themeCatalog());
    QSettings().sync();
    const QByteArray panelsBefore = registryState(registry);
    const QByteArray settingsBefore = directoryDigest(m_settingsDirectory.path());
    QSignalSpy panelChanges(&registry, &PanelRegistry::panelsChanged);
    QSignalSpy panelRevisions(&registry, &PanelRegistry::revisionChanged);
    PresetLibrary library(registry, builtInRoot(), userRoot);

    QQmlEngine engine;
    QmlWarnings warnings(&engine);
    engine.addImportPath(renderingImportRoot());
    QQmlComponent component(&engine,
                            runtimeComponent(QStringLiteral("PresetBrowser.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(900, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Opens one preset page the way Panel Studio does and selects every card
    // on it with a click. Returns how many cards were selected, or -1.
    const auto browse = [&](const QString &kind, const QString &scope) -> int
    {
        const QVariantList cards = kind == QStringLiteral("panel")
            ? library.panelPresets(scope) : library.iconPresets(scope);
        const std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("kind"), kind},
            {QStringLiteral("scope"), scope},
            {QStringLiteral("presets"), cards},
            {QStringLiteral("catalogStatus"), library.catalogStatus()},
        }));
        auto *browser = qobject_cast<QQuickItem *>(object.get());
        if (!browser)
        {
            return -1;
        }
        browser->setParentItem(window.contentItem());
        browser->setSize(QSizeF(860, 720));
        QSignalSpy selected(browser, SIGNAL(presetSelected(QString)));
        for (int index = 0; index < cards.size(); ++index)
        {
            const QString id =
                cards.at(index).toMap().value(QStringLiteral("id")).toString();
            QQuickItem *card = nullptr;
            for (int attempt = 0; attempt < 100 && !card; ++attempt)
            {
                QMetaObject::invokeMethod(browser, "focusCard",
                                          Q_ARG(QVariant, QVariant(index)));
                QTest::qWait(10);
                card = findItem(browser, QStringLiteral("preset-card-") + id);
                if (card && card->width() <= 0)
                {
                    card = nullptr;
                }
            }
            if (!card)
            {
                qWarning() << "no card was created for" << id;
                return -1;
            }
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                              card->mapToScene(QPointF(40, 20)).toPoint());
            if (selected.count() != index + 1 ||
                selected.last().first().toString() != id)
            {
                qWarning() << "clicking the card did not select" << id;
                return -1;
            }
            browser->setProperty("selectedPresetId", id);
            if (!card->property("selected").toBool())
            {
                qWarning() << "the card does not show as selected:" << id;
                return -1;
            }
        }
        return int(cards.size());
    };

    // With no user presets: both built-in catalogs in full, both user pages
    // empty.
    QCOMPARE(browse(QStringLiteral("panel"), QStringLiteral("builtin")), 15);
    QCOMPARE(browse(QStringLiteral("panel"), QStringLiteral("user")), 0);
    QCOMPARE(browse(QStringLiteral("icon"), QStringLiteral("builtin")), 15);
    QCOMPARE(browse(QStringLiteral("icon"), QStringLiteral("user")), 0);
    QSettings().sync();
    QCOMPARE(registryState(registry), panelsBefore);
    QCOMPARE(directoryDigest(m_settingsDirectory.path()), settingsBefore);
    QCOMPARE(library.revision(), 0);
    QVERIFY(!QFileInfo::exists(userRoot));

    // The user pages list what the store holds, and selecting there changes
    // no panel either.
    QVERIFY(library.duplicatePreset(QStringLiteral("panel"),
                                    QStringLiteral("mechanical-collapsible-rail"),
                                    QStringLiteral("My Rail"))
                .value(QStringLiteral("success")).toBool());
    QVERIFY(library.duplicatePreset(QStringLiteral("icon"),
                                    QStringLiteral("holographic-tile"),
                                    QStringLiteral("My Tiles"))
                .value(QStringLiteral("success")).toBool());
    QCOMPARE(browse(QStringLiteral("panel"), QStringLiteral("user")), 1);
    QCOMPARE(browse(QStringLiteral("icon"), QStringLiteral("user")), 1);
    QSettings().sync();
    QCOMPARE(registryState(registry), panelsBefore);
    QCOMPARE(directoryDigest(m_settingsDirectory.path()), settingsBefore);
    QCOMPARE(panelChanges.count(), 0);
    QCOMPARE(panelRevisions.count(), 0);

    QVERIFY2(warnings.messages.isEmpty(),
             qPrintable(warnings.messages.join(QLatin1Char('\n'))));
}

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Arch Dock Test"));
    QCoreApplication::setApplicationName(QStringLiteral("Preset Library"));

    PresetLibraryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PresetLibraryTest.moc"
