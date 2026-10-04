#include "presets/PresetApplication.h"
#include "presets/PresetDefaultStore.h"
#include "presets/PresetPreviewRecovery.h"
#include "presets/PresetPreviewSession.h"
#include "presets/UserPresetStore.h"
#include "PresetTestSupport.h"

#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <functional>

using namespace ArchDock;

class PresetPreviewSessionTest final : public QObject
{
    Q_OBJECT
private slots:
    void panelPreparationPreservesIndependentIcons();
    void iconPreparationRejectsEveryPanelMutation();
    void customizedSnapshotsAreIndependentUserResources();
    void styleCustomizationLeavesNoStaleOverrides();
    void lineageChangesOnlyForGovernedSettings();
    void defaultsAreSeparateAndVersioned();
    void defaultsRejectCorruptOrUnwritableStores();
    void recoveryJournalPreservesSnapshotAndConversionProgress();
    void recoveryJournalRejectsUnsafeReplacement();
    void sessionTransitionsCommitAndExactRollback();
    void sessionGuardsAndReplacement();
    void sessionTemporaryConversionAndFailureCleanup();
    void sessionIconIsolationAndUserActions();
    void sessionInterruptionsKeepRecoverableEvidence();
};

void PresetPreviewSessionTest::panelPreparationPreservesIndependentIcons()
{
    const auto panel = PanelPresetDefinition::fromVariantMap(PresetTestSupport::panelPresetMap());
    const auto icon = IconPresetDefinition::fromVariantMap(PresetTestSupport::iconPresetMap());
    QVERIFY(panel && icon);
    auto before = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
                                          QStringLiteral("bottom"), true);
    before.presetOrigin = PanelPresetOrigin{};
    before.presetOrigin->iconPresetId = QStringLiteral("user-chosen");
    before.presetOrigin->iconPresetRevision = 4;
    before.iconStyle.styleReference = QStringLiteral("neon-orange");
    before.motion.iconProfile = QStringLiteral("shake");
    before = before.normalized();
    const auto preserved = PresetApplication::preparePanel(before, {}, *panel, {}, {});
    QVERIFY(preserved);
    QCOMPARE(preserved->previousPanel, before);
    QCOMPARE(preserved->candidatePanel.motion.iconProfile, before.motion.iconProfile);
    QCOMPARE(preserved->candidatePanel.iconStyle.styleReference, before.iconStyle.styleReference);
    QCOMPARE(preserved->candidatePanel.iconStyle.globalDefaults, before.iconStyle.globalDefaults);
    QCOMPARE(preserved->candidatePanel.iconStyle.perEntryOverrides, before.iconStyle.perEntryOverrides);
    QCOMPARE(preserved->candidatePanel.iconStyle.size, panel->panel.configuration.iconStyle.size);
    QCOMPARE(preserved->candidatePanel.presetOrigin->iconPresetId, QStringLiteral("user-chosen"));
    QCOMPARE(preserved->candidatePanel.settingsRevision, before.settingsRevision + 1);
    const auto paired = PresetApplication::preparePanel(before, {}, *panel, {}, icon);
    QVERIFY(paired);
    QCOMPARE(paired->candidatePanel.iconStyle.styleReference, icon->icon.iconStyleId);
    QCOMPARE(paired->candidatePanel.motion.iconProfile, icon->icon.motion.profileId);
    QCOMPARE(paired->candidatePanel.presetOrigin->panelPresetId, panel->identity.id);
    QCOMPARE(paired->candidatePanel.presetOrigin->iconPresetId, icon->identity.id);
}

void PresetPreviewSessionTest::iconPreparationRejectsEveryPanelMutation()
{
    const auto icon = IconPresetDefinition::fromVariantMap(PresetTestSupport::iconPresetMap());
    QVERIFY(icon);
    auto before = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
                                          QStringLiteral("bottom"), true);
    before.motion.panelProfile = QStringLiteral("collapse");
    before.motion.reducedMotion = true;
    before = before.normalized();
    const auto draft = PresetApplication::prepareIcon(before, {}, *icon,
        {{QStringLiteral("animationSpeed"), 1.4}});
    QVERIFY(draft);
    QVERIFY(PresetApplication::iconOnlyChange(before, draft->candidatePanel));
    QCOMPARE(draft->candidatePanel.motion.panelProfile, before.motion.panelProfile);
    QCOMPARE(draft->candidatePanel.motion.reducedMotion, before.motion.reducedMotion);
    const QList<std::function<void(PanelDefinition &)>> mutations{
        [](auto &p) { p.surface.completeThemeId = QStringLiteral("changed"); },
        [](auto &p) { p.placement.width++; },
        [](auto &p) { p.layout.scale += 0.1; },
        [](auto &p) { p.visibility.visible = !p.visibility.visible; },
        [](auto &p) { p.content.type = QStringLiteral("launcher"); },
        [](auto &p) { p.presentation.revealHandle = QStringLiteral("bar"); },
        [](auto &p) { p.motion.panelProfile = QStringLiteral("changed"); },
        [](auto &p) { p.motion.reducedMotion = !p.motion.reducedMotion; },
        [](auto &p) { p.iconStyle.size++; },
        [](auto &p) { p.host.screenIndex++; },
        [](auto &p) { p.presetOrigin->panelPresetId = QStringLiteral("changed"); }
    };
    for (const auto &mutate : mutations)
    {
        auto candidate = draft->candidatePanel;
        mutate(candidate);
        QVERIFY(!PresetApplication::iconOnlyChange(before, candidate));
    }
    QString error;
    QVERIFY(!PresetApplication::prepareIcon(before, {}, *icon,
        {{QStringLiteral("width"), 900}}, &error));
    QCOMPARE(error, QStringLiteral("icon-only-violation"));
}

