#include <QDBusConnection>
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

class DropService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "local.PanelWindow")
public:
    bool accepts = false;
public slots:
    bool addPanelEntries(const QString &panel, const QStringList &urls)
    { return accepts && panel == "free-test" && !urls.isEmpty(); }
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
    }

    void delivery()
    {
        QFETCH(QString, serviceMode);
        QFETCH(bool, editMode);
        QFETCH(bool, acceptDrops);
        QFETCH(bool, accepted);
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
        QDropEvent drop(QPointF(30, 30), Qt::CopyAction, &mime,
                        Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &drop);
        QCOMPARE(drop.isAccepted(), accepted);
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
        service.accepts = app.arguments().value(2) == "success";
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
