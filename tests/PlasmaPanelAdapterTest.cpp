#include "integration/PlasmaPanelAdapter.h"

#include <QTest>

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

    QList<std::optional<int>> replies{readback, 0, 1, 0};
    QStringList scripts;
    const ArchDock::PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const ArchDock::PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.allApplied());
    QCOMPARE(scripts.size(), 4);
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

    QList<std::optional<int>> replies{0, 2, 2, 37};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.ownershipVerified);
    QVERIFY(result.allApplied());
    QVERIFY(!result.hasUnsupported());
    QCOMPARE(result.fields.size(), 4);
    QCOMPARE(result.fields.at(0).requestedValue, QStringLiteral("top"));
    QCOMPARE(result.fields.at(1).requestedValue, QStringLiteral("2"));
    QCOMPARE(result.fields.at(2).requestedValue, QStringLiteral("end"));
    QCOMPARE(result.fields.at(3).requestedValue, QStringLiteral("37"));
    QVERIFY(scripts.at(1).contains(QStringLiteral("panel.screen = 2;")));
    QVERIFY(scripts.at(2).contains(QStringLiteral("panel.alignment = 'right';")));
    QVERIFY(scripts.at(3).contains(QStringLiteral("panel.offset = 37;")));
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

    QList<std::optional<int>> replies{2, 0, readback, 0};
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

void PlasmaPanelAdapterTest::reportsUnsupportedAndContinues()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.offset = 17;

    QList<std::optional<int>> replies{0, -1002, 1, 17};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.ownershipVerified);
    QVERIFY(!result.allApplied());
    QVERIFY(result.hasUnsupported());
    QCOMPARE(scripts.size(), 4);
    QCOMPARE(result.fields.at(1).status, PlasmaPanelApplyStatus::Unsupported);
    QCOMPARE(result.fields.at(1).failure, PlasmaPanelApplyFailure::PropertyUnsupported);
    QCOMPARE(result.fields.at(3).status, PlasmaPanelApplyStatus::Applied);
}

void PlasmaPanelAdapterTest::reportsReadbackMismatch()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;

    QList<std::optional<int>> replies{1, 0, 1, 0};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QCOMPARE(result.fields.at(0).status, PlasmaPanelApplyStatus::Failed);
    QCOMPARE(result.fields.at(0).failure, PlasmaPanelApplyFailure::ReadbackMismatch);
    QVERIFY(result.fields.at(0).actualValue.has_value());
    QCOMPARE(*result.fields.at(0).actualValue, QStringLiteral("bottom"));
    QCOMPARE(scripts.size(), 4);
}

void PlasmaPanelAdapterTest::stopsBeforeMutationWhenOwnershipIsDenied()
{
    using namespace ArchDock;

    QList<std::optional<int>> replies{-1001, 0, 1, 0};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("wrong-owner"), {});

    QVERIFY(!result.ownershipVerified);
    QCOMPARE(scripts.size(), 1);
    QCOMPARE(result.fields.size(), 4);
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
    QCOMPARE(result.fields.size(), 4);
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

    QCOMPARE(scripts.size(), 4);
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

    QList<std::optional<int>> replies{0, 0, 1, 0};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        -1, QString{}, QString{}, {});

    QVERIFY(scripts.isEmpty());
    QCOMPARE(result.fields.size(), 4);
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
    QList<std::optional<int>> replies{1, 0, 1, 0};
    QStringList scripts;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&replies, &scripts);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, panelId, token, {});

    QVERIFY(result.allApplied());
    QCOMPARE(scripts.size(), 4);
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
