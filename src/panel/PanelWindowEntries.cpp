// PanelWindow: what a panel shows and what its icons do. Dock entries for
// native and free panels (a panel's own entries merged with running
// applications), icon overrides, activation and window actions, pins, panel
// content transactions and folder entries.

#include "PanelWindow.h"
#include "../model/PanelIconCapacity.h"

#include "../model/IconEntryIdentity.h"
#include "../model/FolderContentModel.h"
#include "IconOverrideTransaction.h"

#include <KDesktopFile>
#include <KIO/OpenUrlJob>
#include <KIO/PreviewJob>
#include <KFileItem>
#include <QCryptographicHash>
#include <QDir>
#include <QPixmap>
#include <QStandardPaths>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>

namespace
{
// One panel-specific free entry built from a local URL. A desktop file also
// records the desktop identity it launches, so a running instance of the same
// application can be merged into this entry on a hybrid panel.
QVariantMap freeUrlEntry(const QUrl &url,
                         const QString &entryId,
                         QMimeDatabase &mimeDatabase,
                         QString *desktopIdentity)
{
    const QFileInfo info(url.toLocalFile());
    QString iconName;
    QString displayName = info.fileName();
    if (info.isDir())
    {
        // Match Dolphin, including .directory artwork and special-folder icons.
        iconName = KFileItem(url).iconName();
    }
    else if (info.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0)
    {
        // KDE's reader follows the Desktop Entry rules; a generic INI reader
        // splits a name at its commas.
        const KDesktopFile desktopEntry(info.absoluteFilePath());
        displayName = desktopEntry.readName();
        if (displayName.isEmpty())
            displayName = info.completeBaseName();
        iconName = desktopEntry.readIcon();
        if (iconName.isEmpty())
            iconName = QStringLiteral("application-x-executable");
        if (desktopIdentity)
        {
            *desktopIdentity = ArchDock::IconEntryIdentity::forApplication(
                QString{}, info.fileName());
        }
    }
    else
    {
        iconName = mimeDatabase.mimeTypeForFile(info).iconName();
    }
    QVariantMap entry{
        {QStringLiteral("appId"), entryId},
        {QStringLiteral("panelEntryId"), entryId},
        {QStringLiteral("desktopFileName"), QString{}},
        {QStringLiteral("baseIconName"), iconName},
        {QStringLiteral("iconName"), iconName},
        {QStringLiteral("baseDisplayName"), displayName},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("pinned"), true},
        {QStringLiteral("running"), false},
        {QStringLiteral("active"), false},
        {QStringLiteral("minimized"), false},
        {QStringLiteral("windowCount"), 0},
        {QStringLiteral("windowIds"), QStringList{}},
        {QStringLiteral("windowTitles"), QStringList{}},
        {QStringLiteral("isFolder"), info.isDir()}};
    entry.insert(
        QStringLiteral("stableIdentity"),
        ArchDock::IconEntryIdentity::forEntry(entry));
    const QVariantMap launcher = DockModel::desktopEntryActions(info.absoluteFilePath());
    entry.insert(QStringLiteral("canPin"), true);
    entry.insert(QStringLiteral("canNewInstance"), launcher.value(QStringLiteral("canNewInstance")));
    entry.insert(QStringLiteral("desktopActions"), launcher.value(QStringLiteral("desktopActions")));
    return entry;
}
}

QVariantList PanelWindow::dockEntries(const QString &panelType) const
{
    return m_dockModel.panelEntries(panelType);
}

