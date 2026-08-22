#include "PanelPlacement.h"

#include <QTest>

#include <algorithm>

namespace
{
bool hasIssue(const ArchDock::NativePanelPlacementResult &result,
              ArchDock::NativePlacementField field,
              ArchDock::NativePlacementIssueKind kind,
              ArchDock::NativePlacementIssueCode code)
{
    return std::any_of(
        result.issues.cbegin(),
        result.issues.cend(),
        [field, kind, code](const ArchDock::NativePlacementIssue &issue)
        {
            return issue.field == field && issue.kind == kind && issue.code == code;
        });
}
}

class PanelPlacementTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreCanonical();
    void normalizesObservedAliases();
    void mapsAlignmentAliasesByOrientation();
    void keepsLengthModesDistinct();
    void acceptsDimensionBoundaries();
    void rejectsInvalidValues();
    void rejectsInvalidLengthCombinations();
    void reportsUnsupportedRequests();
    void preservesStableScreenIdWithFallback();
};

void PanelPlacementTest::defaultsAreCanonical()
{
    using namespace ArchDock;

    const NativePanelPlacement defaults = defaultNativePanelPlacement();
    QCOMPARE(defaults.screen.stableId, QString{});
    QCOMPARE(defaults.screen.fallbackIndex, 0);
    QCOMPARE(defaults.edge, NativePanelEdge::Bottom);
    QCOMPARE(defaults.alignment, NativePanelAlignment::Center);
    QCOMPARE(defaults.offset, 0);
    QCOMPARE(defaults.thickness, 76);
    QCOMPARE(defaults.lengthMode, NativePanelLengthMode::Fixed);
    QCOMPARE(defaults.minimumLength, kNativePanelMinimumDimension);
    QCOMPARE(defaults.maximumLength, kNativePanelMaximumDimension);
    QCOMPARE(defaults.fixedLength, 720);
    QCOMPARE(defaults.floatingMargin, 0);
    QCOMPARE(defaults.visibilityMode, NativePanelVisibilityMode::Always);
    QCOMPARE(defaults.reservedSpace, NativePanelReservedSpace::Automatic);

    const NativePanelPlacementResult result = normalizeNativePanelPlacement({});
    QVERIFY(result.isValid());
    QVERIFY(result.isSupported());
    QVERIFY(result.issues.isEmpty());
    QVERIFY(result.placement.has_value());
    QCOMPARE(result.placement->edge, defaults.edge);
    QCOMPARE(result.placement->lengthMode, defaults.lengthMode);
}

void PanelPlacementTest::normalizesObservedAliases()
{
    using namespace ArchDock;

    NativePanelPlacementRequest request;
    request.screenStableId = QStringLiteral("  output:DP-1  ");
    request.edge = QStringLiteral(" Top_Edge ");
    request.alignment = QStringLiteral("left");
    request.lengthMode = QStringLiteral("custom");
    request.visibilityMode = QStringLiteral("auto_hide");
    request.reservedSpace = QStringLiteral("windows-go-below");

    NativePlacementCapabilities capabilities;
    capabilities.supportsReservedSpace = true;
    const NativePanelPlacementResult result = normalizeNativePanelPlacement(
        request, capabilities);

    QVERIFY(result.isValid());
    QVERIFY(result.isSupported());
    QVERIFY(result.placement.has_value());
    QCOMPARE(result.placement->screen.stableId, QStringLiteral("output:DP-1"));
    QCOMPARE(result.placement->edge, NativePanelEdge::Top);
    QCOMPARE(result.placement->alignment, NativePanelAlignment::Start);
    QCOMPARE(result.placement->lengthMode, NativePanelLengthMode::Fixed);
    QCOMPARE(result.placement->visibilityMode, NativePanelVisibilityMode::AutoHide);
    QCOMPARE(result.placement->reservedSpace, NativePanelReservedSpace::DoNotReserve);
}

