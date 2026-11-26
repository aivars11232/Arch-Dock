#pragma once

#include "PanelDefinition.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

// Reads saved panel records of any supported version and brings them to
// version 2 (PanelDefinition), saying whether the saved file must be
// rewritten.
namespace ArchDock
{

enum class PanelMigrationStatus
{
    Success,
    NoSource,
    InvalidJson,
    InvalidRoot,
    InvalidRecord,
    UnsupportedVersion
};

struct PanelMigrationResult
{
    PanelMigrationStatus status = PanelMigrationStatus::InvalidJson;
    QList<PanelDefinition> definitions;
    QByteArray serializedVersionTwo;
    QString diagnostic;
    bool sourceWasLegacy = false;
    bool rewriteRequired = false;

    [[nodiscard]] bool ok() const;
};

class SettingsMigration
{
public:
    [[nodiscard]] static PanelMigrationResult migratePanelRecords(
        const QByteArray &source);
    [[nodiscard]] static QByteArray serializeVersionTwo(
        const QList<PanelDefinition> &definitions);
    [[nodiscard]] static QString statusName(PanelMigrationStatus status);
    // Once after the update from 0.1.1-53 or -54 (the caller keeps the
    // mark): the AI-made "modelled" icon relief and the "high" 3D quality
    // those versions saved as plain defaults go back to "auto" and "medium",
    // in a panel record as toLegacyMap() or toPersistedMap() write it, and in
    // both copies of the flat look it remembers. True when anything changed.
    [[nodiscard]] static bool lightenLegacy3DDefaults(QVariantMap *record);
};

}