// The free-panel content semantics: `empty` shows nothing, `launcher` shows
// the panel's own ordered entries, `tasks` shows running applications, and
// `hybrid` shows the panel's entries with running-only applications after
// them. A running instance of a pinned desktop entry is merged into that
// entry: the entry keeps its own identity, label and glyph, gains the running
// state, and carries the application id under `runningAppId` so window
// actions reach the application model.
QVariantList PanelWindow::freePanelEntries(
    const ArchDock::PanelDefinition &definition) const
{
    QVariantList entries;
    const QString contentType = definition.content.type;
    if (contentType == QStringLiteral("empty"))
    {
        return entries;
    }

    QHash<QString, int> panelIndexByIdentity;
    if (contentType != QStringLiteral("tasks"))
    {
        QMimeDatabase mimeDatabase;
        for (const QString &entryId : definition.content.canonicalEntryOrder())
        {
            if (ArchDock::PanelContent::isUrlEntryId(entryId))
            {
                const QUrl url = QUrl::fromEncoded(entryId.mid(9).toUtf8());
                if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile()))
                {
                    continue;
                }
                QString desktopIdentity;
                QVariantMap entry = freeUrlEntry(
                    url, entryId, mimeDatabase, &desktopIdentity);
                if (!desktopIdentity.isEmpty())
                {
                    panelIndexByIdentity.insert(desktopIdentity, entries.size());
                }
                entries.append(entry);
                continue;
            }
            QVariantMap entry = m_dockModel.applicationEntry(entryId);
            if (entry.isEmpty())
            {
                continue;
            }
            entry.insert(QStringLiteral("pinned"), true);
            entry.insert(QStringLiteral("panelEntryId"), entryId);
            panelIndexByIdentity.insert(
                entry.value(QStringLiteral("stableIdentity")).toString(),
                entries.size());
            entries.append(entry);
        }
    }

    if (contentType == QStringLiteral("tasks") || contentType == QStringLiteral("hybrid"))
    {
        static const QStringList runningStateKeys{
            QStringLiteral("running"),
            QStringLiteral("active"),
            QStringLiteral("minimized"),
            QStringLiteral("windowCount"),
            QStringLiteral("windowIds"),
            QStringLiteral("windowTitles"),
            QStringLiteral("windowPreviews"),
        };
        for (const QVariant &value : m_dockModel.panelEntries(QStringLiteral("tasks")))
        {
            QVariantMap running = value.toMap();
            const auto merged = panelIndexByIdentity.constFind(
                running.value(QStringLiteral("stableIdentity")).toString());
            if (merged != panelIndexByIdentity.cend())
            {
                QVariantMap target = entries.at(*merged).toMap();
                for (const QString &key : runningStateKeys)
                {
                    target.insert(key, running.value(key));
                }
                target.insert(
                    QStringLiteral("runningAppId"),
                    running.value(QStringLiteral("appId")));
                entries[*merged] = target;
                continue;
            }
            running.insert(QStringLiteral("pinned"), false);
            running.insert(QStringLiteral("panelEntryId"), QString{});
            entries.append(running);
        }
    }
    return entries;
}

QVariantList PanelWindow::dockEntriesForPanel(const QString &panelId,
                                              const QString &panelType) const
{
    QVariantList entries;
    const std::optional<ArchDock::PanelDefinition> definition =
        runtimePanelDefinition(panelId);
    if (definition && definition->host.kind == ArchDock::PanelHostKind::FreeDesktop)
    {
        // A free host's applet stores `panelType=empty` as an ownership
        // marker; the record's content type is authoritative for what it
        // shows, so the requested type is deliberately ignored here.
        if (!definition.has_value())
        {
            return entries;
        }
        entries = freePanelEntries(*definition);
    }
    else
    {
        // A desktop preview, or Panel Studio's draft, may show other content
        // than the panel's Plasma widget was set up with.
        const bool previewed = (m_presetAudition &&
            m_presetAudition->previewDefinition(panelId).has_value()) ||
            (m_studioDraft && m_studioDraft->panel.identity.id == panelId);
        entries = m_dockModel.panelEntries(previewed && definition ? definition->content.type : panelType);
    }

    if (!definition.has_value())
    {
        return entries;
    }
    // A title change refetches every entry; the panel's icon style is
    // resolved once for all of them.
    PanelRegistry::IconStyleBatchCache styleCache;
    for (QVariant &value : entries)
    {
        QVariantMap entry = value.toMap();
        const auto overlay = entryOverlay(entry);
        for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) entry.insert(it.key(), it.value());
        if (!definition->content.showBadges) entry[QStringLiteral("badgeText")] = QString{};
        if (!definition->content.showProgress) entry[QStringLiteral("progress")] = -1.0;
        if (!definition->content.showTemporaryStatus) entry[QStringLiteral("temporaryStatus")] = QString{};
        const QString identity = ArchDock::IconEntryIdentity::forEntry(entry);
        entry.insert(QStringLiteral("stableIdentity"), identity);
        entry.insert(
            QStringLiteral("iconPropertiesSupported"),
            ArchDock::IconEntryIdentity::isValid(identity) &&
                entry.value(QStringLiteral("pinned")).toBool());
        const QVariantMap resolution =
            m_panelRegistry.resolveIconEntryOverride(*definition, entry, &styleCache);
        // The style definition travels once per entry, as
        // resolvedIconStyleDefinition below (what the renderer reads first).
        // Its copy inside the resolution doubled every entry refresh on the
        // bus, which panels refetch whenever a window changes.
        QVariantMap compactResolution = resolution;
        compactResolution.remove(QStringLiteral("iconStyleDefinition"));
        entry.insert(QStringLiteral("iconOverrideResolution"), compactResolution);
        entry.insert(
            QStringLiteral("iconOverride"),
            resolution.value(QStringLiteral("override")));
        entry.insert(
            QStringLiteral("iconOverrideApplied"),
            resolution.value(QStringLiteral("overrideApplied")));
        // How a folder's opening shows its contents, from the icon's own
        // override (see setPanelFolderView).
        entry.insert(QStringLiteral("folderView"),
            resolution.value(QStringLiteral("override")).toMap()
                .value(QStringLiteral("extensions")).toMap()
                .value(QStringLiteral("folderView")).toMap());
        entry.insert(
            QStringLiteral("resolvedGlyph"),
            resolution.value(QStringLiteral("resolvedGlyph")));
        entry.insert(
            QStringLiteral("resolvedLabel"),
            resolution.value(QStringLiteral("resolvedLabel")));
        entry.insert(
            QStringLiteral("tileEnabled"),
            resolution.value(QStringLiteral("tileEnabled")));
        entry.insert(
            QStringLiteral("animationProfileReference"),
            resolution.value(QStringLiteral("animationProfileReference")));
        entry.insert(
            QStringLiteral("resolvedIconStyleDefinition"),
            resolution.value(QStringLiteral("iconStyleDefinition")));
        entry.insert(
            QStringLiteral("iconName"),
            resolution.value(QStringLiteral("resolvedGlyph")));
        entry.insert(
            QStringLiteral("displayName"),
            resolution.value(QStringLiteral("resolvedLabel")));
        value = entry;
    }
    entries.append(statusEntriesFor(*definition));
    return ArchDock::PanelContentTransaction::segmentEntries(*definition, entries);
}

