#include "model/PanelCapabilityResolver.h"

#include <QJsonDocument>
#include <QtTest>

#include <algorithm>

using namespace ArchDock;

namespace
{

PanelDefinition panelFor(PanelHostKind kind,
                         const QString &layout = QStringLiteral("adaptive"))
{
    PanelDefinition definition = PanelDefinition::defaults(
        QStringLiteral("capability-panel"),
        QStringLiteral("Capability Panel"),
        kind == PanelHostKind::FreeDesktop
            ? QStringLiteral("free")
            : QStringLiteral("bottom"),
        false);
    definition.layout.pathType = layout;
    definition.surface.rendererTier.clear();
    return definition;
}

ThemeCapabilityProfile themeFor(
    const QString &id,
    const QVector<PanelHostKind> &hosts,
    const QVector<PanelLayoutKind> &layouts,
    const QVector<RendererTier> &tiers,
    RendererTier preferred)
{
    ThemeCapabilityProfile theme;
    theme.id = id;
    theme.hostKinds = hosts;
    theme.capabilities = {
        PanelCapability::DynamicTint,
        PanelCapability::IconStateStyling,
    };
    theme.layouts = layouts;
    theme.rendererTiers = tiers;
    theme.preferredRendererTier = preferred;
    return theme;
}

RendererAvailability *rendererByTier(QVector<RendererAvailability> *renderers,
                                     RendererTier tier)
{
    const auto found = std::find_if(
        renderers->begin(),
        renderers->end(),
        [tier](const RendererAvailability &renderer)
        {
            return renderer.tier == tier;
        });
    return found == renderers->end() ? nullptr : &*found;
}

const CapabilityDecision *decisionById(const QVector<CapabilityDecision> &decisions,
                                       const QString &id)
{
    const auto found = std::find_if(
        decisions.cbegin(),
        decisions.cend(),
        [&id](const CapabilityDecision &decision)
        {
            return decision.id == id;
        });
    return found == decisions.cend() ? nullptr : &*found;
}

const RendererCandidateDecision *rendererChoiceByTier(
    const QVector<RendererCandidateDecision> &choices,
    RendererTier tier)
{
    const auto found = std::find_if(
        choices.cbegin(),
        choices.cend(),
        [tier](const RendererCandidateDecision &decision)
        {
            return decision.tier == tier;
        });
    return found == choices.cend() ? nullptr : &*found;
}

QByteArray canonicalBytes(const CapabilityResolution &resolution)
{
    return QJsonDocument::fromVariant(resolution.toVariantMap()).toJson(
        QJsonDocument::Compact);
}

QVariantMap themeMap(bool reversed)
{
    QVariantMap capabilities;
    if (reversed)
    {
        capabilities.insert(
            QStringLiteral("rotation"),
            QVariantMap{{QStringLiteral("maximumDegrees"), 90.0},
                        {QStringLiteral("minimumDegrees"), -90.0},
                        {QStringLiteral("mode"), QStringLiteral("bounded")}});
        capabilities.insert(
            QStringLiteral("rendererTiers"),
            QVariantList{QStringLiteral("procedural2d")});
        capabilities.insert(
            QStringLiteral("presentationMechanisms"), QVariantList{});
        capabilities.insert(
            QStringLiteral("layouts"),
            QVariantList{QStringLiteral("adaptive"), QStringLiteral("ring")});
        capabilities.insert(
            QStringLiteral("hosts"),
            QVariantList{QStringLiteral("native-edge"), QStringLiteral("free-desktop")});
        capabilities.insert(
            QStringLiteral("features"),
            QVariantList{QStringLiteral("icon-state-styling"),
                         QStringLiteral("dynamic-tint")});
        capabilities.insert(
            QStringLiteral("fallbackRendererTiers"), QVariantList{});
        capabilities.insert(
            QStringLiteral("preferredRendererTier"),
            QStringLiteral("procedural2d"));
    }
    else
    {
        capabilities.insert(
            QStringLiteral("preferredRendererTier"),
            QStringLiteral("procedural2d"));
        capabilities.insert(
            QStringLiteral("fallbackRendererTiers"), QVariantList{});
        capabilities.insert(
            QStringLiteral("features"),
            QVariantList{QStringLiteral("dynamic-tint"),
                         QStringLiteral("icon-state-styling")});
        capabilities.insert(
            QStringLiteral("hosts"),
            QVariantList{QStringLiteral("free-desktop"), QStringLiteral("native-edge")});
        capabilities.insert(
            QStringLiteral("layouts"),
            QVariantList{QStringLiteral("ring"), QStringLiteral("adaptive")});
        capabilities.insert(
            QStringLiteral("presentationMechanisms"), QVariantList{});
        capabilities.insert(
            QStringLiteral("rendererTiers"),
            QVariantList{QStringLiteral("procedural2d")});
        capabilities.insert(
            QStringLiteral("rotation"),
            QVariantMap{{QStringLiteral("mode"), QStringLiteral("bounded")},
                        {QStringLiteral("minimumDegrees"), -90.0},
                        {QStringLiteral("maximumDegrees"), 90.0}});
    }

    QVariantMap theme;
    if (reversed)
    {
        theme.insert(QStringLiteral("capabilities"), capabilities);
        theme.insert(QStringLiteral("name"), QStringLiteral("Deterministic"));
        theme.insert(QStringLiteral("id"), QStringLiteral("deterministic"));
    }
    else
    {
        theme.insert(QStringLiteral("id"), QStringLiteral("deterministic"));
        theme.insert(QStringLiteral("name"), QStringLiteral("Deterministic"));
        theme.insert(QStringLiteral("capabilities"), capabilities);
    }
    return theme;
}

}

