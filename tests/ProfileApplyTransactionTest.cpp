#include "panel/ProfileApplyTransaction.h"
#include <QFile>
#include <QJsonDocument>
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
            if (physical.value(key(panel)) == panel) return true;
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
