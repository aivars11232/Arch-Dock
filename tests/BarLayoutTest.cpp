// SPDX-License-Identifier: GPL-3.0-or-later
#include "bars/BarLayout.h"

#include <QtTest>

using ArchDock::Bars::BarPlacement;
using ArchDock::Bars::BarRequest;
using ArchDock::Bars::placeOnEdge;

class BarLayoutTest : public QObject
{
    Q_OBJECT

private:
    static BarPlacement find(const QList<BarPlacement> &placed, const QString &id)
    {
        for (const BarPlacement &placement : placed)
            if (placement.id == id)
                return placement;
        return {};
    }

    static BarRequest bar(const char *id, const char *alignment, int order, int length, int offset = 0)
    {
        return {QString::fromLatin1(id), QString::fromLatin1(alignment), order, length, offset};
    }

private slots:
    void aMiddleBarIsCentred()
    {
        const auto placed = placeOnEdge({bar("dock", "center", 0, 600)}, 1920);
        QCOMPARE(find(placed, QStringLiteral("dock")), (BarPlacement{QStringLiteral("dock"), 660, 600}));
    }

    void barsStandSideBySideFromTheirEnds()
    {
        // Two at the start, in their order whatever the list's, and one at
        // the end: side by side, a gap apart.
        const auto placed = placeOnEdge({bar("b", "start", 1, 300), bar("a", "start", 0, 200),
                                         bar("z", "end", 0, 400)}, 1920);
        QCOMPARE(find(placed, QStringLiteral("a")), (BarPlacement{QStringLiteral("a"), 0, 200}));
        QCOMPARE(find(placed, QStringLiteral("b")), (BarPlacement{QStringLiteral("b"), 208, 300}));
        QCOMPARE(find(placed, QStringLiteral("z")), (BarPlacement{QStringLiteral("z"), 1520, 400}));
        // In from the ends when asked.
        const auto inset = placeOnEdge({bar("a", "start", 0, 200), bar("z", "end", 0, 400)}, 1920, 8, 12);
        QCOMPARE(find(inset, QStringLiteral("a")).start, 12);
        QCOMPARE(find(inset, QStringLiteral("z")).start, 1508);
    }

    void theMiddleGroupMovesAsideOfTheOthers()
    {
        // Centred, the two middle bars would lie over the long start bar:
        // they stand right of it instead.
        const auto placed = placeOnEdge({bar("long", "start", 0, 900), bar("m1", "center", 0, 300),
                                         bar("m2", "center", 1, 300)}, 1920);
        QCOMPARE(find(placed, QStringLiteral("m1")), (BarPlacement{QStringLiteral("m1"), 908, 300}));
        QCOMPARE(find(placed, QStringLiteral("m2")), (BarPlacement{QStringLiteral("m2"), 1216, 300}));
    }

    void offsetsMoveBarsAsPlasmaMovesPanels()
    {
        const auto placed = placeOnEdge({bar("s", "start", 0, 200, 30), bar("m", "center", 0, 400, 100),
                                         bar("e", "end", 0, 300, 20)}, 1920);
        QCOMPARE(find(placed, QStringLiteral("s")).start, 30);
        QCOMPARE(find(placed, QStringLiteral("m")).start, 860);
        QCOMPARE(find(placed, QStringLiteral("e")).start, 1600);
    }

    void nothingGoesPastTheEdgeOrOverAnother()
    {
        // Too much for the edge: the last one is shortened to the room left.
        const auto placed = placeOnEdge({bar("a", "start", 0, 800), bar("b", "start", 1, 800)}, 1200);
        QCOMPARE(find(placed, QStringLiteral("a")), (BarPlacement{QStringLiteral("a"), 0, 800}));
        QCOMPARE(find(placed, QStringLiteral("b")), (BarPlacement{QStringLiteral("b"), 808, 392}));
        // An end bar never runs into the start group, nor a middle one
        // into either.
        const auto crowded = placeOnEdge({bar("s", "start", 0, 700), bar("e", "end", 0, 300),
                                          bar("m", "center", 0, 500)}, 1200);
        const BarPlacement s = find(crowded, QStringLiteral("s"));
        const BarPlacement e = find(crowded, QStringLiteral("e"));
        const BarPlacement m = find(crowded, QStringLiteral("m"));
        QCOMPARE(e, (BarPlacement{QStringLiteral("e"), 900, 300}));
        QVERIFY(m.start >= s.start + s.length + 8);
        QVERIFY(m.start + m.length + 8 <= e.start);
        QCOMPARE(m.length, 184);
        for (const BarPlacement &placement : crowded)
            QVERIFY(placement.start >= 0 && placement.start + placement.length <= 1200);
    }

    void anUnknownAlignmentIsTheMiddle()
    {
        const auto placed = placeOnEdge({bar("x", "sideways", 0, 100)}, 1000);
        QCOMPARE(find(placed, QStringLiteral("x")).start, 450);
    }
};

QTEST_GUILESS_MAIN(BarLayoutTest)
#include "BarLayoutTest.moc"
