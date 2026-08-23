#include "integration/PlasmaPanelAdapter.h"

#include <QTest>

#include <algorithm>
#include <optional>

namespace
{
ArchDock::PlasmaPanelAdapter fixtureAdapter(QList<std::optional<int>> *replies,
                                            QStringList *scripts)
{
    return ArchDock::PlasmaPanelAdapter(
        [replies, scripts](const QString &script) -> std::optional<int>
        {
            scripts->append(script);
            if (replies->isEmpty())
            {
                return std::nullopt;
            }
            return replies->takeFirst();
        });
}

QList<std::optional<int>> successfulFixedReplies(int edge = 1,
                                                 int screen = 0,
                                                 int alignment = 1,
                                                 int offset = 0,
                                                 int thickness = 76,
                                                 int length = 720)
{
    return {edge, screen, alignment, offset, thickness, length, length, length, 1};
}
}

class PlasmaPanelAdapterTest final : public QObject
{
    Q_OBJECT

private slots:
    void edgeScriptsUseSupportedProperties_data();
    void edgeScriptsUseSupportedProperties();
    void appliesAndReadsBackEveryField();
    void mapsVerticalAlignment_data();
    void mapsVerticalAlignment();
    void mapsGeometryModes_data();
    void mapsGeometryModes();
    void reportsUnsupportedAndContinues();
    void reportsReadbackMismatch();
    void stopsBeforeMutationWhenOwnershipIsDenied();
    void reportsMissingContainmentForEveryField();
    void reportsScriptAndFixtureFailures();
    void rejectsInvalidRequestsWithoutExecuting();
    void escapesOwnershipValuesInEveryScript();
};

void PlasmaPanelAdapterTest::edgeScriptsUseSupportedProperties_data()
{
    QTest::addColumn<int>("edge");
    QTest::addColumn<int>("readback");
    QTest::addColumn<QString>("propertyValue");

    using ArchDock::NativePanelEdge;
    QTest::newRow("top") << static_cast<int>(NativePanelEdge::Top) << 0
                         << QStringLiteral("top");
    QTest::newRow("bottom") << static_cast<int>(NativePanelEdge::Bottom) << 1
                            << QStringLiteral("bottom");
    QTest::newRow("left") << static_cast<int>(NativePanelEdge::Left) << 2
                          << QStringLiteral("left");
    QTest::newRow("right") << static_cast<int>(NativePanelEdge::Right) << 3
                           << QStringLiteral("right");
}

void PlasmaPanelAdapterTest::edgeScriptsUseSupportedProperties()
{
    QFETCH(int, edge);
    QFETCH(int, readback);
    QFETCH(QString, propertyValue);

    ArchDock::NativePanelPlacement placement;
    placement.edge = static_cast<ArchDock::NativePanelEdge>(edge);

    QList<std::optional<int>> replies = successfulFixedReplies(readback);
    QStringList scripts;
    const ArchDock::PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const ArchDock::PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.allApplied());
    QCOMPARE(scripts.size(), 9);
    QVERIFY(scripts.constFirst().contains(
        QStringLiteral("panel.location = '%1';").arg(propertyValue)));
    QVERIFY(result.fields.constFirst().actualValue.has_value());
    QCOMPARE(*result.fields.constFirst().actualValue, propertyValue);
}