void PresetPreviewSessionTest::customizedSnapshotsAreIndependentUserResources()
{
    const auto panel = PanelPresetDefinition::fromVariantMap(PresetTestSupport::panelPresetMap());
    const auto icon = IconPresetDefinition::fromVariantMap(PresetTestSupport::iconPresetMap());
    QVERIFY(panel && icon);
    const auto originalPanel = panel->toVariantMap();
    const auto originalIcon = icon->toVariantMap();
    const auto before = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
                                                 QStringLiteral("bottom"), true);
    const auto draft = PresetApplication::preparePanel(before, {}, *panel,
        {{QStringLiteral("width"), 980}}, icon);
    QVERIFY(draft);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    UserPresetStore store(temporary.path());
    QString error;
    const auto panelCopy = store.save(PresetApplication::panelSnapshot(
        *panel, draft->candidatePanel, QStringLiteral("My panel")), &error);
    QVERIFY2(panelCopy, qPrintable(error));
    const auto iconCopy = store.save(PresetApplication::iconSnapshot(
        *icon, draft->candidatePanel, QStringLiteral("My icons")), &error);
    QVERIFY2(iconCopy, qPrintable(error));
    QVERIFY(PresetIdentity::isUserId(panelCopy->identity.id));
    QVERIFY(PresetIdentity::isUserId(iconCopy->identity.id));
    QCOMPARE(panelCopy->panel.configuration.placement.width, 980);
    QCOMPARE(panelCopy->identity.derivedFromPresetId, panel->identity.id);
    QCOMPARE(iconCopy->icon.visualOverrides, icon->icon.visualOverrides);
    QCOMPARE(store.panelPresets().size(), 1);
    QCOMPARE(store.iconPresets().size(), 1);
    const auto second = store.save(PresetApplication::panelSnapshot(
        *panelCopy, draft->candidatePanel, QStringLiteral("Another panel")), &error);
    QVERIFY2(second, qPrintable(error));
    QVERIFY(second->identity.id != panelCopy->identity.id);
    QCOMPARE(store.panelPresets().size(), 2);
    QCOMPARE(panel->toVariantMap(), originalPanel);
    QCOMPARE(icon->toVariantMap(), originalIcon);
}

void PresetPreviewSessionTest::styleCustomizationLeavesNoStaleOverrides()
{
    const auto panel = PanelPresetDefinition::fromVariantMap(PresetTestSupport::panelPresetMap());
    const auto icon = IconPresetDefinition::fromVariantMap(PresetTestSupport::iconPresetMap());
    QVERIFY(panel && icon);
    QVERIFY(!icon->icon.visualOverrides.isEmpty());
    const auto before = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
        QStringLiteral("bottom"), true).normalized();
    const auto changed = PresetApplication::prepareIcon(before, {}, *icon,
        {{QStringLiteral("iconStyle"), QStringLiteral("plain-original")}, {QStringLiteral("animationSpeed"), 1.4}});
    QVERIFY(changed);
    QVERIFY(PresetApplication::iconOnlyChange(before, changed->candidatePanel));
    QVERIFY(!changed->candidatePanel.iconStyle.globalDefaults.contains(QStringLiteral("presetOverrides")));
    QVERIFY(changed->candidatePanel.presetOrigin->customizedAfterApply);
    QTemporaryDir temporary;
    UserPresetStore store(temporary.path());
    QString error;
    const auto saved = store.save(PresetApplication::iconSnapshot(*icon, changed->candidatePanel,
        QStringLiteral("Plain customized icons")), &error);
    QVERIFY2(saved, qPrintable(error));
    QCOMPARE(saved->icon.iconStyleId, QStringLiteral("plain-original"));
    QCOMPARE(saved->icon.motion.speed, 1.4);
    QVERIFY(saved->icon.visualOverrides.isEmpty());
    QVERIFY(saved->icon.stateOverrides.isEmpty());
    QVERIFY(saved->compatibility.requiredStyleCapabilities.isEmpty());
    QCOMPARE(store.iconPresets().first(), *saved);
    const auto applied = PresetApplication::prepareIcon(before, {}, *icon, {});
    QVERIFY(applied);
    const auto panelDraft = PresetApplication::preparePanel(applied->candidatePanel, {}, *panel,
        {{QStringLiteral("iconStyle"), QStringLiteral("metallic-red")}}, {});
    QVERIFY(panelDraft);
    QVERIFY(!panelDraft->candidatePanel.iconStyle.globalDefaults.contains(QStringLiteral("presetOverrides")));
    QCOMPARE(panelDraft->candidatePanel.presetOrigin->iconPresetId, icon->identity.id);
}