std::optional<QVariantMap> PanelWindow::iconEntryForIdentity(
    const QString &panelId,
    const QString &entryIdentity) const
{
    const QString identity = entryIdentity.trimmed();
    if (!ArchDock::IconEntryIdentity::isValid(identity))
    {
        return std::nullopt;
    }
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    if (!definition.has_value())
    {
        return std::nullopt;
    }
    for (const QVariant &value : dockEntriesForPanel(
             panelId, definition->content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() == identity)
        {
            return entry;
        }
    }
    return std::nullopt;
}

QVariantMap PanelWindow::iconOverrideSnapshot(
    const QString &panelId,
    const QVariantMap &entry) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    const QString identity = ArchDock::IconEntryIdentity::forEntry(entry);
    if (!definition.has_value() ||
        !ArchDock::IconEntryIdentity::isValid(identity))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"),
             definition.has_value()
                 ? QStringLiteral("invalid-entry-identity")
                 : QStringLiteral("panel-not-found")},
        };
    }

    const QVariantMap resolution =
        m_panelRegistry.resolveIconEntryOverride(*definition, entry);
    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"),
         QVariant::fromValue<qulonglong>(definition->settingsRevision)},
        {QStringLiteral("entryIdentity"), identity},
        {QStringLiteral("baseGlyph"),
         entry.value(QStringLiteral("baseIconName"),
                     entry.value(QStringLiteral("iconName")))},
        {QStringLiteral("baseLabel"),
         entry.value(QStringLiteral("baseDisplayName"),
                     entry.value(QStringLiteral("displayName")))},
        {QStringLiteral("override"),
         resolution.value(QStringLiteral("override"))},
        {QStringLiteral("resolution"), resolution},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
    };
}

QVariantMap PanelWindow::iconOverrideSnapshotForIdentity(
    const QString &panelId,
    const QString &entryIdentity) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    const QString identity = entryIdentity.trimmed();
    if (!definition.has_value() ||
        !ArchDock::IconEntryIdentity::isValid(identity))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"),
             definition.has_value()
                 ? QStringLiteral("invalid-entry-identity")
                 : QStringLiteral("panel-not-found")},
        };
    }

    const std::optional<QVariantMap> entry = iconEntryForIdentity(
        panelId, identity);
    if (!entry.has_value())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"), QStringLiteral("entry-not-found")},
        };
    }
    if (!entry->value(QStringLiteral("iconPropertiesSupported")).toBool())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"), QStringLiteral("entry-not-supported")},
        };
    }
    return iconOverrideSnapshot(panelId, *entry);
}

QVariantMap PanelWindow::applyIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity,
    const QVariantMap &overrideValues)
{
    return commitIconOverrideTransaction(
        panelId,
        expectedRevision,
        entryIdentity,
        overrideValues,
        false);
}

QVariantMap PanelWindow::resetIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity)
{
    return commitIconOverrideTransaction(
        panelId,
        expectedRevision,
        entryIdentity,
        {},
        true);
}

