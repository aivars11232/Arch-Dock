#include "PresetDefaultStore.h"
#include "../model/PresetIdentity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace ArchDock
{
namespace
{
void error(QString *target, const QString &value)
{
    if (target) *target = value;
}
bool validId(const QString &id)
{
    return id.isEmpty() || PresetIdentity::isValidId(id);
}
}

QString PresetDefaultStore::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("presets/defaults.json"));
}
PresetDefaultStore::PresetDefaultStore(QString filePath) : m_filePath(std::move(filePath)) {}
QString PresetDefaultStore::filePath() const { return m_filePath; }

std::optional<PresetDefaults> PresetDefaultStore::load(QString *errorCode) const
{
    error(errorCode, QString{});
    const QFileInfo info(m_filePath);
    if (info.isSymLink())
    {
        error(errorCode, QStringLiteral("unsafe-default-store"));
        return std::nullopt;
    }
    if (!info.exists()) return PresetDefaults{};
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        error(errorCode, QStringLiteral("default-store-unreadable"));
        return std::nullopt;
    }
    const QByteArray bytes = file.read(MaximumBytes + 1);
    if (bytes.size() > MaximumBytes)
    {
        error(errorCode, QStringLiteral("default-store-limit-exceeded"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    const auto object = document.object();
    const QStringList keys{QStringLiteral("format"), QStringLiteral("iconPresetId"),
                          QStringLiteral("panelPresetId"), QStringLiteral("version")};
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        object.keys() != keys ||
        object.value(QStringLiteral("format")).toString() !=
            QStringLiteral("org.archdock.preset-defaults") ||
        !object.value(QStringLiteral("version")).isDouble() ||
        object.value(QStringLiteral("version")).toDouble() != CurrentVersion ||
        !object.value(QStringLiteral("panelPresetId")).isString() ||
        !object.value(QStringLiteral("iconPresetId")).isString())
    {
        error(errorCode, QStringLiteral("invalid-default-store"));
        return std::nullopt;
    }
    PresetDefaults result{object.value(QStringLiteral("panelPresetId")).toString(),
                          object.value(QStringLiteral("iconPresetId")).toString()};
    if (!validId(result.panelPresetId) || !validId(result.iconPresetId))
    {
        error(errorCode, QStringLiteral("invalid-default-preset-id"));
        return std::nullopt;
    }
    return result;
}

bool PresetDefaultStore::setDefault(
    const QString &kind, const QString &presetId, QString *errorCode) const
{
    if ((kind != QStringLiteral("panel") && kind != QStringLiteral("icon")) ||
        !validId(presetId))
    {
        error(errorCode, QStringLiteral("invalid-default-selection"));
        return false;
    }
    auto selection = load(errorCode);
    if (!selection) return false;
    if (kind == QStringLiteral("panel")) selection->panelPresetId = presetId;
    else selection->iconPresetId = presetId;
    const QJsonObject object{
        {QStringLiteral("format"), QStringLiteral("org.archdock.preset-defaults")},
        {QStringLiteral("version"), CurrentVersion},
        {QStringLiteral("panelPresetId"), selection->panelPresetId},
        {QStringLiteral("iconPresetId"), selection->iconPresetId}};
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath()))
    {
        error(errorCode, QStringLiteral("default-store-unwritable"));
        return false;
    }
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    {
        error(errorCode, QStringLiteral("default-store-unwritable"));
        return false;
    }
    error(errorCode, QString{});
    return true;
}
}
