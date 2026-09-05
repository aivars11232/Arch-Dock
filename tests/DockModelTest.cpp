#include "DockModel.h"
#include "WindowModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    void activationOutcomeSeparatesVerifiedLaunchFromRequest();
    void applicationEntryAndDesktopFileAreAvailableRegardlessOfPinning();

private:
    QString writeDesktopEntry(const QString &fileName,
                              const QString &name,
                              const QString &iconName,
                              const QString &execCommand = QStringLiteral("/bin/true"));

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

// TASK-0033 Phase A: a free panel pins an application as its desktop entry,
// so the model must hand out one snapshot and one desktop file for any
// application it knows, whether that application is pinned globally or only
// running.
void DockModelTest::applicationEntryAndDesktopFileAreAvailableRegardlessOfPinning()
{
    const QString desktopPath = writeDesktopEntry(
        QStringLiteral("org.example.runner.desktop"),
        QStringLiteral("Runner"),
        QStringLiteral("applications-games"));
    QVERIFY(!desktopPath.isEmpty());

    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.applicationEntry(QStringLiteral("org.example.runner.desktop")).isEmpty());
    QVERIFY(model.desktopFileForApplication(QStringLiteral("org.example.runner.desktop")).isEmpty());

    WindowItem window;
    window.internalId = QStringLiteral("runner-window");
    window.desktopFileName = desktopPath;
    window.resourceClass = QStringLiteral("runner");
    window.caption = QStringLiteral("Running");
    windowModel.setWindows({window});

    const QVariantList running = model.panelEntries(QStringLiteral("tasks"));
    QCOMPARE(running.size(), 1);
    const QString appId = running.constFirst().toMap().value(QStringLiteral("appId")).toString();
    QVERIFY(!appId.isEmpty());

    const QVariantMap entry = model.applicationEntry(appId);
    QCOMPARE(entry.value(QStringLiteral("appId")).toString(), appId);
    QVERIFY(entry.value(QStringLiteral("running")).toBool());
    QVERIFY(!entry.value(QStringLiteral("pinned")).toBool());
    QCOMPARE(entry.value(QStringLiteral("stableIdentity")),
             running.constFirst().toMap().value(QStringLiteral("stableIdentity")));
    QCOMPARE(model.desktopFileForApplication(appId),
             QFileInfo(desktopPath).absoluteFilePath());
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

// TASK-0031: a click asks for an activation. Only a started process is a
// verified success; raising a window is a request the compositor never answers,
// and it may never be reported as a launch that succeeded.
void DockModelTest::activationOutcomeSeparatesVerifiedLaunchFromRequest()
{
    const QString launchable = writeDesktopEntry(
        QStringLiteral("org.example.launchable.desktop"),
        QStringLiteral("Launchable"),
        QStringLiteral("applications-system"));
    QVERIFY(!launchable.isEmpty());
    const QString broken = writeDesktopEntry(
        QStringLiteral("org.example.broken.desktop"),
        QStringLiteral("Broken"),
        QStringLiteral("applications-system"),
        QStringLiteral("/nonexistent/arch-dock-should-not-exist"));
    QVERIFY(!broken.isEmpty());

    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(launchable)));
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(broken)));

    const QVariantList entries = model.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 2);
    QString launchableId;
    QString brokenId;
    for (const QVariant &value : entries)
    {
        const QVariantMap entry = value.toMap();
        const QString appId = entry.value(QStringLiteral("appId")).toString();
        if (entry.value(QStringLiteral("displayName")).toString()
                == QStringLiteral("Launchable"))
        {
            launchableId = appId;
        }
        else
        {
            brokenId = appId;
        }
    }
    QVERIFY(!launchableId.isEmpty());
    QVERIFY(!brokenId.isEmpty());

    // A program that starts is a verified success.
    QCOMPARE(model.activateApplicationOutcome(launchableId),
             DockModel::ActivationOutcome::Launched);
    // A program that cannot start is a verified failure, not an unknown.
    QCOMPARE(model.activateApplicationOutcome(brokenId),
             DockModel::ActivationOutcome::Failed);
    // An entry that does not exist cannot have been launched.
    QCOMPARE(model.activateApplicationOutcome(QStringLiteral("org.example.absent")),
             DockModel::ActivationOutcome::UnknownEntry);

    // With a window present the call raises it, which the compositor never
    // confirms, so the outcome stays an explicit request.
    WindowItem window;
    window.internalId = QStringLiteral("launchable-window");
    window.desktopFileName = launchable;
    window.resourceClass = QStringLiteral("launchable");
    window.iconName = QStringLiteral("applications-system");
    window.caption = QStringLiteral("Launchable");
    windowModel.setWindows({window});
    QCOMPARE(model.activateApplicationOutcome(launchableId),
             DockModel::ActivationOutcome::ActivationRequested);

    // The historical bool contract is unchanged in every case.
    QVERIFY(model.activateApplication(launchableId));
    QVERIFY(!model.activateApplication(brokenId));
    QVERIFY(!model.activateApplication(QStringLiteral("org.example.absent")));
}

QString DockModelTest::writeDesktopEntry(const QString &fileName,
                                         const QString &name,
                                         const QString &iconName,
                                         const QString &execCommand)
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
    file.write("\nExec=");
    file.write(execCommand.toUtf8());
    file.write("\n");
    file.close();
    return path;
}

QTEST_GUILESS_MAIN(DockModelTest)

#include "DockModelTest.moc"