void PanelPlacementTest::mapsAlignmentAliasesByOrientation()
{
    using namespace ArchDock;

    NativePanelPlacementRequest verticalStart;
    verticalStart.edge = QStringLiteral("left");
    verticalStart.alignment = QStringLiteral("right");
    const NativePanelPlacementResult startResult = normalizeNativePanelPlacement(
        verticalStart);
    QVERIFY(startResult.isSupported());
    QCOMPARE(startResult.placement->alignment, NativePanelAlignment::Start);

    NativePanelPlacementRequest verticalEnd = verticalStart;
    verticalEnd.alignment = QStringLiteral("left");
    const NativePanelPlacementResult endResult = normalizeNativePanelPlacement(verticalEnd);
    QVERIFY(endResult.isSupported());
    QCOMPARE(endResult.placement->alignment, NativePanelAlignment::End);

    NativePanelPlacementRequest currentVerticalAlias = verticalStart;
    currentVerticalAlias.alignment = QStringLiteral("top");
    const NativePanelPlacementResult topResult = normalizeNativePanelPlacement(
        currentVerticalAlias);
    QVERIFY(topResult.isSupported());
    QCOMPARE(topResult.placement->alignment, NativePanelAlignment::Start);
}

void PanelPlacementTest::keepsLengthModesDistinct()
{
    using namespace ArchDock;

    NativePanelPlacementRequest fit;
    fit.lengthMode = QStringLiteral("fit");
    const NativePanelPlacementResult fitResult = normalizeNativePanelPlacement(fit);
    QVERIFY(fitResult.isSupported());
    QCOMPARE(fitResult.placement->lengthMode, NativePanelLengthMode::Fit);

    NativePanelPlacementRequest fixed;
    fixed.lengthMode = QStringLiteral("fixed");
    const NativePanelPlacementResult fixedResult = normalizeNativePanelPlacement(fixed);
    QVERIFY(fixedResult.isSupported());
    QCOMPARE(fixedResult.placement->lengthMode, NativePanelLengthMode::Fixed);

    NativePanelPlacementRequest fill;
    fill.lengthMode = QStringLiteral("fill");
    const NativePanelPlacementResult fillResult = normalizeNativePanelPlacement(fill);
    QVERIFY(fillResult.isSupported());
    QCOMPARE(fillResult.placement->lengthMode, NativePanelLengthMode::Fill);

    NativePanelPlacementRequest dynamic;
    dynamic.dynamicLength = true;
    const NativePanelPlacementResult dynamicResult = normalizeNativePanelPlacement(dynamic);
    QVERIFY(dynamicResult.isSupported());
    QCOMPARE(dynamicResult.placement->lengthMode, NativePanelLengthMode::Fit);
}

void PanelPlacementTest::acceptsDimensionBoundaries()
{
    using namespace ArchDock;

    NativePanelPlacementRequest minimum;
    minimum.thickness = kNativePanelMinimumDimension;
    minimum.minimumLength = kNativePanelMinimumDimension;
    minimum.maximumLength = kNativePanelMinimumDimension;
    minimum.fixedLength = kNativePanelMinimumDimension;
    const NativePanelPlacementResult minimumResult = normalizeNativePanelPlacement(minimum);
    QVERIFY(minimumResult.isSupported());

    NativePanelPlacementRequest maximum;
    maximum.screenFallbackIndex = 9;
    maximum.offset = kNativePanelMaximumDimension;
    maximum.thickness = kNativePanelMaximumDimension;
    maximum.minimumLength = kNativePanelMaximumDimension;
    maximum.maximumLength = kNativePanelMaximumDimension;
    maximum.fixedLength = kNativePanelMaximumDimension;
    maximum.floatingMargin = 1;
    NativePlacementCapabilities capabilities;
    capabilities.supportsFloatingMargin = true;
    const NativePanelPlacementResult maximumResult = normalizeNativePanelPlacement(
        maximum, capabilities);
    QVERIFY(maximumResult.isSupported());
    QCOMPARE(maximumResult.placement->screen.fallbackIndex, 9);
    QCOMPARE(maximumResult.placement->thickness, kNativePanelMaximumDimension);
}

