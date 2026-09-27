#include "SystemStatus.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class SystemStatusTest final : public QObject
{
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &value)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
    }
private slots:
    void readingsRequireValidSourcesAndCpuDelta()
    {
        QTemporaryDir root;
        const auto proc = root.path() + "/proc";
        const auto sys = root.path() + "/sys";
        auto missing = SystemStatus::readSample(proc, sys, {}, false);
        for (const auto &value : missing.metrics)
            QVERIFY(!value.toMap().value("available").toBool());
        QVERIFY(write(proc + "/stat", "cpu 100 0 0 100 0 0 0 0 10 0\n"));
        QVERIFY(write(proc + "/meminfo", "MemTotal: 1000 kB\nMemAvailable: 250 kB\n"));
        QVERIFY(write(sys + "/class/power_supply/BAT0/type", "Battery"));
        QVERIFY(write(sys + "/class/power_supply/BAT0/capacity", "0"));
        QVERIFY(write(sys + "/class/drm/card0/device/gpu_busy_percent", "0"));
        auto first = SystemStatus::readSample(proc, sys, {}, false);
        QVERIFY(!first.metrics.value("cpu").toMap().value("available").toBool());
        QCOMPARE(first.metrics.value("memory").toMap().value("value").toInt(), 75);
        QVERIFY(first.metrics.value("battery").toMap().value("available").toBool());
        QVERIFY(first.metrics.value("gpu").toMap().value("available").toBool());
        QVERIFY(write(proc + "/stat", "cpu 150 0 0 150 0 0 0 0 50 0\n"));
        auto second = SystemStatus::readSample(proc, sys, first, false);
        QVERIFY(second.metrics.value("cpu").toMap().value("available").toBool());
        QCOMPARE(second.metrics.value("cpu").toMap().value("value").toInt(), 50);
        QVERIFY(write(proc + "/stat", "cpu 1 0 0 1\n"));
        QVERIFY(write(proc + "/meminfo", "MemTotal: 1000 kB\n"));
        QVERIFY(write(sys + "/class/power_supply/BAT0/capacity", "invalid"));
        QVERIFY(QFile::remove(sys + "/class/drm/card0/device/gpu_busy_percent"));
        auto invalid = SystemStatus::readSample(proc, sys, second, false);
        for (const auto &value : invalid.metrics)
            QVERIFY(!value.toMap().value("available").toBool());
        QVERIFY(write(sys + "/class/drm/card0/device/gpu_busy_percent", "42"));
        auto recovered = SystemStatus::readSample(proc, sys, invalid, false);
        QCOMPARE(recovered.metrics.value("gpu").toMap().value("value").toInt(), 42);
        QVERIFY(recovered.metrics.value("gpu").toMap().value("available").toBool());
    }
    void discoveryIsBoundedAndSamplingFollowsDemand()
    {
        SystemStatus status;
        QSignalSpy changes(&status, &SystemStatus::statusChanged);
        QTRY_VERIFY(status.sampleCount() >= 2);
        const auto discovered = status.sampleCount();
        QTest::qWait(2100);
        QCOMPARE(status.sampleCount(), discovered);
        status.setEnabled(true);
        QTRY_VERIFY(status.sampleCount() > discovered);
        QVERIFY(!changes.isEmpty());
        QVERIFY(!status.entries().isEmpty());
        status.setEnabled(false);
        QTest::qWait(150);
        const auto stopped = status.sampleCount();
        QTest::qWait(2100);
        QCOMPARE(status.sampleCount(), stopped);
    }
};
QTEST_GUILESS_MAIN(SystemStatusTest)
#include "SystemStatusTest.moc"
