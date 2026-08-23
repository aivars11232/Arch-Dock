#include "integration/PlasmaPanelAdapter.h"

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include <optional>

namespace
{
class FakePlasmaHost final
{
public:
    QMap<QString, int> values{
        {QStringLiteral("edge"), 1},
        {QStringLiteral("screen"), 0},
        {QStringLiteral("alignment"), 1},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("height"), 76},
        {QStringLiteral("maximumLength"), 720},
        {QStringLiteral("minimumLength"), 720},
        {QStringLiteral("length"), 720},
        {QStringLiteral("lengthMode"), 1},
        {QStringLiteral("hiding"), 0},
        {QStringLiteral("temporaryHidden"), 0},
    };
    QSet<QString> unsupported;
    QList<std::optional<int>> forcedReplies;
    QString coerceFirstMutationProperty;
    int coercedValue = 0;
    QString failFirstMutationProperty;
    QString failRollbackProperty;
    QMap<QString, int> mutationCounts;
    QStringList scripts;

    std::optional<int> execute(const QString &script)
    {
        scripts.append(script);
        if (!forcedReplies.isEmpty())
        {
            return forcedReplies.takeFirst();
        }

        if (script.contains(QStringLiteral("var actualHiding")) &&
            script.contains(QStringLiteral("var temporaryCode")))
        {
            return executeVisibilityState(script);
        }

        const QString property = propertyForScript(script);
        if (property.isEmpty())
        {
            return -1003;
        }
        if (unsupported.contains(property))
        {
            return -1002;
        }

        const std::optional<int> mutation = mutationValue(script, property);
        if (mutation.has_value())
        {
            const int mutationCount = ++mutationCounts[property];
            values[property] = property == coerceFirstMutationProperty && mutationCount == 1
                ? coercedValue
                : *mutation;
            if (property == failFirstMutationProperty && mutationCount == 1)
            {
                return -1003;
            }
            if (property == failRollbackProperty && mutationCount > 1)
            {
                return -1003;
            }
        }
        return values.value(property);
    }

private:
    std::optional<int> executeVisibilityState(const QString &script)
    {
        if (unsupported.contains(QStringLiteral("hiding")) ||
            unsupported.contains(QStringLiteral("temporaryHidden")))
        {
            return -1002;
        }

        const auto mutate = [this](const QString &property, int requestedValue)
            -> std::optional<int>
        {
            const int mutationCount = ++mutationCounts[property];
            values[property] = property == coerceFirstMutationProperty && mutationCount == 1
                ? coercedValue
                : requestedValue;
            if (property == failFirstMutationProperty && mutationCount == 1)
            {
                return -1003;
            }
            if (property == failRollbackProperty && mutationCount > 1)
            {
                return -1003;
            }
            return std::nullopt;
        };

        const QRegularExpression temporaryExpression(
            QStringLiteral("panel\\.writeConfig\\('temporaryHidden', '([01])'\\);"));
        const QRegularExpressionMatch temporaryMatch = temporaryExpression.match(script);
        if (temporaryMatch.hasMatch())
        {
            const std::optional<int> failure = mutate(
                QStringLiteral("temporaryHidden"), temporaryMatch.captured(1).toInt());
            if (failure.has_value())
            {
                return failure;
            }
        }

        const QRegularExpression hidingExpression(
            QStringLiteral("panel\\.hiding = '([^']+)';"));
        const QRegularExpressionMatch hidingMatch = hidingExpression.match(script);
        if (hidingMatch.hasMatch())
        {
            const QMap<QString, int> hidingValues{
                {QStringLiteral("none"), 0},
                {QStringLiteral("autohide"), 1},
                {QStringLiteral("dodgewindows"), 2},
                {QStringLiteral("windowsgobelow"), 3},
            };
            if (!hidingValues.contains(hidingMatch.captured(1)))
            {
                return -1004;
            }
            const std::optional<int> failure = mutate(
                QStringLiteral("hiding"), hidingValues.value(hidingMatch.captured(1)));
            if (failure.has_value())
            {
                return failure;
            }
        }

        return values.value(QStringLiteral("hiding")) * 2 +
            values.value(QStringLiteral("temporaryHidden"));
    }