QVariantMap PanelWindow::commitIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity,
    const QVariantMap &overrideValues,
    bool reset)
{
    if (profileBusy())
        return {{QStringLiteral("success"), false},
                {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

    ArchDock::IconOverrideTransactionOutcome outcome;
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    if (!current.has_value())
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.errorCode = QStringLiteral("panel-not-found");
        outcome.errorMessage = QStringLiteral("the target panel does not exist");
        return outcome.toVariantMap();
    }

    bool currentEntry = false;
    bool iconPropertiesSupported = false;
    QVariantMap entryAfterCommit;
    for (const QVariant &value : dockEntriesForPanel(panelId, current->content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() ==
            entryIdentity.trimmed())
        {
            currentEntry = true;
            iconPropertiesSupported = entry.value(
                QStringLiteral("iconPropertiesSupported")).toBool();
            entryAfterCommit = entry;
            break;
        }
    }
    if (!currentEntry)
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.previousRevision = current->settingsRevision;
        outcome.revision = current->settingsRevision;
        outcome.reset = reset;
        outcome.errorCode = QStringLiteral("entry-not-found");
        outcome.errorMessage = QStringLiteral(
            "the target entry is no longer present on this panel");
        return outcome.toVariantMap();
    }
    if (!iconPropertiesSupported)
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.previousRevision = current->settingsRevision;
        outcome.revision = current->settingsRevision;
        outcome.reset = reset;
        outcome.errorCode = QStringLiteral("entry-not-supported");
        outcome.errorMessage = QStringLiteral(
            "running-only or transient entries cannot store icon properties");
        return outcome.toVariantMap();
    }

    const auto draft = ArchDock::IconOverrideTransaction::prepare(
        *current,
        {panelId,
         expectedRevision,
         entryIdentity,
         overrideValues,
         reset},
        &outcome,
        [this](const QString &styleReference)
        {
            const QVariantMap style =
                m_panelRegistry.iconStyleDefinition(styleReference);
            return style.value(QStringLiteral("selectionStatus")).toString() ==
                    QStringLiteral("selected") &&
                style.value(QStringLiteral("resolvedStyleId")).toString() ==
                    styleReference;
        });
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }

    QString persistenceError;
    if (!m_panelRegistry.persistPanelDefinitionTransaction(
            draft->previousPanel,
            draft->candidatePanel,
            m_settings.transactionSnapshot(),
            &persistenceError))
    {
        const std::optional<ArchDock::PanelDefinition> latest =
            m_panelRegistry.panelDefinition(panelId);
        const bool conflict = latest.has_value() &&
            latest->settingsRevision != expectedRevision;
        outcome.status = conflict
            ? ArchDock::IconOverrideTransactionStatus::RevisionConflict
            : ArchDock::IconOverrideTransactionStatus::PersistenceFailed;
        outcome.errorCode = conflict
            ? QStringLiteral("stale-revision")
            : QStringLiteral("persistence-failed");
        outcome.errorMessage = persistenceError;
        return outcome.toVariantMap();
    }

    outcome.status = ArchDock::IconOverrideTransactionStatus::Succeeded;
    outcome.revision = draft->candidatePanel.settingsRevision;
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(false);
    QVariantMap result = outcome.toVariantMap();
    for (const QVariant &value : dockEntriesForPanel(
             panelId, draft->candidatePanel.content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() ==
            entryIdentity.trimmed())
        {
            entryAfterCommit = entry;
            break;
        }
    }
    result.insert(
        QStringLiteral("resolution"),
        entryAfterCommit.value(QStringLiteral("iconOverrideResolution")));
    return result;
}

bool PanelWindow::activateDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    if (appId.startsWith(QStringLiteral("free-url:")))
    {
        return m_dockModel.openUrl(
            QUrl::fromEncoded(appId.mid(9).toUtf8()));
    }
    return m_dockModel.activateApplication(appId);
}

// A short launch status on the entry's overlay, keyed by its desktop file.
void PanelWindow::showLaunchStatus(const QString &appId, const QString &text)
{
    for (const auto &value : m_dockModel.panelEntries(QStringLiteral("hybrid")))
    {
        const auto entry = value.toMap();
        if (entry.value(QStringLiteral("appId")).toString() != appId) continue;
        QString desktop = entry.value(QStringLiteral("desktopFileName"), appId).toString();
        if (!desktop.endsWith(QStringLiteral(".desktop"))) desktop += QStringLiteral(".desktop");
        m_overlayModel.setTemporaryStatus(QFileInfo(desktop).fileName(), text);
    }
}