class PanelCapabilityResolverTest final : public QObject
{
    Q_OBJECT

private slots:
    void layoutVocabularyRoundTrips();
    void nativeEdgeUsesOnlyLinearProceduralCapabilities();
    void nativeEdgeRejectsRingTheme();
    void freeDesktopAcceptsRingTheme();
    void freeDesktopResolvesBoundedRotation();
    void productionNativeRotationIsUnavailable();
    void syntheticNativeRotationCanBeBounded();
    void bakedTwoPointFiveDCanBeAvailableInSyntheticInventory();
    void trueThreeDReportsNotInstalled();
    void trueThreeDReportsDisabledSeparately();
    void selectsFirstSafeFallback();
    void noFallbackReturnsDeterministicUnavailableResult();
    void equivalentSourceMapsSerializeIdentically();
    void resolvingHasNoInputSideEffects();
};

void PanelCapabilityResolverTest::layoutVocabularyRoundTrips()
{
    for (int value = 0; value < static_cast<int>(PanelLayoutKind::Count); ++value)
    {
        const auto kind = static_cast<PanelLayoutKind>(value);
        const QString name = panelLayoutKindName(kind);
        QVERIFY(!name.isEmpty());
        QCOMPARE(panelLayoutKindFromName(name), std::optional<PanelLayoutKind>(kind));
        QCOMPARE(PanelDefinition::normalizeLegacyValue(
                     QStringLiteral("layout"), name.toUpper()).toString(),
                 name);
    }
}

void PanelCapabilityResolverTest::nativeEdgeUsesOnlyLinearProceduralCapabilities()
{
    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::NativeEdge),
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::NativeEdge),
        PanelCapabilityResolver::proceduralThemeProfile(),
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QCOMPARE(result.renderer.effectiveTier,
             std::optional<RendererTier>(RendererTier::Procedural2D));
    QVERIFY(decisionById(result.layouts, QStringLiteral("adaptive"))->available);
    QVERIFY(decisionById(result.layouts, QStringLiteral("horizontal"))->available);
    QVERIFY(decisionById(result.layouts, QStringLiteral("vertical"))->available);
    QVERIFY(!decisionById(result.layouts, QStringLiteral("ring"))->available);
    QVERIFY(decisionById(result.controls,
                         QStringLiteral("native-edge-placement"))->available);
    QVERIFY(!decisionById(result.controls,
                          QStringLiteral("arbitrary-xy-placement"))->available);
}

void PanelCapabilityResolverTest::nativeEdgeRejectsRingTheme()
{
    const ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("ring-only"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Ring},
        {RendererTier::Procedural2D},
        RendererTier::Procedural2D);
    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::NativeEdge, QStringLiteral("ring")),
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::NativeEdge),
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(!result.available);
    QCOMPARE(result.reason, CapabilityReasonCode::HostLayoutUnsupported);
    const CapabilityDecision *ring = decisionById(
        result.layouts, QStringLiteral("ring"));
    QVERIFY(ring);
    QVERIFY(!ring->available);
    QCOMPARE(ring->reason, CapabilityReasonCode::HostLayoutUnsupported);
}

void PanelCapabilityResolverTest::freeDesktopAcceptsRingTheme()
{
    ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("ring"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Ring},
        {RendererTier::Procedural2D},
        RendererTier::Procedural2D);
    theme.rotation = {RotationSupport::Bounded, -90.0, 90.0};
    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop, QStringLiteral("ring")),
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop),
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QVERIFY(decisionById(result.layouts, QStringLiteral("ring"))->available);
}