void PresetPreviewSessionTest::lineageChangesOnlyForGovernedSettings()
{
    auto before = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
                                          QStringLiteral("bottom"), true);
    before.presetOrigin = PanelPresetOrigin{};
    before.presetOrigin->panelPresetId = QStringLiteral("obsidian-glass-dock");
    auto changed = before;
    changed.host.nativePanelId = 47;
    PresetApplication::markCustomized(before, &changed);
    QVERIFY(!changed.presetOrigin->customizedAfterApply);
    changed.placement.width++;
    PresetApplication::markCustomized(before, &changed);
    QVERIFY(changed.presetOrigin->customizedAfterApply);
    changed = before;
    changed.iconStyle.tileMode = QStringLiteral("custom");
    PresetApplication::markCustomized(before, &changed);
    QVERIFY(changed.presetOrigin->customizedAfterApply);
}

void PresetPreviewSessionTest::defaultsAreSeparateAndVersioned()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    PresetDefaultStore store(temporary.filePath(QStringLiteral("presets/defaults.json")));
    QString error;
    const auto empty = store.load(&error);
    QVERIFY(empty);
    QVERIFY(empty->panelPresetId.isEmpty());
    QVERIFY(empty->iconPresetId.isEmpty());
    QVERIFY(!QFileInfo::exists(store.filePath()));
    const QString panelFile = temporary.filePath(QStringLiteral("panels.json"));
    const QByteArray activePanels("existing active configuration");
    QVERIFY(PresetTestSupport::writeBytes(panelFile, activePanels));
    QVERIFY2(store.setDefault(QStringLiteral("panel"), QStringLiteral("obsidian-glass-dock"), &error),
             qPrintable(error));
    QVERIFY2(store.setDefault(QStringLiteral("icon"), QStringLiteral("glass-tile"), &error),
             qPrintable(error));
    const auto selected = store.load(&error);
    QVERIFY(selected);
    QCOMPARE(selected->panelPresetId, QStringLiteral("obsidian-glass-dock"));
    QCOMPARE(selected->iconPresetId, QStringLiteral("glass-tile"));
    QVERIFY(store.setDefault(QStringLiteral("panel"), {}, &error));
    QCOMPARE(store.load()->iconPresetId, QStringLiteral("glass-tile"));
    QVERIFY(store.load()->panelPresetId.isEmpty());
    const auto before = PresetTestSupport::readBytes(store.filePath());
    QVERIFY(!store.setDefault(QStringLiteral("profile"), QStringLiteral("glass-tile"), &error));
    QVERIFY(!store.setDefault(QStringLiteral("icon"), QStringLiteral("../../outside"), &error));
    QCOMPARE(PresetTestSupport::readBytes(store.filePath()), before);
    QCOMPARE(PresetTestSupport::readBytes(panelFile), activePanels);
}

void PresetPreviewSessionTest::defaultsRejectCorruptOrUnwritableStores()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    PresetDefaultStore store(temporary.filePath(QStringLiteral("defaults.json")));
    const QList<QByteArray> invalid{
        QByteArray("not JSON"),
        QByteArray(R"({"format":"org.archdock.preset-defaults","version":2,"panelPresetId":"","iconPresetId":""})"),
        QByteArray(R"({"format":"org.archdock.preset-defaults","version":1,"panelPresetId":"../outside","iconPresetId":""})"),
        QByteArray(PresetDefaultStore::MaximumBytes + 1, ' ')};
    for (const auto &bytes : invalid)
    {
        QVERIFY(PresetTestSupport::writeBytes(store.filePath(), bytes));
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!store.setDefault(QStringLiteral("icon"), QStringLiteral("glass-tile"), &error));
        QCOMPARE(PresetTestSupport::readBytes(store.filePath()), bytes);
    }
    const QString parentFile = temporary.filePath(QStringLiteral("file"));
    QVERIFY(PresetTestSupport::writeBytes(parentFile, QByteArray("keep")));
    PresetDefaultStore unwritable(parentFile + QStringLiteral("/defaults.json"));
    QString error;
    QVERIFY(!unwritable.setDefault(QStringLiteral("icon"), QStringLiteral("glass-tile"), &error));
    QCOMPARE(error, QStringLiteral("default-store-unwritable"));
    QCOMPARE(PresetTestSupport::readBytes(parentFile), QByteArray("keep"));
}

