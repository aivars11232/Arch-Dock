#include "panel/ProfileApplyTransaction.h"
#include "persistence/ConfigurationBackup.h"
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

using namespace ArchDock;
namespace
{
QString key(const PanelDefinition &panel) { return panel.identity.id + ":" + ProfileApplyTransaction::hostToken(panel); }
struct Hosts
{
    QList<PanelDefinition> registry;
    QMap<QString, PanelDefinition> physical;
    QStringList events;
    QString guard;
    QString failApply;
    QString failRestore;
    bool failCommit = false;
    bool concurrentEdit = false;
    int nextId = 100;
    Hosts()
    {
        auto native = PanelDefinition::defaults("bottom", "Bottom", "bottom", true);
        native.host.nativePanelId = 10; native.host.nativeDockAppletId = 11;
        native.host.nativeOwnershipToken = "owned-bottom"; native.host.nativeRecoveryState = "ready";
        native.settingsRevision = 7;
        auto free = PanelDefinition::defaults("free-old", "Free old", "free", false);
        free.host.freeDesktopContainmentId = 20; free.host.freeDockAppletId = 21;
        free.host.freeOwnershipToken = "owned-free"; free.host.freeHostState = "hosted-owned";
        free.settingsRevision = 4;
        registry = {native.normalized(), free.normalized()};
        for (const auto &panel : registry) physical.insert(key(panel), panel);
    }
    ProfileApplyTransaction::Operations operations()
    {
        ProfileApplyTransaction::Operations op;
        op.guard = [this] { return guard; };
        op.backupConfiguration = [this](QString *) { events.append("configuration-backup"); return true; };
        op.snapshot = [this](QString *) { return registry; };
        op.matches = [this](const auto &expected, QString *error) {
            if (registry == expected) return true;
            if (error) *error = "profile-registry-revision-conflict";
            return false;
        };
        op.prepare = [](const auto &, auto &, QStringList *, QString *) { return true; };
        op.capture = [this](ProfileHostSnapshot &snapshot, QString *error) {
            events.append("capture:" + snapshot.definition.identity.id);
            if (!physical.contains(key(snapshot.definition))) { if (error) *error = "host-not-owned"; return false; }
            snapshot.hostState = {{"position", snapshot.definition.placement.x}};
            return true;
        };
        op.create = [this](PanelDefinition &panel, const QString &token, QString *) {
            events.append("create:" + panel.identity.id);
            if (panel.host.kind == PanelHostKind::FreeDesktop)
            {
                panel.host.freeDesktopContainmentId = 20; panel.host.freeDockAppletId = nextId++;
                panel.host.freeOwnershipToken = token; panel.host.freeHostState = "hosted-owned";
            }
            else
            {
                panel.host.nativePanelId = nextId++; panel.host.nativeDockAppletId = nextId++;
                panel.host.nativeOwnershipToken = token; panel.host.nativeRecoveryState = "ready";
            }
            panel = panel.normalized(); physical.insert(key(panel), panel); return true;
        };
        op.apply = [this](const auto &, const PanelDefinition &panel, QString *error) {
            events.append("apply:" + panel.identity.id);
            physical.insert(key(panel), panel);
            if (concurrentEdit) { registry.first().settingsRevision++; registry.first().iconStyle.size = 77; concurrentEdit = false; }
            if (panel.identity.id == failApply) { if (error) *error = "injected-host-apply-failure"; return false; }
            return true;
        };
        op.verify = [this](const PanelDefinition &panel, QString *error) {
            events.append("verify:" + panel.identity.id);
            auto actual = physical.value(key(panel));
            // Native capture checks the host, not a registry-only revision.
            actual.settingsRevision = panel.settingsRevision;
            if (actual == panel) return true;
            if (error) *error = "host-readback-failed";
            return false;
        };
        op.restore = [this](ProfileHostSnapshot &snapshot, QString *error) {
            events.append("restore:" + snapshot.definition.identity.id);
            if (snapshot.definition.identity.id == failRestore) { if (error) *error = "injected-restore-failure"; return false; }
            if (!physical.contains(key(snapshot.definition)))
            {
                auto create = operations().create;
                if (!create(snapshot.definition, ProfileApplyTransaction::hostToken(snapshot.definition), error)) return false;
            }
            physical.insert(key(snapshot.definition), snapshot.definition); return true;
        };
        op.remove = [this](const PanelDefinition &panel, QString *) {
            events.append("remove:" + panel.identity.id);
            physical.remove(key(panel)); return true;
        };
        op.commit = [this](const auto &expected, const auto &candidate, QString *error) {
            events.append("commit");
            if (registry != expected || failCommit) { if (error) *error = "injected-registry-commit-failure"; return false; }
            registry = candidate; return true;
        };
        op.publish = [this] { events.append("publish"); };
        return op;
    }
    ProfileDefinition profile() const { return ProfileDefinition::capture("Arrangement", registry); }
};
bool success(const QVariantMap &result) { return result.value("success").toBool(); }
}

