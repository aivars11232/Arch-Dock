#include "panel/PanelContentTransaction.h"
#include "model/PanelIconCapacity.h"

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
    void statusEntriesRemainInTheirOwnSegment()
    {
        auto panel = freePanel();
        auto status = panel.segments.first();
        status.id = "system"; status.source = "status"; status.order = 1;
        status.entryIds = {"status:cpu"};
        panel.segments.append(status);
        const QVariantList entries{QVariantMap{{"appId", kA}, {"pinned", true}},
            QVariantMap{{"appId", "status:cpu"}, {"isStatus", true}},
            QVariantMap{{"appId", "status:memory"}, {"isStatus", true}}};
        QString error;
        const auto rows = PanelContentTransaction::segmentEntries(panel, entries, &error, true);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.first().toMap().value("segmentId").toString(), "main");
        QCOMPARE(rows.last().toMap().value("segmentId").toString(), "system");
        QCOMPARE(rows.last().toMap().value("appId").toString(), "status:cpu");
        panel.segments.last().entryIds.clear();
        QCOMPARE(PanelContentTransaction::segmentEntries(panel, entries, &error, true).size(), 3);
        const QVariantList forged{QVariantMap{{"appId", "status:cpu"}, {"pinned", true}}};
        QVERIFY(PanelContentTransaction::segmentEntries(panel, forged, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void canonicalOrderCoversEveryEntryExactlyOnce();
    void nativePanelsRefuseEveryOperation();
    void emptyEdgePanelsHoldTheirOwnEntries();
    void addAppendsNewUrlsAndSkipsDuplicates();
    void removeDropsExactlyOneEntry();
    void moveBeforeReordersDeterministically();
    void setOrderRequiresAPermutation();
    void everyChangeSpendsExactlyOneRevision();
    void segmentsPartitionEntriesWithoutStealing();
    void segmentClaimsFailClosed();
    void contentOperationsPreserveSegmentOwnership();
    void capacityFollowsTheShapeAndTheIconSize();
    void addStopsWhenTheShapeIsFull();
};

// A shape holds its length in slots of an icon and a 16 pixel gap; bigger
// icons fit fewer, a row has no limit.
void PanelContentTransactionTest::capacityFollowsTheShapeAndTheIconSize()
{
    using ArchDock::panelIconCapacity;
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 150, 52, 6), std::optional<int>(13));
    QCOMPARE(panelIconCapacity(QStringLiteral("ring"), 150, 84, 6), std::optional<int>(9));
    QCOMPARE(panelIconCapacity(QStringLiteral("hexagon"), 150, 52, 6), std::optional<int>(13));
    // An open arc holds one more than its slots: one at each end.
    QCOMPARE(panelIconCapacity(QStringLiteral("arc"), 150, 52, 6), std::optional<int>(6));
    // A star's outline, notches at 0.46 of the radius.
    QCOMPARE(panelIconCapacity(QStringLiteral("star"), 150, 52, 5), std::optional<int>(15));
    // A Möbius band holds its length in slots less the two gaps where its
    // loops cross, a little less round its tight loop ends: at radius 150,
    // 12 to a ring's 13; ten need a radius of 125.
    QCOMPARE(panelIconCapacity(QStringLiteral("mobius"), 150, 52, 6), std::optional<int>(12));
    QCOMPARE(panelIconCapacity(QStringLiteral("mobius"), 200, 52, 6), std::optional<int>(17));
    QCOMPARE(ArchDock::panelMinimumRadius(QStringLiteral("mobius"), 10, 52, 6).value(), 125);
    QCOMPARE(panelIconCapacity(QStringLiteral("mobius"), 125, 52, 6), std::optional<int>(10));
    // Seen tilted, half its track is left to its icons: half as many fit.
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 150, 52, 6, 1.0, 1.0, 0.5), std::optional<int>(6));
    QCOMPARE(ArchDock::panelMinimumRadius(QStringLiteral("circular"), 13, 52, 6, 1.0, 1.0, 0.5).value(), 282);
    // A 3D panel squeezed sideways holds fewer: its track is an ellipse of
    // radii 75 and 150, its icons as large as before.
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 150, 52, 6, 0.5, 1.0), std::optional<int>(10));
    QVERIFY(!panelIconCapacity(QStringLiteral("horizontal"), 150, 52, 6).has_value());
    QVERIFY(!panelIconCapacity(QStringLiteral("grid"), 150, 52, 6).has_value());
    // The least radius and the largest icon that still hold them.
    QCOMPARE(ArchDock::panelMinimumRadius(QStringLiteral("circular"), 13, 52, 6), std::optional<int>(141));
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 141, 52, 6), std::optional<int>(13));
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 140, 52, 6), std::optional<int>(12));
    QCOMPARE(ArchDock::panelMaximumIconSize(QStringLiteral("circular"), 13, 150, 6), std::optional<int>(56));
    QCOMPARE(panelIconCapacity(QStringLiteral("circular"), 150, 57, 6), std::optional<int>(12));
    QCOMPARE(ArchDock::panelMinimumRadius(QStringLiteral("arc"), 6, 52, 6), std::optional<int>(150));
    QVERIFY(!ArchDock::panelMinimumRadius(QStringLiteral("horizontal"), 40, 52, 6).has_value());
}

