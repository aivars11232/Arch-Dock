#include "WindowModel.h"
#include "content/WindowPreviewModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

class WindowPreviewModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void individualStateAndTitleFallback()
    {
        WindowItem first;
        first.internalId = QStringLiteral("first");
        first.caption = QStringLiteral("Document one");
        first.active = true;
        first.canActivate = true;
        first.canClose = true;
        WindowItem second;
        second.internalId = QStringLiteral("second");
        second.caption = QStringLiteral("  ");
        second.minimized = true;
        second.canMinimize = true;

        const auto rows = ArchDock::WindowPreviewModel::entries(
            {first, second, WindowItem{}}, QStringLiteral("Editor"));
        QCOMPARE(rows.size(), 2);
        const auto a = rows.at(0).toMap();
        const auto b = rows.at(1).toMap();
        QCOMPARE(a.value("windowId").toString(), QStringLiteral("first"));
        QCOMPARE(a.value("title").toString(), QStringLiteral("Document one"));
        QVERIFY(a.value("active").toBool());
        QVERIFY(a.value("canActivate").toBool());
        QVERIFY(a.value("canClose").toBool());
        QVERIFY(!a.value("canMinimize").toBool());
        QCOMPARE(b.value("windowId").toString(), QStringLiteral("second"));
        QCOMPARE(b.value("title").toString(), QStringLiteral("Editor"));
        QVERIFY(b.value("minimized").toBool());
        QVERIFY(!b.value("active").toBool());
        QVERIFY(!b.value("canActivate").toBool());
        QVERIFY(!b.value("canClose").toBool());
        QVERIFY(b.value("canMinimize").toBool());
        QVERIFY(!ArchDock::WindowPreviewModel::entries({second}, {})
                     .first().toMap().value("title").toString().isEmpty());
    }

    void followsUpdatesAndRemovalWithoutKeepingStaleWindows()
    {
        WindowModel source;
        QAbstractItemModelTester tester(
            &source, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy changed(&source, &QAbstractItemModel::dataChanged);
        WindowItem window;
        window.internalId = QStringLiteral("one");
        window.caption = QStringLiteral("Original");
        source.addWindow(window);
        source.addWindow(window);
        QCOMPARE(source.rowCount(), 1);
        window.caption = QStringLiteral("Renamed");
        window.active = true;
        window.canActivate = true;
        QVERIFY(source.updateWindow(window));
        QCOMPARE(changed.count(), 1);
        auto rows = ArchDock::WindowPreviewModel::entries(source.windows(), {});
        QCOMPARE(rows.first().toMap().value("title").toString(), QStringLiteral("Renamed"));
        QVERIFY(rows.first().toMap().value("active").toBool());
        QVERIFY(source.data(source.index(0), WindowModel::CanActivateRole).toBool());

        window.canClose = true;
        window.canMinimize = true;
        QVERIFY(source.updateWindow(window));
        QCOMPARE(changed.count(), 2);
        QVERIFY(source.data(source.index(0), WindowModel::CanCloseRole).toBool());
        QVERIFY(source.data(source.index(0), WindowModel::CanMinimizeRole).toBool());
        QVERIFY(source.updateWindow(window));
        QCOMPARE(changed.count(), 2);
        QVERIFY(source.removeWindow(window.internalId));
        QVERIFY(ArchDock::WindowPreviewModel::entries(source.windows(), {}).isEmpty());
        QVERIFY(!source.updateWindow(window));
        QVERIFY(!source.removeWindow(window.internalId));
    }
};

QTEST_GUILESS_MAIN(WindowPreviewModelTest)
#include "WindowPreviewModelTest.moc"