    static QString propertyForScript(const QString &script)
    {
        if (script.contains(QStringLiteral("panel.maximumLength")))
        {
            return QStringLiteral("maximumLength");
        }
        if (script.contains(QStringLiteral("panel.minimumLength")))
        {
            return QStringLiteral("minimumLength");
        }
        if (script.contains(QStringLiteral("panel.lengthMode")))
        {
            return QStringLiteral("lengthMode");
        }
        if (script.contains(QStringLiteral("panel.length")))
        {
            return QStringLiteral("length");
        }
        if (script.contains(QStringLiteral("panel.location")))
        {
            return QStringLiteral("edge");
        }
        if (script.contains(QStringLiteral("panel.screen")))
        {
            return QStringLiteral("screen");
        }
        if (script.contains(QStringLiteral("panel.alignment")))
        {
            return QStringLiteral("alignment");
        }
        if (script.contains(QStringLiteral("panel.offset")))
        {
            return QStringLiteral("offset");
        }
        if (script.contains(QStringLiteral("panel.height")))
        {
            return QStringLiteral("height");
        }
        return {};
    }

    static std::optional<int> stringMutationValue(const QString &script,
                                                  const QString &property,
                                                  const QMap<QString, int> &mapping)
    {
        const QRegularExpression expression(
            QStringLiteral("panel\\.%1 = '([^']+)';").arg(property));
        const QRegularExpressionMatch match = expression.match(script);
        if (!match.hasMatch() || !mapping.contains(match.captured(1)))
        {
            return std::nullopt;
        }
        return mapping.value(match.captured(1));
    }

    static std::optional<int> mutationValue(const QString &script,
                                            const QString &property)
    {
        if (property == QLatin1String("edge"))
        {
            return stringMutationValue(
                script,
                QStringLiteral("location"),
                {{QStringLiteral("top"), 0},
                 {QStringLiteral("bottom"), 1},
                 {QStringLiteral("left"), 2},
                 {QStringLiteral("right"), 3}});
        }
        if (property == QLatin1String("alignment"))
        {
            return stringMutationValue(
                script,
                QStringLiteral("alignment"),
                {{QStringLiteral("left"), 0},
                 {QStringLiteral("center"), 1},
                 {QStringLiteral("right"), 2}});
        }
        if (property == QLatin1String("lengthMode"))
        {
            return stringMutationValue(
                script,
                QStringLiteral("lengthMode"),
                {{QStringLiteral("fit"), 0},
                 {QStringLiteral("custom"), 1},
                 {QStringLiteral("fill"), 2}});
        }

        const QRegularExpression expression(
            QStringLiteral("panel\\.%1 = (-?\\d+);").arg(property));
        const QRegularExpressionMatch match = expression.match(script);
        if (!match.hasMatch())
        {
            return std::nullopt;
        }
        return match.captured(1).toInt();
    }
};