// Icons are added only while the panel's shape has room for them side by
// side; the refusal says how many fit.
void PanelContentTransactionTest::addStopsWhenTheShapeIsFull()
{
    PanelDefinition ring = freePanel();
    ring.layout.pathType = QStringLiteral("circular");
    ring.layout.radius = 60;
    ring.iconStyle.size = 52;
    QCOMPARE(ArchDock::panelIconCapacity(ring), std::optional<int>(5));
    QCOMPARE(ArchDock::panelIconCount(ring), 3);
    PanelContentRequest request;
    request.operation = PanelContentOperation::Add;
    request.urls = {QStringLiteral("file:///tmp/b.desktop"), QStringLiteral("file:///tmp/c.desktop")};
    PanelContentOutcome outcome;
    const auto filled = PanelContentTransaction::prepare(ring, request, &outcome);
    QVERIFY2(filled.has_value(), qPrintable(outcome.errorMessage));
    QCOMPARE(ArchDock::panelIconCount(*filled), 5);
    request.urls = {QStringLiteral("file:///tmp/d.desktop")};
    QVERIFY(!PanelContentTransaction::prepare(*filled, request, &outcome).has_value());
    QVERIFY(!outcome.success);
    QCOMPARE(outcome.errorCode, QStringLiteral("panel-full"));
    QVERIFY(outcome.errorMessage.contains(QStringLiteral("5 icons")));
    // A row grows with its icons.
    PanelDefinition row = *filled;
    row.layout.pathType = QStringLiteral("horizontal");
    QVERIFY(PanelContentTransaction::prepare(row, request, &outcome).has_value());

    // Squeezing a full 3D ring sideways leaves its five icons too little
    // room; widening it, or squeezing a panel that has room, does not.
    PanelDefinition flat = *filled;
    flat.surface.rendererTier = QStringLiteral("true3d");
    PanelDefinition squeezed = flat;
    squeezed.surface.parameters3D.insert(QStringLiteral("scaleX"), 0.6);
    const auto refusal = ArchDock::panelIconRoomRefusal(flat, squeezed);
    QVERIFY(refusal.has_value());
    QVERIFY2(refusal->contains(QStringLiteral("its 5 icons fit only")), qPrintable(*refusal));
    PanelDefinition wider = flat;
    wider.surface.parameters3D.insert(QStringLiteral("scaleX"), 1.5);
    QVERIFY(!ArchDock::panelIconRoomRefusal(flat, wider).has_value());
    // A panel already holding more than it has room for keeps its icons
    // while a change gives them no less room.
    QVERIFY(!ArchDock::panelIconRoomRefusal(squeezed, squeezed).has_value());
    PanelDefinition roomy = ring;
    roomy.surface.rendererTier = QStringLiteral("true3d");
    PanelDefinition roomySqueezed = roomy;
    roomySqueezed.surface.parameters3D.insert(QStringLiteral("scaleX"), 0.6);
    QVERIFY(!ArchDock::panelIconRoomRefusal(roomy, roomySqueezed).has_value());

    // A look with bigger icons grows the full ring to the least radius its
    // five icons fit at; one that leaves them room changes nothing.
    PanelDefinition bigger = *filled;
    bigger.iconStyle.size = 84;
    ArchDock::growPanelToFitIcons(*filled, bigger);
    QCOMPARE(bigger.layout.radius, ArchDock::panelMinimumRadius(QStringLiteral("circular"), 5, 84, 6).value());
    QVERIFY(!ArchDock::panelIconRoomRefusal(*filled, bigger).has_value());
    PanelDefinition smaller = *filled;
    smaller.iconStyle.size = 32;
    ArchDock::growPanelToFitIcons(*filled, smaller);
    QCOMPARE(smaller.layout.radius, 60);
}

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

// An Empty panel along an edge holds the folders, files and applications
// added to it, as a free panel does, and so does an Empty bar, beside its
// Plasma widgets.
void PanelContentTransactionTest::emptyEdgePanelsHoldTheirOwnEntries()
{
    PanelDefinition empty = PanelDefinition::defaults(
        QStringLiteral("panel-top"), QStringLiteral("Top"), QStringLiteral("top"), false);
    empty.content.type = QStringLiteral("empty");
    PanelContentRequest request;
    request.operation = PanelContentOperation::Add;
    request.urls = {QStringLiteral("file:///tmp/a.desktop"), QStringLiteral("file:///tmp/notes.txt")};
    PanelContentOutcome outcome;
    const auto candidate = PanelContentTransaction::prepare(empty, request, &outcome);
    QVERIFY(candidate.has_value());
    QVERIFY(outcome.success);
    QCOMPARE(candidate->content.canonicalEntryOrder().size(), 2);

    PanelDefinition bar = empty;
    bar.host.edgeHost = QStringLiteral("bar");
    PanelContentOutcome barOutcome;
    const auto barCandidate = PanelContentTransaction::prepare(bar, request, &barOutcome);
    QVERIFY(barCandidate.has_value());
    QVERIFY(barOutcome.success);
    QCOMPARE(barCandidate->content.canonicalEntryOrder().size(), 2);
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