void PanelCapabilityResolverTest::freeDesktopResolvesBoundedRotation()
{
    PanelDefinition definition = panelFor(
        PanelHostKind::FreeDesktop, QStringLiteral("ring"));
    definition.layout.angle = 45.0;
    ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("bounded-ring"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Ring},
        {RendererTier::Procedural2D},
        RendererTier::Procedural2D);
    theme.rotation = {RotationSupport::Bounded, -60.0, 75.0};

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        definition,
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop),
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QVERIFY(result.rotation.available);
    QCOMPARE(result.rotation.support, RotationSupport::Bounded);
    QCOMPARE(result.rotation.minimumDegrees, -60.0);
    QCOMPARE(result.rotation.maximumDegrees, 75.0);
}

void PanelCapabilityResolverTest::productionNativeRotationIsUnavailable()
{
    PanelDefinition definition = panelFor(PanelHostKind::NativeEdge);
    definition.layout.angle = 10.0;
    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        definition,
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::NativeEdge),
        PanelCapabilityResolver::proceduralThemeProfile(),
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(!result.available);
    QVERIFY(!result.rotation.available);
    QCOMPARE(result.rotation.reason,
             CapabilityReasonCode::HostCapabilityUnavailable);
}

void PanelCapabilityResolverTest::syntheticNativeRotationCanBeBounded()
{
    PanelDefinition definition = panelFor(PanelHostKind::NativeEdge);
    definition.layout.angle = 12.0;
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::NativeEdge);
    host.id = QStringLiteral("synthetic-native-bounded");
    host.capabilities.append(PanelCapability::WholePanelRotation);
    host.rotation = {RotationSupport::Bounded, -30.0, 30.0};
    ThemeCapabilityProfile theme = PanelCapabilityResolver::proceduralThemeProfile(
        QStringLiteral("synthetic-bounded-theme"));
    theme.rotation = {RotationSupport::Bounded, -15.0, 20.0};

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        definition,
        host,
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QVERIFY(result.rotation.available);
    QCOMPARE(result.rotation.support, RotationSupport::Bounded);
    QCOMPARE(result.rotation.minimumDegrees, -15.0);
    QCOMPARE(result.rotation.maximumDegrees, 20.0);
}

void PanelCapabilityResolverTest::bakedTwoPointFiveDCanBeAvailableInSyntheticInventory()
{
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    host.rendererTiers.append(RendererTier::Baked2_5D);
    ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("baked"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Adaptive},
        {RendererTier::Baked2_5D},
        RendererTier::Baked2_5D);
    QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    RendererAvailability *baked = rendererByTier(
        &renderers, RendererTier::Baked2_5D);
    QVERIFY(baked);
    baked->installed = true;
    baked->enabled = true;

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        renderers,
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QCOMPARE(result.renderer.effectiveTier,
             std::optional<RendererTier>(RendererTier::Baked2_5D));
}

void PanelCapabilityResolverTest::trueThreeDReportsNotInstalled()
{
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    host.rendererTiers.append(RendererTier::True3D);
    const ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("true3d"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Adaptive},
        {RendererTier::True3D},
        RendererTier::True3D);

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(!result.available);
    QCOMPARE(result.renderer.reason,
             CapabilityReasonCode::NoSafeRendererFallback);
    QCOMPARE(result.renderer.evaluatedTiers.constFirst().reason,
             CapabilityReasonCode::RendererNotInstalled);
    const RendererCandidateDecision *choice = rendererChoiceByTier(
        result.rendererChoices, RendererTier::True3D);
    QVERIFY(choice);
    QVERIFY(!choice->available);
    QCOMPARE(choice->reason, CapabilityReasonCode::RendererNotInstalled);
}

void PanelCapabilityResolverTest::trueThreeDReportsDisabledSeparately()
{
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    host.rendererTiers.append(RendererTier::True3D);
    const ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("true3d-disabled"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Adaptive},
        {RendererTier::True3D},
        RendererTier::True3D);
    QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    RendererAvailability *renderer = rendererByTier(
        &renderers, RendererTier::True3D);
    QVERIFY(renderer);
    renderer->installed = true;
    renderer->enabled = false;

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        renderers,
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(!result.available);
    QCOMPARE(result.renderer.evaluatedTiers.constFirst().reason,
             CapabilityReasonCode::RendererDisabled);
    const RendererCandidateDecision *choice = rendererChoiceByTier(
        result.rendererChoices, RendererTier::True3D);
    QVERIFY(choice);
    QVERIFY(!choice->available);
    QCOMPARE(choice->reason, CapabilityReasonCode::RendererDisabled);
}