ArchDock::PlasmaPanelAdapter fixtureAdapter(FakePlasmaHost *host)
{
    return ArchDock::PlasmaPanelAdapter(
        [host](const QString &script)
        {
            return host->execute(script);
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
    void mapsGeometryModes_data();
    void mapsGeometryModes();
    void reportsUnsupportedAndRollsBack();
    void reportsReadbackMismatchAndRollsBack();
    void rollsBackWhenPersistenceFails();
    void reportsRollbackFailurePrecisely();
    void stopsBeforeMutationWhenOwnershipIsDenied();
    void reportsMissingContainmentForEveryField();
    void reportsScriptFailureBeforeMutation();
    void rejectsInvalidRequestsWithoutExecuting();
    void escapesOwnershipValuesInEveryScript();
    void appliesAndReadsBackVisibilityState();
    void reportsUnsupportedVisibilityBeforeMutation();
    void rollsBackVisibilityReadbackMismatch();
    void rollsBackVisibilityWhenPersistenceFails();
    void reportsVisibilityRollbackFailurePrecisely();
    void stopsVisibilityMutationWhenOwnershipIsDenied();
    void rejectsInvalidVisibilityRequestsWithoutExecuting();
    void escapesVisibilityOwnershipValuesInEveryScript();
};

void PlasmaPanelAdapterTest::edgeScriptsUseSupportedProperties_data()
{
    QTest::addColumn<int>("edge");
    QTest::addColumn<QString>("propertyValue");

    using ArchDock::NativePanelEdge;
    QTest::newRow("top") << static_cast<int>(NativePanelEdge::Top) << QStringLiteral("top");
    QTest::newRow("bottom") << static_cast<int>(NativePanelEdge::Bottom)
                            << QStringLiteral("bottom");
    QTest::newRow("left") << static_cast<int>(NativePanelEdge::Left) << QStringLiteral("left");
    QTest::newRow("right") << static_cast<int>(NativePanelEdge::Right) << QStringLiteral("right");
}

void PlasmaPanelAdapterTest::edgeScriptsUseSupportedProperties()
{
    QFETCH(int, edge);
    QFETCH(QString, propertyValue);

    ArchDock::NativePanelPlacement placement;
    placement.edge = static_cast<ArchDock::NativePanelEdge>(edge);
    FakePlasmaHost host;
    const ArchDock::PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const ArchDock::PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.success());
    QCOMPARE(host.scripts.size(), 27);
    QVERIFY(host.scripts.at(9).contains(
        QStringLiteral("panel.location = '%1';").arg(propertyValue)));
    QCOMPARE(result.hostState.value(QStringLiteral("edge")).toString(), propertyValue);
}

void PlasmaPanelAdapterTest::appliesAndReadsBackEveryField()
{
    using namespace ArchDock;

    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.screen.fallbackIndex = 2;
    placement.alignment = NativePanelAlignment::End;
    placement.offset = 37;

    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.ownershipVerified);
    QVERIFY(result.success());
    QVERIFY(result.allApplied());
    QVERIFY(!result.hasUnsupported());
    QCOMPARE(result.status, QStringLiteral("applied"));
    QVERIFY(result.errorCode.isEmpty());
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
    QVERIFY(host.scripts.at(10).contains(QStringLiteral("panel.screen = 2;")));
    QVERIFY(host.scripts.at(11).contains(QStringLiteral("panel.alignment = 'right';")));
    QVERIFY(host.scripts.at(12).contains(QStringLiteral("panel.offset = 37;")));
    QCOMPARE(result.hostState.value(QStringLiteral("screen")).toString(), QStringLiteral("2"));
    QCOMPARE(result.hostState.value(QStringLiteral("alignment")).toString(), QStringLiteral("end"));

    const QVariantMap structured = result.toVariantMap();
    QVERIFY(structured.value(QStringLiteral("success")).toBool());
    QCOMPARE(structured.value(QStringLiteral("requested")).toMap().size(), 9);
    QCOMPARE(structured.value(QStringLiteral("applied")).toMap().size(), 9);
    QVERIFY(structured.value(QStringLiteral("unsupported")).toList().isEmpty());
    QVERIFY(structured.value(QStringLiteral("failed")).toList().isEmpty());
}

void PlasmaPanelAdapterTest::mapsVerticalAlignment_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<QString>("propertyValue");
    QTest::addColumn<QString>("logicalValue");

    using ArchDock::NativePanelAlignment;
    QTest::newRow("start-is-top") << static_cast<int>(NativePanelAlignment::Start)
                                  << QStringLiteral("right") << QStringLiteral("start");
    QTest::newRow("center") << static_cast<int>(NativePanelAlignment::Center)
                            << QStringLiteral("center") << QStringLiteral("center");
    QTest::newRow("end-is-bottom") << static_cast<int>(NativePanelAlignment::End)
                                   << QStringLiteral("left") << QStringLiteral("end");
}

void PlasmaPanelAdapterTest::mapsVerticalAlignment()
{
    QFETCH(int, alignment);
    QFETCH(QString, propertyValue);
    QFETCH(QString, logicalValue);

    ArchDock::NativePanelPlacement placement;
    placement.edge = ArchDock::NativePanelEdge::Left;
    placement.alignment = static_cast<ArchDock::NativePanelAlignment>(alignment);
    FakePlasmaHost host;
    const ArchDock::PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const ArchDock::PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.success());
    QVERIFY(host.scripts.at(11).contains(
        QStringLiteral("panel.alignment = '%1';").arg(propertyValue)));
    QCOMPARE(result.hostState.value(QStringLiteral("alignment")).toString(), logicalValue);
}

