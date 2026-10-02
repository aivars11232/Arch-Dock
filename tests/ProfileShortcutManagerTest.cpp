#include "integration/ProfileShortcutManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

using namespace ArchDock;
namespace
{
struct Native
{
    bool available = true;
    bool failReadback = false;
    int failedRemovals = 0;
    int assignments = 0;
    QMap<QString, QString> keys;
    QMap<QString, std::function<void()>> callbacks;
    QMap<QString, QString> external;
    ProfileShortcutManager::NativeOperations operations()
    {
        ProfileShortcutManager::NativeOperations result;
        result.available = [this] { return available; };
        result.keys = [this](const QString &id) { return keys.contains(id) ? QStringList{keys[id]} : QStringList{}; };
        result.conflicts = [this](const QString &id, const QString &key) {
            QVariantList conflicts;
            if (external.contains(key)) conflicts.append(QVariantMap{{"name", external[key]}, {"component", "kwin"}});
            for (auto it = keys.begin(); it != keys.end(); ++it)
                if (it.key() != id && it.value() == key) conflicts.append(QVariantMap{{"name", it.key()}});
            return conflicts;
        };
        result.assign = [this](const QString &id, const QString &, const QString &key, std::function<void()> callback, QString *) {
            ++assignments; callbacks[id] = std::move(callback);
            keys[id] = failReadback ? QString("Meta+F12") : key;
            failReadback = false;
            return true;
        };
        result.remove = [this](const QString &id, QString *error) {
            if (failedRemovals > 0) { --failedRemovals; *error = "unregister-refused"; return false; }
            keys.remove(id); callbacks.remove(id); return true;
        };
        return result;
    }
};
struct Fixture
{
    QTemporaryDir directory;
    Native native;
    QMap<QString, ProfileDefinition> profiles;
    QString applied;
    int revision = 0;
    int applyCount = 0;
    Fixture()
    {
        for (const QString &id : {QString("profile-work"), QString("profile-home")})
        {
            auto profile = ProfileDefinition::capture(id, {PanelDefinition::defaults("bottom", "Bottom", "bottom", true)});
            profile.id = id; profiles[id] = profile;
        }
    }
    auto lookup() { return [this](const QString &id) -> std::optional<ProfileDefinition> {
        return profiles.contains(id) ? std::optional(profiles[id]) : std::nullopt;
    }; }
    auto apply() { return [this](const QString &id, int rev) {
        ++applyCount; applied = id; revision = rev;
        return QVariantMap{{"success", true}, {"revision", rev}};
    }; }
    QString path() const { return directory.filePath("profile-shortcuts.json"); }
};
bool success(const QVariantMap &result) { return result.value("success").toBool(); }
}
class ProfileShortcutManagerTest final : public QObject
{
    Q_OBJECT
private slots:
    void defaultOffAndExplicitEnablePersistWithoutApplying()
    {
        Fixture f;
        {
            ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
            QVERIFY(!manager.status().value("enabled").toBool());
            QVERIFY(!QFileInfo::exists(f.path()));
            QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
            QCOMPARE(f.native.assignments, 0);
            QVERIFY(success(manager.setEnabled(true)));
            QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
            QCOMPARE(f.applyCount, 0);
            QCOMPARE(QFileInfo(f.path()).permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther), QFile::Permissions{});
        }
        QVERIFY(f.native.keys.isEmpty());
        ProfileShortcutManager restarted(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(restarted.status().value("enabled").toBool());
        QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
        QCOMPARE(f.applyCount, 0);
    }
    void nativeAndLocalConflictsLeaveExistingShortcutsIntact()
    {
        Fixture f;
        f.native.external["Meta+D"] = "Show Desktop";
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QVERIFY(success(manager.setEnabled(true)));
        const auto conflict = manager.setBinding("profile-home", "Meta+D");
        QCOMPARE(conflict.value("errorCode").toString(), QString("profile-shortcut-conflict"));
        QVERIFY(!conflict.value("conflicts").toList().isEmpty());
        QVERIFY(!success(manager.setBinding("profile-home", "Ctrl+Alt+F9")));
        QCOMPARE(f.native.external.value("Meta+D"), QString("Show Desktop"));
        QCOMPARE(f.native.keys.size(), 1);
        QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
    }
    void activationResolvesCurrentRevisionAndStableIdentity()
    {
        Fixture f;
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QVERIFY(success(manager.setEnabled(true)));
        f.profiles["profile-work"].name = "Renamed";
        f.profiles["profile-work"].revision = 7;
        manager.refreshProfiles();
        f.native.callbacks.value("profile-work")();
        QCOMPARE(f.applied, QString("profile-work"));
        QCOMPARE(f.revision, 7);
        QVERIFY(manager.status().value("lastActivation").toMap().value("success").toBool());
        const auto oldCallback = f.native.callbacks.value("profile-work");
        QVERIFY(success(manager.setEnabled(false)));
        QVERIFY(f.native.keys.isEmpty());
        Fixture fresh;
        ProfileShortcutManager incomplete(fresh.lookup(), fresh.apply(), fresh.path(), fresh.native.operations());
        QVERIFY(success(incomplete.setEnabled(true)));
        fresh.native.failReadback = true;
        fresh.native.failedRemovals = 1;
        QCOMPARE(incomplete.setBinding("profile-home", "Ctrl+Alt+F10").value("errorCode").toString(), QString("profile-shortcut-cleanup-incomplete"));
        const auto pending = incomplete.status().value("bindings").toList();
        QCOMPARE(pending.size(), 1);
        QCOMPARE(pending.first().toMap().value("profileId").toString(), QString("profile-home"));
        QVERIFY(!pending.first().toMap().value("registeredKeys").toStringList().isEmpty());
        QVERIFY(success(incomplete.clearBinding("profile-home")));
        QVERIFY(fresh.native.keys.isEmpty());
        oldCallback();
        QCOMPARE(f.applyCount, 1);
        QVERIFY(success(manager.setEnabled(true)));
        QVERIFY(success(manager.clearBinding("profile-work")));
        QVERIFY(f.native.keys.isEmpty());
        f.profiles.remove("profile-work");
        oldCallback();
        QCOMPARE(f.applyCount, 1);
    }
    void invalidProfilesAndSequencesNeverRegister()
    {
        Fixture f;
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setEnabled(true)));
        for (const QString &id : {QString("missing"), QString("../profile-work"), QString("profile-work\n")})
            QVERIFY(!success(manager.setBinding(id, "Ctrl+Alt+F9")));
        for (const QString &key : {QString{}, QString("not a key"), QString("Ctrl+K, Ctrl+C")})
            QVERIFY(!success(manager.setBinding("profile-work", key)));
        QCOMPARE(f.native.assignments, 0);
        QVERIFY(f.native.keys.isEmpty());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        f.profiles.remove("profile-work");
        f.native.callbacks.value("profile-work")();
        QCOMPARE(f.applyCount, 0);
        manager.refreshProfiles();
        QVERIFY(f.native.keys.isEmpty());
        QVERIFY(manager.status().value("bindings").toList().isEmpty());
    }
    void failedNativeReadbackRestoresThePreviousBinding()
    {
        Fixture f;
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QVERIFY(success(manager.setEnabled(true)));
        f.native.failReadback = true;
        QVERIFY(!success(manager.setBinding("profile-work", "Ctrl+Alt+F10")));
        QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
        f.native.failedRemovals = 1;
        QVERIFY(!success(manager.clearBinding("profile-work")));
        QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
        QVERIFY(success(manager.clearBinding("profile-work")));
    }
    void incompleteCleanupExposesTheRemainingNativeKeyAndAllowsRetry()
    {
        Fixture f;
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QVERIFY(success(manager.setEnabled(true)));
        const auto callback = f.native.callbacks.value("profile-work");
        f.native.failedRemovals = 2;
        QCOMPARE(manager.setEnabled(false).value("errorCode").toString(), QString("profile-shortcut-cleanup-incomplete"));
        QVERIFY(!manager.status().value("enabled").toBool());
        QCOMPARE(manager.status().value("bindings").toList().first().toMap().value("registeredKeys").toStringList(), QStringList{"Ctrl+Alt+F9"});
        callback(); QCOMPARE(f.applyCount, 0);
        QVERIFY(success(manager.setEnabled(false)));
        QVERIFY(f.native.keys.isEmpty());
    }
    void configurationWriteFailureRestoresNativeAssignments()
    {
        Fixture f;
        const auto root = f.directory.filePath("configuration");
        ProfileShortcutManager manager(f.lookup(), f.apply(), root + "/shortcuts.json", f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QVERIFY(success(manager.setEnabled(true)));
        QVERIFY(QDir().rename(root, root + "-backup"));
        QFile blocker(root); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        QCOMPARE(manager.setBinding("profile-work", "Ctrl+Alt+F10").value("errorCode").toString(), QString("profile-shortcut-save-failed"));
        QCOMPARE(f.native.keys.value("profile-work"), QString("Ctrl+Alt+F9"));
        QVERIFY(manager.status().value("enabled").toBool());
        Fixture disabled; disabled.native.available = false;
        QFile parent(disabled.directory.filePath("blocked")); QVERIFY(parent.open(QIODevice::WriteOnly)); parent.close();
        ProfileShortcutManager offline(disabled.lookup(), disabled.apply(), parent.fileName() + "/shortcuts.json", disabled.native.operations());
        QCOMPARE(offline.setBinding("profile-work", "Ctrl+Alt+F9").value("errorCode").toString(), QString("profile-shortcut-save-failed"));
        QVERIFY(disabled.native.keys.isEmpty());
    }
    void unavailableServiceAndCorruptConfigurationAreVisible()
    {
        Fixture f; f.native.available = false;
        ProfileShortcutManager manager(f.lookup(), f.apply(), f.path(), f.native.operations());
        QVERIFY(success(manager.setBinding("profile-work", "Ctrl+Alt+F9")));
        QCOMPARE(manager.setEnabled(true).value("errorCode").toString(), QString("profile-shortcuts-service-unavailable"));
        QCOMPARE(f.native.assignments, 0);
        QFile file(f.path()); QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray future("{\"version\":99,\"enabled\":true,\"bindings\":{}}");
        QCOMPARE(file.write(future), future.size()); file.close();
        ProfileShortcutManager invalid(f.lookup(), f.apply(), f.path(), f.native.operations());
        QCOMPARE(invalid.status().value("errorCode").toString(), QString("invalid-profile-shortcut-file"));
        QVERIFY(!invalid.status().value("enabled").toBool());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), future);
    }
};
QTEST_GUILESS_MAIN(ProfileShortcutManagerTest)
#include "ProfileShortcutManagerTest.moc"