void PanelCapabilityResolverTest::selectsFirstSafeFallback()
{
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    host.rendererTiers.append(RendererTier::True3D);
    host.rendererTiers.append(RendererTier::Baked2_5D);
    ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("fallback"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Adaptive},
        {RendererTier::True3D, RendererTier::Baked2_5D,
         RendererTier::Procedural2D},
        RendererTier::True3D);
    theme.fallbackRendererTiers = {
        RendererTier::Baked2_5D,
        RendererTier::Procedural2D,
    };
    QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    RendererAvailability *baked = rendererByTier(
        &renderers, RendererTier::Baked2_5D);
    QVERIFY(baked);
    baked->installed = true;
    baked->enabled = true;

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        renderers,
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(result.available);
    QVERIFY(result.renderer.fallbackApplied);
    QCOMPARE(result.renderer.effectiveTier,
             std::optional<RendererTier>(RendererTier::Baked2_5D));
    QCOMPARE(result.renderer.reason,
             CapabilityReasonCode::RendererNotInstalled);
}

void PanelCapabilityResolverTest::noFallbackReturnsDeterministicUnavailableResult()
{
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    host.rendererTiers.append(RendererTier::True3D);
    const ThemeCapabilityProfile theme = themeFor(
        QStringLiteral("no-fallback"),
        {PanelHostKind::FreeDesktop},
        {PanelLayoutKind::Adaptive},
        {RendererTier::True3D},
        RendererTier::True3D);

    const CapabilityResolution first = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());
    const CapabilityResolution second = PanelCapabilityResolver::resolve(
        panelFor(PanelHostKind::FreeDesktop),
        host,
        theme,
        PanelCapabilityResolver::productionRenderers(),
        PanelCapabilityResolver::productionPlatform());

    QVERIFY(!first.available);
    QCOMPARE(first.reason, CapabilityReasonCode::NoSafeRendererFallback);
    QVERIFY(!first.renderer.effectiveTier.has_value());
    QCOMPARE(first.toVariantMap(), second.toVariantMap());
    QCOMPARE(canonicalBytes(first), canonicalBytes(second));
}

void PanelCapabilityResolverTest::equivalentSourceMapsSerializeIdentically()
{
    QString firstError;
    QString secondError;
    const std::optional<ThemeCapabilityProfile> firstTheme =
        PanelCapabilityResolver::themeProfileFromVariantMap(
            themeMap(false), &firstError);
    const std::optional<ThemeCapabilityProfile> secondTheme =
        PanelCapabilityResolver::themeProfileFromVariantMap(
            themeMap(true), &secondError);
    QVERIFY2(firstTheme.has_value(), qPrintable(firstError));
    QVERIFY2(secondTheme.has_value(), qPrintable(secondError));

    const PanelDefinition definition = panelFor(
        PanelHostKind::FreeDesktop, QStringLiteral("ring"));
    const HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    const QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    const PlatformCapabilityProfile platform =
        PanelCapabilityResolver::productionPlatform();
    const CapabilityResolution first = PanelCapabilityResolver::resolve(
        definition, host, *firstTheme, renderers, platform);
    const CapabilityResolution second = PanelCapabilityResolver::resolve(
        definition, host, *secondTheme, renderers, platform);

    QCOMPARE(first.toVariantMap(), second.toVariantMap());
    QCOMPARE(canonicalBytes(first), canonicalBytes(second));
}

void PanelCapabilityResolverTest::resolvingHasNoInputSideEffects()
{
    PanelDefinition definition = panelFor(
        PanelHostKind::FreeDesktop, QStringLiteral("ring"));
    definition.settingsRevision = 17;
    HostCapabilityProfile host =
        PanelCapabilityResolver::productionHostProfile(PanelHostKind::FreeDesktop);
    ThemeCapabilityProfile theme = PanelCapabilityResolver::proceduralThemeProfile();
    QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    PlatformCapabilityProfile platform =
        PanelCapabilityResolver::productionPlatform();
    const PanelDefinition definitionBefore = definition;
    const HostCapabilityProfile hostBefore = host;
    const ThemeCapabilityProfile themeBefore = theme;
    const QVector<RendererAvailability> renderersBefore = renderers;
    const PlatformCapabilityProfile platformBefore = platform;

    const CapabilityResolution result = PanelCapabilityResolver::resolve(
        definition, host, theme, renderers, platform);

    QVERIFY(result.available);
    QCOMPARE(definition, definitionBefore);
    QCOMPARE(host, hostBefore);
    QCOMPARE(theme, themeBefore);
    QCOMPARE(renderers, renderersBefore);
    QCOMPARE(platform, platformBefore);
}

QTEST_MAIN(PanelCapabilityResolverTest)

#include "PanelCapabilityResolverTest.moc"
