#include "panel/PanelContentTransaction.h"

#include <QTest>

using ArchDock::PanelContent;
using ArchDock::PanelContentOperation;
using ArchDock::PanelContentOutcome;
using ArchDock::PanelContentRequest;
using ArchDock::PanelContentTransaction;
using ArchDock::PanelDefinition;

namespace
{

PanelDefinition freePanel()
{
    PanelDefinition definition = PanelDefinition::defaults(
        QStringLiteral("free-9"),
        QStringLiteral("Ring"),
        QStringLiteral("free"),
        false);
    definition.settingsRevision = 4;
    definition.content.type = QStringLiteral("launcher");
    definition.content.urls = {
        QStringLiteral("file:///tmp/a.desktop"),
        QStringLiteral("file:///tmp/Folder/"),
    };
    definition.content.applicationIds = {QStringLiteral("org.kde.kate")};
    definition.content.entryOrder = definition.content.canonicalEntryOrder();
    return definition.normalized();
}

const QString kA = PanelContent::urlEntryId(QStringLiteral("file:///tmp/a.desktop"));
const QString kFolder = PanelContent::urlEntryId(QStringLiteral("file:///tmp/Folder/"));
const QString kKate = QStringLiteral("org.kde.kate");

}

class PanelContentTransactionTest final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalOrderCoversEveryEntryExactlyOnce();
    void nativePanelsRefuseEveryOperation();
    void addAppendsNewUrlsAndSkipsDuplicates();
    void removeDropsExactlyOneEntry();
    void moveBeforeReordersDeterministically();
    void setOrderRequiresAPermutation();
    void everyChangeSpendsExactlyOneRevision();
    void segmentsPartitionEntriesWithoutStealing();
    void segmentClaimsFailClosed();
    void contentOperationsPreserveSegmentOwnership();
};

void PanelContentTransactionTest::canonicalOrderCoversEveryEntryExactlyOnce()
{
    PanelContent content;
    content.applicationIds = {kKate, QStringLiteral(" "), kKate};
    content.urls = {
        QStringLiteral("file:///tmp/a.desktop"),
        QStringLiteral("file:///tmp/Folder/"),
        QStringLiteral("file:///tmp/a.desktop"),
    };
    // A stored order may reference unknown ids, repeat ids, or omit some.
    content.entryOrder = {kFolder, QStringLiteral("bogus"), kFolder, kKate};

    QCOMPARE(content.canonicalEntryOrder(), QStringList({kFolder, kKate, kA}));
    QVERIFY(PanelContent::isUrlEntryId(kA));
    QVERIFY(!PanelContent::isUrlEntryId(kKate));
    QCOMPARE(PanelContent::urlFromEntryId(kFolder),
             QStringLiteral("file:///tmp/Folder/"));
    QVERIFY(PanelContent::urlFromEntryId(kKate).isEmpty());
    QVERIFY(PanelContent::urlEntryId(QString{}).isEmpty());

    // The legacy map carries the canonical order and a round trip is stable.
    PanelDefinition definition = freePanel();
    definition.content.entryOrder = {kFolder, QStringLiteral("bogus")};
    const QVariantMap flat = definition.toLegacyMap();
    QCOMPARE(flat.value(QStringLiteral("contentOrder")).toStringList(),
             QStringList({kFolder, kKate, kA}));
    const auto parsed = PanelDefinition::fromLegacyMap(flat);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->content.entryOrder, QStringList({kFolder, kKate, kA}));
    QCOMPARE(parsed->normalized().content.entryOrder, parsed->content.entryOrder);
}

void PanelContentTransactionTest::nativePanelsRefuseEveryOperation()
{
    const PanelDefinition native = PanelDefinition::defaults(
        QStringLiteral("bottom"),
        QStringLiteral("Bottom"),
        QStringLiteral("bottom"),
        true);
    for (PanelContentOperation operation : {
             PanelContentOperation::Add,
             PanelContentOperation::Remove,
             PanelContentOperation::MoveBefore,
             PanelContentOperation::SetOrder})
    {
        PanelContentRequest request;
        request.operation = operation;
        request.urls = {QStringLiteral("file:///tmp/a.desktop")};
        request.entryId = kA;
        PanelContentOutcome outcome;
        QVERIFY(!PanelContentTransaction::prepare(native, request, &outcome).has_value());
        QVERIFY(!outcome.success);
        QCOMPARE(outcome.errorCode, QStringLiteral("native-content-unsupported"));
    }
}

