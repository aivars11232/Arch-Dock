#pragma once

#include "ProfileApplyTransaction.h"
#include "integration/ProfileShortcutManager.h"

// Profiles: saved sets of panels the user can create, rename, duplicate,
// import, export, apply and bind to global shortcuts. Served over D-Bus as
// org.archdock.Profiles; applying one runs ProfileApplyTransaction.
namespace ArchDock
{
class ProfileManager final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.archdock.Profiles")
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap shortcutStatus READ shortcutStatus NOTIFY changed)
public:
    explicit ProfileManager(ProfileApplyTransaction::Operations operations,
        QString root = ProfileStore::defaultRoot(),
        QString journalPath = ProfileApplyTransaction::defaultJournalPath(), QObject *parent = nullptr,
        ProfileShortcutManager::NativeOperations shortcuts = {});
    ~ProfileManager() override;
    [[nodiscard]] int revision() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] QVariantMap status() const;
    [[nodiscard]] QVariantMap shortcutStatus() const;
    [[nodiscard]] std::optional<PanelDefinition> previewDefinition(const QString &id) const;
    // A profile holds the panels that are on the desktop: native panels
    // always, a free panel only while it has a complete host link
    // (freeHostState hosted-owned, or hosted-stale after a failed check).
    // A detached free panel keeps its record
    // in the registry for "Put back on the desktop", but it is not part of
    // the arrangement. On 2026-10-12 a profile had captured 21 detached
    // records and one Apply created 13 desktop widgets before the owner
    // killed the backend; Apply now refuses to create more free panels than
    // MaximumFreePanelsCreatedPerApply at once.
    static constexpr int MaximumFreePanelsCreatedPerApply = 4;
    [[nodiscard]] static bool onTheDesktop(const PanelDefinition &panel);
    [[nodiscard]] static QList<PanelDefinition> panelsOnTheDesktop(const QList<PanelDefinition> &panels);
    // Free panels of the profile that an apply would create as new desktop
    // widgets: visible, and without a hosted record of the same id, kind,
    // content type and screen (the transaction's own rule).
    [[nodiscard]] static int freePanelsToCreate(const ProfileDefinition &profile,
        const QList<PanelDefinition> &desktop);
public slots:
    QVariantList listProfiles() const;
    QVariantMap createProfile(const QString &name);
    QVariantMap saveProfile(const QString &id, int expectedRevision);
    QVariantMap renameProfile(const QString &id, int expectedRevision, const QString &name);
    QVariantMap duplicateProfile(const QString &id, int expectedRevision, const QString &name);
    QVariantMap deleteProfile(const QString &id, int expectedRevision);
    QVariantMap importProfile(const QString &sourceUrl);
    QVariantMap exportProfile(const QString &id, int expectedRevision, const QString &destinationUrl);
    QVariantMap applyProfile(const QString &id, int expectedRevision);
    QVariantMap recoverInterruptedApply();
    QVariantMap getStatus() const;
    QVariantMap getShortcutStatus() const;
    QVariantMap setShortcutsEnabled(bool enabled);
    QVariantMap setProfileShortcut(const QString &id, const QString &sequence);
    QVariantMap clearProfileShortcut(const QString &id);
signals:
    void changed();
    void profilesChanged();
    void profileDeleted(const QString &id);
private:
    [[nodiscard]] QString guard() const;
    [[nodiscard]] QVariantMap finish(const std::optional<ProfileDefinition> &profile,
        const QString &error);
    [[nodiscard]] QVariantMap failure(const QString &error) const;
    ProfileStore m_store;
    ProfileApplyTransaction::Operations m_operations;
    ProfileApplyTransaction m_transaction;
    ProfileShortcutManager *m_shortcuts = nullptr;
    int m_revision = 0;
};
}
