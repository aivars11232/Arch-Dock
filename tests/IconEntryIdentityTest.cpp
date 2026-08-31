#include "model/IconEntryIdentity.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::IconEntryIdentity;

class IconEntryIdentityTest final : public QObject
{
    Q_OBJECT

private slots:
    void pinnedAndRunningDesktopEntriesShareIdentity();
    void desktopIdentityWinsOverTransientApplicationIds();
    void identityChangesWhenTheDesktopEntryChanges();
    void freeEntriesUseCanonicalLocalUrlIdentity();
    void unsafeAndEmptySourcesAreRejected();
};

void IconEntryIdentityTest::pinnedAndRunningDesktopEntriesShareIdentity()
{
    const QString pinned = IconEntryIdentity::forApplication(
        QStringLiteral("org.kde.dolphin.desktop"),
        QStringLiteral("/usr/share/applications/org.kde.dolphin.desktop"));
    const QString running = IconEntryIdentity::forApplication(
        QStringLiteral("org.kde.dolphin.desktop"),
        QStringLiteral("org.kde.dolphin.desktop"));

    QCOMPARE(pinned, QStringLiteral("desktop.org.kde.dolphin.desktop"));
    QCOMPARE(running, pinned);
    QVERIFY(IconEntryIdentity::isValid(pinned));
}

void IconEntryIdentityTest::desktopIdentityWinsOverTransientApplicationIds()
{
    const QString first = IconEntryIdentity::forApplication(
        QStringLiteral("wayland-transient-id"),
        QStringLiteral("org.example.Editor.desktop"));
    const QString second = IconEntryIdentity::forApplication(
        QStringLiteral("x11-transient-id"),
        QStringLiteral("/opt/share/applications/org.example.editor.desktop"));

    QCOMPARE(first, QStringLiteral("desktop.org.example.editor.desktop"));
    QCOMPARE(second, first);
}

void IconEntryIdentityTest::identityChangesWhenTheDesktopEntryChanges()
{
    const QString first = IconEntryIdentity::forApplication(
        QStringLiteral("same-runtime-id"),
        QStringLiteral("org.example.first.desktop"));
    const QString second = IconEntryIdentity::forApplication(
        QStringLiteral("same-runtime-id"),
        QStringLiteral("org.example.second.desktop"));

    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
}

void IconEntryIdentityTest::freeEntriesUseCanonicalLocalUrlIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("nested"))));
    QFile file(directory.filePath(QStringLiteral("nested/item.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("identity\n");
    file.close();

    const QUrl direct = QUrl::fromLocalFile(file.fileName());
    const QUrl redundant = QUrl::fromLocalFile(
        directory.filePath(QStringLiteral("nested/../nested/item.txt")));
    const QString first = IconEntryIdentity::forFreeUrl(direct);
    const QString second = IconEntryIdentity::forFreeUrl(redundant);

    QVERIFY(first.startsWith(QStringLiteral("free.sha256-")));
    QCOMPARE(second, first);
    QCOMPARE(IconEntryIdentity::forApplication(
                 QStringLiteral("free-url:") +
                     QString::fromUtf8(direct.toEncoded())),
             first);
    QVERIFY(IconEntryIdentity::isValid(first));
}

void IconEntryIdentityTest::unsafeAndEmptySourcesAreRejected()
{
    QVERIFY(IconEntryIdentity::forApplication({}, {}).isEmpty());
    QVERIFY(IconEntryIdentity::forFreeUrl(
        QUrl(QStringLiteral("https://example.invalid/icon"))).isEmpty());
    QVERIFY(!IconEntryIdentity::isValid(QStringLiteral("free.file:///tmp/a")));
    QVERIFY(!IconEntryIdentity::isValid(QStringLiteral("desktop.bad key")));
}

QTEST_GUILESS_MAIN(IconEntryIdentityTest)

#include "IconEntryIdentityTest.moc"