// The same activation as activateDockEntry, but reporting what happened.
//
// `succeeded` means a file or URL entry was handed to its opener, or, for a
// D-Bus caller, that KDE's launcher started the application: that reply waits
// for the launcher. `requested` means the activation was handed on and cannot
// be confirmed: a window raise goes to the compositor, which does not report
// back, and a direct (non-D-Bus) caller's application start, whose later
// failure shows as a "Launch failed" status. The caller must not treat it as
// success. `failed` means the entry could not be launched (unknown entry,
// missing program, untrusted desktop file, or the launcher's error). The
// bool-returning activateDockEntry is unchanged, so existing callers keep
// their exact contract.
QVariantMap PanelWindow::activateDockEntryOutcome(const QString &appId)
{
    QVariantMap result;
    result.insert(QStringLiteral("appId"), appId);
    result.insert(QStringLiteral("reason"), QString{});
    if (appId.startsWith(QStringLiteral("status:")))
        return {{QStringLiteral("outcome"), QStringLiteral("failed")},
                {QStringLiteral("reason"), QStringLiteral("informational-entry")}};

    if (appId.startsWith(QStringLiteral("free-url:")))
    {
        const bool opened =
            m_dockModel.openUrl(QUrl::fromEncoded(appId.mid(9).toUtf8()));
        result.insert(QStringLiteral("outcome"),
                      opened ? QStringLiteral("succeeded")
                             : QStringLiteral("failed"));
        if (!opened)
        {
            result.insert(QStringLiteral("reason"),
                          QStringLiteral("no-url-handler"));
        }
        const auto desktop = QFileInfo(QUrl::fromEncoded(appId.mid(9).toUtf8()).toLocalFile()).fileName();
        m_overlayModel.setTemporaryStatus(desktop,
            opened ? tr("Launch request accepted") : tr("Launch request failed"));
        return result;
    }

    QString status;
    switch (m_dockModel.activateApplicationOutcome(appId))
    {
    case DockModel::ActivationOutcome::LaunchRequested:
        // KDE's launcher starts the program asynchronously. A D-Bus caller
        // gets its answer when the launcher reports (DockModel::launchFinished,
        // handled in the constructor): "succeeded" only for a start that
        // happened. A direct caller gets "requested" now.
        if (calledFromDBus())
        {
            setDelayedReply(true);
            m_pendingLaunchReplies[appId].append(message());
            return {};
        }
        result.insert(QStringLiteral("outcome"), QStringLiteral("requested"));
        result.insert(QStringLiteral("reason"), QStringLiteral("launch-requested"));
        break;
    case DockModel::ActivationOutcome::ActivationRequested:
        result.insert(QStringLiteral("outcome"), QStringLiteral("requested"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("activation-not-verifiable"));
        break;
    case DockModel::ActivationOutcome::Failed:
        result.insert(QStringLiteral("outcome"), QStringLiteral("failed"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("launch-failed"));
        status = tr("Launch failed");
        break;
    case DockModel::ActivationOutcome::UnknownEntry:
        result.insert(QStringLiteral("outcome"), QStringLiteral("failed"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("unknown-entry"));
        status = tr("Launch failed");
        break;
    }
    if (!status.isEmpty())
    {
        showLaunchStatus(appId, status);
    }
    return result;
}

bool PanelWindow::activateDockWindow(const QString &appId, const QString &windowId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.activateApplicationWindow(appId, windowId);
}

bool PanelWindow::requestDockWindowAction(
    const QString &appId, const QString &windowId, const QString &action)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.requestApplicationWindowAction(appId, windowId, action);
}

bool PanelWindow::launchDockEntry(
    const QString &panelId, const QString &appId, const QString &desktopAction)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
    {
        return false;
    }
    for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
    {
        const auto entry = value.toMap();
        if (entry.value(QStringLiteral("appId")).toString() != appId)
        {
            continue;
        }
        QString desktopPath;
        if (ArchDock::PanelContent::isUrlEntryId(appId))
        {
            const auto url = QUrl::fromEncoded(appId.mid(9).toUtf8());
            if (url.isLocalFile())
                desktopPath = url.toLocalFile();
        }
        else
        {
            desktopPath = m_dockModel.desktopFileForApplication(appId);
        }
        return m_dockModel.launchDesktopEntry(desktopPath, desktopAction);
    }
    return false;
}

bool PanelWindow::minimizeDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.minimizeApplication(appId);
}

bool PanelWindow::closeDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.closeApplication(appId);
}

bool PanelWindow::closeAllDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.closeAllApplication(appId);
}

bool PanelWindow::togglePinnedDockEntry(const QString &appId)
{
    return m_dockModel.togglePinnedApplication(appId);
}

bool PanelWindow::moveDockEntryBefore(const QString &appId, const QString &beforeAppId)
{
    // Free entries are panel-specific and reorder through
    // movePanelEntryBefore; the shared application model cannot order them
    // and must never receive them.
    if (ArchDock::PanelContent::isUrlEntryId(appId) ||
        ArchDock::PanelContent::isUrlEntryId(beforeAppId))
    {
        return false;
    }
    return m_dockModel.moveApplicationBefore(appId, beforeAppId);
}

bool PanelWindow::pinDockUrl(const QString &url)
{
    return pinDockUrls({url});
}

bool PanelWindow::pinDockUrls(const QStringList &urls)
{
    bool pinnedAny = false;
    for (const QString &urlString : urls)
    {
        const QUrl url = QUrl::fromUserInput(urlString);
        if (url.isValid() && !url.isEmpty())
        {
            pinnedAny = m_dockModel.pinUrl(url) || pinnedAny;
        }
    }
    return pinnedAny;
}

bool PanelWindow::pinPanelUrls(const QString &panelId, const QStringList &urls)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
        QStringLiteral("free"))
    {
        return pinDockUrls(urls);
    }
    return addPanelEntries(panelId, urls);
}

bool PanelWindow::removePanelContent(const QString &panelId, const QString &entryId)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
        QStringLiteral("free"))
    {
        return false;
    }
    return removePanelEntry(panelId, entryId);
}

