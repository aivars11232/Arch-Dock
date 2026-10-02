#pragma once

#include <QMap>
#include <QStringList>
#include <functional>

namespace ArchDock
{
// Data-only snapshots. Locations are supplied by the application, never by a
// backup manifest. Installed resources and transaction journals are not roots.
class ConfigurationBackup final
{
public:
    struct Location { QString path; bool directory = true; };
    using Locations = QMap<QString, Location>;
    static constexpr qint64 MaximumBytes = 256 * 1024 * 1024;
    static constexpr int MaximumFiles = 4096;
    static QString defaultRoot();
    static Locations defaultLocations();
    explicit ConfigurationBackup(QString root = defaultRoot(),
        Locations locations = defaultLocations());
    [[nodiscard]] QString capture(const QString &reason, QString *error = nullptr);
    [[nodiscard]] QStringList backups(QString *error = nullptr) const;
    [[nodiscard]] bool restore(const QString &id, QString *error = nullptr);
    [[nodiscard]] bool recover(QString *error = nullptr);
    [[nodiscard]] bool recoveryPending() const;
    [[nodiscard]] bool prune(int retain, QString *error = nullptr);
    // Deterministic storage interruption seam; production leaves it unset.
    void setCheckpoint(std::function<bool(const QString &)> checkpoint);
private:
    QString m_root;
    Locations m_locations;
    std::function<bool(const QString &)> m_checkpoint;
    [[nodiscard]] bool apply(const QString &id, QString *error);
};
}