void PlasmaPanelAdapterTest::appliesAndReadsBackEveryField()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.screen.fallbackIndex = 2;
    placement.alignment = NativePanelAlignment::End;
    placement.offset = 37;

    QList<std::optional<int>> replies = successfulFixedReplies(0, 2, 2, 37);
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.ownershipVerified);
    QVERIFY(result.allApplied());
    QVERIFY(!result.hasUnsupported());
    QCOMPARE(result.fields.size(), 9);
    QCOMPARE(result.fields.at(0).requestedValue, QStringLiteral("top"));
    QCOMPARE(result.fields.at(1).requestedValue, QStringLiteral("2"));
    QCOMPARE(result.fields.at(2).requestedValue, QStringLiteral("end"));
    QCOMPARE(result.fields.at(3).requestedValue, QStringLiteral("37"));
    QCOMPARE(result.fields.at(4).requestedValue, QStringLiteral("76"));
    QCOMPARE(result.fields.at(5).requestedValue, QStringLiteral("720"));
    QCOMPARE(result.fields.at(6).requestedValue, QStringLiteral("720"));
    QCOMPARE(result.fields.at(7).requestedValue, QStringLiteral("720"));
    QCOMPARE(result.fields.at(8).requestedValue, QStringLiteral("fixed"));
    QVERIFY(scripts.at(1).contains(QStringLiteral("panel.screen = 2;")));
    QVERIFY(scripts.at(2).contains(QStringLiteral("panel.alignment = 'right';")));
    QVERIFY(scripts.at(3).contains(QStringLiteral("panel.offset = 37;")));
    QVERIFY(scripts.at(4).contains(QStringLiteral("panel.height = 76;")));
    QVERIFY(scripts.at(5).contains(QStringLiteral("panel.maximumLength = 720;")));
    QVERIFY(scripts.at(6).contains(QStringLiteral("panel.minimumLength = 720;")));
    QVERIFY(scripts.at(7).contains(QStringLiteral("panel.length = 720;")));
    QVERIFY(scripts.at(8).contains(QStringLiteral("panel.lengthMode = 'custom';")));
}

void PlasmaPanelAdapterTest::mapsVerticalAlignment_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<int>("readback");
    QTest::addColumn<QString>("propertyValue");
    QTest::addColumn<QString>("logicalValue");

    using ArchDock::NativePanelAlignment;
    QTest::newRow("start-is-top") << static_cast<int>(NativePanelAlignment::Start) << 2
                                  << QStringLiteral("right") << QStringLiteral("start");
    QTest::newRow("center") << static_cast<int>(NativePanelAlignment::Center) << 1
                            << QStringLiteral("center") << QStringLiteral("center");
    QTest::newRow("end-is-bottom") << static_cast<int>(NativePanelAlignment::End) << 0
                                   << QStringLiteral("left") << QStringLiteral("end");
}

void PlasmaPanelAdapterTest::mapsVerticalAlignment()
{
    QFETCH(int, alignment);
    QFETCH(int, readback);
    QFETCH(QString, propertyValue);
    QFETCH(QString, logicalValue);

    ArchDock::NativePanelPlacement placement;
    placement.edge = ArchDock::NativePanelEdge::Left;
    placement.alignment = static_cast<ArchDock::NativePanelAlignment>(alignment);

    QList<std::optional<int>> replies = successfulFixedReplies(2, 0, readback);
    QStringList scripts;
    const ArchDock::PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const ArchDock::PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.allApplied());
    QVERIFY(scripts.at(2).contains(
        QStringLiteral("panel.alignment = '%1';").arg(propertyValue)));
    QVERIFY(result.fields.at(2).actualValue.has_value());
    QCOMPARE(*result.fields.at(2).actualValue, logicalValue);
}

void PlasmaPanelAdapterTest::mapsGeometryModes_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("modeReadback");
    QTest::addColumn<QString>("plasmaMode");
    QTest::addColumn<QString>("logicalMode");

    using ArchDock::NativePanelLengthMode;
    QTest::newRow("fit") << static_cast<int>(NativePanelLengthMode::Fit) << 0
                         << QStringLiteral("fit") << QStringLiteral("fit");
    QTest::newRow("fixed") << static_cast<int>(NativePanelLengthMode::Fixed) << 1
                           << QStringLiteral("custom") << QStringLiteral("fixed");
    QTest::newRow("fill") << static_cast<int>(NativePanelLengthMode::Fill) << 2
                          << QStringLiteral("fill") << QStringLiteral("fill");
}