class ProfileApplyTransactionTest final : public QObject
{
    Q_OBJECT
private slots:
    void configurationBackupIsRequiredAndDurableBeforeHostMutation()
    {
        QTemporaryDir directory; Hosts hosts;
        const auto previous = hosts.registry, physical = hosts.physical.values();
        const QString path = directory.filePath("settings.conf");
        QFile config(path); QVERIFY(config.open(QIODevice::WriteOnly));
        QCOMPARE(config.write("original configuration"), qint64(22)); config.close();
        ConfigurationBackup backup(directory.filePath("backups"), {{"settings", {path, false}}});
        auto op = hosts.operations();
        op.backupConfiguration = [&](QString *error) { return !backup.capture("profile-apply", error).isEmpty(); };
        const auto apply = op.apply;
        op.apply = [&](const auto &before, const auto &after, QString *error) {
            if (backup.backups().size() != 1) { if (error) *error = "missing-configuration-backup"; return false; }
            return apply(before, after, error);
        };
        ProfileApplyTransaction transaction(op, directory.filePath("journal.json"));
        QVERIFY(success(transaction.apply(hosts.profile())));
        const auto ids = backup.backups(); QCOMPARE(ids.size(), 1);
        QFile snapshot(directory.filePath("backups/" + ids.first() + "/files/settings/settings.conf"));
        QVERIFY(snapshot.open(QIODevice::ReadOnly)); QCOMPARE(snapshot.readAll(), QByteArray("original configuration"));

        Hosts refused;
        const auto registryBefore = refused.registry, physicalBefore = refused.physical.values();
        auto failure = refused.operations();
        backup.setCheckpoint([](const QString &point) { return point != "capture-manifest"; });
        failure.backupConfiguration = [&](QString *error) { return !backup.capture("refused", error).isEmpty(); };
        const QString failedJournal = directory.filePath("failed-journal.json");
        ProfileApplyTransaction failed(failure, failedJournal);
        const auto result = failed.apply(refused.profile());
        QVERIFY(!success(result)); QCOMPARE(result.value("errorCode").toString(), QString("backup-manifest-failed"));
        QCOMPARE(refused.registry, registryBefore); QCOMPARE(refused.physical.values(), physicalBefore);
        QVERIFY(!QFileInfo::exists(failedJournal));
        QVERIFY(!QFileInfo::exists(failedJournal + ".backup.json"));
        QVERIFY(!refused.events.contains("apply:bottom"));
        auto missing = refused.operations(); missing.backupConfiguration = {};
        ProfileApplyTransaction absent(missing, directory.filePath("absent.json"));
        QCOMPARE(absent.apply(refused.profile()).value("errorCode").toString(), QString("profile-host-operations-unavailable"));
    }
    void freeDisplayMoveVerifiesReplacementBeforeRemoval()
    {
        QTemporaryDir directory; Hosts hosts;
        const auto previous = hosts.registry.last();
        auto profile = hosts.profile();
        profile.panels.last().host.screenIndex = 1;
        profile.panels.last().visibility.visible = true;
        ProfileApplyTransaction transaction(hosts.operations(), directory.filePath("journal.json"));
        const auto result = transaction.apply(profile);
        QVERIFY2(success(result), qPrintable(result.value("errorCode").toString()));
        QCOMPARE(hosts.registry.last().identity.id, previous.identity.id);
        QCOMPARE(hosts.registry.last().host.screenIndex, 1);
        QVERIFY(hosts.registry.last().host.freeOwnershipToken != previous.host.freeOwnershipToken);
        QVERIFY(hosts.events.indexOf("verify:free-old") < hosts.events.indexOf("remove:free-old"));
        QVERIFY(!hosts.physical.contains(key(previous)));
        QVERIFY(hosts.physical.contains(key(hosts.registry.last())));
    }
    void appliesOneVerifiedSetAndBacksUpBeforeMutation()
    {
        QTemporaryDir directory; Hosts hosts;
        const auto previous = hosts.registry;
        auto profile = hosts.profile();
        profile.panels.removeLast();
        profile.panels.append(PanelDefinition::defaults("panel-new", "New", "top", false).normalized());
        const QString path = directory.filePath("journal.json");
        auto op = hosts.operations();
        const auto create = op.create;
        op.create = [&](auto &panel, const auto &token, auto *error) {
            if (!QFileInfo::exists(path) || !QFileInfo::exists(path + ".backup.json"))
            { if (error) *error = "backup-not-durable-before-create"; return false; }
            return create(panel, token, error);
        };
        ProfileApplyTransaction transaction(op, path);
        const auto result = transaction.apply(profile);
        QVERIFY2(success(result), qPrintable(result.value("errorCode").toString()));
        QCOMPARE(hosts.events.count("commit"), 1);
        QCOMPARE(hosts.events.count("publish"), 1);
        QVERIFY(hosts.events.indexOf("verify:panel-new") < hosts.events.indexOf("remove:free-old"));
        QVERIFY(hosts.events.indexOf("remove:free-old") < hosts.events.indexOf("commit"));
        QCOMPARE(hosts.registry.size(), 2);
        QCOMPARE(hosts.registry.first().settingsRevision, previous.first().settingsRevision + 1);
        QCOMPARE(hosts.registry.last().settingsRevision, quint64(1));
        QVERIFY(!hosts.physical.contains(key(previous.last())));
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(QFileInfo::exists(path + ".backup.json"));
        QVERIFY(!transaction.active());
    }
    void failedHostApplyRestoresPreviousUsableConfiguration()
    {
        QTemporaryDir directory; Hosts hosts;
        const auto previous = hosts.registry; const auto physical = hosts.physical;
        hosts.failApply = "free-old";
        auto profile = hosts.profile();
        profile.panels.first().placement.x = 320;
        profile.panels.last().layout.pathType = "ring";
        profile.panels.last() = profile.panels.last().normalized();
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(hosts.operations(), path);
        const auto result = transaction.apply(profile);
        QVERIFY(!success(result));
        QCOMPARE(result.value("rollbackStatus").toString(), QString("complete"));
        QVERIFY(hosts.registry == previous);
        QVERIFY(hosts.physical == physical);
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(hosts.events.count("commit"), 0);
    }
    void failedCommitRemovesCandidatesAndRecreatesRemovedOwnedHosts()
    {
        QTemporaryDir directory; Hosts hosts;
        auto profile = hosts.profile(); profile.panels.removeLast();
        profile.panels.append(PanelDefinition::defaults("panel-new", "New", "top", false).normalized());
        auto op = hosts.operations();
        const auto commit = op.commit;
        bool first = true;
        op.commit = [&](const auto &before, const auto &after, auto *error) {
            if (first) { first = false; hosts.events.append("commit"); if (error) *error = "injected-commit-failure"; return false; }
            return commit(before, after, error);
        };
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(op, path);
        const auto result = transaction.apply(profile);
        QVERIFY(!success(result));
        QCOMPARE(result.value("rollbackStatus").toString(), QString("complete"));
        QCOMPARE(hosts.registry.last().identity.id, QString("free-old"));
        QVERIFY(hosts.registry.last().host.freeDockAppletId != 21);
        QCOMPARE(hosts.physical.size(), 2);
        QVERIFY(!QFileInfo::exists(path));
    }
    void partialRollbackRetainsPreciseRecordAndExplicitRecoveryWorks()
    {
        QTemporaryDir directory; Hosts hosts;
        hosts.failApply = "free-old"; hosts.failRestore = "bottom";
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(hosts.operations(), path);
        const auto result = transaction.apply(hosts.profile());
        QVERIFY(!success(result));
        QCOMPARE(result.value("state").toString(), QString("BLOCKED"));
        QVERIFY(!result.value("rollbackErrors").toStringList().isEmpty());
        QVERIFY(QFileInfo::exists(path));
        const int events = hosts.events.size();
        ProfileApplyTransaction restarted(hosts.operations(), path);
        QCOMPARE(hosts.events.size(), events); // Loading does not recover or apply.
        QVERIFY(restarted.active());
        QVERIFY(!success(restarted.apply(hosts.profile())));
        hosts.failApply.clear(); hosts.failRestore.clear();
        const auto recovered = restarted.recover();
        QVERIFY2(success(recovered), qPrintable(recovered.value("errorCode").toString()));
        QVERIFY(!restarted.active());
        QVERIFY(!QFileInfo::exists(path));
    }
    void restoredCommitRecovery_data()
    {
        QTest::addColumn<bool>("interrupt");
        QTest::addColumn<QString>("conflict");
        QTest::newRow("interruption") << true << QString();
        QTest::newRow("journal-cleanup-failure") << false << QString();
        QTest::newRow("external-revision") << true << QString("revision");
        QTest::newRow("uncertain-ownership") << true << QString("ownership");
    }
    void restoredCommitRecovery()
    {
        QFETCH(bool, interrupt); QFETCH(QString, conflict);
        QTemporaryDir directory; Hosts hosts;
        const auto before = hosts.registry;
        const QString path = directory.filePath("journal.json");
        const QString registryPath = directory.filePath("registry.json");
        auto profile = hosts.profile(); profile.panels.removeLast();
        profile.panels.append(PanelDefinition::defaults("panel-new", "New", "top", false).normalized());
        auto op = hosts.operations(); const auto commit = op.commit;
        int commits = 0;
        bool durable = false;
        struct Interrupted {};
        op.commit = [&](const auto &expected, const auto &candidate, QString *error) {
            if (++commits == 1) { if (error) *error = "injected-apply-commit-failure"; return false; }
            if (!commit(expected, candidate, error)) return false;
            QVariantList values;
            for (const auto &panel : hosts.registry) values.append(panel.toPersistedMap());
            const auto bytes = QJsonDocument::fromVariant(values).toJson();
            QSaveFile file(registryPath);
            durable = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
            if (interrupt) throw Interrupted{}; // Exact post-commit, pre-clear boundary.
            return durable;
        };
        op.publish = [&] {
            hosts.events.append("publish");
            QFile::setPermissions(directory.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        };
        ProfileApplyTransaction transaction(op, path);
        if (interrupt)
        {
            bool interrupted = false;
            try { (void)transaction.apply(profile); } catch (const Interrupted &) { interrupted = true; }
            QVERIFY(interrupted);
        }
        else
        {
            const auto result = transaction.apply(profile);
            QVERIFY(!success(result));
            QCOMPARE(result.value("state").toString(), QString("BLOCKED"));
            QVERIFY(result.value("rollbackErrors").toStringList().contains("profile-journal-clear-failed"));
            QVERIFY(QFile::setPermissions(directory.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        }
        QVERIFY(durable); QVERIFY(QFileInfo::exists(path));
        QFile saved(registryPath); QVERIFY(saved.open(QIODevice::ReadOnly));
        hosts.registry.clear();
        for (const auto &value : QJsonDocument::fromJson(saved.readAll()).toVariant().toList())
        {
            const auto panel = PanelDefinition::fromLegacyMap(value.toMap());
            QVERIFY(panel); hosts.registry.append(*panel);
        }
        QCOMPARE(hosts.registry.last().identity.id, QString("free-old"));
        QVERIFY(hosts.registry.last().host.freeDockAppletId != before.last().host.freeDockAppletId);
        QCOMPARE(hosts.physical.size(), 2);
        const auto restored = hosts.registry;
        if (conflict == "revision") hosts.registry.first().settingsRevision++;
        if (conflict == "ownership") hosts.physical.remove(key(hosts.registry.last()));
        const auto protectedRegistry = hosts.registry, protectedHosts = hosts.physical.values();
        const int eventCount = hosts.events.size();
        ProfileApplyTransaction restarted(hosts.operations(), path);
        QCOMPARE(hosts.events.size(), eventCount);
        const auto result = restarted.recover();
        if (!conflict.isEmpty())
        {
            QVERIFY(!success(result)); QVERIFY(QFileInfo::exists(path));
            QCOMPARE(hosts.registry, protectedRegistry); QCOMPARE(hosts.physical.values(), protectedHosts);
        }
        else
        {
            QVERIFY2(success(result), qPrintable(result.value("errorCode").toString()));
            QCOMPARE(hosts.registry, restored); QCOMPARE(hosts.physical.size(), 2);
            QVERIFY(!QFileInfo::exists(path)); QVERIFY(!restarted.active());
            const auto events = hosts.events;
            QVERIFY(success(restarted.recover())); QCOMPARE(hosts.events, events);
            ProfileApplyTransaction again(hosts.operations(), path);
            QVERIFY(success(again.recover())); QCOMPARE(hosts.events, events);
        }
        QCOMPARE(hosts.events.count("restore:free-old"), 1);
        QCOMPARE(commits, 2);
    }
    void concurrentChangesAreNeverOverwrittenByRollback()
    {
        QTemporaryDir directory; Hosts hosts; hosts.concurrentEdit = true;
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(hosts.operations(), path);
        const auto result = transaction.apply(hosts.profile());
        QVERIFY(!success(result));
        QCOMPARE(result.value("state").toString(), QString("BLOCKED"));
        QCOMPARE(hosts.registry.first().iconStyle.size, 77);
        QCOMPARE(hosts.registry.first().settingsRevision, quint64(8));
        QCOMPARE(hosts.events.count("commit"), 0);
        QVERIFY(!hosts.events.contains("restore:bottom"));
        QVERIFY(QFileInfo::exists(path));
    }
    void restoredCommitFailureRecoversWithoutRestoringHostsAgain()
    {
        QTemporaryDir directory; Hosts hosts;
        auto profile = hosts.profile(); profile.panels.removeLast();
        auto op = hosts.operations();
        op.commit = [](const auto &, const auto &, QString *error) {
            if (error) *error = "injected-commit-failure"; return false;
        };
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(op, path);
        QVERIFY(!success(transaction.apply(profile)));
        QFile journal(path); QVERIFY(journal.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(journal.readAll()).toVariant().toMap().value("state").toString(),
            QString("ROLLBACK_COMMITTING")); journal.close();
        const auto physical = hosts.physical;
        const int restorations = hosts.events.count("restore:free-old");
        ProfileApplyTransaction restarted(hosts.operations(), path);
        const auto result = restarted.recover();
        QVERIFY2(success(result), qPrintable(result.value("errorCode").toString()));
        QCOMPARE(hosts.physical, physical);
        QCOMPARE(hosts.events.count("restore:free-old"), restorations);
        QCOMPARE(hosts.registry.last().host.freeDockAppletId, physical.value(key(hosts.registry.last())).host.freeDockAppletId);
        QVERIFY(!QFileInfo::exists(path));
    }
    void futureRecoveryRecordIsRetainedWithoutHostOperations()
    {
        QTemporaryDir directory; Hosts hosts; hosts.failApply = "free-old"; hosts.failRestore = "bottom";
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(hosts.operations(), path);
        QVERIFY(!success(transaction.apply(hosts.profile())));
        QFile journal(path); QVERIFY(journal.open(QIODevice::ReadOnly));
        auto record = QJsonDocument::fromJson(journal.readAll()).toVariant().toMap(); journal.close();
        record["version"] = 99;
        const auto bytes = QJsonDocument::fromVariant(record).toJson();
        QVERIFY(journal.open(QIODevice::WriteOnly)); QCOMPARE(journal.write(bytes), bytes.size()); journal.close();
        const auto events = hosts.events;
        ProfileApplyTransaction restarted(hosts.operations(), path);
        QVERIFY(!success(restarted.recover())); QCOMPARE(hosts.events, events);
        QVERIFY(journal.open(QIODevice::ReadOnly)); QCOMPARE(journal.readAll(), bytes);
    }
    void guardsAndUnownedHostsFailBeforeMutation()
    {
        QTemporaryDir directory; Hosts hosts; hosts.guard = "edit-mode-active";
        const QString path = directory.filePath("journal.json");
        ProfileApplyTransaction transaction(hosts.operations(), path);
        QVERIFY(!success(transaction.apply(hosts.profile())));
        QVERIFY(hosts.events.isEmpty());
        hosts.guard.clear(); hosts.physical.remove(key(hosts.registry.last()));
        QVERIFY(!success(transaction.apply(hosts.profile())));
        QVERIFY(!hosts.events.contains("commit"));
        QVERIFY(!hosts.events.contains("apply:bottom"));
        QVERIFY(!QFileInfo::exists(path));
    }
    void invalidRecoveryRecordIsRetainedWithoutHostOperations()
    {
        QTemporaryDir directory; Hosts hosts;
        const QString path = directory.filePath("journal.json");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{"); file.close();
        ProfileApplyTransaction transaction(hosts.operations(), path);
        QVERIFY(transaction.active());
        QVERIFY(!success(transaction.recover()));
        QVERIFY(hosts.events.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("{"));
    }
};
QTEST_GUILESS_MAIN(ProfileApplyTransactionTest)
#include "ProfileApplyTransactionTest.moc"
