// SPDX-License-Identifier: GPL-3.0-or-later
#include "bars/BarWidgets.h"

#include <QSet>
#include <QtTest>

using ArchDock::Bars::BarWidget;
using namespace ArchDock::Bars;

class BarWidgetsTest : public QObject
{
    Q_OBJECT

private slots:
    void entriesAreReadAndWrittenTheSameWay()
    {
        const QStringList entries{QStringLiteral("w1:org.kde.plasma.kickoff"), QStringLiteral("dock"),
                                  QStringLiteral("w2:org.kde.plasma.digitalclock")};
        const QList<BarWidget> widgets = barWidgetsFromEntries(entries);
        QCOMPARE(widgets.size(), 3);
        QCOMPARE(widgets.at(0), (BarWidget{QStringLiteral("w1"), QStringLiteral("org.kde.plasma.kickoff")}));
        QVERIFY(widgets.at(1).dock());
        QCOMPARE(widgets.at(2).plugin, QStringLiteral("org.kde.plasma.digitalclock"));
        QCOMPARE(barWidgetEntries(widgets), entries);
    }

    void theDockIsThereOnce()
    {
        // Missing: first.
        QList<BarWidget> widgets = barWidgetsFromEntries({QStringLiteral("w1:org.kde.plasma.digitalclock")});
        QCOMPARE(widgets.size(), 2);
        QVERIFY(widgets.first().dock());
        // Twice: once, where it first was.
        widgets = barWidgetsFromEntries({QStringLiteral("w1:org.kde.plasma.digitalclock"), QStringLiteral("dock"),
                                         QStringLiteral("dock")});
        QCOMPARE(widgets.size(), 2);
        QVERIFY(widgets.last().dock());
        // Every bar holds it, an Empty one too, whose dock shows its own items.
        widgets = barWidgetsFromEntries({});
        QCOMPARE(widgets.size(), 1);
        QVERIFY(widgets.first().dock());
    }

    void badEntriesAndRepeatedKeysAreLeftOut()
    {
        const QList<BarWidget> widgets = barWidgetsFromEntries({
            QStringLiteral("no colon"),
            QStringLiteral(":org.kde.plasma.digitalclock"),
            QStringLiteral("w 1:org.kde.plasma.digitalclock"),
            QStringLiteral("w1:org.kde plasma"),
            QStringLiteral("w2:org.archdock.dock"),
            QStringLiteral("w3:org.archdock.bar"),
            QStringLiteral("dock:org.kde.plasma.digitalclock"),
            QStringLiteral("w4:org.kde.plasma.digitalclock"),
            QStringLiteral("w4:org.kde.plasma.kickoff"),
        });
        // The dock, first as it was missing, and the one good widget.
        QCOMPARE(widgets.size(), 2);
        QVERIFY(widgets.first().dock());
        QCOMPARE(widgets.last(), (BarWidget{QStringLiteral("w4"), QStringLiteral("org.kde.plasma.digitalclock")}));
        QVERIFY(!validWidgetKey(QString(65, QLatin1Char('a'))));
        QVERIFY(validWidgetKey(QString(64, QLatin1Char('a'))));
    }

    void newKeysAreValidAndNeverTaken()
    {
        QList<BarWidget> widgets{{BarWidget::dockKey(), {}}};
        QSet<QString> keys;
        for (int count = 0; count < 200; ++count)
        {
            const QString key = newWidgetKey(widgets);
            QVERIFY(validWidgetKey(key));
            QVERIFY(!keys.contains(key));
            keys.insert(key);
            widgets.append({key, QStringLiteral("org.kde.plasma.digitalclock")});
        }
    }
};

QTEST_GUILESS_MAIN(BarWidgetsTest)
#include "BarWidgetsTest.moc"