namespace
{
PresetPreviewRecord previewRecord()
{
    PresetPreviewRecord record;
    record.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.kind = QStringLiteral("panel");
    record.panelId = QStringLiteral("free-x12345678");
    record.hostKind = QStringLiteral("free-desktop");
    record.temporary = true;
    record.previewToken = QStringLiteral("archdock-preview-test");
    record.snapshot = PanelDefinition::defaults(record.panelId, QStringLiteral("Preview"),
                                               QStringLiteral("free"), false).normalized();
    return record;
}

struct PreviewHarness
{
    QString root;
    PanelDefinition original;
    PanelDefinition stored;
    std::optional<PanelDefinition> adopted;
    PanelPresetDefinition panel;
    IconPresetDefinition icon;
    PresetPreviewSession *session = nullptr;
    QString guardError;
    QStringList phases;
    std::function<void()> onApply;
    bool createSuccess = true;
    bool applySuccess = true;
    bool restoreSuccess = true;
    bool convertSuccess = true;
    bool commitSuccess = true;
    bool removeSuccess = true;
    bool recoverSuccess = true;
    bool ownedTemporaryHost = false;
    QString hostToken;
    int hostWidth = 720;
    int creates = 0;
    int applications = 0;
    int restores = 0;
    int removals = 0;
    int commits = 0;

