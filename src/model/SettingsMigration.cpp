#include "SettingsMigration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

namespace ArchDock
{

bool PanelMigrationResult::ok() const
{
    return status == PanelMigrationStatus::Success ||
        status == PanelMigrationStatus::NoSource;
}

PanelMigrationResult SettingsMigration::migratePanelRecords(const QByteArray &source)
{
    PanelMigrationResult result;
    if (source.isEmpty())
    {
        result.status = PanelMigrationStatus::NoSource;
        result.diagnostic = QStringLiteral("panel registry source is absent");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(source, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        result.status = PanelMigrationStatus::InvalidJson;
        result.diagnostic = QStringLiteral("panel registry JSON error at offset %1: %2")
            .arg(parseError.offset)
            .arg(parseError.errorString());
        return result;
    }
    if (!document.isArray())
    {
        result.status = PanelMigrationStatus::InvalidRoot;
        result.diagnostic = QStringLiteral("panel registry root must be an array");
        return result;
    }

    const QJsonArray records = document.array();
    result.definitions.reserve(records.size());
    QSet<QString> panelIds;
    for (qsizetype index = 0; index < records.size(); ++index)
    {
        const QJsonValue value = records.at(index);
        if (!value.isObject())
        {
            result.status = PanelMigrationStatus::InvalidRecord;
            result.diagnostic = QStringLiteral("panel record %1 must be an object").arg(index);
            result.definitions.clear();
            return result;
        }

        const QVariantMap record = value.toObject().toVariantMap();
        int sourceVersion = 1;
        if (record.contains(QStringLiteral("schemaVersion")))
        {
            bool versionOk = false;
            sourceVersion = record.value(QStringLiteral("schemaVersion")).toInt(&versionOk);
            if (!versionOk || sourceVersion < 1)
            {
                result.status = PanelMigrationStatus::InvalidRecord;
                result.diagnostic = QStringLiteral(
                    "panel record %1 has an invalid schema version").arg(index);
                result.definitions.clear();
                return result;
            }
        }
        if (sourceVersion > PanelDefinition::CurrentSchemaVersion)
        {
            result.status = PanelMigrationStatus::UnsupportedVersion;
            result.diagnostic = QStringLiteral(
                "panel record %1 uses unsupported schema version %2")
                .arg(index)
                .arg(sourceVersion);
            result.definitions.clear();
            return result;
        }

        QString recordError;
        const std::optional<PanelDefinition> definition =
            PanelDefinition::fromLegacyMap(record, &recordError);
        if (!definition.has_value())
        {
            result.status = PanelMigrationStatus::InvalidRecord;
            result.diagnostic = QStringLiteral("panel record %1 is invalid: %2")
                .arg(index)
                .arg(recordError);
            result.definitions.clear();
            return result;
        }
        if (panelIds.contains(definition->identity.id))
        {
            result.status = PanelMigrationStatus::InvalidRecord;
            result.diagnostic = QStringLiteral("panel record %1 duplicates id '%2'")
                .arg(index)
                .arg(definition->identity.id);
            result.definitions.clear();
            return result;
        }
        panelIds.insert(definition->identity.id);
        result.definitions.append(*definition);
        result.sourceWasLegacy = result.sourceWasLegacy ||
            sourceVersion < PanelDefinition::CurrentSchemaVersion;
    }

    result.status = PanelMigrationStatus::Success;
    result.rewriteRequired = result.sourceWasLegacy;
    result.serializedVersionTwo = serializeVersionTwo(result.definitions);
    return result;
}

QByteArray SettingsMigration::serializeVersionTwo(
    const QList<PanelDefinition> &definitions)
{
    QJsonArray records;
    for (const PanelDefinition &definition : definitions)
    {
        records.append(QJsonObject::fromVariantMap(
            definition.normalized().toPersistedMap()));
    }
    return QJsonDocument(records).toJson(QJsonDocument::Compact);
}

QString SettingsMigration::statusName(PanelMigrationStatus status)
{
    switch (status)
    {
    case PanelMigrationStatus::Success:
        return QStringLiteral("success");
    case PanelMigrationStatus::NoSource:
        return QStringLiteral("no-source");
    case PanelMigrationStatus::InvalidJson:
        return QStringLiteral("invalid-json");
    case PanelMigrationStatus::InvalidRoot:
        return QStringLiteral("invalid-root");
    case PanelMigrationStatus::InvalidRecord:
        return QStringLiteral("invalid-record");
    case PanelMigrationStatus::UnsupportedVersion:
        return QStringLiteral("unsupported-version");
    }
    return QStringLiteral("invalid-json");
}

}