void PanelContentTransactionTest::addAppendsNewUrlsAndSkipsDuplicates()
{
    const PanelDefinition current = freePanel();
    PanelContentRequest request;
    request.operation = PanelContentOperation::Add;
    request.urls = {
        QStringLiteral("file:///tmp/a.desktop"),
        QStringLiteral("file:///tmp/new.txt"),
    };
    PanelContentOutcome outcome;
    const auto candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QVERIFY(outcome.success);
    QVERIFY(outcome.changed);
    const QString kNew = PanelContent::urlEntryId(QStringLiteral("file:///tmp/new.txt"));
    QCOMPARE(candidate->content.entryOrder, QStringList({kKate, kA, kFolder, kNew}));
    QCOMPARE(candidate->content.urls.size(), 3);
    QCOMPARE(candidate->content.urls.last(), QStringLiteral("file:///tmp/new.txt"));
    // The rest of the definition is untouched.
    QCOMPARE(candidate->content.type, current.content.type);
    QCOMPARE(candidate->layout, current.layout);

    // Only duplicates: valid, but nothing to persist.
    request.urls = {QStringLiteral("file:///tmp/a.desktop")};
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QVERIFY(outcome.success);
    QVERIFY(!outcome.changed);
    QCOMPARE(outcome.entryOrder, current.content.entryOrder);

    // An unparsable URL is a precise error.
    request.urls = {QString{}};
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QVERIFY(!outcome.success);
    QCOMPARE(outcome.errorCode, QStringLiteral("invalid-url"));
}

void PanelContentTransactionTest::removeDropsExactlyOneEntry()
{
    const PanelDefinition current = freePanel();
    PanelContentRequest request;
    request.operation = PanelContentOperation::Remove;
    request.entryId = kA;
    PanelContentOutcome outcome;
    auto candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->content.entryOrder, QStringList({kKate, kFolder}));
    QCOMPARE(candidate->content.urls, QStringList({QStringLiteral("file:///tmp/Folder/")}));
    QCOMPARE(candidate->content.applicationIds, current.content.applicationIds);

    request.entryId = kKate;
    candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->content.entryOrder, QStringList({kA, kFolder}));
    QVERIFY(candidate->content.applicationIds.isEmpty());
    QCOMPARE(candidate->content.urls, current.content.urls);

    request.entryId = QStringLiteral("free-url:file:///tmp/missing");
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QCOMPARE(outcome.errorCode, QStringLiteral("unknown-entry"));
}

void PanelContentTransactionTest::moveBeforeReordersDeterministically()
{
    const PanelDefinition current = freePanel();
    QCOMPARE(current.content.entryOrder, QStringList({kKate, kA, kFolder}));

    PanelContentRequest request;
    request.operation = PanelContentOperation::MoveBefore;
    request.entryId = kFolder;
    request.beforeEntryId = kKate;
    PanelContentOutcome outcome;
    auto candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->content.entryOrder, QStringList({kFolder, kKate, kA}));

    // Empty target moves to the end.
    request.entryId = kKate;
    request.beforeEntryId.clear();
    candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->content.entryOrder, QStringList({kA, kFolder, kKate}));

    // Moving an entry before itself, or to where it already is, changes nothing.
    request.entryId = kA;
    request.beforeEntryId = kA;
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QVERIFY(outcome.success);
    QVERIFY(!outcome.changed);
    request.entryId = kKate;
    request.beforeEntryId = kA;
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QVERIFY(outcome.success);
    QVERIFY(!outcome.changed);

    // Unknown ids on either side are refused.
    request.entryId = kA;
    request.beforeEntryId = QStringLiteral("org.kde.unknown");
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QCOMPARE(outcome.errorCode, QStringLiteral("unknown-entry"));
}

void PanelContentTransactionTest::setOrderRequiresAPermutation()
{
    const PanelDefinition current = freePanel();
    PanelContentRequest request;
    request.operation = PanelContentOperation::SetOrder;
    PanelContentOutcome outcome;

    request.order = {kFolder, kA, kKate};
    auto candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->content.entryOrder, request.order);

    for (const QStringList &invalid : {
             QStringList{kFolder, kA},
             QStringList{kFolder, kA, kKate, kKate},
             QStringList{kFolder, kA, QStringLiteral("org.kde.other")}})
    {
        request.order = invalid;
        QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
        QVERIFY(!outcome.success);
        QCOMPARE(outcome.errorCode, QStringLiteral("order-mismatch"));
    }

    request.order = current.content.entryOrder;
    QVERIFY(!PanelContentTransaction::prepare(current, request, &outcome).has_value());
    QVERIFY(outcome.success);
    QVERIFY(!outcome.changed);
}

void PanelContentTransactionTest::everyChangeSpendsExactlyOneRevision()
{
    const PanelDefinition current = freePanel();
    PanelContentRequest request;
    request.operation = PanelContentOperation::Remove;
    request.entryId = kFolder;
    PanelContentOutcome outcome;
    const auto candidate = PanelContentTransaction::prepare(current, request, &outcome);
    QVERIFY(candidate.has_value());
    QCOMPARE(candidate->settingsRevision, current.settingsRevision + 1);
    QCOMPARE(candidate->identity, current.identity);
    QCOMPARE(candidate->host, current.host);
    QCOMPARE(outcome.entryOrder, candidate->content.entryOrder);
    QCOMPARE(outcome.toVariantMap().value(QStringLiteral("changed")).toBool(), true);
}

