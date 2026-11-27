#include "panel/FreeGeometrySettle.h"

#include <QTest>

class FreeGeometrySettleTest final : public QObject
{
    Q_OBJECT

private slots:
    // Three equal readings of the right size end the wait when nothing moves;
    // a move ends it only once Plasma shows the place asked for, or after the
    // widget has had its moment (a place Plasma keeps the panel from).
    void waitsForAMoveToLand()
    {
        using ArchDock::freeGeometrySettled;
        QVERIFY(!freeGeometrySettled(2, false, false, 1000));
        QVERIFY(freeGeometrySettled(3, false, false, 0));
        QVERIFY(!freeGeometrySettled(3, true, false, 75));
        QVERIFY(!freeGeometrySettled(3, true, false, ArchDock::kFreeMoveSettleMs - 1));
        QVERIFY(freeGeometrySettled(3, true, true, 75));
        QVERIFY(freeGeometrySettled(3, true, false, ArchDock::kFreeMoveSettleMs));
    }
};

QTEST_APPLESS_MAIN(FreeGeometrySettleTest)

#include "FreeGeometrySettleTest.moc"
