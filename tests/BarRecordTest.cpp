// SPDX-License-Identifier: GPL-3.0-or-later
#include "bars/BarRecord.h"

#include <QtTest>

using ArchDock::Bars::BarRecord;
using ArchDock::Bars::parseBarRecords;

class BarRecordTest : public QObject
{
    Q_OBJECT

private slots:
    void aFullRecordIsReadAsWritten()
    {
        const QList<BarRecord> records = parseBarRecords(R"([{
            "id": "panel-2", "type": "tasks", "screen": 1, "screenId": "output:DP-2",
            "edge": "left", "alignment": "end", "order": 3, "offset": 40, "thickness": 64,
            "lengthMode": "fit", "fixedLength": 500, "minimumLength": 100, "maximumLength": 900,
            "floatingMargin": 6, "visibilityMode": "dodge", "reservesSpace": true, "visible": false
        }])");
        QCOMPARE(records.size(), 1);
        const BarRecord &record = records.first();
        QCOMPARE(record.id, QStringLiteral("panel-2"));
        QCOMPARE(record.type, QStringLiteral("tasks"));
        QCOMPARE(record.screenIndex, 1);
        QCOMPARE(record.screenId, QStringLiteral("output:DP-2"));
        QCOMPARE(record.edge, QStringLiteral("left"));
        QVERIFY(record.vertical());
        QCOMPARE(record.alignment, QStringLiteral("end"));
        QCOMPARE(record.order, 3);
        QCOMPARE(record.offset, 40);
        QCOMPARE(record.thickness, 64);
        QCOMPARE(record.lengthMode, QStringLiteral("fit"));
        QCOMPARE(record.fixedLength, 500);
        QCOMPARE(record.minimumLength, 100);
        QCOMPARE(record.maximumLength, 900);
        QCOMPARE(record.floatingMargin, 6);
        QCOMPARE(record.visibilityMode, QStringLiteral("dodge"));
        QVERIFY(record.reservesSpace);
        QVERIFY(!record.visible);
    }

    void missingValuesTakeTheDefaults()
    {
        const QList<BarRecord> records = parseBarRecords(
            R"([{"id": "a", "edge": "bottom"}, {"id": "b", "edge": "top", "visibilityMode": "auto-hide"}])");
        QCOMPARE(records.size(), 2);
        BarRecord expected;
        expected.id = QStringLiteral("a");
        QCOMPARE(records.at(0), expected);
        // An order missing is the record's place in the list; a bar that
        // hides keeps no room unless it says so.
        QCOMPARE(records.at(1).order, 1);
        QVERIFY(!records.at(1).reservesSpace);
    }

    void badRecordsAreLeftOutAndBadValuesBounded()
    {
        QString error;
        const QList<BarRecord> records = parseBarRecords(R"([
            {"edge": "bottom"},
            {"id": "free-1", "edge": "free"},
            {"id": "x", "edge": "TOP", "type": "everything", "alignment": "sideways",
             "thickness": 99999, "minimumLength": 500, "maximumLength": 20, "floatingMargin": -5,
             "lengthMode": "grow", "screen": -3},
            {"id": "x", "edge": "bottom"},
            "not a record"
        ])", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(records.size(), 1);
        const BarRecord &record = records.first();
        QCOMPARE(record.edge, QStringLiteral("top"));
        QCOMPARE(record.type, QStringLiteral("hybrid"));
        QCOMPARE(record.alignment, QStringLiteral("center"));
        QCOMPARE(record.lengthMode, QStringLiteral("fixed"));
        QCOMPARE(record.thickness, 4096);
        QCOMPARE(record.maximumLength, 500);
        QCOMPARE(record.floatingMargin, 0);
        QCOMPARE(record.screenIndex, 0);
    }

    void aDocumentThatIsNoListSaysSo()
    {
        QString error;
        QVERIFY(parseBarRecords("{\"id\": \"a\"}", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(parseBarRecords("[{", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(parseBarRecords("[]", &error).isEmpty());
        QVERIFY(error.isEmpty());
    }

    void widgetsAreReadInTheirOrder()
    {
        const QList<BarRecord> records = parseBarRecords(R"([
            {"id": "panel-1", "edge": "bottom", "type": "launcher", "widgets": [
                {"key": "w1", "plugin": "org.kde.plasma.kickoff"}, {"key": "dock"},
                {"key": "w2", "plugin": "org.kde.plasma.digitalclock"},
                {"key": "w2", "plugin": "org.kde.plasma.systemtray"}, {"key": "w3"}]},
            {"id": "panel-2", "edge": "top"},
            {"id": "panel-3", "edge": "top", "type": "empty", "widgets": [
                {"key": "dock"}, {"key": "w1", "plugin": "org.kde.plasma.digitalclock"}]}
        ])");
        QCOMPARE(records.size(), 3);
        const auto &first = records.at(0).widgets;
        QCOMPARE(first.size(), 3);
        QCOMPARE(first.at(0).plugin, QStringLiteral("org.kde.plasma.kickoff"));
        QVERIFY(first.at(1).dock());
        QCOMPARE(first.at(2).plugin, QStringLiteral("org.kde.plasma.digitalclock"));
        // Without a list a bar holds its dock alone; an empty bar no dock.
        QCOMPARE(records.at(1).widgets.size(), 1);
        QVERIFY(records.at(1).widgets.first().dock());
        QCOMPARE(records.at(2).widgets.size(), 1);
        QCOMPARE(records.at(2).widgets.first().key, QStringLiteral("w1"));
    }
};

QTEST_GUILESS_MAIN(BarRecordTest)
#include "BarRecordTest.moc"