void PanelContentTransactionTest::segmentsPartitionEntriesWithoutStealing()
{
    auto panel = freePanel();
    auto custom = panel.segments.first();
    custom.id = QStringLiteral("files");
    custom.source = QStringLiteral("custom");
    custom.entryIds = {kFolder};
    custom.order = 0;
    auto launcher = panel.segments.first();
    launcher.id = QStringLiteral("launchers");
    launcher.source = QStringLiteral("launcher");
    launcher.order = 1;
    auto tasks = launcher;
    tasks.id = QStringLiteral("tasks");
    tasks.source = QStringLiteral("tasks");
    tasks.order = 2;
    panel.segments = {tasks, custom, launcher};
    const QVariantList entries{
        QVariantMap{{"appId", kA}, {"pinned", true}, {"running", true}},
        QVariantMap{{"appId", kFolder}, {"pinned", true}},
        QVariantMap{{"appId", kKate}, {"running", true}},
    };
    QString error;
    const auto projected = PanelContentTransaction::segmentEntries(panel, entries, &error, true);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(projected.size(), 3);
    QCOMPARE(projected.at(0).toMap().value("appId").toString(), kFolder);
    QCOMPARE(projected.at(0).toMap().value("segmentId").toString(), QStringLiteral("files"));
    QCOMPARE(projected.at(1).toMap().value("appId").toString(), kA);
    QCOMPARE(projected.at(1).toMap().value("segmentId").toString(), QStringLiteral("launchers"));
    QCOMPARE(projected.at(2).toMap().value("appId").toString(), kKate);
    QCOMPARE(projected.at(2).toMap().value("segmentId").toString(), QStringLiteral("tasks"));
}

void PanelContentTransactionTest::segmentClaimsFailClosed()
{
    auto panel = freePanel();
    panel.segments.first().source = QStringLiteral("custom");
    panel.segments.first().entryIds = {kKate};
    const QVariantList entries{QVariantMap{{"appId", kA}, {"pinned", true}}};
    QString error;
    QVERIFY(PanelContentTransaction::segmentEntries(panel, entries, &error, true).isEmpty());
    QVERIFY(error.contains(QStringLiteral("foreign or unavailable")));
    // A window disappearing between snapshots does not transfer its claim.
    QVERIFY(PanelContentTransaction::segmentEntries(panel, entries, &error).isEmpty());
    QVERIFY(error.isEmpty());
    auto other = panel.segments.first();
    other.id = QStringLiteral("other");
    other.order = 1;
    panel.segments.append(other);
    QVERIFY(PanelContentTransaction::segmentEntries(panel, entries, &error, true).isEmpty());
    QVERIFY(!error.isEmpty());
    panel.segments = {ArchDock::PanelSegmentDefinition{}};
    other.entryIds.clear();
    other.source = QStringLiteral("inherited");
    panel.segments.append(other);
    QVERIFY(PanelContentTransaction::segmentEntries(panel, entries, &error, true).isEmpty());
    QVERIFY(error.contains(QStringLiteral("one automatic")));
    panel.segments = {ArchDock::PanelSegmentDefinition{}};
    panel.segments.first().source = QStringLiteral("launcher");
    panel.segments.first().entryIds = {kA};
    other.source = QStringLiteral("tasks");
    panel.segments.append(other);
    const QVariantList unpinned{QVariantMap{{"appId", kA}, {"running", true}, {"pinned", false}}};
    QVERIFY(PanelContentTransaction::segmentEntries(panel, unpinned, &error).isEmpty());
    QVERIFY(error.isEmpty());
    panel.segments = {ArchDock::PanelSegmentDefinition{}};
    panel.segments.first().source = QStringLiteral("status");
    QVERIFY(PanelContentTransaction::segmentEntries(panel, entries, &error, true).isEmpty());
    QVERIFY(error.contains(QStringLiteral("no segment status provider")));
}

void PanelContentTransactionTest::contentOperationsPreserveSegmentOwnership()
{
    auto panel = freePanel();
    auto custom = panel.segments.first();
    custom.id = QStringLiteral("folder");
    custom.source = QStringLiteral("custom");
    custom.order = 1;
    custom.entryIds = {kFolder};
    panel.segments.append(custom);
    const auto before = panel.toPersistedMap();
    PanelContentRequest request;
    request.operation = PanelContentOperation::MoveBefore;
    request.entryId = kFolder;
    request.beforeEntryId = kA;
    PanelContentOutcome outcome;
    QVERIFY(!PanelContentTransaction::prepare(panel, request, &outcome));
    QCOMPARE(outcome.errorCode, QStringLiteral("cross-segment-move"));
    request.operation = PanelContentOperation::SetOrder;
    request.order = {kFolder, kA, kKate};
    QVERIFY(!PanelContentTransaction::prepare(panel, request, &outcome));
    QCOMPARE(outcome.errorCode, QStringLiteral("cross-segment-move"));
    QCOMPARE(panel.toPersistedMap(), before);
    request.operation = PanelContentOperation::Remove;
    const auto removed = PanelContentTransaction::prepare(panel, request, &outcome);
    QVERIFY(removed);
    QVERIFY(removed->segments.last().entryIds.isEmpty());
    QCOMPARE(removed->segments.first(), panel.segments.first());
}

QTEST_GUILESS_MAIN(PanelContentTransactionTest)

#include "PanelContentTransactionTest.moc"