void PlasmaPanelAdapterTest::mapsGeometryModes_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<QString>("plasmaMode");
    QTest::addColumn<int>("operationCount");

    using ArchDock::NativePanelLengthMode;
    QTest::newRow("fit") << static_cast<int>(NativePanelLengthMode::Fit)
                         << QStringLiteral("fit") << 8;
    QTest::newRow("fixed") << static_cast<int>(NativePanelLengthMode::Fixed)
                           << QStringLiteral("custom") << 9;
    QTest::newRow("fill") << static_cast<int>(NativePanelLengthMode::Fill)
                          << QStringLiteral("fill") << 8;
}

void PlasmaPanelAdapterTest::mapsGeometryModes()
{
    QFETCH(int, mode);
    QFETCH(QString, plasmaMode);
    QFETCH(int, operationCount);

    using namespace ArchDock;
    NativePanelPlacement placement;
    placement.thickness = 88;
    placement.lengthMode = static_cast<NativePanelLengthMode>(mode);
    placement.minimumLength = 120;
    placement.maximumLength = 840;
    placement.fixedLength = 640;

    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(result.success());
    QCOMPARE(host.scripts.size(), operationCount * 3);
    QVERIFY(host.scripts.at(operationCount + 4).contains(QStringLiteral("panel.height = 88;")));
    QVERIFY(std::any_of(
        host.scripts.cbegin() + operationCount,
        host.scripts.cbegin() + (operationCount * 2),
        [&plasmaMode](const QString &script)
        {
            return script.contains(
                QStringLiteral("panel.lengthMode = '%1';").arg(plasmaMode));
        }));
    QCOMPARE(result.hostState.value(QStringLiteral("height")).toString(), QStringLiteral("88"));
}

void PlasmaPanelAdapterTest::reportsUnsupportedAndRollsBack()
{
    using namespace ArchDock;
    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.offset = 17;

    FakePlasmaHost host;
    host.unsupported.insert(QStringLiteral("height"));
    const QMap<QString, int> original = host.values;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QVERIFY(!result.success());
    QVERIFY(result.hasUnsupported());
    QCOMPARE(result.status, QStringLiteral("rolled-back"));
    QCOMPARE(result.errorCode, QStringLiteral("property-unsupported"));
    QVERIFY(result.rollbackAttempted);
    QVERIFY(result.rollbackSucceeded);
    QCOMPARE(result.fields.at(4).status, PlasmaPanelApplyStatus::Unsupported);
    QCOMPARE(result.fields.at(4).failure, PlasmaPanelApplyFailure::PropertyUnsupported);
    QCOMPARE(host.values, original);
    QCOMPARE(result.hostState.value(QStringLiteral("edge")).toString(), QStringLiteral("bottom"));
    QCOMPARE(result.toVariantMap().value(QStringLiteral("unsupported")).toList().size(), 1);
}

void PlasmaPanelAdapterTest::reportsReadbackMismatchAndRollsBack()
{
    using namespace ArchDock;
    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;

    FakePlasmaHost host;
    host.coerceFirstMutationProperty = QStringLiteral("height");
    host.coercedValue = 77;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QCOMPARE(result.status, QStringLiteral("rolled-back"));
    QCOMPARE(result.errorCode, QStringLiteral("readback-mismatch"));
    QVERIFY(result.rollbackSucceeded);
    QCOMPARE(result.fields.at(4).status, PlasmaPanelApplyStatus::Failed);
    QCOMPARE(result.fields.at(4).failure, PlasmaPanelApplyFailure::ReadbackMismatch);
    QVERIFY(result.fields.at(4).actualValue.has_value());
    QCOMPARE(*result.fields.at(4).actualValue, QStringLiteral("77"));
    QVERIFY(result.fields.at(4).hostValue.has_value());
    QCOMPARE(*result.fields.at(4).hostValue, QStringLiteral("76"));
}

void PlasmaPanelAdapterTest::rollsBackWhenPersistenceFails()
{
    using namespace ArchDock;
    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;
    placement.screen.fallbackIndex = 2;

    FakePlasmaHost host;
    const QMap<QString, int> original = host.values;
    bool persistenceCalled = false;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        placement,
        [&persistenceCalled]
        {
            persistenceCalled = true;
            return false;
        });

    QVERIFY(persistenceCalled);
    QVERIFY(!result.success());
    QCOMPARE(result.status, QStringLiteral("rolled-back"));
    QCOMPARE(result.errorCode, QStringLiteral("persistence-failed"));
    QVERIFY(result.allApplied());
    QVERIFY(result.rollbackAttempted);
    QVERIFY(result.rollbackSucceeded);
    QCOMPARE(host.values, original);
    QCOMPARE(result.hostState.value(QStringLiteral("screen")).toString(), QStringLiteral("0"));
}

