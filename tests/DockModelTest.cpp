#include "DockModel.h"
#include "WindowModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
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
    void groupedPreviewsFollowWindowChangesAndSelectById();
    void individualActionsRevalidateMembershipAndCapability();
    void desktopActionsUseNativeValidationAndLaunch();
    void folderSnapshotsUseTheBoundedProvider();
    void onlyEntryRelevantWindowChangesRebuildTheDock();
    void desktopEntriesAreReadWithDesktopEntryRules();

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

void DockModelTest::groupedPreviewsFollowWindowChangesAndSelectById()
{
    WindowModel source;
    DockModel model(source);
    QSignalSpy actions(&model, &DockModel::windowActionRequested);
    WindowItem first;
    first.internalId = QStringLiteral("first-window");
    first.desktopFileName = QStringLiteral("org.example.group.desktop");
    first.caption = QStringLiteral("First document");
    first.canActivate = true;
    WindowItem second = first;
    second.internalId = QStringLiteral("second-window");
    second.caption = QStringLiteral("Second document");
    second.minimized = true;
    source.setWindows({first, second});
    QCOMPARE(model.rowCount(), 1);
    const QString appId = model.data(model.index(0), DockModel::AppIdRole).toString();
    auto previews = [&model, &appId] {
        return model.applicationEntry(appId).value("windowPreviews").toList();
    };
    QCOMPARE(previews().size(), 2);
    QCOMPARE(model.data(model.index(0), DockModel::WindowPreviewsRole).toList(), previews());
    QVERIFY(previews().at(1).toMap().value("minimized").toBool());
    QVERIFY(model.activateApplicationWindow(appId, second.internalId));
    QCOMPARE(actions.size(), 1);
    QCOMPARE(actions.at(0).at(0).toString(), second.internalId);
    QCOMPARE(actions.at(0).at(1).toString(), QStringLiteral("activate"));
    second.caption = QStringLiteral("Renamed document");
    QVERIFY(source.updateWindow(second));
    QCOMPARE(previews().at(1).toMap().value("title").toString(), second.caption);
    QVERIFY(source.removeWindow(first.internalId));
    QCOMPARE(previews().size(), 1);
    QCOMPARE(previews().first().toMap().value("windowId").toString(), second.internalId);
    QVERIFY(!model.activateApplicationWindow(appId, first.internalId));
    QCOMPARE(actions.size(), 1);
    QVERIFY(source.removeWindow(second.internalId));
    QVERIFY(previews().isEmpty());
    QVERIFY(!model.activateApplicationWindow(appId, second.internalId));
    QCOMPARE(actions.size(), 1);
}

