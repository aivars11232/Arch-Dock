#include "model/FolderContentModel.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::FolderContentModel;

namespace
{
bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("test document\n") > 0;
}
}

class FolderContentModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsEmptyMissingAndUnreadable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto url = QUrl::fromLocalFile(directory.path());
        auto snapshot = FolderContentModel::snapshot(url);
        QCOMPARE(snapshot.value("status").toString(), QStringLiteral("empty"));
        QVERIFY(snapshot.value("entries").toList().isEmpty());
        QVERIFY(!snapshot.value("truncated").toBool());
        QCOMPARE(FolderContentModel::snapshot(QUrl("https://example.test/folder"))
                     .value("errorCode").toString(), QStringLiteral("invalid-folder-url"));
        QCOMPARE(FolderContentModel::snapshot(QUrl::fromLocalFile(directory.filePath("missing")))
                     .value("errorCode").toString(), QStringLiteral("folder-missing"));
        QVERIFY(writeFile(directory.filePath("file")));
        QCOMPARE(FolderContentModel::snapshot(QUrl::fromLocalFile(directory.filePath("file")))
                     .value("errorCode").toString(), QStringLiteral("not-a-folder"));
        const auto permissions = QFile::permissions(directory.path());
        const auto restore = qScopeGuard([&] { QFile::setPermissions(directory.path(), permissions); });
        QVERIFY(QFile::setPermissions(directory.path(), {}));
        QCOMPARE(FolderContentModel::snapshot(url).value("errorCode").toString(),
                 QStringLiteral("folder-unreadable"));
    }

    void preservesIdentityWithoutRecursiveEnumeration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(QDir(directory.path()).mkdir("nested"));
        QVERIFY(writeFile(directory.filePath("nested/inside.txt")));
        const QString document = directory.filePath(QString::fromUtf8("A # café.txt"));
        QVERIFY(writeFile(document));
        QVERIFY(writeFile(directory.filePath(".hidden")));
        const auto url = QUrl::fromLocalFile(directory.path());
        const auto rows = FolderContentModel::snapshot(url).value("entries").toList();
        QCOMPARE(rows.size(), 2);
        QVERIFY(rows.first().toMap().value("isDirectory").toBool());
        const auto child = rows.last().toMap();
        QCOMPARE(child.value("name").toString(), QFileInfo(document).fileName());
        QVERIFY(child.value("selectable").toBool());
        QVERIFY(child.value("id").toString().contains("%23"));
        QCOMPARE(FolderContentModel::snapshot(url).value("entries").toList(), rows);
        QString error;
        QCOMPARE(FolderContentModel::resolveChild(url, child.value("id").toString(), &error),
                 QUrl::fromLocalFile(document));
        QVERIFY(error.isEmpty());
        QVERIFY(FolderContentModel::resolveChild(url, "folder-child:file:///etc/passwd", &error).isEmpty());
        QCOMPARE(error, QStringLiteral("child-not-listed"));
        QTemporaryDir other;
        QVERIFY(FolderContentModel::resolveChild(QUrl::fromLocalFile(other.path()),
                                                 child.value("id").toString()).isEmpty());
        QVERIFY(QFile::remove(document));
        QVERIFY(FolderContentModel::resolveChild(url, child.value("id").toString(), &error).isEmpty());
        QCOMPARE(error, QStringLiteral("child-not-listed"));
    }

    void blocksUnsafeChildrenAndRevalidatesReplacements()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto url = QUrl::fromLocalFile(directory.path());
        const QString document = directory.filePath("document.txt");
        QVERIFY(writeFile(document));
        const QString id = FolderContentModel::snapshot(url).value("entries")
                               .toList().first().toMap().value("id").toString();
        QVERIFY(QFile::setPermissions(document, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QString error;
        QVERIFY(FolderContentModel::resolveChild(url, id, &error).isEmpty());
        QCOMPARE(error, QStringLiteral("executable-entry"));
        QVERIFY(QFile::link(document, directory.filePath("link.txt")));
        QVERIFY(writeFile(directory.filePath("launcher.desktop")));
        const auto rows = FolderContentModel::snapshot(url).value("entries").toList();
        QCOMPARE(rows.size(), 3);
        for (const auto &value : rows)
        {
            const auto entry = value.toMap();
            QVERIFY(!entry.value("selectable").toBool());
            QVERIFY(!entry.value("blockedReason").toString().isEmpty());
            QVERIFY(FolderContentModel::resolveChild(url, entry.value("id").toString()).isEmpty());
        }
        QVERIFY(QDir(directory.path()).removeRecursively());
        QCOMPARE(FolderContentModel::snapshot(url).value("status").toString(), QStringLiteral("unavailable"));
    }

    void boundsLargeFoldersAndReportsTruncation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int index = 0; index < FolderContentModel::EntryLimit + 12; ++index)
            QVERIFY(writeFile(directory.filePath(QString::number(index) + ".txt")));
        const auto snapshot = FolderContentModel::snapshot(QUrl::fromLocalFile(directory.path()));
        QCOMPARE(snapshot.value("status").toString(), QStringLiteral("ready"));
        QCOMPARE(snapshot.value("entries").toList().size(), FolderContentModel::EntryLimit);
        QCOMPARE(snapshot.value("limit").toInt(), FolderContentModel::EntryLimit);
        QVERIFY(snapshot.value("truncated").toBool());
        QSet<QString> ids;
        for (const auto &value : snapshot.value("entries").toList())
            ids.insert(value.toMap().value("id").toString());
        QCOMPARE(ids.size(), FolderContentModel::EntryLimit);
    }
};

QTEST_GUILESS_MAIN(FolderContentModelTest)
#include "FolderContentModelTest.moc"