    explicit PreviewHarness(QString directory) : root(std::move(directory))
    {
        original = PanelDefinition::defaults(QStringLiteral("bottom"), QStringLiteral("Bottom"),
                                             QStringLiteral("bottom"), true);
        original.visibility.visible = true;
        original.host.nativePanelId = 42;
        original.host.nativeDockAppletId = 73;
        original.host.nativeOwnershipToken = QStringLiteral("archdock-native-test");
        original.host.nativeRecoveryState = QStringLiteral("ready");
        original = original.normalized();
        stored = original;
        panel = *PanelPresetDefinition::fromVariantMap(PresetTestSupport::panelPresetMap());
        icon = *IconPresetDefinition::fromVariantMap(PresetTestSupport::iconPresetMap());
    }
    QString journalPath() const { return QDir(root).filePath(QStringLiteral("journal.json")); }
    QString defaultsPath() const { return QDir(root).filePath(QStringLiteral("defaults.json")); }
    QVariantMap request(bool temporary = false, const QString &kind = QStringLiteral("panel")) const
    {
        return {{QStringLiteral("panelId"), QStringLiteral("bottom")},
            {QStringLiteral("kind"), kind}, {QStringLiteral("newPanel"), temporary}};
    }
    void observeJournal()
    {
        const auto record = PresetPreviewRecovery(journalPath()).load();
        phases.append(record ? record->phase : QStringLiteral("ABSENT"));
    }
    PresetPreviewSession::Operations operations()
    {
        PresetPreviewSession::Operations ops;
        ops.guard = [this](const QString &) { return guardError; };
        ops.prepare = [this](const QVariantMap &request, QString *error)
            -> std::optional<PresetPreviewSession::Prepared>
        {
            PresetPreviewSession::Prepared result;
            result.record = previewRecord();
            result.record.kind = request.value(QStringLiteral("kind")).toString();
            result.record.temporary = request.value(QStringLiteral("newPanel")).toBool();
            auto snapshot = original;
            if (result.record.temporary) snapshot = result.record.snapshot;
            else
            {
                result.record.panelId = original.identity.id;
                result.record.hostKind = QStringLiteral("native-edge");
                result.record.previewToken = original.host.nativeOwnershipToken;
            }
            result.record.snapshot = snapshot;
            std::optional<PanelSettingsTransactionDraft> draft;
            if (result.record.kind == QStringLiteral("icon") && !result.record.temporary)
            {
                result.iconPreset = icon;
                draft = PresetApplication::prepareIcon(snapshot, {}, icon, {}, error);
            }
            else if (result.record.kind == QStringLiteral("panel"))
            {
                result.panelPreset = panel;
                if (result.record.temporary)
                {
                    result.panelPreset->panel.configuration.placement.edge = QStringLiteral("free");
                    result.panelPreset->panel.configuration.host.kind = PanelHostKind::FreeDesktop;
                }
                draft = PresetApplication::preparePanel(snapshot, {}, *result.panelPreset, {}, {}, error);
            }
            if (!draft) return std::nullopt;
            result.draft = *draft;
            return result;
        };
        ops.validateDraft = [](const auto &prepared, QString *error)
        { return prepared.draft.candidatePanel.isValid(error); };
        ops.captureHost = [this](auto &record, QString *)
        {
            record.containmentId = 42;
            record.appletId = 73;
            record.hostState = {{QStringLiteral("fixedLength"), hostWidth}};
            return true;
        };
        ops.createHost = [this](auto &record, auto &candidate, QString *error)
        {
            observeJournal();
            ++creates;
            ownedTemporaryHost = true;
            hostToken = record.previewToken;
            record.containmentId = 44;
            record.appletId = 75;
            candidate.host.freeDesktopContainmentId = 44;
            candidate.host.freeDockAppletId = 75;
            candidate.host.freeOwnershipToken = record.previewToken;
            candidate.host.freeHostState = QStringLiteral("hosted-owned");
            if (!createSuccess && error) *error = QStringLiteral("create-failed");
            return createSuccess;
        };
        ops.applyHost = [this](const auto &, const auto &candidate, QString *error)
        {
            ++applications;
            hostWidth = candidate.placement.width;
            if (onApply) onApply();
            if (!applySuccess && error) *error = QStringLiteral("placement-failed");
            return applySuccess;
        };
        ops.restoreHost = [this](const auto &record, const auto &, QString *error)
        {
            ++restores;
            if (!restoreSuccess) { if (error) *error = QStringLiteral("readback-mismatch"); return false; }
            hostWidth = record.hostState.value(QStringLiteral("fixedLength")).toInt();
            return true;
        };
        ops.commitExisting = [this](const auto &draft, QString *error)
        {
            observeJournal();
            if (!commitSuccess) { if (error) *error = QStringLiteral("persistence-failed"); return false; }
            if (stored != draft.previousPanel) { if (error) *error = QStringLiteral("stale-revision"); return false; }
            stored = draft.candidatePanel;
            ++commits;
            return true;
        };
        ops.convertHost = [this](const auto &record, auto &candidate, QString *error)
        {
            observeJournal();
            hostToken = record.managedToken;
            candidate.host.freeOwnershipToken = record.managedToken;
            if (!convertSuccess && error) *error = QStringLiteral("conversion-unverified");
            return convertSuccess;
        };
        ops.adoptHost = [this](const auto &candidate, QString *error)
        {
            observeJournal();
            if (!commitSuccess) { if (error) *error = QStringLiteral("adoption-failed"); return false; }
            adopted = candidate;
            ++commits;
            return true;
        };
        ops.removeHost = [this](const auto &, QString *error)
        {
            ++removals;
            if (!removeSuccess) { if (error) *error = QStringLiteral("removal-unverified"); return false; }
            ownedTemporaryHost = false;
            hostToken.clear();
            return true;
        };
        ops.recover = [this](const auto &record, QString *error)
        {
            if (!recoverSuccess) { if (error) *error = QStringLiteral("host-service-unavailable"); return false; }
            if (record.temporary)
            {
                if (adopted && adopted->host.freeOwnershipToken == record.managedToken) return true;
                ownedTemporaryHost = false;
                hostToken.clear();
                ++removals;
            }
            else if (stored.settingsRevision == record.snapshot.settingsRevision)
                hostWidth = record.hostState.value(QStringLiteral("fixedLength")).toInt();
            return true;
        };
        ops.saveCustom = [this](const auto &prepared, const QString &name)
        {
            UserPresetStore store(QDir(root).filePath(QStringLiteral("users")));
            QString error, id;
            if (prepared.panelPreset)
            {
                const auto saved = store.save(PresetApplication::panelSnapshot(
                    *prepared.panelPreset, prepared.draft.candidatePanel, name), &error);
                if (saved) id = saved->identity.id;
            }
            else if (prepared.iconPreset)
            {
                const auto saved = store.save(PresetApplication::iconSnapshot(
                    *prepared.iconPreset, prepared.draft.candidatePanel, name), &error);
                if (saved) id = saved->identity.id;
            }
            return QVariantMap{{QStringLiteral("success"), !id.isEmpty()},
                {QStringLiteral("errorCode"), error}, {QStringLiteral("presetId"), id}};
        };
        ops.restoreBuiltIn = [](const auto &prepared, QString *error)
            -> std::optional<PresetPreviewSession::Prepared>
        {
            auto restored = prepared;
            auto draft = prepared.panelPreset
                ? PresetApplication::preparePanel(prepared.draft.previousPanel, {},
                      *prepared.panelPreset, {}, prepared.recommendedIcons, error)
                : PresetApplication::prepareIcon(prepared.draft.previousPanel, {},
                      *prepared.iconPreset, {}, error);
            if (!draft) return {};
            restored.draft = *draft;
            return restored;
        };
        ops.validDefault = [this](const QString &kind, const QString &id)
        { return (kind == QStringLiteral("panel") && id == panel.identity.id) ||
                 (kind == QStringLiteral("icon") && id == icon.identity.id); };
        return ops;
    }
};
}

