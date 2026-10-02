#pragma once

#include "ProfileApplyTransaction.h"
#include "integration/ProfileShortcutManager.h"

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