void PlasmaPanelAdapterTest::reportsRollbackFailurePrecisely()
{
    using namespace ArchDock;
    NativePanelPlacement placement;
    placement.edge = NativePanelEdge::Top;

    FakePlasmaHost host;
    host.coerceFirstMutationProperty = QStringLiteral("height");
    host.coercedValue = 77;
    host.failRollbackProperty = QStringLiteral("height");
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), placement);

    QCOMPARE(result.status, QStringLiteral("rollback-failed"));
    QVERIFY(result.rollbackAttempted);
    QVERIFY(!result.rollbackSucceeded);
    QCOMPARE(result.rollbackErrorCode, QStringLiteral("script-failure"));
}

void PlasmaPanelAdapterTest::stopsBeforeMutationWhenOwnershipIsDenied()
{
    using namespace ArchDock;
    FakePlasmaHost host;
    host.forcedReplies.append(-1001);
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("wrong-owner"), {});

    QVERIFY(!result.ownershipVerified);
    QCOMPARE(host.scripts.size(), 1);
    QVERIFY(host.mutationCounts.isEmpty());
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
    FakePlasmaHost host;
    host.forcedReplies.append(-1000);
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), {});

    QCOMPARE(host.scripts.size(), 1);
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        QCOMPARE(field.failure, PlasmaPanelApplyFailure::ContainmentMissing);
    }
}

void PlasmaPanelAdapterTest::reportsScriptFailureBeforeMutation()
{
    using namespace ArchDock;
    FakePlasmaHost host;
    host.forcedReplies.append(std::nullopt);
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(
        42, QStringLiteral("panel-1"), QStringLiteral("owner-1"), {});

    QCOMPARE(host.scripts.size(), 1);
    QVERIFY(host.mutationCounts.isEmpty());
    QVERIFY(!result.ownershipVerified);
    QCOMPARE(result.errorCode, QStringLiteral("script-failure"));
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        QCOMPARE(field.failure, PlasmaPanelApplyFailure::ScriptFailure);
    }
}

void PlasmaPanelAdapterTest::rejectsInvalidRequestsWithoutExecuting()
{
    using namespace ArchDock;
    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(-1, {}, {}, {});

    QVERIFY(host.scripts.isEmpty());
    QCOMPARE(result.errorCode, QStringLiteral("invalid-request"));
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
    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelPlacementApplyResult result = adapter.applyPlacement(42, panelId, token, {});

    QVERIFY(result.success());
    QCOMPARE(host.scripts.size(), 27);
    for (const QString &script : host.scripts)
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

void PlasmaPanelAdapterTest::appliesAndReadsBackVisibilityState()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        PlasmaPanelHidingMode::DodgeWindows,
        true);

    QVERIFY(result.success());
    QVERIFY(result.ownershipVerified);
    QCOMPARE(result.status, QStringLiteral("applied"));
    QVERIFY(result.errorCode.isEmpty());
    QCOMPARE(result.requestedHostMode, QStringLiteral("dodgewindows"));
    QVERIFY(result.requestedTemporaryHidden);
    QVERIFY(result.actualHostMode.has_value());
    QVERIFY(result.actualTemporaryHidden.has_value());
    QCOMPARE(*result.actualHostMode, QStringLiteral("dodgewindows"));
    QCOMPARE(*result.actualTemporaryHidden, true);
    QCOMPARE(host.values.value(QStringLiteral("hiding")), 2);
    QCOMPARE(host.values.value(QStringLiteral("temporaryHidden")), 1);
    QCOMPARE(host.scripts.size(), 3);

    const QVariantMap structured = result.toVariantMap();
    QVERIFY(structured.value(QStringLiteral("success")).toBool());
    QCOMPARE(structured.value(QStringLiteral("requested")).toMap()
                 .value(QStringLiteral("hostMode")).toString(),
             QStringLiteral("dodgewindows"));
    QCOMPARE(structured.value(QStringLiteral("hostState")).toMap()
                 .value(QStringLiteral("temporaryHidden")).toBool(),
             true);
}