void PresetPreviewSessionTest::recoveryJournalPreservesSnapshotAndConversionProgress()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    PresetPreviewRecovery journal(temporary.filePath(QStringLiteral("journal.json")));
    QString error;
    QVERIFY(!journal.load(&error));
    QVERIFY(error.isEmpty());
    auto record = previewRecord();
    // A revision beyond JSON's integer precision must round-trip exactly.
    record.snapshot.settingsRevision = 9007199254740993ULL;
    record.hostState = {{QStringLiteral("fixedLength"), 720},
                        {QStringLiteral("alignment"), QStringLiteral("center")}};
    QVERIFY2(journal.save(record, &error), qPrintable(error));
    auto loaded = journal.load(&error);
    QVERIFY2(loaded, qPrintable(error));
    QVERIFY(loaded->snapshot == record.snapshot);
    QCOMPARE(loaded->hostState, record.hostState);
    record.phase = QStringLiteral("ACTIVE");
    record.containmentId = 42;
    record.appletId = 73;
    QVERIFY(journal.save(record, &error));
    record.managedToken = QStringLiteral("archdock-managed-test");
    for (const auto &phase : {QStringLiteral("CONVERTING"), QStringLiteral("ADOPTING"),
                              QStringLiteral("COMMITTED")})
    {
        record.phase = phase;
        QVERIFY2(journal.save(record, &error), qPrintable(error));
        loaded = journal.load(&error);
        QVERIFY(loaded);
        QCOMPARE(loaded->phase, phase);
        QCOMPARE(loaded->previewToken, record.previewToken);
        QCOMPARE(loaded->managedToken, record.managedToken);
        QVERIFY(loaded->snapshot == record.snapshot);
    }
    QVERIFY(journal.clear(&error));
    QVERIFY(!QFileInfo::exists(journal.filePath()));
    QVERIFY(journal.clear(&error));
}

void PresetPreviewSessionTest::recoveryJournalRejectsUnsafeReplacement()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    PresetPreviewRecovery journal(temporary.filePath(QStringLiteral("journal.json")));
    auto record = previewRecord();
    QString error;
    QVERIFY(journal.save(record, &error));
    const auto before = PresetTestSupport::readBytes(journal.filePath());
    auto other = record;
    other.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(!journal.save(other, &error));
    QCOMPARE(error, QStringLiteral("preview-journal-busy"));
    other = record;
    other.phase = QStringLiteral("UNRECOGNIZED");
    QVERIFY(!journal.save(other, &error));
    QCOMPARE(error, QStringLiteral("invalid-preview-record"));
    other = record;
    other.snapshot.extensions.insert(QStringLiteral("large"),
                                     QString(PresetPreviewRecovery::MaximumBytes, QLatin1Char('x')));
    QVERIFY(!journal.save(other, &error));
    QCOMPARE(error, QStringLiteral("preview-journal-limit-exceeded"));
    QCOMPARE(PresetTestSupport::readBytes(journal.filePath()), before);
    const QList<QByteArray> invalid{QByteArray("not JSON"), QByteArray("{\"version\":2}"),
        QByteArray(PresetPreviewRecovery::MaximumBytes + 1, ' ')};
    for (const auto &bytes : invalid)
    {
        QVERIFY(PresetTestSupport::writeBytes(journal.filePath(), bytes));
        QVERIFY(!journal.load(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!journal.save(record, &error));
        QVERIFY(!journal.clear(&error));
        QCOMPARE(PresetTestSupport::readBytes(journal.filePath()), bytes);
    }
}

void PresetPreviewSessionTest::sessionTransitionsCommitAndExactRollback()
{
    QTemporaryDir temporary;
    PreviewHarness harness(temporary.path());
    PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
    harness.session = &session;
    QStringList states;
    connect(&session, &PresetPreviewSession::changed, &session, [&] { states.append(session.state()); });
    QVERIFY(session.beginPreview(harness.request()).value(QStringLiteral("success")).toBool());
    QCOMPARE(session.state(), QStringLiteral("ACTIVE"));
    QVERIFY(harness.stored == harness.original);
    QVERIFY(session.previewDefinition(QStringLiteral("bottom")));
    QCOMPARE(session.previewDefinition(QStringLiteral("bottom"))->visibility, harness.original.visibility);
    QVERIFY(session.updateDraft({{QStringLiteral("width"), 990}}).value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.hostWidth, 990);
    QVERIFY(session.updateDraft({{QStringLiteral("width"), 990}, {QStringLiteral("screen"), 1}})
        .value(QStringLiteral("success")).toBool());
    const auto streamed = session.previewDefinition(QStringLiteral("bottom"));
    QVERIFY(streamed);
    QCOMPARE(streamed->host.screenIndex, 1);
    QCOMPARE(streamed->host.nativePanelId, harness.original.host.nativePanelId);
    QCOMPARE(streamed->host.nativeOwnershipToken, harness.original.host.nativeOwnershipToken);
    QVERIFY(session.revert().value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.hostWidth, 720);
    QVERIFY(harness.stored == harness.original);
    QVERIFY(!session.previewDefinition(QStringLiteral("bottom")));
    QVERIFY(!QFileInfo::exists(harness.journalPath()));
    QVERIFY(session.beginPreview(harness.request()).value(QStringLiteral("success")).toBool());
    QVERIFY(session.applyAsActive().value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.commits, 1);
    QCOMPARE(harness.stored.settingsRevision, harness.original.settingsRevision + 1);
    QCOMPARE(session.state(), QStringLiteral("IDLE"));
    QVERIFY(!QFileInfo::exists(harness.journalPath()));
    for (const auto &state : {QStringLiteral("PREPARING"), QStringLiteral("ACTIVE"),
         QStringLiteral("ROLLING_BACK"), QStringLiteral("COMMITTING"), QStringLiteral("COMMITTED"), QStringLiteral("IDLE")})
        QVERIFY(states.contains(state));
    QVERIFY(harness.phases.contains(QStringLiteral("COMMITTING")));
}