void DockModelTest::individualActionsRevalidateMembershipAndCapability()
{
    WindowModel source;
    DockModel model(source);
    QSignalSpy actions(&model, &DockModel::windowActionRequested);
    WindowItem first;
    first.internalId = QStringLiteral("first");
    first.desktopFileName = QStringLiteral("org.example.group.desktop");
    first.canActivate = first.canMinimize = first.canClose = true;
    WindowItem second = first;
    second.internalId = QStringLiteral("second");
    WindowItem other = first;
    other.internalId = QStringLiteral("other");
    other.desktopFileName = QStringLiteral("org.example.other.desktop");
    source.setWindows({first, second, other});
    QString appId;
    for (const auto &value : model.panelEntries(QStringLiteral("tasks")))
        if (value.toMap().value("windowIds").toStringList().contains(first.internalId))
            appId = value.toMap().value("appId").toString();
    QVERIFY(!appId.isEmpty());
    for (const auto &action : {QStringLiteral("activate"), QStringLiteral("minimize"), QStringLiteral("close")})
    {
        QVERIFY(model.requestApplicationWindowAction(appId, second.internalId, action));
        QCOMPARE(actions.last().at(0).toString(), second.internalId);
        QCOMPARE(actions.last().at(1).toString(), action);
    }
    QVERIFY(!model.requestApplicationWindowAction(appId, second.internalId, QStringLiteral("restore")));
    second.minimized = true;
    QVERIFY(source.updateWindow(second));
    QVERIFY(model.requestApplicationWindowAction(appId, second.internalId, QStringLiteral("restore")));
    QCOMPARE(actions.last().at(0).toString(), second.internalId);
    QCOMPARE(actions.last().at(1).toString(), QStringLiteral("restore"));
    const auto dispatched = actions.size();
    QVERIFY(!model.requestApplicationWindowAction(appId, second.internalId, QStringLiteral("minimize")));
    QVERIFY(!model.requestApplicationWindowAction(appId, other.internalId, QStringLiteral("close")));
    QVERIFY(!model.requestApplicationWindowAction(appId, second.internalId, QStringLiteral("unknown")));
    second.canActivate = second.canMinimize = second.canClose = false;
    QVERIFY(source.updateWindow(second));
    for (const auto &action : {QStringLiteral("activate"), QStringLiteral("minimize"),
                              QStringLiteral("restore"), QStringLiteral("close")})
        QVERIFY(!model.requestApplicationWindowAction(appId, second.internalId, action));
    QVERIFY(source.removeWindow(second.internalId));
    QVERIFY(!model.requestApplicationWindowAction(appId, second.internalId, QStringLiteral("close")));
    QVERIFY(!model.requestApplicationWindowAction(QStringLiteral("missing"), first.internalId, QStringLiteral("activate")));
    QCOMPARE(actions.size(), dispatched);
    const auto transient = model.applicationEntry(appId);
    QVERIFY(!transient.value("canNewInstance").toBool());
    QVERIFY(!transient.value("canPin").toBool());
    QVERIFY(transient.value("desktopActions").toList().isEmpty());
}

void DockModelTest::desktopActionsUseNativeValidationAndLaunch()
{
    const QString mainMarker = m_desktopDirectory.filePath(QStringLiteral("main-launched"));
    const QString actionMarker = m_desktopDirectory.filePath(QStringLiteral("action-launched"));
    const QString path = writeDesktopEntry(QStringLiteral("org.example.actions.desktop"),
        QStringLiteral("Actions"), QStringLiteral("applications-system"),
        QStringLiteral("/usr/bin/touch \"%1\"").arg(mainMarker));
    QVERIFY(!path.isEmpty());
    QFile file(path);
    QVERIFY(file.open(QIODevice::Append));
    file.write(QStringLiteral(
        "Actions=WriteNote;Hidden;Empty;\n"
        "[Desktop Action WriteNote]\nName=Write note\nExec=/usr/bin/touch \"%1\"\n"
        "[Desktop Action Hidden]\nName=Hidden\nExec=/bin/true\nNoDisplay=true\n"
        "[Desktop Action Empty]\nName=Empty\n").arg(actionMarker).toUtf8());
    file.close();
    WindowModel source;
    DockModel model(source);
    QVERIFY(!DockModel::desktopEntryActions(path).value("canNewInstance").toBool());
    QVERIFY(!model.launchDesktopEntry(path, QString{}));
    QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
    const auto metadata = DockModel::desktopEntryActions(path);
    QVERIFY(metadata.value("canNewInstance").toBool());
    const auto actions = metadata.value("desktopActions").toList();
    QCOMPARE(actions.size(), 1);
    QCOMPARE(actions.first().toMap().value("id").toString(), QStringLiteral("WriteNote"));
    QVERIFY(!actions.first().toMap().contains("exec"));
    QVERIFY(!model.launchDesktopEntry(path, QStringLiteral("Hidden")));
    QVERIFY(!model.launchDesktopEntry(path, QStringLiteral("Empty")));
    QVERIFY(!model.launchDesktopEntry(path, QStringLiteral("unknown")));
    QVERIFY(model.launchDesktopEntry(path, QString{}));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(mainMarker), 5000);
    QVERIFY(model.launchDesktopEntry(path, QStringLiteral("WriteNote")));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(actionMarker), 5000);
    // Resolve again at dispatch: a removed desktop action must not run.
    QCOMPARE(writeDesktopEntry(QStringLiteral("org.example.actions.desktop"),
        QStringLiteral("Actions"), QStringLiteral("applications-system")), path);
    QVERIFY(DockModel::desktopEntryActions(path).value("desktopActions").toList().isEmpty());
    QVERIFY(!model.launchDesktopEntry(path, QStringLiteral("WriteNote")));
}