bool PanelWindow::commitPanelContentTransaction(
    const QString &panelId,
    const ArchDock::PanelContentRequest &request)
{
    if (profileBusy()) return false;

    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    if (!current.has_value())
    {
        return false;
    }
    ArchDock::PanelContentOutcome outcome;
    ArchDock::PanelContentRequest drawnRequest = request;
    drawnRequest.seenTrackFactor = seenRoomFactor(*current);
    const std::optional<ArchDock::PanelDefinition> candidate =
        ArchDock::PanelContentTransaction::prepare(*current, drawnRequest, &outcome);
    if (!outcome.success)
    {
        qWarning().noquote() << "Arch Dock panel content"
                             << ArchDock::PanelContentTransaction::operationName(
                                    request.operation)
                             << "refused for" << panelId << outcome.errorCode
                             << outcome.errorMessage;
        return false;
    }
    if (!candidate.has_value())
    {
        // Valid but nothing to change: no revision is spent.
        return true;
    }
    QString persistenceError;
    if (!m_panelRegistry.persistPanelDefinitionTransaction(
            *current, *candidate, m_settings.transactionSnapshot(), &persistenceError))
    {
        qWarning().noquote() << "Arch Dock panel content" << panelId
                             << "could not be persisted:" << persistenceError;
        return false;
    }
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(false);
    return true;
}

QVariantMap PanelWindow::panelIconRoom(const QString &panelId) const
{
    const std::optional<ArchDock::PanelDefinition> definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("panel-not-found")}};
    const int count = ArchDock::panelIconCount(*definition);
    // As it is now, and the size it may change to, as Panel Studio's limits.
    const double seen = seenRoomFactor(*definition);
    const double resized = predictedSeenRoom(*definition);
    const auto capacity = ArchDock::panelIconCapacity(*definition, seen);
    const auto radius = ArchDock::panelMinimumRadius(*definition, resized);
    const auto size = ArchDock::panelMaximumIconSize(*definition, resized);
    return {{QStringLiteral("success"), true},
            {QStringLiteral("capacity"), capacity ? *capacity : -1},
            {QStringLiteral("count"), count},
            {QStringLiteral("seen"), seen},
            {QStringLiteral("minimumRadius"), radius ? *radius : -1},
            {QStringLiteral("maximumIconSize"), size ? *size : -1}};
}

bool PanelWindow::addPanelEntries(const QString &panelId, const QStringList &entries)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::Add;
    for (const QString &item : entries)
    {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        QUrl url;
        if (ArchDock::PanelContent::isUrlEntryId(trimmed))
        {
            url = QUrl::fromEncoded(trimmed.mid(9).toUtf8());
        }
        else if (trimmed.contains(QStringLiteral("://")) ||
                 trimmed.startsWith(QLatin1Char('/')))
        {
            url = QUrl::fromUserInput(trimmed);
        }
        else
        {
            // An application id is pinned to the panel as its desktop entry,
            // so the panel owns a plain local file rather than a reference
            // into the shared model.
            const QString desktopFile = m_dockModel.desktopFileForApplication(trimmed);
            if (desktopFile.isEmpty())
            {
                continue;
            }
            url = QUrl::fromLocalFile(desktopFile);
        }
        if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile()))
        {
            continue;
        }
        request.urls.append(url.toString());
    }
    if (request.urls.isEmpty())
    {
        return false;
    }
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::setPanelDecorationSeat(const QString &panelId, const QString &entryId)
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return false;
    const QVariantMap result = applyPanelSettingsTransaction(panelId, definition->settingsRevision,
        {{QStringLiteral("panelDecorationSeat"), entryId.trimmed()}}, {});
    return result.value(QStringLiteral("success")).toBool();
}

bool PanelWindow::seatPanelUrl(const QString &panelId, const QString &url)
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    const QUrl parsed = QUrl::fromUserInput(url.trimmed());
    if (!definition || !parsed.isValid() || parsed.isEmpty())
        return false;
    // On the panel first (already there, it stays as it is), then in the
    // seat, by the id the panel knows it by.
    pinPanelUrls(panelId, {parsed.toString()});
    const QString entryId = definition->host.kind == ArchDock::PanelHostKind::NativeEdge
        ? m_dockModel.applicationIdForUrl(parsed)
        : ArchDock::PanelContent::urlEntryId(parsed.toString());
    return !entryId.isEmpty() && setPanelDecorationSeat(panelId, entryId);
}

bool PanelWindow::removePanelEntry(const QString &panelId, const QString &entryId)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::Remove;
    request.entryId = entryId;
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::movePanelEntryBefore(const QString &panelId,
                                       const QString &entryId,
                                       const QString &beforeEntryId)
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return false;
    if (definition->host.kind == ArchDock::PanelHostKind::NativeEdge)
    {
        QHash<QString, QString> owners;
        for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
        {
            const auto entry = value.toMap();
            owners.insert(entry.value(QStringLiteral("appId")).toString(),
                entry.value(QStringLiteral("segmentId")).toString());
        }
        if (!owners.contains(entryId) || (!beforeEntryId.isEmpty() &&
            (!owners.contains(beforeEntryId) || owners.value(entryId) != owners.value(beforeEntryId))))
            return false;
        return m_dockModel.moveApplicationBefore(entryId, beforeEntryId);
    }
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::MoveBefore;
    request.entryId = entryId;
    request.beforeEntryId = beforeEntryId;
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::setPanelEntryOrder(const QString &panelId, const QStringList &entryIds)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::SetOrder;
    request.order = entryIds;
    return commitPanelContentTransaction(panelId, request);
}

