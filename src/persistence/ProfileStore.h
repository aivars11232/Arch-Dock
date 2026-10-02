#pragma once

#include "../model/PanelDefinition.h"

#include <QSet>
#include <QStringList>
#include <optional>

namespace ArchDock
{
struct ProfileReferences
{
    QSet<QString> themes;
    QSet<QString> iconStyles;
    QSet<QString> motions;
};

// Complete configurations, independent of preset catalogs and live hosts.
// Ownership associations belong only to a local transaction backup.
struct ProfileDefinition
{
    static constexpr int SchemaVersion = 1;
    static constexpr qsizetype MaximumPanels = 64;
    QString id;
    QString name;
    int revision = 1;
    QString screenPolicy = QStringLiteral("stable-id-then-index");
    QVariantMap metadata;
    QList<PanelDefinition> panels;

    [[nodiscard]] static QString format();
    [[nodiscard]] static bool validId(const QString &id);
    [[nodiscard]] static PanelDefinition portablePanel(const PanelDefinition &panel);
    [[nodiscard]] static ProfileDefinition capture(const QString &name,
        const QList<PanelDefinition> &panels);
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] static std::optional<ProfileDefinition> fromVariantMap(
        const QVariantMap &value, QString *errorCode = nullptr);
    // Effective fallbacks do not alter the saved requested configuration.
    [[nodiscard]] QList<PanelDefinition> resolved(const ProfileReferences &references,
        QStringList *diagnostics = nullptr) const;
    bool operator==(const ProfileDefinition &) const = default;
};

class ProfileStore final
{
public:
    static constexpr qsizetype MaximumBytes = 4 * 1024 * 1024;
    static constexpr qsizetype MaximumProfiles = 512;
    [[nodiscard]] static QString defaultRoot();
    explicit ProfileStore(QString root = defaultRoot());
    [[nodiscard]] QString rootDirectory() const;
    [[nodiscard]] QList<ProfileDefinition> profiles(QStringList *diagnostics = nullptr) const;
    [[nodiscard]] std::optional<ProfileDefinition> load(const QString &id,
        QString *errorCode = nullptr) const;
    // Existing profiles use their current revision as a compare-and-save guard.
    [[nodiscard]] std::optional<ProfileDefinition> save(const ProfileDefinition &profile,
        QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<ProfileDefinition> create(const QString &name,
        const QList<PanelDefinition> &panels, QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<ProfileDefinition> rename(const QString &id,
        int expectedRevision, const QString &name, QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<ProfileDefinition> duplicate(const QString &id,
        int expectedRevision, const QString &name, QString *errorCode = nullptr) const;
    [[nodiscard]] bool remove(const QString &id, int expectedRevision,
        QString *errorCode = nullptr) const;
    // A JSON document and its adjacent <filename>.assets directory. Assets
    // pass the existing data-only theme validator before any managed copy.
    // Import creates new local profile/panel identities and never applies it.
    [[nodiscard]] std::optional<ProfileDefinition> importProfile(const QString &path,
        const QString &name = {}, QString *errorCode = nullptr) const;
    [[nodiscard]] bool exportProfile(const QString &id, int expectedRevision,
        const QString &path, QString *errorCode = nullptr) const;

private:
    QString m_root;
};
}