void PlasmaPanelAdapterTest::mapsGeometryModes()
{
    QFETCH(int, mode);
    QFETCH(int, modeReadback);
    QFETCH(QString, plasmaMode);
    QFETCH(QString, logicalMode);

    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.thickness = 88;
    placement.lengthMode = static_cast<NativePanelLengthMode>(mode);
    placement.minimumLength = 120;
    placement.maximumLength = 840;
    placement.fixedLength = 640;

    QList<std::optional<int>> replies{1, 0, 1, 0, 88};
    if (placement.lengthMode == NativePanelLengthMode::Fixed)
    {
        replies += QList<std::optional<int>>{640, 640, 640, modeReadback};
    }
    else
    {
        replies += QList<std::optional<int>>{modeReadback, 840, 120};
    }

    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.allApplied());
    QVERIFY(result.ownershipVerified);
    QVERIFY(scripts.at(4).contains(QStringLiteral("panel.height = 88;")));
    QCOMPARE(result.fields.at(4).field, NativePlacementField::Thickness);

    if (placement.lengthMode == NativePanelLengthMode::Fixed)
    {
        QCOMPARE(scripts.size(), 9);
        QVERIFY(scripts.at(5).contains(QStringLiteral("panel.maximumLength = 640;")));
        QVERIFY(scripts.at(6).contains(QStringLiteral("panel.minimumLength = 640;")));
        QVERIFY(scripts.at(7).contains(QStringLiteral("panel.length = 640;")));
        QVERIFY(scripts.at(8).contains(
            QStringLiteral("panel.lengthMode = '%1';").arg(plasmaMode)));
        QCOMPARE(result.fields.at(8).field, NativePlacementField::LengthMode);
        QVERIFY(result.fields.at(8).actualValue.has_value());
        QCOMPARE(*result.fields.at(8).actualValue, logicalMode);
    }
    else
    {
        QCOMPARE(scripts.size(), 8);
        QVERIFY(scripts.at(5).contains(
            QStringLiteral("panel.lengthMode = '%1';").arg(plasmaMode)));
        QVERIFY(scripts.at(6).contains(QStringLiteral("panel.maximumLength = 840;")));
        QVERIFY(scripts.at(7).contains(QStringLiteral("panel.minimumLength = 120;")));
        QVERIFY(std::none_of(
            scripts.cbegin(),
            scripts.cend(),
            [](const QString &script)
            {
                return script.contains(QStringLiteral("panel.length ="));
            }));
        QCOMPARE(result.fields.at(5).field, NativePlacementField::LengthMode);
        QVERIFY(result.fields.at(5).actualValue.has_value());
        QCOMPARE(*result.fields.at(5).actualValue, logicalMode);
    }
}

void PlasmaPanelAdapterTest::reportsUnsupportedAndContinues()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.offset = 17;

    QList<std::optional<int>> replies = successfulFixedReplies(0, 0, 1, 17);
    replies[4] = -1002;
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.ownershipVerified);
    QVERIFY(!result.allApplied());
    QVERIFY(result.hasUnsupported());
    QCOMPARE(scripts.size(), 9);
    QCOMPARE(result.fields.at(4).status, PlasmaPanelApplyStatus::Unsupported);
    QCOMPARE(result.fields.at(4).failure, PlasmaPanelApplyFailure::PropertyUnsupported);
    QCOMPARE(result.fields.at(8).status, PlasmaPanelApplyStatus::Applied);
}

void PlasmaPanelAdapterTest::reportsReadbackMismatch()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;

    QList<std::optional<int>> replies = successfulFixedReplies();
    replies[4] = 77;
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QCOMPARE(result.fields.at(4).status, PlasmaPanelApplyStatus::Failed);
    QCOMPARE(result.fields.at(4).failure, PlasmaPanelApplyFailure::ReadbackMismatch);
    QVERIFY(result.fields.at(4).actualValue.has_value());
    QCOMPARE(*result.fields.at(4).actualValue, QStringLiteral("77"));
    QCOMPARE(scripts.size(), 9);
}