void DockModelTest::folderSnapshotsUseTheBoundedProvider()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    for (int index = 0; index < 52; ++index)
    {
        QFile file(folder.filePath(QString::number(index) + QStringLiteral(".txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("document\n");
    }
    WindowModel windows;
    DockModel model(windows);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(folder.path())));
    const auto entry = model.panelEntries(QStringLiteral("launcher")).first().toMap();
    QVERIFY(entry.value(QStringLiteral("isFolder")).toBool());
    const auto children = model.folderEntriesForApplication(entry.value(QStringLiteral("appId")).toString());
    QCOMPARE(children.size(), 48);
    for (const auto &value : children)
    {
        const auto child = value.toMap();
        QVERIFY(child.value(QStringLiteral("id")).toString().startsWith(QStringLiteral("folder-child:file:")));
        QVERIFY(child.value(QStringLiteral("url")).toUrl().isLocalFile());
        QVERIFY(child.value(QStringLiteral("selectable")).toBool());
    }
    QVERIFY(model.folderEntriesForApplication(QStringLiteral("unknown")).isEmpty());
}

void DockModelTest::freeEntriesUseCanonicalIdentity()
{
    const QString folderPath = m_desktopDirectory.filePath(
        QStringLiteral("folder"));
    QVERIFY(QDir().mkpath(folderPath));
    QFile metadata(folderPath + QStringLiteral("/.directory"));
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    metadata.write("[Desktop Entry]\nIcon=folder-documents\n");
    metadata.close();
    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(folderPath)));

    const QVariantList entries = model.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().toMap().value(QStringLiteral("iconName")).toString(),
             QStringLiteral("folder-documents"));
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
    // Pinned applications start through KDE's launcher, like Plasma's own
    // launchers: it honours Terminal=, Path= and D-Bus activation, gives the
    // application its own systemd scope (so it outlives Arch Dock) and only
    // runs desktop files KDE trusts. Files outside the application folders
    // must be executable to be trusted.
    const QString marker = m_desktopDirectory.filePath(QStringLiteral("launched-marker"));
    const QString launchable = writeDesktopEntry(
        QStringLiteral("org.example.launchable.desktop"),
        QStringLiteral("Launchable"),
        QStringLiteral("applications-system"),
        QStringLiteral("/usr/bin/touch \"%1\"").arg(marker));
    QVERIFY(!launchable.isEmpty());
    const QString broken = writeDesktopEntry(
        QStringLiteral("org.example.broken.desktop"),
        QStringLiteral("Broken"),
        QStringLiteral("applications-system"),
        QStringLiteral("/nonexistent/arch-dock-should-not-exist"));
    QVERIFY(!broken.isEmpty());
    const QString untrusted = writeDesktopEntry(
        QStringLiteral("org.example.untrusted.desktop"),
        QStringLiteral("Untrusted"),
        QStringLiteral("applications-system"),
        QStringLiteral("/usr/bin/touch \"%1-untrusted\"").arg(marker));
    QVERIFY(!untrusted.isEmpty());
    for (const QString &path : {launchable, broken})
    {
        QFile file(path);
        QVERIFY(file.setPermissions(file.permissions() | QFileDevice::ExeOwner));
    }

    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(launchable)));
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(broken)));
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(untrusted)));

    QHash<QString, QString> ids;
    for (const QVariant &value : model.panelEntries(QStringLiteral("launcher")))
    {
        const QVariantMap entry = value.toMap();
        ids.insert(entry.value(QStringLiteral("displayName")).toString(),
                   entry.value(QStringLiteral("appId")).toString());
    }
    QCOMPARE(ids.size(), 3);
    const QString launchableId = ids.value(QStringLiteral("Launchable"));
    const QString brokenId = ids.value(QStringLiteral("Broken"));
    const QString untrustedId = ids.value(QStringLiteral("Untrusted"));

    // KDE's launcher starts the program asynchronously: the click is a
    // request, and the program really runs.
    QSignalSpy finished(&model, &DockModel::launchFinished);
    QCOMPARE(model.activateApplicationOutcome(launchableId),
             DockModel::ActivationOutcome::LaunchRequested);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker), 5000);
    // The launcher confirms the start; that confirmation is what the applet
    // waits for before it plays a launch animation.
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(finished.constFirst().at(0).toString(), launchableId);
    QVERIFY(finished.constFirst().at(1).toBool());
    // A program that is not there is a verified failure, reported at once.
    QCOMPARE(model.activateApplicationOutcome(brokenId),
             DockModel::ActivationOutcome::Failed);
    // A desktop file KDE does not trust is never run.
    QCOMPARE(model.activateApplicationOutcome(untrustedId),
             DockModel::ActivationOutcome::Failed);
    QTest::qWait(300);
    QVERIFY(!QFileInfo::exists(marker + QStringLiteral("-untrusted")));
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
    QVERIFY(!model.activateApplication(untrustedId));
    QVERIFY(!model.activateApplication(QStringLiteral("org.example.absent")));
}

