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

QTEST_GUILESS_MAIN(PanelContentTransactionTest)

#include "PanelContentTransactionTest.moc"
