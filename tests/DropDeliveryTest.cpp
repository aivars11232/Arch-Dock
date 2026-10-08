#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include "../src/panel/PanelWindowHelpers.h"

#include <optional>

class DropService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "local.PanelWindow")
public:
    bool accepts = false;
    bool recoveryActive = false;
    bool dropDuringRecovery = false;
    bool guardRecovery = false;
    int revision = 0;
public slots:
    bool addPanelEntries(const QString &panel, const QStringList &urls)
    {
        dropDuringRecovery = recoveryActive;
        const bool committed = accepts && panel == "free-test" && !urls.isEmpty();
        if (committed) ++revision;
        return committed;
    }
    bool beginRecovery(const QString &client)
    {
        QDBusInterface shell(client, "/DropProbe", "org.kde.PlasmaShell",
            QDBusConnection::sessionBus());
        shell.setTimeout(5000);
        recoveryActive = true;
        const int capturedRevision = revision;
        const std::function<bool()> current = guardRecovery
            ? std::function<bool()>([this, capturedRevision] { return revision == capturedRevision; })
            : std::function<bool()>{};
        bool superseded = false;
        QDBusReply<QString> result;
        try {
            result = PanelWindowHelpers::callPlasmaScript(shell, "recovery-probe", true, current);
        } catch (const PanelWindowHelpers::PlasmaScriptSuperseded &) {
            superseded = true;
        }
        recoveryActive = false;
        if (guardRecovery)
            return superseded && revision != capturedRevision && dropDuringRecovery;
        return result.isValid() && result.value() == "ARCHDOCK_RESULT:1" && dropDuringRecovery;
    }
};

class DropReplyProbe : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.PlasmaShell")
public:
    bool querySeen = false;
    QDBusMessage request;
    bool finishRecovery()
    { return QDBusConnection::sessionBus().send(request.createReply(QVariantList{QString("ARCHDOCK_RESULT:1")})); }
public slots:
    QString evaluateScript(const QString &script)
    {
        if (script != "recovery-probe") return {};
        // Plasma cannot answer the geometry query until synchronous drop
        // delivery returns. Keep that real D-Bus reply pending through delivery.
        setDelayedReply(true);
        request = message();
        querySeen = true;
        return {};
    }
};

class DropDeliveryTest : public QObject
{
    Q_OBJECT
private slots:
    void delivery_data()
    {
        QTest::addColumn<QString>("serviceMode");
        QTest::addColumn<bool>("editMode");
        QTest::addColumn<bool>("acceptDrops");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("committed") << QString("success") << false << true << true;
        QTest::newRow("backend-refused") << QString("failure") << false << true << false;
        QTest::newRow("service-absent") << QString() << false << true << false;
        QTest::newRow("edit-mode") << QString("success") << true << true << false;
        QTest::newRow("disabled") << QString("success") << false << false << false;
        QTest::newRow("drop-during-background-recovery") << QString("recovery") << false << true << true;
        QTest::newRow("drop-supersedes-background-recovery-intent") << QString("superseded") << false << true << true;
    }

    void delivery()
    {
        QFETCH(QString, serviceMode);
        QFETCH(bool, editMode);
        QFETCH(bool, acceptDrops);
        QFETCH(bool, accepted);
        const bool backgroundRecovery = serviceMode == "recovery" || serviceMode == "superseded";
        QProcess service;
        if (!serviceMode.isEmpty()) {
            service.start(QCoreApplication::applicationFilePath(), {"--drop-server", serviceMode});
            QVERIFY(service.waitForStarted());
            QVERIFY(service.waitForReadyRead());
            QCOMPARE(service.readLine().trimmed(), QByteArray("READY"));
        }
        // Every row owns its own engine/window and texture lifetime.
        QQuickView view;
        view.engine()->addImportPath(QStringLiteral(ARCHDOCK_RENDERING_IMPORT_ROOT));
        QQmlComponent component(view.engine());
        const QByteArray source = QByteArray(R"(
import QtQuick
import ArchDock.Integration 1.0
import ")") + QByteArray(ARCHDOCK_DOCK_UI_URL) + QByteArray(R"(" as DockUi
DockUi.DockEntry {
    entry: ({ appId: "target", displayName: "Target", iconName: "folder" })
    entryIndex: 0; vertical: false; baseSize: 60
    magnification: 1; magnificationEnabled: false; hoveredIndex: -1
    tileShape: "rounded"; appearance: "glass"
    showReflection: false; showIndicator: false; showTooltip: false
    motion: "none"; motionTrigger: "hover"; motionIntensity: 1
    motionDuration: 0; reducedMotion: true; inputEnabled: true
    editMode: false; acceptDrops: true; openIconProperties: function() {}
    invoke: function() {}; reorder: function() { return false }
    pinUrls: function(urls) {
        const values=[]; for (const url of urls) values.push(url.toString())
        return backend.addUrls("free-test", true, values)
    }
    setHoveredIndex: function() {}; openPanelStudio: function() {}
    DropBackend { id: backend }
}
)");
        component.setData(source, QUrl("file:///drop-delivery.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(component.create());
        QVERIFY(item);
        DropReplyProbe probe;
        if (backgroundRecovery) {
            QVERIFY(QDBusConnection::sessionBus().registerObject(
                "/DropProbe", &probe, QDBusConnection::ExportAllSlots));
        }
        item->setProperty("editMode", editMode);
        item->setProperty("acceptDrops", acceptDrops);
        view.setContent(QUrl(), &component, item);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(folder.path())});
        QDragEnterEvent enter(QPoint(30, 30), Qt::CopyAction, &mime,
                              Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &enter);
        std::optional<QDBusPendingCall> recovery;
        if (backgroundRecovery) {
            auto request = QDBusMessage::createMethodCall("org.archdock.ArchDock",
                "/Control", "local.PanelWindow", "beginRecovery");
            request << QDBusConnection::sessionBus().baseService();
            recovery = QDBusConnection::sessionBus().asyncCall(request);
            QTRY_VERIFY(probe.querySeen);
        }
        QDropEvent drop(QPointF(30, 30), Qt::CopyAction, &mime,
                        Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &drop);
        QCOMPARE(drop.isAccepted(), accepted);
        if (backgroundRecovery) {
            QVERIFY(probe.finishRecovery());
            QTRY_VERIFY(recovery->isFinished());
            const QDBusPendingReply<bool> result(*recovery);
            QVERIFY(result.isValid());
            QVERIFY(result.value());
            QDBusConnection::sessionBus().unregisterObject("/DropProbe");
        }
        QTest::mouseMove(&view, QPoint(-20, -20));
        QTRY_VERIFY(item->property("iconVisualState").toString() != "drop");
        if (editMode) QCOMPARE(item->property("iconVisualState").toString(), QString("edit"));
        if (!serviceMode.isEmpty()) {
            service.terminate();
            QVERIFY(service.waitForFinished());
        }
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (app.arguments().value(1) == "--drop-server") {
        DropService service;
        service.accepts = app.arguments().value(2) == "success" ||
            app.arguments().value(2) == "recovery" || app.arguments().value(2) == "superseded";
        service.guardRecovery = app.arguments().value(2) == "superseded";
        auto bus = QDBusConnection::sessionBus();
        if (!bus.registerService("org.archdock.ArchDock") ||
            !bus.registerObject("/Control", &service, QDBusConnection::ExportAllSlots))
            return 2;
        QTextStream(stdout) << "READY" << Qt::endl;
        return app.exec();
    }
    DropDeliveryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "DropDeliveryTest.moc"
