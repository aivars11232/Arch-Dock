#include "persistence/RemovedPanelStore.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::RemovedPanelStore;

// The kept settings of removed panels: newest first, a fixed number kept
// once each removal has gone through, a refused removal leaving the list as
// it was, an entry dropped once its panel is back, and nothing read through
// a link.
class RemovedPanelStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsNewestFirstWithinTheLimit();
    void aRefusedRemovalLeavesTheListAsItWas();
    void forgetsAnEntryBroughtBack();
    void refusesALinkAndReplacesADamagedFile();
};

void RemovedPanelStoreTest::keepsNewestFirstWithinTheLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RemovedPanelStore store(directory.filePath(QStringLiteral("removed-panels.json")));
    QVERIFY(store.entries().isEmpty());
    for (int index = 0; index < RemovedPanelStore::MaximumEntries + 3; ++index)
    {
        QVERIFY(!store.keep({{QStringLiteral("id"), QStringLiteral("free-%1").arg(index)},
                             {QStringLiteral("name"), QStringLiteral("Panel %1").arg(index)},
                             {QStringLiteral("iconSize"), 40 + index}},
                            QStringLiteral("its owner removed it")).isEmpty());
        // Each removal went through.
        QVERIFY(store.trim());
    }
    const auto entries = store.entries();
    QCOMPARE(entries.size(), RemovedPanelStore::MaximumEntries);
    QCOMPARE(entries.first().record.value(QStringLiteral("id")).toString(),
             QStringLiteral("free-%1").arg(RemovedPanelStore::MaximumEntries + 2));
    QCOMPARE(entries.first().record.value(QStringLiteral("iconSize")).toInt(),
             40 + RemovedPanelStore::MaximumEntries + 2);
    QCOMPARE(entries.first().cause, QStringLiteral("its owner removed it"));
    QVERIFY(entries.first().removedAt.isValid());
    QCOMPARE(entries.last().record.value(QStringLiteral("id")).toString(), QStringLiteral("free-3"));
    QVERIFY(store.keep({}, QStringLiteral("nothing")).isEmpty());
}

void RemovedPanelStoreTest::aRefusedRemovalLeavesTheListAsItWas()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RemovedPanelStore store(directory.filePath(QStringLiteral("removed-panels.json")));
    const auto keep = [&store](const QString &id)
    {
        return store.keep({{QStringLiteral("id"), id}}, QStringLiteral("its owner removed it"));
    };
    for (int index = 0; index < RemovedPanelStore::MaximumEntries; ++index)
        QVERIFY(!keep(QStringLiteral("free-%1").arg(index)).isEmpty());
    QVERIFY(store.trim());
    const auto before = store.entries();
    QCOMPARE(before.size(), RemovedPanelStore::MaximumEntries);

    // Kept while the list is full, then Plasma refuses the removal: the
    // oldest kept panel is still there.
    const QString refused = keep(QStringLiteral("free-refused"));
    QVERIFY(!refused.isEmpty());
    QCOMPARE(store.entries().size(), RemovedPanelStore::MaximumEntries + 1);
    QVERIFY(store.forget(refused));
    const auto after = store.entries();
    QCOMPARE(after.size(), before.size());
    for (qsizetype index = 0; index < before.size(); ++index)
        QCOMPARE(after.at(index).key, before.at(index).key);

    // Kept, and the removal goes through: only now does the oldest go.
    QVERIFY(!keep(QStringLiteral("free-removed")).isEmpty());
    QVERIFY(store.trim());
    const auto trimmed = store.entries();
    QCOMPARE(trimmed.size(), RemovedPanelStore::MaximumEntries);
    QCOMPARE(trimmed.first().record.value(QStringLiteral("id")).toString(), QStringLiteral("free-removed"));
    QCOMPARE(trimmed.last().key, before.at(RemovedPanelStore::MaximumEntries - 2).key);

    // Removals that never finish cannot grow the list without end.
    for (int index = 0; index < RemovedPanelStore::StoredEntriesLimit + 5; ++index)
        QVERIFY(!keep(QStringLiteral("free-stuck-%1").arg(index)).isEmpty());
    QCOMPARE(store.entries().size(), RemovedPanelStore::StoredEntriesLimit);
}

void RemovedPanelStoreTest::forgetsAnEntryBroughtBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    RemovedPanelStore store(directory.filePath(QStringLiteral("removed-panels.json")));
    const QString key = store.keep({{QStringLiteral("id"), QStringLiteral("free-1")}}, QStringLiteral("a"));
    QVERIFY(!key.isEmpty());
    QVERIFY(!store.keep({{QStringLiteral("id"), QStringLiteral("free-2")}}, QStringLiteral("b")).isEmpty());
    QCOMPARE(store.entries().last().key, key);
    QVERIFY(store.entry(key).has_value());
    QVERIFY(store.forget(key));
    QVERIFY(!store.entry(key).has_value());
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().first().record.value(QStringLiteral("id")).toString(), QStringLiteral("free-2"));
    // Forgetting what is not there changes nothing.
    QVERIFY(store.forget(key));
    QCOMPARE(store.entries().size(), 1);
}

void RemovedPanelStoreTest::refusesALinkAndReplacesADamagedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString elsewhere = directory.filePath(QStringLiteral("elsewhere.json"));
    {
        QFile file(elsewhere);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"([{"key":"k","removedAt":"2026-01-01T00:00:00Z","cause":"c","record":{"id":"free-9"}}])");
    }
    const QString linked = directory.filePath(QStringLiteral("linked.json"));
    QVERIFY(QFile::link(elsewhere, linked));
    QString error;
    QVERIFY(RemovedPanelStore(linked).entries(&error).isEmpty());
    QCOMPARE(error, QStringLiteral("removed-panels-file-unsafe"));

    const QString damaged = directory.filePath(QStringLiteral("damaged.json"));
    {
        QFile file(damaged);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not json");
    }
    RemovedPanelStore store(damaged);
    error.clear();
    QVERIFY(store.entries(&error).isEmpty());
    QCOMPARE(error, QStringLiteral("removed-panels-file-invalid"));
    QVERIFY(!store.keep({{QStringLiteral("id"), QStringLiteral("free-1")}}, QStringLiteral("a")).isEmpty());
    QCOMPARE(store.entries().size(), 1);

    // A link in the file's place is replaced, never written through.
    RemovedPanelStore linkedStore(linked);
    QVERIFY(!linkedStore.keep({{QStringLiteral("id"), QStringLiteral("free-3")}}, QStringLiteral("c")).isEmpty());
    QVERIFY(!QFileInfo(linked).isSymLink());
    QFile untouched(elsewhere);
    QVERIFY(untouched.open(QIODevice::ReadOnly));
    QVERIFY(untouched.readAll().contains("free-9"));
}

QTEST_GUILESS_MAIN(RemovedPanelStoreTest)
#include "RemovedPanelStoreTest.moc"