// A dragged window reports a new geometry on every frame and a terminal can
// retitle itself many times a second. The dock shows names, titles, icons and
// state, not geometry: a geometry, screen, maximized or fullscreen change must
// not rebuild every entry (and, through the backend, refresh every panel);
// a title change still must.
void DockModelTest::onlyEntryRelevantWindowChangesRebuildTheDock()
{
    WindowModel windowModel;
    WindowItem window;
    window.internalId = QStringLiteral("window-one");
    window.desktopFileName = QStringLiteral("org.example.editor.desktop");
    window.resourceClass = QStringLiteral("transient-editor");
    window.iconName = QStringLiteral("applications-development");
    window.caption = QStringLiteral("Editor");
    window.frameGeometry = QRect(10, 10, 400, 300);
    windowModel.setWindows({window});
    DockModel model(windowModel);
    QSignalSpy roles(&windowModel, &QAbstractItemModel::dataChanged);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy counts(&model, &DockModel::countChanged);

    for (int step = 1; step <= 30; ++step)
    {
        window.frameGeometry.moveTo(10 + step, 10 + step);
        QVERIFY(windowModel.updateWindow(window));
    }
    window.maximized = true;
    QVERIFY(windowModel.updateWindow(window));
    window.screenIndex = 1;
    QVERIFY(windowModel.updateWindow(window));
    QCOMPARE(resets.count(), 0);
    QCOMPARE(counts.count(), 0);
    // The window model names only the role that changed.
    QCOMPARE(roles.count(), 32);
    QCOMPARE(roles.first().at(2).value<QList<int>>(),
             QList<int>{WindowModel::FrameGeometryRole});

    window.caption = QStringLiteral("Editor - changed");
    QVERIFY(windowModel.updateWindow(window));
    QCOMPARE(resets.count(), 1);
    QCOMPARE(model.panelEntries(QStringLiteral("tasks")).constFirst().toMap()
                 .value(QStringLiteral("windowTitles")).toStringList(),
             QStringList{QStringLiteral("Editor - changed")});
}

// Desktop entries follow the Desktop Entry rules, not a generic INI reader's:
// that one returned a name containing a comma as a two-item list (read back
// as an empty name) and cut a value at a semicolon.
void DockModelTest::desktopEntriesAreReadWithDesktopEntryRules()
{
    const QString path = writeDesktopEntry(
        QStringLiteral("org.example.punctuation.desktop"),
        QStringLiteral("Foo, the Bar; and more"),
        QStringLiteral("applications-system"));
    QVERIFY(!path.isEmpty());
    WindowModel windowModel;
    DockModel model(windowModel);
    QVERIFY(model.pinUrl(QUrl::fromLocalFile(path)));
    const QVariantList entries = model.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 1);
    const QVariantMap entry = entries.constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Foo, the Bar; and more"));
    QCOMPARE(entry.value(QStringLiteral("iconName")).toString(),
             QStringLiteral("applications-system"));
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

QTEST_MAIN(DockModelTest)

#include "DockModelTest.moc"
