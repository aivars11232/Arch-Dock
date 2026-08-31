#include "DockModel.h"
#include "WindowModel.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class DockModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void snapshotsExposeStableIdentityAndBaseData();
    void pinnedAndRunningRepresentationsShareOneIdentity();
    void freeEntriesUseCanonicalIdentity();
    void legacyCustomGlyphRemainsExplicitBaseData();

private:
    QString writeDesktopEntry(const QString &fileName,
                              const QString &name,
                              const QString &iconName);

    QTemporaryDir m_settingsDirectory;
    QTemporaryDir m_desktopDirectory;
};

void DockModelTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(m_desktopDirectory.isValid());
    QSettings::setPath(
        QSettings::NativeFormat,
        QSettings::UserScope,
        m_settingsDirectory.path());
}

void DockModelTest::cleanup()
{
    QSettings settings;
    settings.clear();
    settings.sync();
}

void DockModelTest::snapshotsExposeStableIdentityAndBaseData()
{
    WindowModel windowModel;
    WindowItem window;
    window.internalId = QStringLiteral("window-one");
    window.desktopFileName = QStringLiteral("org.example.editor.desktop");
    window.resourceClass = QStringLiteral("transient-editor");
    window.iconName = QStringLiteral("applications-development");
    window.caption = QStringLiteral("Editor");
    windowModel.setWindows({window});
    DockModel model(windowModel);

    const QVariantList entries = model.panelEntries(QStringLiteral("tasks"));
    QCOMPARE(entries.size(), 1);
    const QVariantMap entry = entries.constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("stableIdentity")).toString(),
             QStringLiteral("desktop.org.example.editor.desktop"));
    QCOMPARE(entry.value(QStringLiteral("baseIconName")).toString(),
             entry.value(QStringLiteral("iconName")).toString());
    QCOMPARE(entry.value(QStringLiteral("baseDisplayName")).toString(),
             entry.value(QStringLiteral("displayName")).toString());
}

void DockModelTest::pinnedAndRunningRepresentationsShareOneIdentity()
{
    const QString desktopPath = writeDesktopEntry(
        QStringLiteral("org.example.shared.desktop"),
        QStringLiteral("Shared app"),
        QStringLiteral("applications-system"));
    QVERIFY(!desktopPath.isEmpty());

    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(desktopPath)));
    QVariantList entries = model.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 1);
    const QString pinnedIdentity = entries.constFirst().toMap()
        .value(QStringLiteral("stableIdentity")).toString();

    WindowItem window;
    window.internalId = QStringLiteral("shared-window");
    window.desktopFileName = desktopPath;
    window.resourceClass = QStringLiteral("different-runtime-class");
    window.iconName = QStringLiteral("runtime-icon");
    window.caption = QStringLiteral("Running shared app");
    windowModel.setWindows({window});

    entries = model.panelEntries(QStringLiteral("hybrid"));
    QCOMPARE(entries.size(), 1);
    const QVariantMap merged = entries.constFirst().toMap();
    QCOMPARE(merged.value(QStringLiteral("stableIdentity")).toString(),
             pinnedIdentity);
    QVERIFY(merged.value(QStringLiteral("pinned")).toBool());
    QVERIFY(merged.value(QStringLiteral("running")).toBool());
}

void DockModelTest::freeEntriesUseCanonicalIdentity()
{
    const QString folderPath = m_desktopDirectory.filePath(
        QStringLiteral("folder"));
    QVERIFY(QDir().mkpath(folderPath));
    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(folderPath)));

    const QVariantList entries = model.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.constFirst().toMap()
        .value(QStringLiteral("stableIdentity")).toString()
        .startsWith(QStringLiteral("free.sha256-")));
}

void DockModelTest::legacyCustomGlyphRemainsExplicitBaseData()
{
    const QByteArray legacyBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("legacy-custom-glyph"),
         QStringLiteral("utilities-terminal")},
    }).toJson(QJsonDocument::Compact);
    QSettings settings;
    settings.setValue(QStringLiteral("dock/customIcons"), legacyBytes);
    settings.sync();

    WindowModel windowModel;
    WindowItem window;
    window.internalId = QStringLiteral("legacy-window");
    window.resourceClass = QStringLiteral("legacy-custom-glyph");
    window.iconName = QStringLiteral("applications-system");
    window.caption = QStringLiteral("Legacy custom glyph");
    windowModel.setWindows({window});
    DockModel model(windowModel);

    const QVariantMap entry = model.panelEntries(
        QStringLiteral("tasks")).constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("baseIconName")).toString(),
             QStringLiteral("utilities-terminal"));
    QCOMPARE(entry.value(QStringLiteral("iconName")).toString(),
             QStringLiteral("utilities-terminal"));

    model.pin(0);
    settings.sync();
    QCOMPARE(settings.value(QStringLiteral("dock/customIcons")).toByteArray(),
             legacyBytes);
}

QString DockModelTest::writeDesktopEntry(const QString &fileName,
                                         const QString &name,
                                         const QString &iconName)
{
    const QString path = m_desktopDirectory.filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return {};
    }
    file.write("[Desktop Entry]\nType=Application\nName=");
    file.write(name.toUtf8());
    file.write("\nIcon=");
    file.write(iconName.toUtf8());
    file.write("\nExec=/bin/true\n");
    file.close();
    return path;
}

QTEST_GUILESS_MAIN(DockModelTest)

#include "DockModelTest.moc"
