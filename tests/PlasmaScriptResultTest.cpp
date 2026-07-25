#include "PlasmaScriptResult.h"

#include <QTest>

class PlasmaScriptResultTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsStrictSentinel_data();
    void acceptsStrictSentinel();
    void rejectsUnverifiedOutput_data();
    void rejectsUnverifiedOutput();
};

void PlasmaScriptResultTest::acceptsStrictSentinel_data()
{
    QTest::addColumn<QString>("output");
    QTest::addColumn<int>("expected");

    QTest::newRow("success") << QStringLiteral("ARCHDOCK_RESULT:1") << 1;
    QTest::newRow("zero") << QStringLiteral("ARCHDOCK_RESULT:0") << 0;
    QTest::newRow("negative") << QStringLiteral(" \nARCHDOCK_RESULT:-1\t") << -1;
}

void PlasmaScriptResultTest::acceptsStrictSentinel()
{
    QFETCH(QString, output);
    QFETCH(int, expected);

    const std::optional<int> result = ArchDock::parsePlasmaScriptResult(output);
    QVERIFY(result.has_value());
    QCOMPARE(*result, expected);
}

void PlasmaScriptResultTest::rejectsUnverifiedOutput_data()
{
    QTest::addColumn<QString>("output");

    QTest::newRow("plain integer") << QStringLiteral("42");
    QTest::newRow("script error") << QStringLiteral("ReferenceError at line 42");
    QTest::newRow("prefix") << QStringLiteral("prefix ARCHDOCK_RESULT:1");
    QTest::newRow("suffix") << QStringLiteral("ARCHDOCK_RESULT:1\nerror line 9");
    QTest::newRow("duplicate")
        << QStringLiteral("ARCHDOCK_RESULT:1\nARCHDOCK_RESULT:1");
    QTest::newRow("blank") << QString();
    QTest::newRow("overflow")
        << QStringLiteral("ARCHDOCK_RESULT:999999999999999999999999");
}

void PlasmaScriptResultTest::rejectsUnverifiedOutput()
{
    QFETCH(QString, output);
    QVERIFY(!ArchDock::parsePlasmaScriptResult(output).has_value());
}

QTEST_MAIN(PlasmaScriptResultTest)

#include "PlasmaScriptResultTest.moc"
