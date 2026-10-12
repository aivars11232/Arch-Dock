#include "panel/ProfileManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace ArchDock;
namespace
{
struct Registry
{
    QList<PanelDefinition> panels;
    int commits = 0;
    int creates = 0;
    int backups = 0;
    bool failBackup = false;
    QString guard;
    Registry()
    {
        auto panel = PanelDefinition::defaults("bottom", "Bottom", "bottom", true);
        panel.visibility.visible = false;
        panels = {panel.normalized()};
    }
    ProfileApplyTransaction::Operations operations()
    {
        ProfileApplyTransaction::Operations op;
        op.guard = [this] { return guard; };
        op.backupConfiguration = [this](QString *error) {
            if (failBackup) { if (error) *error = "backup-storage-unavailable"; return false; }
            ++backups; return true;
        };
        op.snapshot = [this](QString *) { return panels; };
        op.matches = [this](const auto &expected, QString *) { return panels == expected; };
        op.prepare = [](const auto &, auto &, QStringList *, QString *) { return true; };
        op.capture = [](auto &, QString *) { return true; };
        op.create = [this](auto &, const QString &, QString *) { ++creates; return false; };
        op.apply = [](const auto &, const auto &, QString *) { return true; };
        op.verify = [](const auto &, QString *) { return true; };
        op.restore = [](auto &, QString *) { return true; };
        op.remove = [](const auto &, QString *) { return true; };
        op.commit = [this](const auto &before, const auto &after, QString *) {
            if (before != panels) return false;
            ++commits; panels = after; return true;
        };
        op.publish = [] {};
        return op;
    }
};
bool success(const QVariantMap &result) { return result.value("success").toBool(); }
PanelDefinition freePanel(const QString &id, const QString &state, bool linked = false)
{
    static int applet = 70;
    auto panel = PanelDefinition::defaults(id, id, "free", false);
    panel.host.freeHostState = state;
    if (linked || state == "hosted-owned" || state == "hosted-stale") panel.host.freeOwnershipToken = "archdock-" + id;
    if (linked || state == "hosted-owned") { panel.host.freeDesktopContainmentId = 7; panel.host.freeDockAppletId = applet++; }
    return panel.normalized();
}
QStringList ids(const QList<PanelDefinition> &panels)
{
    QStringList result;
    for (const auto &panel : panels) result.append(panel.identity.id);
    return result;
}
QVariantMap first(const ProfileManager &manager) { return manager.listProfiles().first().toMap(); }
}
class ProfileManagerTest final : public QObject
{
    Q_OBJECT
private slots:
    void backupFailureRefusesPublicApplyBeforeMutation()
    {
        QTemporaryDir directory; Registry registry;
        ProfileManager manager(registry.operations(), directory.filePath("profiles"), directory.filePath("journal.json"));
        const auto created = manager.createProfile("Recoverable");
        QVERIFY(success(created)); QCOMPARE(registry.backups, 0);
        registry.failBackup = true;
        const auto result = manager.applyProfile(created.value("profileId").toString(), 1);
        QVERIFY(!success(result)); QCOMPARE(result.value("errorCode").toString(), QString("backup-storage-unavailable"));
        QCOMPARE(registry.commits, 0); QCOMPARE(registry.creates, 0); QVERIFY(!manager.active());
        registry.failBackup = false;
        QVERIFY(success(manager.applyProfile(created.value("profileId").toString(), 1)));
        QCOMPARE(registry.backups, 1); QCOMPARE(registry.commits, 1);
    }
    void nativeShortcutUsesTheTransactionAndDeletionIsVerified()
    {
        QTemporaryDir directory; Registry registry;
        QMap<QString, QString> keys;
        QMap<QString, std::function<void()>> callbacks;
        ProfileShortcutManager::NativeOperations native;
        native.available = [] { return true; };
        native.conflicts = [](const QString &, const QString &) { return QVariantList{}; };
        native.keys = [&keys](const QString &id) { return keys.contains(id) ? QStringList{keys[id]} : QStringList{}; };
        native.assign = [&](const QString &id, const QString &, const QString &key, std::function<void()> callback, QString *) {
            keys[id] = key; callbacks[id] = std::move(callback); return true;
        };
        native.remove = [&](const QString &id, QString *) { keys.remove(id); callbacks.remove(id); return true; };
        ProfileManager manager(registry.operations(), directory.filePath("profiles"),
            directory.filePath("journal.json"), nullptr, native);
        const auto created = manager.createProfile("Arrangement");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        QVERIFY(success(manager.setProfileShortcut(id, "Ctrl+Alt+F9")));
        QVERIFY(keys.isEmpty());
        QVERIFY(success(manager.setShortcutsEnabled(true)));
        QVERIFY(success(manager.renameProfile(id, 1, "Renamed")));
        const auto activate = callbacks.value(id);
        registry.guard = "popup-open";
        activate();
        QCOMPARE(registry.commits, 0);
        QVERIFY(!manager.shortcutStatus().value("lastActivation").toMap().value("success").toBool());
        QVERIFY(!success(manager.clearProfileShortcut(id)));
        registry.guard.clear();
        activate();
        QCOMPARE(registry.commits, 1);
        QCOMPARE(manager.status().value("profileId").toString(), id);
        QVERIFY(manager.shortcutStatus().value("lastActivation").toMap().value("success").toBool());
        QVERIFY(!success(manager.setProfileShortcut("missing", "Ctrl+Alt+F10")));
        QVERIFY(!success(manager.deleteProfile(id, 1)));
        QVERIFY(keys.contains(id));
        QVERIFY(success(manager.deleteProfile(id, 2)));
        QVERIFY(keys.isEmpty());
        QVERIFY(manager.shortcutStatus().value("bindings").toList().isEmpty());
        activate();
        QCOMPARE(registry.commits, 1);
        const auto last = manager.shortcutStatus().value("lastActivation").toMap();
        QVERIFY(last.value("success").toBool());
    }
    void actionsPersistAndSignalsIdentifyDeletion()
    {
        QTemporaryDir directory; Registry registry;
        const QString root = directory.filePath("profiles");
        const QString journal = directory.filePath("journal.json");
        ProfileManager manager(registry.operations(), root, journal);
        QSignalSpy deleted(&manager, &ProfileManager::profileDeleted);
        QVERIFY(manager.listProfiles().isEmpty());
        QVERIFY(!QFileInfo::exists(root));
        const auto created = manager.createProfile("Current arrangement");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        QVERIFY(success(manager.renameProfile(id, 1, "Renamed")));
        QVERIFY(!success(manager.saveProfile(id, 1)));
        registry.panels.first().iconStyle.size = 68;
        QVERIFY(success(manager.saveProfile(id, 2)));
        QCOMPARE(ProfileStore(root).load(id)->panels.first().iconStyle.size, 68);
        const auto duplicate = manager.duplicateProfile(id, 3, "Copy");
        QVERIFY(success(duplicate));
        QCOMPARE(manager.listProfiles().size(), 2);
        QVERIFY(!success(manager.deleteProfile(id, 2)));
        QVERIFY(success(manager.deleteProfile(id, 3)));
        QCOMPARE(deleted.size(), 1);
        QCOMPARE(deleted.first().first().toString(), id);
        ProfileManager restarted(registry.operations(), root, journal);
        QCOMPARE(restarted.listProfiles().size(), 1);
        QCOMPARE(registry.commits, 0);
        QCOMPARE(registry.creates, 0);
    }
    void onlyExplicitApplyCommitsThroughTheTransaction()
    {
        QTemporaryDir directory; Registry registry;
        ProfileManager manager(registry.operations(), directory.filePath("profiles"), directory.filePath("journal.json"));
        const auto created = manager.createProfile("Arrangement");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        QCOMPARE(registry.commits, 0);
        QVERIFY(!success(manager.applyProfile(id, 2)));
        QVariantMap result;
        QVERIFY(QMetaObject::invokeMethod(&manager, "applyProfile", Qt::DirectConnection,
            Q_RETURN_ARG(QVariantMap, result), Q_ARG(QString, id), Q_ARG(int, 1)));
        QVERIFY2(success(result), qPrintable(result.value("errorCode").toString()));
        QCOMPARE(registry.commits, 1);
        QVERIFY(!manager.active());
        QCOMPARE(manager.status().value("profileId").toString(), id);
        QCOMPARE(manager.metaObject()->classInfo(manager.metaObject()->indexOfClassInfo("D-Bus Interface")).value(), "org.archdock.Profiles");
    }
    void localInterchangeDoesNotApplyOrEnableAnything()
    {
        QTemporaryDir directory; Registry registry;
        ProfileManager manager(registry.operations(), directory.filePath("profiles"), directory.filePath("journal.json"));
        const auto created = manager.createProfile("Arrangement");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        const QString url = QUrl::fromLocalFile(directory.filePath("export.json")).toString();
        QVERIFY(success(manager.exportProfile(id, 1, url)));
        QVERIFY(success(manager.importProfile(url)));
        QCOMPARE(manager.listProfiles().size(), 2);
        QVERIFY(!success(manager.importProfile("https://example.invalid/profile.json")));
        QVERIFY(!success(manager.exportProfile(id, 1, "https://example.invalid/export.json")));
        QCOMPARE(registry.commits, 0);
        QCOMPARE(registry.creates, 0);
        QVERIFY(!manager.active());
    }
    // The owner (2026-10-12): a profile had captured 21 free panels whose
    // widgets were long gone, and one Apply created 13 of them again.
    void profilesHoldOnlyThePanelsOnTheDesktop()
    {
        QTemporaryDir directory; Registry registry;
        registry.panels.append(freePanel("free-1", "hosted-owned"));
        registry.panels.append(freePanel("free-2", "detached"));
        registry.panels.append(freePanel("free-3", "unhosted"));
        registry.panels.append(freePanel("free-4", "hosted-stale"));
        // A failed check leaves a linked panel stale; its widget is most
        // likely still on the desktop, so the profile keeps it.
        registry.panels.append(freePanel("free-6", "hosted-stale", true));
        const QString root = directory.filePath("profiles");
        ProfileManager manager(registry.operations(), root, directory.filePath("journal.json"));
        const auto created = manager.createProfile("Desk");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        QCOMPARE(ids(ProfileStore(root).load(id)->panels), QStringList({"bottom", "free-1", "free-6"}));
        QCOMPARE(first(manager).value("panelCount").toInt(), 3);
        // The hidden built-in bottom panel is kept but not shown.
        QCOMPARE(first(manager).value("shownPanelCount").toInt(), 2);
        QCOMPARE(first(manager).value("freePanelCount").toInt(), 2);
        QCOMPARE(first(manager).value("freePanelsToCreate").toInt(), 0);
        QCOMPARE(first(manager).value("freePanelLimit").toInt(), ProfileManager::MaximumFreePanelsCreatedPerApply);
        // The free panel leaves the desktop: Apply would create it again, and
        // saving the profile again drops it for the panel that is there now.
        registry.panels[1] = freePanel("free-1", "detached");
        registry.panels.append(freePanel("free-5", "hosted-owned"));
        QCOMPARE(first(manager).value("freePanelsToCreate").toInt(), 1);
        QVERIFY(success(manager.saveProfile(id, 1)));
        QCOMPARE(ids(ProfileStore(root).load(id)->panels), QStringList({"bottom", "free-6", "free-5"}));
        QCOMPARE(first(manager).value("freePanelsToCreate").toInt(), 0);
        QCOMPARE(registry.commits, 0); QCOMPARE(registry.creates, 0);
    }
    void applyRefusesToCreateMoreFreePanelsThanTheLimit()
    {
        QTemporaryDir directory; Registry registry;
        const int limit = ProfileManager::MaximumFreePanelsCreatedPerApply;
        for (int i = 1; i <= limit + 1; ++i) registry.panels.append(freePanel(QString("free-%1").arg(i), "hosted-owned"));
        ProfileManager manager(registry.operations(), directory.filePath("profiles"), directory.filePath("journal.json"));
        const auto created = manager.createProfile("Many");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        QCOMPARE(first(manager).value("panelCount").toInt(), limit + 2);
        for (int i = 1; i < registry.panels.size(); ++i) registry.panels[i] = freePanel(registry.panels[i].identity.id, "detached");
        QCOMPARE(first(manager).value("freePanelsToCreate").toInt(), limit + 1);
        const auto refused = manager.applyProfile(id, 1);
        QVERIFY(!success(refused));
        QCOMPARE(refused.value("errorCode").toString(), QString("profile-creates-too-many-free-panels"));
        QCOMPARE(refused.value("freePanelsToCreate").toInt(), limit + 1);
        QCOMPARE(refused.value("freePanelLimit").toInt(), limit);
        QCOMPARE(registry.creates, 0); QCOMPARE(registry.commits, 0); QVERIFY(!manager.active());
        // One panel back on the desktop: the apply is within the limit and
        // reaches the host creation (which this fixture refuses).
        registry.panels[1] = freePanel("free-1", "hosted-owned");
        QCOMPARE(first(manager).value("freePanelsToCreate").toInt(), limit);
        const auto attempted = manager.applyProfile(id, 1);
        QVERIFY(!success(attempted));
        QVERIFY(attempted.value("errorCode").toString() != refused.value("errorCode").toString());
        QCOMPARE(registry.creates, 1); QCOMPARE(registry.commits, 0); QVERIFY(!manager.active());
    }
    void guardsAndInvalidProfilesNeverApply()
    {
        QTemporaryDir directory; Registry registry;
        const QString root = directory.filePath("profiles");
        ProfileManager manager(registry.operations(), root, directory.filePath("journal.json"));
        const auto created = manager.createProfile("Arrangement");
        QVERIFY(success(created));
        const QString id = created.value("profileId").toString();
        registry.guard = "popup-open";
        QVERIFY(!success(manager.createProfile("Blocked")));
        QVERIFY(!success(manager.renameProfile(id, 1, "Blocked")));
        QVERIFY(!success(manager.applyProfile(id, 1)));
        registry.guard.clear();
        auto future = ProfileStore(root).load(id)->toVariantMap();
        future["schemaVersion"] = 99;
        const auto bytes = QJsonDocument::fromVariant(future).toJson();
        QFile file(root + "/" + id + ".json");
        QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size()); file.close();
        QVERIFY(!success(manager.applyProfile(id, 1)));
        QVERIFY(manager.listProfiles().isEmpty());
        QCOMPARE(registry.commits, 0);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), bytes);
    }
};
QTEST_GUILESS_MAIN(ProfileManagerTest)
#include "ProfileManagerTest.moc"
