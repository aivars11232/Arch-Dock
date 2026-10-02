#include "ProfileManager.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace ArchDock
{
ProfileManager::ProfileManager(ProfileApplyTransaction::Operations operations,
    QString root, QString journal, QObject *parent, ProfileShortcutManager::NativeOperations shortcuts)
    : QObject(parent), m_store(std::move(root)), m_operations(operations),
      m_transaction(std::move(operations), journal, this)
{
    connect(&m_transaction, &ProfileApplyTransaction::changed, this, [this] { ++m_revision; emit changed(); });
    m_shortcuts = new ProfileShortcutManager(
        [this](const QString &id) { return m_store.load(id); },
        [this](const QString &id, int revision) { return applyProfile(id, revision); },
        QFileInfo(journal).dir().filePath(QStringLiteral("profile-shortcuts.json")), std::move(shortcuts), this);
    connect(m_shortcuts, &ProfileShortcutManager::changed, this, [this] { ++m_revision; emit changed(); });
    connect(this, &ProfileManager::profilesChanged, m_shortcuts, &ProfileShortcutManager::refreshProfiles);
}
ProfileManager::~ProfileManager()
{
    disconnect(m_shortcuts, nullptr, this, nullptr);
    delete m_shortcuts;
}
int ProfileManager::revision() const { return m_revision; }
bool ProfileManager::active() const { return m_transaction.active(); }
QVariantMap ProfileManager::status() const { return m_transaction.status(); }
QVariantMap ProfileManager::getStatus() const { return status(); }
QVariantMap ProfileManager::shortcutStatus() const { return m_shortcuts->status(); }
QVariantMap ProfileManager::getShortcutStatus() const { return shortcutStatus(); }
QVariantMap ProfileManager::setShortcutsEnabled(bool enabled)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    return m_shortcuts->setEnabled(enabled);
}
QVariantMap ProfileManager::setProfileShortcut(const QString &id, const QString &sequence)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    return m_shortcuts->setBinding(id, sequence);
}
QVariantMap ProfileManager::clearProfileShortcut(const QString &id)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    return m_shortcuts->clearBinding(id);
}
std::optional<PanelDefinition> ProfileManager::previewDefinition(const QString &id) const
{
    return m_transaction.previewDefinition(id);
}
QString ProfileManager::guard() const
{
    if (active()) return QStringLiteral("profile-recovery-or-apply-active");
    return m_operations.guard ? m_operations.guard() : QString{};
}
QVariantMap ProfileManager::failure(const QString &code) const
{
    return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), code}};
}
QVariantMap ProfileManager::finish(const std::optional<ProfileDefinition> &profile, const QString &code)
{
    if (!profile) return failure(code);
    ++m_revision; emit changed(); emit profilesChanged();
    return {{QStringLiteral("success"), true}, {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("profileId"), profile->id}, {QStringLiteral("revision"), profile->revision}};
}
QVariantList ProfileManager::listProfiles() const
{
    QVariantList result;
    for (const auto &profile : m_store.profiles())
        result.append(QVariantMap{{QStringLiteral("id"), profile.id}, {QStringLiteral("name"), profile.name},
            {QStringLiteral("revision"), profile.revision}, {QStringLiteral("panelCount"), profile.panels.size()},
            {QStringLiteral("screenPolicy"), profile.screenPolicy}, {QStringLiteral("metadata"), profile.metadata}});
    return result;
}
QVariantMap ProfileManager::createProfile(const QString &name)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    if (!m_operations.snapshot) return failure(QStringLiteral("profile-host-operations-unavailable"));
    QString code;
    const auto panels = m_operations.snapshot(&code);
    if (!code.isEmpty()) return failure(code);
    const auto profile = m_store.create(name, panels, &code);
    return finish(profile, code);
}
QVariantMap ProfileManager::saveProfile(const QString &id, int revision)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    QString code;
    auto profile = m_store.load(id, &code);
    if (!profile) return failure(code);
    if (profile->revision != revision) return failure(QStringLiteral("stale-profile-revision"));
    if (!m_operations.snapshot) return failure(QStringLiteral("profile-host-operations-unavailable"));
    const auto panels = m_operations.snapshot(&code);
    if (!code.isEmpty()) return failure(code);
    profile->panels = ProfileDefinition::capture(profile->name, panels).panels;
    const auto saved = m_store.save(*profile, &code);
    return finish(saved, code);
}
QVariantMap ProfileManager::renameProfile(const QString &id, int revision, const QString &name)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    QString code; const auto profile = m_store.rename(id, revision, name, &code);
    return finish(profile, code);
}
QVariantMap ProfileManager::duplicateProfile(const QString &id, int revision, const QString &name)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    QString code; const auto profile = m_store.duplicate(id, revision, name, &code);
    return finish(profile, code);
}
QVariantMap ProfileManager::deleteProfile(const QString &id, int revision)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    QString code;
    const auto profile = m_store.load(id, &code);
    if (!profile) return failure(code);
    if (profile->revision != revision) return failure(QStringLiteral("stale-profile-revision"));
    const auto cleared = m_shortcuts->clearBinding(id);
    if (!cleared.value(QStringLiteral("success")).toBool()) return cleared;
    if (!m_store.remove(id, revision, &code)) return failure(code);
    ++m_revision; emit profileDeleted(id); emit changed(); emit profilesChanged();
    return {{QStringLiteral("success"), true}, {QStringLiteral("errorCode"), QString{}}, {QStringLiteral("profileId"), id}};
}
QVariantMap ProfileManager::importProfile(const QString &sourceUrl)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    const QUrl url(sourceUrl);
    if (!url.isLocalFile()) return failure(QStringLiteral("profile-import-requires-local-file"));
    QString code; const auto profile = m_store.importProfile(url.toLocalFile(), {}, &code);
    return finish(profile, code);
}
QVariantMap ProfileManager::exportProfile(const QString &id, int revision, const QString &destinationUrl)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    const QUrl url(destinationUrl);
    if (!url.isLocalFile()) return failure(QStringLiteral("profile-export-requires-local-file"));
    QString code;
    const bool exported = m_store.exportProfile(id, revision, url.toLocalFile(), &code);
    return {{QStringLiteral("success"), exported}, {QStringLiteral("errorCode"), code}, {QStringLiteral("profileId"), id}};
}
QVariantMap ProfileManager::applyProfile(const QString &id, int revision)
{
    const auto guarded = guard(); if (!guarded.isEmpty()) return failure(guarded);
    QString code; const auto profile = m_store.load(id, &code);
    if (!profile) return failure(code);
    if (profile->revision != revision) return failure(QStringLiteral("stale-profile-revision"));
    return m_transaction.apply(*profile);
}
QVariantMap ProfileManager::recoverInterruptedApply() { return m_transaction.recover(); }
}
