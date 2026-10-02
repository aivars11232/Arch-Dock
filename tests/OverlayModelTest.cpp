#include "content/OverlayModel.h"

#include <QSignalSpy>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <KService>
#include <QTest>
#include <limits>

using ArchDock::OverlayModel;

class OverlayModelTest final : public QObject
{
    Q_OBJECT
private slots:
    void idleExpiryStopsWithoutLosingSourceRecovery()
    {
        OverlayModel model;
        auto *expiry = model.findChild<QTimer *>(QStringLiteral("overlay-expiry"));
        QVERIFY(expiry);
        QVERIFY(!expiry->isActive());
        model.setPublishing(false);
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"urgent", true}}, 0));
        QVERIFY(expiry->isActive());
        model.removeSource(":1.42");
        QVERIFY(!expiry->isActive());
        model.setTemporaryStatus("sample.desktop", "Launch request failed");
        QVERIFY(expiry->isActive());
        model.expire(OverlayModel::MaximumAgeMs);
        QVERIFY(!expiry->isActive());
        QVERIFY(model.snapshot("sample.desktop").value("temporaryStatus").toString().isEmpty());
        QVERIFY(model.ingest(":1.43", "sample.desktop", {{"urgent", true}}, 10));
        QVERIFY(expiry->isActive());
        model.expire(OverlayModel::MaximumAgeMs + 10);
        QVERIFY(!expiry->isActive());
        QVERIFY(!model.available());
        model.setPublishing(true);
        QVERIFY(model.ingest(":1.44", "sample.desktop", {{"urgent", true}}, 20));
        QVERIFY(expiry->isActive());
        QVERIFY(model.snapshot("sample.desktop").value("urgent").toBool());
    }

    void supportedDbusSourceUpdatesAndDisconnects()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_OVERLAY_SOURCE_TEST"))
            QSKIP("Run by the disposable overlay-source-smoke D-Bus test");
        QTemporaryDir root;
        QVERIFY(root.isValid());
        qputenv("XDG_DATA_HOME", (root.path() + "/data").toUtf8());
        qputenv("XDG_CONFIG_HOME", (root.path() + "/config").toUtf8());
        qputenv("XDG_CACHE_HOME", (root.path() + "/cache").toUtf8());
        QVERIFY(QDir().mkpath(root.path() + "/data/applications"));
        QFile desktop(root.path() + "/data/applications/org.archdock.overlayfixture.desktop");
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write("[Desktop Entry]\nType=Application\nName=Overlay fixture\nExec=/usr/bin/true\nIcon=applications-system\n");
        desktop.close();
        QVERIFY(KService::serviceByStorageId(QStringLiteral("org.archdock.overlayfixture.desktop")));
        OverlayModel model;
        QSignalSpy changes(&model, &OverlayModel::changed);
        {
            auto sender = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("overlay-fixture"));
            QVERIFY(sender.isConnected());
            for (int i = 0; i < 100; ++i) {
                auto signal = QDBusMessage::createSignal(QStringLiteral("/Overlay"),
                    QStringLiteral("com.canonical.Unity.LauncherEntry"), QStringLiteral("Update"));
                signal << QStringLiteral("application://org.archdock.overlayfixture.desktop")
                       << QVariantMap{{"count", qlonglong(i)}, {"count-visible", true},
                                      {"progress", i / 100.0}, {"progress-visible", true}, {"urgent", true}};
                QVERIFY(sender.send(signal));
            }
            QTRY_COMPARE(model.snapshot("org.archdock.overlayfixture.desktop").value("badgeText").toString(), "99");
            QCOMPARE(model.snapshot("org.archdock.overlayfixture.desktop").value("progress").toDouble(), 0.99);
            QVERIFY(model.snapshot("org.archdock.overlayfixture.desktop").value("urgent").toBool());
            QTRY_VERIFY(!changes.isEmpty());
            QVERIFY(changes.count() < 10);
            QDBusConnection::disconnectFromBus(QStringLiteral("overlay-fixture"));
        }
        QTRY_VERIFY(!model.snapshot("org.archdock.overlayfixture.desktop").value("overlayAvailable").toBool());
    }
    void partialUpdatesAndSourceLoss()
    {
        OverlayModel model;
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"count", 12LL},
            {"count-visible", true}, {"progress", 0.4}, {"progress-visible", true}}, 0));
        QCOMPARE(model.snapshot("sample.desktop").value("badgeText").toString(), "12");
        QCOMPARE(model.snapshot("sample.desktop").value("progress").toDouble(), 0.4);
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"urgent", true}}, 10));
        QVERIFY(model.snapshot("sample.desktop").value("urgent").toBool());
        QCOMPARE(model.snapshot("sample.desktop").value("badgeText").toString(), "12");
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"count-visible", false}}, 20));
        QVERIFY(model.snapshot("sample.desktop").value("badgeText").toString().isEmpty());
        model.removeSource(":1.42");
        QVERIFY(!model.available());
        QCOMPARE(model.snapshot("sample.desktop").value("progress").toDouble(), -1.0);
    }
    void rejectsMalformedDataWithoutChangingGoodState()
    {
        OverlayModel model;
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"count", 3}, {"count-visible", true}}, 0));
        for (const QVariantMap &values : {QVariantMap{{"count", -1}},
             QVariantMap{{"count", "8"}}, QVariantMap{{"count-visible", 1}},
             QVariantMap{{"progress", 1.1}}, QVariantMap{{"progress", -0.1}},
             QVariantMap{{"progress", std::numeric_limits<double>::quiet_NaN()}},
             QVariantMap{{"count", 9}, {"urgent", "true"}}})
            QVERIFY(!model.ingest(":1.42", "sample.desktop", values, 10));
        QVERIFY(!model.ingest(":1.42", "../sample.desktop", {{"count", 9}}, 10));
        QCOMPARE(model.snapshot("sample.desktop").value("badgeText").toString(), "3");
        QVERIFY(!model.snapshot("other.desktop").value("overlayAvailable").toBool());
    }
    void ambiguousAndExpiredSourcesAreUnavailable()
    {
        OverlayModel model;
        QVERIFY(model.ingest(":1.42", "sample.desktop", {{"urgent", true}}, 0));
        QVERIFY(model.ingest(":1.43", "sample.desktop", {{"urgent", false}}, 10));
        QVERIFY(!model.snapshot("sample.desktop").value("overlayAvailable").toBool());
        model.removeSource(":1.43");
        QVERIFY(model.snapshot("sample.desktop").value("urgent").toBool());
        model.expire(OverlayModel::MaximumAgeMs);
        QVERIFY(!model.snapshot("sample.desktop").value("overlayAvailable").toBool());
        model.setTemporaryStatus("sample.desktop", "Launch request failed");
        QVERIFY(!model.snapshot("sample.desktop").value("temporaryStatus").toString().isEmpty());
        model.expire(OverlayModel::MaximumAgeMs);
        QVERIFY(model.snapshot("sample.desktop").value("temporaryStatus").toString().isEmpty());
    }
    void hiddenUpdatesCoalesceAndRevealPublishesOnce()
    {
        OverlayModel model;
        QSignalSpy changes(&model, &OverlayModel::changed);
        model.setPublishing(false);
        for (int i = 0; i < 100; ++i)
            QVERIFY(model.ingest(":1.42", "sample.desktop", {{"count", i}, {"count-visible", true}}, i));
        QTest::qWait(130);
        QCOMPARE(changes.count(), 0);
        model.setPublishing(true);
        QTRY_COMPARE(changes.count(), 1);
        QCOMPARE(model.snapshot("sample.desktop").value("badgeText").toString(), "99");
    }
};

QTEST_GUILESS_MAIN(OverlayModelTest)
#include "OverlayModelTest.moc"