void PresetPreviewSessionTest::sessionGuardsAndReplacement()
{
    QTemporaryDir temporary;
    PreviewHarness harness(temporary.path());
    PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
    harness.session = &session;
    for (const auto &guard : {QStringLiteral("edit-mode-active"), QStringLiteral("popup-open"),
                              QStringLiteral("drag-active")})
    {
        harness.guardError = guard;
        QCOMPARE(session.beginPreview(harness.request()).value(QStringLiteral("errorCode")).toString(), guard);
        QCOMPARE(session.state(), QStringLiteral("IDLE"));
        QVERIFY(!QFileInfo::exists(harness.journalPath()));
    }
    harness.guardError.clear();
    bool busyRefused = false;
    harness.onApply = [&] {
        busyRefused = session.beginPreview(harness.request()).value(QStringLiteral("errorCode")).toString()
            == QStringLiteral("preview-busy");
    };
    QVERIFY(session.beginPreview(harness.request()).value(QStringLiteral("success")).toBool());
    QVERIFY(busyRefused);
    harness.onApply = {};
    harness.guardError = QStringLiteral("drag-active");
    QVERIFY(!session.updateDraft({{QStringLiteral("width"), 1000}}).value(QStringLiteral("success")).toBool());
    QVERIFY(!session.applyAsActive().value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.commits, 0);
    QCOMPARE(session.state(), QStringLiteral("ACTIVE"));
    harness.guardError.clear();
    QVERIFY(session.beginPreview(harness.request(true)).value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.restores, 1);
    QCOMPARE(harness.creates, 1);
    QVERIFY(session.cancel().value(QStringLiteral("success")).toBool());
    QVERIFY(!harness.ownedTemporaryHost);
    QCOMPARE(harness.commits, 0);
}

void PresetPreviewSessionTest::sessionTemporaryConversionAndFailureCleanup()
{
    for (int failure = 0; failure < 5; ++failure)
    {
        QTemporaryDir temporary;
        PreviewHarness harness(temporary.path());
        PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
        harness.session = &session;
        if (failure == 1) harness.createSuccess = false;
        if (failure == 2) harness.applySuccess = false;
        const bool began = session.beginPreview(harness.request(true)).value(QStringLiteral("success")).toBool();
        if (failure == 1 || failure == 2)
        {
            QVERIFY(!began);
            QCOMPARE(session.state(), QStringLiteral("IDLE"));
        }
        else
        {
            QVERIFY(began);
            if (failure == 3) harness.convertSuccess = false;
            if (failure == 4) harness.commitSuccess = false;
            const bool committed = session.applyAsActive().value(QStringLiteral("success")).toBool();
            QCOMPARE(committed, failure == 0);
            QCOMPARE(harness.commits, failure == 0 ? 1 : 0);
            QCOMPARE(session.state(), QStringLiteral("IDLE"));
        }
        QVERIFY(harness.stored == harness.original);
        QVERIFY(!QFileInfo::exists(harness.journalPath()));
        QVERIFY(!QFileInfo::exists(harness.defaultsPath()));
        if (failure == 0)
        {
            QVERIFY(harness.adopted);
            QCOMPARE(harness.adopted->settingsRevision, quint64(1));
            QVERIFY(!harness.adopted->host.freeOwnershipToken.startsWith(QStringLiteral("archdock-preview-")));
            QVERIFY(harness.phases.contains(QStringLiteral("PREPARING")));
            QVERIFY(harness.phases.contains(QStringLiteral("CONVERTING")));
            QVERIFY(harness.phases.contains(QStringLiteral("ADOPTING")));
        }
        else QVERIFY(!harness.ownedTemporaryHost);
    }
}

