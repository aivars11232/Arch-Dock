#include "persistence/ConfigurationBackup.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace ArchDock;
namespace {
bool write(const QString &path, const QByteArray &bytes) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray read(const QString &path) {
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
ConfigurationBackup fixture(const QTemporaryDir &dir) {
    return ConfigurationBackup(dir.filePath("backups"),
        {{"settings", {dir.filePath("config/arch.conf"), false}},
         {"presets", {dir.filePath("presets"), true}}});
}
}
class ConfigurationBackupTest final : public QObject {
    Q_OBJECT
private slots:
    void completeUserDataRestoresAndExcludesTemporaryContent() {
        QTemporaryDir dir;
        const auto config = dir.filePath("config/arch.conf");
        const auto preset = dir.filePath("presets/panels/user-one.json");
        QVERIFY(write(config, "legacy-original\n"));
        QVERIFY(QFile::setPermissions(config, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup));
        const auto permissions = QFileInfo(config).permissions();
        QVERIFY(write(preset, "{\"version\":1}"));
        QVERIFY(write(dir.filePath("presets/defaults.json"), "{\"panel\":\"user-one\"}"));
        QVERIFY(write(dir.filePath("presets/assets/source.svgz"), "compressed-vector-source"));
        QVERIFY(write(dir.filePath("presets/.temporary.json"), "temporary"));
        QVERIFY(write(dir.filePath("presets/untrusted.sh"), "exit 1"));
        QVERIFY(write(dir.filePath("installed/builtin.json"), "built-in"));
        auto service = fixture(dir); QString error;
        const auto id = service.capture("legacy-upgrade", &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const auto manifest = read(dir.filePath("backups/" + id + "/manifest.json"));
        QVERIFY(manifest.contains("user-one.json")); QVERIFY(manifest.contains("defaults.json"));
        QVERIFY(!manifest.contains("temporary")); QVERIFY(!manifest.contains("untrusted"));
        QVERIFY(!manifest.contains("builtin"));
        QVERIFY(!(QFileInfo(dir.filePath("backups/" + id + "/files/settings/settings.conf")).permissions() & QFile::ReadOther));
        QVERIFY(write(config, "new-invalid")); QVERIFY(QFile::remove(preset));
        QVERIFY(QFile::remove(dir.filePath("presets/assets/source.svgz")));
        QVERIFY(write(dir.filePath("presets/icons/user-new.json"), "new"));
        QVERIFY2(service.restore(id, &error), qPrintable(error));
        QCOMPARE(read(config), QByteArray("legacy-original\n"));
        QCOMPARE(QFileInfo(config).permissions(), permissions);
        QCOMPARE(read(preset), QByteArray("{\"version\":1}"));
        QCOMPARE(read(dir.filePath("presets/assets/source.svgz")), QByteArray("compressed-vector-source"));
        QVERIFY(!QFileInfo::exists(dir.filePath("presets/icons/user-new.json")));
        QCOMPARE(read(dir.filePath("installed/builtin.json")), QByteArray("built-in"));
        QCOMPARE(read(dir.filePath("presets/.temporary.json")), QByteArray("temporary"));
        QVERIFY(!service.recoveryPending());
    }
    void failedCaptureLeavesOriginalAndNoPartialBackup() {
        QTemporaryDir dir; const auto path = dir.filePath("config/arch.conf");
        QVERIFY(write(path, "original")); auto service = fixture(dir); QString error;
        service.setCheckpoint([](const QString &point) { return point != "capture-manifest"; });
        QVERIFY(service.capture("upgrade", &error).isEmpty());
        QCOMPARE(read(path), QByteArray("original")); QVERIFY(service.backups().isEmpty());
        QCOMPARE(QDir(dir.filePath("backups")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
    }
    void partialRestoreRollsBackExactFiles() {
        QTemporaryDir dir;
        QVERIFY(write(dir.filePath("config/arch.conf"), "old"));
        QVERIFY(write(dir.filePath("presets/defaults.json"), "old-default"));
        auto service = fixture(dir); QString error;
        const auto id = service.capture("before-upgrade", &error); QVERIFY(!id.isEmpty());
        QVERIFY(write(dir.filePath("config/arch.conf"), "current"));
        QVERIFY(write(dir.filePath("presets/defaults.json"), "current-default"));
        int writes = 0;
        service.setCheckpoint([&](const QString &point) { return point != "restore-file" || ++writes != 2; });
        QVERIFY(!service.restore(id, &error)); QCOMPARE(writes, 2);
        QCOMPARE(read(dir.filePath("config/arch.conf")), QByteArray("current"));
        QCOMPARE(read(dir.filePath("presets/defaults.json")), QByteArray("current-default"));
        QVERIFY(!service.recoveryPending());
    }
    void interruptedRestoreRecoversAndPinsPreviousConfiguration() {
        QTemporaryDir dir; const auto path = dir.filePath("config/arch.conf");
        QVERIFY(write(path, "old")); auto service = fixture(dir); QString error;
        const auto target = service.capture("target", &error); QVERIFY(!target.isEmpty());
        QVERIFY(write(path, "previous")); const auto before = service.capture("previous", &error);
        QVERIFY(!before.isEmpty()); QVERIFY(write(path, "partial"));
        QVERIFY(write(dir.filePath("backups/restore.json"), QJsonDocument(QJsonObject{
            {"version", 1}, {"before", before}, {"target", target}}).toJson()));
        QVERIFY(service.recoveryPending()); QVERIFY(service.capture("refused", &error).isEmpty());
        QVERIFY(service.prune(1, &error)); QCOMPARE(service.backups().size(), 2);
        QVERIFY2(service.recover(&error), qPrintable(error)); QCOMPARE(read(path), QByteArray("previous"));
        QVERIFY(!service.recoveryPending());
    }
    void retentionKeepsLatestValidAndRejectsTampering() {
        QTemporaryDir dir; const auto path = dir.filePath("config/arch.conf");
        QVERIFY(write(path, "original")); auto service = fixture(dir); QString error;
        QString latest;
        for (int i = 0; i < 4; ++i) { latest = service.capture("retention", &error); QVERIFY(!latest.isEmpty()); QTest::qWait(2); }
        QVERIFY(service.prune(0, &error)); QCOMPARE(service.backups(), QStringList{latest});
        QVERIFY(write(dir.filePath("backups/" + latest + "/files/settings/settings.conf"), "tampered"));
        QVERIFY(!service.restore(latest, &error)); QCOMPARE(error, QString("backup-integrity-failed"));
        QCOMPARE(read(path), QByteArray("original"));
        QVERIFY(!service.restore("../../elsewhere", &error)); QCOMPARE(error, QString("backup-invalid-id"));
    }
    void clockRollbackAndInvalidCopiesDoNotEvictNewestValidSnapshot() {
        QTemporaryDir dir; const auto path = dir.filePath("config/arch.conf");
        QVERIFY(write(path, "previous")); auto service = fixture(dir); QString error;
        const auto first = service.capture("previous", &error); QVERIFY(!first.isEmpty());
        const auto future = QString("20991231235959000-") + first.mid(18);
        QVERIFY(QDir().rename(dir.filePath("backups/" + first), dir.filePath("backups/" + future)));
        const auto manifestPath = dir.filePath("backups/" + future + "/manifest.json");
        auto manifest = QJsonDocument::fromJson(read(manifestPath)).object();
        manifest["id"] = future; QVERIFY(write(manifestPath, QJsonDocument(manifest).toJson()));
        QVERIFY(write(path, "newest"));
        const auto latest = service.capture("newest", &error); QVERIFY2(!latest.isEmpty(), qPrintable(error));
        const auto corrupt = service.capture("corrupt", &error); QVERIFY(!corrupt.isEmpty());
        QVERIFY(write(dir.filePath("backups/" + corrupt + "/files/settings/settings.conf"), "corrupt"));
        const auto incomplete = QString("20000101000000000-") + first.mid(18);
        QVERIFY(QDir().mkpath(dir.filePath("backups/" + incomplete)));
        QVERIFY2(service.prune(1, &error), qPrintable(error));
        QCOMPARE(service.backups(), QStringList{latest});
        QCOMPARE(QDir(dir.filePath("backups")).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{latest});
        QVERIFY(write(path, "changed")); QVERIFY2(service.restore(latest, &error), qPrintable(error));
        QCOMPARE(read(path), QByteArray("newest"));
    }
    void symlinkExecutableAndOversizeDataAreRefused() {
        QTemporaryDir dir; const auto path = dir.filePath("config/arch.conf");
        QVERIFY(write(path, "original")); auto service = fixture(dir); QString error;
        QVERIFY(write(dir.filePath("outside.json"), "private"));
        QVERIFY(QDir().mkpath(dir.filePath("presets")));
        QVERIFY(QFile::link(dir.filePath("outside.json"), dir.filePath("presets/link.json")));
        QVERIFY(service.capture("unsafe", &error).isEmpty()); QCOMPARE(error, QString("backup-symlink"));
        QVERIFY(QFile::remove(dir.filePath("presets/link.json")));
        QVERIFY(write(dir.filePath("presets/run.json"), "executable"));
        QVERIFY(QFile::setPermissions(dir.filePath("presets/run.json"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(service.capture("unsafe", &error).isEmpty()); QCOMPARE(error, QString("backup-executable-data"));
        QVERIFY(QFile::remove(dir.filePath("presets/run.json")));
        QFile large(dir.filePath("presets/large.png")); QVERIFY(large.open(QIODevice::WriteOnly));
        QVERIFY(large.resize(ConfigurationBackup::MaximumBytes + 1)); large.close();
        QVERIFY(service.capture("oversize", &error).isEmpty()); QCOMPARE(error, QString("backup-size-limit"));
        QCOMPARE(read(path), QByteArray("original"));
    }
};
QTEST_GUILESS_MAIN(ConfigurationBackupTest)
#include "ConfigurationBackupTest.moc"
