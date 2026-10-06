#pragma once

#include "PanelDefinition.h"

#include <QByteArray>
#include <QList>
#include <QString>

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
};

}