QStringList PanelWindow::panelEntryOrder(const QString &panelId) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    return definition.has_value()
        ? definition->content.canonicalEntryOrder()
        : QStringList{};
}

QVariantList PanelWindow::dockFolderEntries(const QString &appId) const
{
    QVariantList entries = m_dockModel.folderEntriesForApplication(appId);
    for (QVariant &entry : entries)
    {
        QVariantMap value = entry.toMap();
        value.insert(QStringLiteral("url"), value.value(QStringLiteral("url")).toUrl().toString());
        entry = value;
    }
    return entries;
}

QUrl PanelWindow::panelFolderUrl(const QString &panelId, const QString &appId) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return {};
    for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
    {
        if (value.toMap().value(QStringLiteral("appId")).toString() != appId)
            continue;
        if (ArchDock::PanelContent::isUrlEntryId(appId))
            return QUrl::fromEncoded(ArchDock::PanelContent::urlFromEntryId(appId).toUtf8());
        return m_dockModel.urlForApplicationId(appId);
    }
    return {};
}

namespace
{
// Where a child's thumbnail is kept: by its address, date and size, so a
// changed file gets a new one.
QString folderPreviewPath(const QVariantMap &child)
{
    const QByteArray key = QCryptographicHash::hash(
        (child.value(QStringLiteral("url")).toString() + QLatin1Char('|')
         + QString::number(child.value(QStringLiteral("modified")).toLongLong()) + QLatin1Char('|')
         + QString::number(child.value(QStringLiteral("size")).toLongLong())).toUtf8(),
        QCryptographicHash::Sha1).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/folder-previews/") + QString::fromLatin1(key) + QStringLiteral(".png");
}

QString childSuffix(const QVariantMap &child)
{
    const QString name = child.value(QStringLiteral("name")).toString();
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? name.mid(dot + 1).toLower() : QString{};
}
}

QVariantMap PanelWindow::panelFolderEntry(const QString &panelId, const QString &appId) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return {};
    for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
        if (value.toMap().value(QStringLiteral("appId")).toString() == appId)
            return value.toMap();
    return {};
}

QVariantMap PanelWindow::panelFolderSnapshot(const QString &panelId, const QString &appId) const
{
    const QUrl folder = panelFolderUrl(panelId, appId);
    QVariantMap result = ArchDock::FolderContentModel::snapshot(folder);
    if (folder.isEmpty())
        result[QStringLiteral("errorCode")] = QStringLiteral("folder-entry-unavailable");
    // In the order the folder's icon asks for (Icons > Sort By), folders
    // first unless told otherwise, and with the children's thumbnails for
    // pictures when it asks for previews and they are made already.
    const QVariantMap view = panelFolderEntry(panelId, appId).value(QStringLiteral("folderView")).toMap();
    const QString sort = view.value(QStringLiteral("sort"), QStringLiteral("name")).toString();
    const bool descending = view.value(QStringLiteral("descending")).toBool();
    const bool foldersFirst = view.value(QStringLiteral("foldersFirst"), true).toBool();
    const bool previews = view.value(QStringLiteral("previews")).toBool();
    QVariantList entries = result.value(QStringLiteral("entries")).toList();
    std::stable_sort(entries.begin(), entries.end(), [&](const QVariant &a, const QVariant &b)
    {
        const QVariantMap left = a.toMap(), right = b.toMap();
        const bool leftFolder = left.value(QStringLiteral("isDirectory")).toBool();
        const bool rightFolder = right.value(QStringLiteral("isDirectory")).toBool();
        if (foldersFirst && leftFolder != rightFolder)
            return leftFolder;
        int order = 0;
        if (sort == QStringLiteral("type"))
            order = QString::compare(childSuffix(left), childSuffix(right));
        else if (sort == QStringLiteral("date"))
            order = left.value(QStringLiteral("modified")).toLongLong() < right.value(QStringLiteral("modified")).toLongLong() ? -1
                : left.value(QStringLiteral("modified")).toLongLong() > right.value(QStringLiteral("modified")).toLongLong() ? 1 : 0;
        else if (sort == QStringLiteral("size"))
            order = left.value(QStringLiteral("size")).toLongLong() < right.value(QStringLiteral("size")).toLongLong() ? -1
                : left.value(QStringLiteral("size")).toLongLong() > right.value(QStringLiteral("size")).toLongLong() ? 1 : 0;
        if (order == 0)
            order = left.value(QStringLiteral("name")).toString().toLower()
                .localeAwareCompare(right.value(QStringLiteral("name")).toString().toLower());
        return descending ? order > 0 : order < 0;
    });
    int pending = 0;
    if (previews)
        for (QVariant &value : entries)
        {
            QVariantMap child = value.toMap();
            if (child.value(QStringLiteral("isDirectory")).toBool() || !child.value(QStringLiteral("selectable")).toBool())
                continue;
            const QString path = folderPreviewPath(child);
            if (QFileInfo::exists(path))
            {
                child.insert(QStringLiteral("iconName"), path);
                child.insert(QStringLiteral("preview"), true);
            }
            else
                ++pending;
            value = child;
        }
    result.insert(QStringLiteral("entries"), entries);
    result.insert(QStringLiteral("previewsPending"), pending);
    result.insert(QStringLiteral("folderView"), view);
    result.insert(QStringLiteral("panelId"), panelId);
    result.insert(QStringLiteral("appId"), appId);
    // Where the other free panels are drawn: the folder's opening keeps
    // clear of them.
    result.insert(QStringLiteral("freePanelAreas"), freePanelAreas(panelId));
    return result;
}