void PanelPlacementTest::rejectsInvalidValues()
{
    using namespace ArchDock;

    NativePanelPlacementRequest request;
    request.screenFallbackIndex = -1;
    request.edge = QStringLiteral("free");
    request.alignment = QStringLiteral("diagonal");
    request.offset = -1;
    request.thickness = kNativePanelMinimumDimension - 1;
    request.minimumLength = kNativePanelMinimumDimension - 1;
    request.maximumLength = kNativePanelMaximumDimension + 1;
    request.fixedLength = kNativePanelMinimumDimension - 1;
    request.floatingMargin = -1;
    request.lengthMode = QStringLiteral("stretch");
    request.visibilityMode = QStringLiteral("sometimes");
    request.reservedSpace = QStringLiteral("maybe");

    const NativePanelPlacementResult result = normalizeNativePanelPlacement(request);
    QVERIFY(!result.isValid());
    QVERIFY(!result.isSupported());
    QVERIFY(!result.placement.has_value());
    QVERIFY(hasIssue(result,
                     NativePlacementField::ScreenFallbackIndex,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::Edge,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::UnknownAlias));
    QVERIFY(hasIssue(result,
                     NativePlacementField::Alignment,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::UnknownAlias));
    QVERIFY(hasIssue(result,
                     NativePlacementField::Offset,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::Thickness,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::LengthMode,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::UnknownAlias));
    QVERIFY(hasIssue(result,
                     NativePlacementField::MinimumLength,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::MaximumLength,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::FixedLength,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::FloatingMargin,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::OutOfRange));
    QVERIFY(hasIssue(result,
                     NativePlacementField::VisibilityMode,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::UnknownAlias));
    QVERIFY(hasIssue(result,
                     NativePlacementField::ReservedSpace,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::UnknownAlias));
}

void PanelPlacementTest::rejectsInvalidLengthCombinations()
{
    using namespace ArchDock;

    NativePanelPlacementRequest inverted;
    inverted.minimumLength = 800;
    inverted.maximumLength = 700;
    inverted.fixedLength = 750;
    const NativePanelPlacementResult invertedResult = normalizeNativePanelPlacement(inverted);
    QVERIFY(!invertedResult.isValid());
    QVERIFY(hasIssue(invertedResult,
                     NativePlacementField::MaximumLength,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::MinimumExceedsMaximum));

    NativePanelPlacementRequest fixedOutsideRange;
    fixedOutsideRange.minimumLength = 100;
    fixedOutsideRange.maximumLength = 200;
    fixedOutsideRange.fixedLength = 300;
    const NativePanelPlacementResult fixedResult = normalizeNativePanelPlacement(
        fixedOutsideRange);
    QVERIFY(!fixedResult.isValid());
    QVERIFY(hasIssue(fixedResult,
                     NativePlacementField::FixedLength,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::FixedLengthOutsideRange));

    NativePanelPlacementRequest conflictingMode;
    conflictingMode.lengthMode = QStringLiteral("fill");
    conflictingMode.dynamicLength = false;
    const NativePanelPlacementResult conflictResult = normalizeNativePanelPlacement(
        conflictingMode);
    QVERIFY(!conflictResult.isValid());
    QVERIFY(hasIssue(conflictResult,
                     NativePlacementField::LengthMode,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::ConflictingValues));
}

void PanelPlacementTest::reportsUnsupportedRequests()
{
    using namespace ArchDock;

    NativePanelPlacementRequest request;
    request.floatingMargin = 8;
    request.reservedSpace = QStringLiteral("reserve");

    const NativePanelPlacementResult unsupported = normalizeNativePanelPlacement(request);
    QVERIFY(unsupported.isValid());
    QVERIFY(!unsupported.isSupported());
    QVERIFY(unsupported.placement.has_value());
    QVERIFY(hasIssue(unsupported,
                     NativePlacementField::FloatingMargin,
                     NativePlacementIssueKind::UnsupportedRequest,
                     NativePlacementIssueCode::CapabilityUnavailable));
    QVERIFY(hasIssue(unsupported,
                     NativePlacementField::ReservedSpace,
                     NativePlacementIssueKind::UnsupportedRequest,
                     NativePlacementIssueCode::CapabilityUnavailable));

    NativePlacementCapabilities capabilities;
    capabilities.supportsFloatingMargin = true;
    capabilities.supportsReservedSpace = true;
    const NativePanelPlacementResult supported = normalizeNativePanelPlacement(
        request, capabilities);
    QVERIFY(supported.isValid());
    QVERIFY(supported.isSupported());
    QVERIFY(supported.issues.isEmpty());
}

void PanelPlacementTest::preservesStableScreenIdWithFallback()
{
    using namespace ArchDock;

    NativePanelPlacementRequest request;
    request.screenStableId = QStringLiteral("  output:missing  ");
    request.screenFallbackIndex = 3;

    const NativePanelPlacementResult result = normalizeNativePanelPlacement(request);
    QVERIFY(result.isSupported());
    QVERIFY(result.placement.has_value());
    QCOMPARE(result.placement->screen.stableId, QStringLiteral("output:missing"));
    QCOMPARE(result.placement->screen.fallbackIndex, 3);
}

QTEST_APPLESS_MAIN(PanelPlacementTest)

#include "PanelPlacementTest.moc"