void PresetPreviewSessionTest::sessionIconIsolationAndUserActions()
{
    QTemporaryDir temporary;
    PreviewHarness harness(temporary.path());
    const auto builtInsBefore = PresetTestSupport::directoryDigest(QStringLiteral(ARCHDOCK_SOURCE_PRESET_ROOT));
    PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
    harness.session = &session;
    QVERIFY(session.beginPreview(harness.request(false, QStringLiteral("icon"))).value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.applications, 0);
    QVERIFY(session.updateDraft({{QStringLiteral("animationSpeed"), 1.6}}).value(QStringLiteral("success")).toBool());
    QCOMPARE(harness.applications, 0);
    QVERIFY(!session.updateDraft({{QStringLiteral("width"), 1000}}).value(QStringLiteral("success")).toBool());
    QVERIFY(session.active());
    const auto saved = session.saveAsCustomPreset(QStringLiteral("My active icon draft"));
    QVERIFY(saved.value(QStringLiteral("success")).toBool());
    QVERIFY(PresetIdentity::isUserId(saved.value(QStringLiteral("presetId")).toString()));
    QCOMPARE(harness.commits, 0);
    QVERIFY(session.restoreBuiltInDefaults().value(QStringLiteral("success")).toBool());
    QCOMPARE(session.previewDefinition(QStringLiteral("bottom"))->motion.speed, harness.icon.icon.motion.speed);
    QVERIFY(session.setAsDefault(QStringLiteral("icon"), harness.icon.identity.id, false)
        .value(QStringLiteral("success")).toBool());
    QCOMPARE(session.defaultSelection().value(QStringLiteral("iconPresetId")).toString(), harness.icon.identity.id);
    QVERIFY(harness.stored == harness.original);
    QVERIFY(session.setAsDefault(QStringLiteral("icon"), harness.icon.identity.id, true)
        .value(QStringLiteral("success")).toBool());
    QVERIFY(session.defaultSelection().value(QStringLiteral("iconPresetId")).toString().isEmpty());
    QVERIFY(session.applyAsActive().value(QStringLiteral("success")).toBool());
    QVERIFY(PresetApplication::iconOnlyChange(harness.original, harness.stored));
    QCOMPARE(harness.applications, 0);
    QCOMPARE(harness.commits, 1);
    QCOMPARE(PresetTestSupport::directoryDigest(QStringLiteral(ARCHDOCK_SOURCE_PRESET_ROOT)), builtInsBefore);
}

void PresetPreviewSessionTest::sessionInterruptionsKeepRecoverableEvidence()
{
    for (const bool temporaryHost : {false, true})
    {
        QTemporaryDir temporary;
        PreviewHarness harness(temporary.path());
        {
            PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
            harness.session = &session;
            QVERIFY(session.beginPreview(harness.request(temporaryHost)).value(QStringLiteral("success")).toBool());
            QVERIFY(QFileInfo::exists(harness.journalPath()));
        }
        PresetPreviewSession restarted(harness.operations(), harness.journalPath(), harness.defaultsPath());
        harness.session = &restarted;
        QCOMPARE(restarted.state(), QStringLiteral("BLOCKED"));
        QVERIFY(!restarted.beginPreview(harness.request()).value(QStringLiteral("success")).toBool());
        harness.recoverSuccess = false;
        QVERIFY(!restarted.recoverInterruptedPreview().value(QStringLiteral("success")).toBool());
        QCOMPARE(restarted.state(), QStringLiteral("BLOCKED"));
        QVERIFY(QFileInfo::exists(harness.journalPath()));
        harness.recoverSuccess = true;
        QVERIFY(restarted.recoverInterruptedPreview().value(QStringLiteral("success")).toBool());
        QCOMPARE(restarted.state(), QStringLiteral("IDLE"));
        if (!temporaryHost) QCOMPARE(harness.hostWidth, 720);
        QVERIFY(!harness.ownedTemporaryHost);
        QVERIFY(harness.stored == harness.original);
        QVERIFY(!QFileInfo::exists(harness.journalPath()));
    }
    QTemporaryDir temporary;
    PreviewHarness harness(temporary.path());
    PresetPreviewSession session(harness.operations(), harness.journalPath(), harness.defaultsPath());
    harness.session = &session;
    QVERIFY(session.beginPreview(harness.request(true)).value(QStringLiteral("success")).toBool());
    harness.removeSuccess = false;
    QVERIFY(!session.cancel().value(QStringLiteral("success")).toBool());
    QCOMPARE(session.state(), QStringLiteral("BLOCKED"));
    QVERIFY(QFileInfo::exists(harness.journalPath()));
    QVERIFY(!session.previewDefinition(QStringLiteral("free-x12345678")));
    QVERIFY(!session.beginPreview(harness.request()).value(QStringLiteral("success")).toBool());
    QVERIFY(session.recoverInterruptedPreview().value(QStringLiteral("success")).toBool());
    QCOMPARE(session.state(), QStringLiteral("IDLE"));
}

QTEST_GUILESS_MAIN(PresetPreviewSessionTest)
#include "PresetPreviewSessionTest.moc"