QVariantMap PanelWindow::setPanelFolderView(const QString &panelId, const QString &appId, const QVariantMap &values)
{
    const QVariantMap entry = panelFolderEntry(panelId, appId);
    const QString identity = entry.value(QStringLiteral("stableIdentity")).toString();
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (identity.isEmpty() || !definition)
        return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("entry-not-found")}};
    // Into the icon's own override, beside whatever else it holds.
    QVariantMap override = entry.value(QStringLiteral("iconOverride")).toMap();
    QVariantMap extensions = override.value(QStringLiteral("extensions")).toMap();
    QVariantMap view = extensions.value(QStringLiteral("folderView")).toMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        view.insert(it.key(), it.value());
    extensions.insert(QStringLiteral("folderView"), view);
    override.insert(QStringLiteral("extensions"), extensions);
    return commitIconOverrideTransaction(panelId, definition->settingsRevision, identity, override, false);
}

bool PanelWindow::requestFolderPreviews(const QString &panelId, const QString &appId)
{
    const QUrl folder = panelFolderUrl(panelId, appId);
    if (folder.isEmpty())
        return false;
    const QVariantList entries = ArchDock::FolderContentModel::snapshot(folder).value(QStringLiteral("entries")).toList();
    KFileItemList items;
    QHash<QUrl, QString> paths;
    for (const QVariant &value : entries)
    {
        const QVariantMap child = value.toMap();
        if (child.value(QStringLiteral("isDirectory")).toBool() || !child.value(QStringLiteral("selectable")).toBool())
            continue;
        const QString path = folderPreviewPath(child);
        if (QFileInfo::exists(path))
            continue;
        const QUrl url(child.value(QStringLiteral("url")).toString());
        items.append(KFileItem(url));
        paths.insert(url, path);
    }
    if (items.isEmpty())
        return true;
    QDir().mkpath(QFileInfo(paths.cbegin().value()).absolutePath());
    // KIO makes the thumbnails as Dolphin's previews are made; each is kept
    // as it comes, and the opening asks for its contents again meanwhile.
    KIO::PreviewJob *job = KIO::filePreview(items, QSize(256, 256));
    connect(job, &KIO::PreviewJob::gotPreview, this, [paths](const KFileItem &item, const QPixmap &pixmap)
    {
        const QString path = paths.value(item.url());
        if (!path.isEmpty())
            pixmap.save(path, "PNG");
    });
    job->start();
    return true;
}

QVariantMap PanelWindow::openPanelFolderChild(const QString &panelId, const QString &appId,
                                            const QString &childId)
{
    QVariantMap result{{QStringLiteral("success"), false},
                       {QStringLiteral("status"), QStringLiteral("rejected")},
                       {QStringLiteral("errorCode"), QStringLiteral("folder-entry-unavailable")}};
    const QUrl folder = panelFolderUrl(panelId, appId);
    if (folder.isEmpty())
        return result;
    QString error;
    const QUrl child = ArchDock::FolderContentModel::resolveChild(folder, childId, &error);
    if (child.isEmpty())
    {
        result[QStringLiteral("errorCode")] = error;
        return result;
    }
    auto *job = new KIO::OpenUrlJob(child, this);
    job->setRunExecutables(false);
    job->setShowOpenOrExecuteDialog(false);
    connect(job, &KJob::result, this, [](KJob *completed) {
        if (completed->error())
            qWarning() << "Arch Dock folder document open failed:" << completed->errorText();
    });
    job->start();
    // Native opening is asynchronous; acceptance is not proof of app startup.
    result[QStringLiteral("success")] = true;
    result[QStringLiteral("status")] = QStringLiteral("accepted");
    result[QStringLiteral("errorCode")] = QString{};
    return result;
}

bool PanelWindow::openDockUrl(const QString &urlString)
{
    const QUrl url = QUrl::fromUserInput(urlString);
    return url.isValid() && !url.isEmpty() && m_dockModel.openUrl(url);
}