void PlasmaPanelAdapterTest::reportsUnsupportedVisibilityBeforeMutation()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    host.unsupported.insert(QStringLiteral("hiding"));
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        PlasmaPanelHidingMode::AutoHide,
        false);

    QVERIFY(!result.success());
    QVERIFY(result.ownershipVerified);
    QCOMPARE(result.errorCode, QStringLiteral("property-unsupported"));
    QCOMPARE(host.scripts.size(), 1);
    QVERIFY(host.mutationCounts.isEmpty());
}

void PlasmaPanelAdapterTest::rollsBackVisibilityReadbackMismatch()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    host.coerceFirstMutationProperty = QStringLiteral("hiding");
    host.coercedValue = 0;
    const QMap<QString, int> original = host.values;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        PlasmaPanelHidingMode::AutoHide,
        true);

    QVERIFY(!result.success());
    QCOMPARE(result.status, QStringLiteral("rolled-back"));
    QCOMPARE(result.errorCode, QStringLiteral("readback-mismatch"));
    QVERIFY(result.rollbackAttempted);
    QVERIFY(result.rollbackSucceeded);
    QCOMPARE(host.values, original);
    QCOMPARE(*result.actualHostMode, QStringLiteral("none"));
    QCOMPARE(*result.actualTemporaryHidden, false);
}

void PlasmaPanelAdapterTest::rollsBackVisibilityWhenPersistenceFails()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    const QMap<QString, int> original = host.values;
    bool persistenceCalled = false;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        PlasmaPanelHidingMode::AutoHide,
        false,
        [&persistenceCalled]
        {
            persistenceCalled = true;
            return false;
        });

    QVERIFY(persistenceCalled);
    QCOMPARE(result.status, QStringLiteral("rolled-back"));
    QCOMPARE(result.errorCode, QStringLiteral("persistence-failed"));
    QVERIFY(result.rollbackSucceeded);
    QCOMPARE(host.values, original);
}

void PlasmaPanelAdapterTest::reportsVisibilityRollbackFailurePrecisely()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    host.coerceFirstMutationProperty = QStringLiteral("hiding");
    host.coercedValue = 0;
    host.failRollbackProperty = QStringLiteral("hiding");
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("owner-1"),
        PlasmaPanelHidingMode::AutoHide,
        false);

    QCOMPARE(result.status, QStringLiteral("rollback-failed"));
    QVERIFY(result.rollbackAttempted);
    QVERIFY(!result.rollbackSucceeded);
    QCOMPARE(result.rollbackErrorCode, QStringLiteral("script-failure"));
}

void PlasmaPanelAdapterTest::stopsVisibilityMutationWhenOwnershipIsDenied()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    host.forcedReplies.append(-1001);
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42,
        QStringLiteral("panel-1"),
        QStringLiteral("wrong-owner"),
        PlasmaPanelHidingMode::AutoHide,
        false);

    QVERIFY(!result.ownershipVerified);
    QCOMPARE(result.errorCode, QStringLiteral("ownership-denied"));
    QCOMPARE(host.scripts.size(), 1);
    QVERIFY(host.mutationCounts.isEmpty());
}

void PlasmaPanelAdapterTest::rejectsInvalidVisibilityRequestsWithoutExecuting()
{
    using namespace ArchDock;

    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        -1, {}, {}, PlasmaPanelHidingMode::None, false);

    QCOMPARE(result.errorCode, QStringLiteral("invalid-request"));
    QVERIFY(host.scripts.isEmpty());
}

void PlasmaPanelAdapterTest::escapesVisibilityOwnershipValuesInEveryScript()
{
    using namespace ArchDock;

    const QString panelId = QStringLiteral("panel-'") + QLatin1Char('\\') +
        QStringLiteral("line\nnext") + QChar(0x2028) + QStringLiteral("id");
    const QString token = QStringLiteral("owner-'") + QLatin1Char('\\') +
        QStringLiteral("line\rnext") + QChar(0x2029) + QStringLiteral("token");
    FakePlasmaHost host;
    const PlasmaPanelAdapter adapter = fixtureAdapter(&host);
    const PlasmaPanelVisibilityApplyResult result = adapter.applyVisibility(
        42, panelId, token, PlasmaPanelHidingMode::AutoHide, false);

    QVERIFY(result.success());
    QCOMPARE(host.scripts.size(), 3);
    for (const QString &script : host.scripts)
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
