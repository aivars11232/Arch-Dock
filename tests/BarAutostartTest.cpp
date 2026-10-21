#include "panel/BarAutostart.h"

#include <QTemporaryDir>
#include <QtTest>

class BarAutostartTest : public QObject
{
    Q_OBJECT

private slots:
    void theEntryStartsTheProgramInTheBackground()
    {
        const QString text = ArchDock::BarAutostart::entryText(QStringLiteral("/usr/bin/arch-dock"));
        QVERIFY(text.startsWith(QStringLiteral("[Desktop Entry]\n")));
        QVERIFY(text.contains(QStringLiteral("\nExec=\"/usr/bin/arch-dock\"\n")));
        // Never opens Panel Studio, never shows in the menu.
        QVERIFY(!text.contains(QStringLiteral("--settings")));
        QVERIFY(text.contains(QStringLiteral("\nNoDisplay=true\n")));
        // A path with spaces stays one word; one with characters a desktop
        // entry treats specially is escaped as the installed entries are.
        QVERIFY(ArchDock::BarAutostart::entryText(QStringLiteral("/opt/arch dock/bin/arch-dock"))
                    .contains(QStringLiteral("\nExec=\"/opt/arch dock/bin/arch-dock\"\n")));
        QVERIFY(ArchDock::BarAutostart::entryText(QStringLiteral("/opt/a$b%c/arch-dock"))
                    .contains(QStringLiteral("\nExec=\"/opt/a\\\\$b%%c/arch-dock\"\n")));
    }

    void theEntryComesAndGoesWithTheBars()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString path = home.filePath(QStringLiteral("autostart/org.archdock.ArchDock.Bars.autostart.desktop"));
        QVERIFY(ArchDock::BarAutostart::sync(true, path, QStringLiteral("/usr/bin/arch-dock")));
        QFile written(path);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(written.readAll()),
                 ArchDock::BarAutostart::entryText(QStringLiteral("/usr/bin/arch-dock")));
        written.close();
        // Written again only when it differs; gone with the last bar.
        QVERIFY(ArchDock::BarAutostart::sync(true, path, QStringLiteral("/usr/bin/arch-dock")));
        QVERIFY(ArchDock::BarAutostart::sync(false, path, QStringLiteral("/usr/bin/arch-dock")));
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(ArchDock::BarAutostart::sync(false, path, QStringLiteral("/usr/bin/arch-dock")));
    }
};

QTEST_GUILESS_MAIN(BarAutostartTest)
#include "BarAutostartTest.moc"
