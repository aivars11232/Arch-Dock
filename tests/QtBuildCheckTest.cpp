// SPDX-License-Identifier: GPL-3.0-or-later
// The start-up line that says Arch Dock runs with another Qt minor version
// than the one it was built with (src/QtBuildCheck.h).

#include "QtBuildCheck.h"

#include <QTest>

class QtBuildCheckTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSameMinorVersionSaysNothing()
    {
        QVERIFY(ArchDock::qtBuildMismatch(QVersionNumber(6, 12, 0), QVersionNumber(6, 12, 3)).isEmpty());
        QVERIFY(ArchDock::qtBuildMismatch(QVersionNumber(6, 12, 3), QVersionNumber(6, 12, 0)).isEmpty());
    }

    void anotherMinorVersionIsSaidPlainly()
    {
        QCOMPARE(ArchDock::qtBuildMismatch(QVersionNumber(6, 12, 0), QVersionNumber(6, 13, 1)),
                 QStringLiteral("Arch Dock was built for Qt 6.12 and runs with Qt 6.13. It keeps working, but its "
                                "bars may stay shown instead of hiding until a rebuilt Arch Dock is installed."));
    }

    void anotherMajorVersionCountsToo()
    {
        QVERIFY(!ArchDock::qtBuildMismatch(QVersionNumber(6, 12, 0), QVersionNumber(7, 12, 0)).isEmpty());
        QVERIFY(!ArchDock::qtBuildMismatch(QVersionNumber(6, 12, 0), QVersionNumber(6, 11, 4)).isEmpty());
    }

    void thisTestRunsWithTheQtItWasBuiltWith()
    {
        QVERIFY2(ArchDock::qtBuildMismatch().isEmpty(), qPrintable(ArchDock::qtBuildMismatch()));
    }
};

QTEST_GUILESS_MAIN(QtBuildCheckTest)
#include "QtBuildCheckTest.moc"