void PlasmaPanelAdapterTest::stopsBeforeMutationWhenOwnershipIsDenied()
{
    using namespace ArchDock;

    QList<std::optional<int>> replies{-1001};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("wrong-owner"), {});

    QVERIFY(!result.ownershipVerified);
    QCOMPARE(scripts.size(), 1);
    QCOMPARE(result.fields.size(), 9);
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        QCOMPARE(field.status, PlasmaPanelApplyStatus::Failed);
        QCOMPARE(field.failure, PlasmaPanelApplyFailure::OwnershipDenied);
    }
}

void PlasmaPanelAdapterTest::reportsMissingContainmentForEveryField()
{
    using namespace ArchDock;

    QList<std::optional<int>> replies{-1000};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), {});

    QCOMPARE(scripts.size(), 1);
    QCOMPARE(result.fields.size(), 9);
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        QCOMPARE(field.failure, PlasmaPanelApplyFailure::ContainmentMissing);
    }
}

void PlasmaPanelAdapterTest::reportsScriptAndFixtureFailures()
{
    using namespace ArchDock;

    QList<std::optional<int>> replies{
        std::nullopt,
        -1003,
        -1004,
        99,
    };
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), {});

    QCOMPARE(scripts.size(), 9);
    QCOMPARE(result.fields.at(0).failure, PlasmaPanelApplyFailure::ScriptFailure);
    QCOMPARE(result.fields.at(1).failure, PlasmaPanelApplyFailure::ScriptFailure);
    QCOMPARE(result.fields.at(2).failure, PlasmaPanelApplyFailure::ReadbackMismatch);
    QCOMPARE(result.fields.at(3).failure, PlasmaPanelApplyFailure::ReadbackMismatch);
    QVERIFY(result.fields.at(3).actualValue.has_value());
    QCOMPARE(*result.fields.at(3).actualValue, QStringLiteral("99"));
}

void PlasmaPanelAdapterTest::rejectsInvalidRequestsWithoutExecuting()
{
    using namespace ArchDock;

    QList<std::optional<int>> replies = successfulFixedReplies();
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        -1, QString{}, QString{}, {});

    QVERIFY(scripts.isEmpty());
    QCOMPARE(result.fields.size(), 9);
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        QCOMPARE(field.failure, PlasmaPanelApplyFailure::InvalidRequest);
    }
}

void PlasmaPanelAdapterTest::escapesOwnershipValuesInEveryScript()
{
    using namespace ArchDock;

    const QString panelId = QStringLiteral("panel-'") + QLatin1Char('\\') +
        QStringLiteral("line\nnext") + QChar(0x2028) + QStringLiteral("id");
    const QString token = QStringLiteral("owner-'") + QLatin1Char('\\') +
        QStringLiteral("line\rnext") + QChar(0x2029) + QStringLiteral("token");
    QList<std::optional<int>> replies = successfulFixedReplies();
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, panelId, token, {});

    QVERIFY(result.allApplied());
    QCOMPARE(scripts.size(), 9);
    for (const QString &script : scripts)
    {
        QVERIFY(script.contains(QStringLiteral("var panel = panelById(42);")));
        QVERIFY(script.contains(QStringLiteral("panel.readConfig('ownerToken', '')")));
        QVERIFY(script.contains(QStringLiteral("panel.readConfig('panelId', '')")));
        QVERIFY(script.contains(QStringLiteral("panel-\\'\\\\line\\nnext\\u2028id")));
        QVERIFY(script.contains(QStringLiteral("owner-\\'\\\\line\\rnext\\u2029token")));
        QVERIFY(!script.contains(panelId));
        QVERIFY(!script.contains(token));
    }
}

QTEST_APPLESS_MAIN(PlasmaPanelAdapterTest)

#include "PlasmaPanelAdapterTest.moc"
