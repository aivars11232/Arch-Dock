#include "UserPresetStore.h"

#include "PresetCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

namespace
{

using namespace ArchDock;
using namespace ArchDock::PresetParsing;

const QString panelDirectoryName = QStringLiteral("panels");
const QString iconDirectoryName = QStringLiteral("icons");
const QString markerFileName = QStringLiteral("user-presets.json");
const QString definitionSuffix = QStringLiteral(".json");

void setError(QString *errorCode, const QString &code)
{
    if (errorCode)
    {
        *errorCode = code;
    }
}

QString definitionPath(const QString &root, const QString &kind, const QString &id)
{
    return QDir(QDir(root).filePath(kind)).filePath(id + definitionSuffix);
}

bool writeJson(const QString &path, const QVariantMap &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    const QByteArray bytes = QJsonDocument(
        QJsonObject::fromVariantMap(object)).toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size() && file.commit();
}

// A store written by a newer Arch Dock is left completely alone: it is
// neither listed nor written, so an older build can never damage it.
bool storeIsUsable(const QString &root,
                   QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QString markerPath = QDir(root).filePath(markerFileName);
    if (!QFileInfo::exists(markerPath))
    {
        return true;
    }
    QVector<PresetValidationDiagnostic> found;
    const std::optional<QVariantMap> marker =
        readPresetDefinitionFile(markerPath, &found);
    const QVariant version = marker.has_value()
        ? marker->value(QStringLiteral("version")) : QVariant{};
    if (!marker.has_value() ||
        marker->value(QStringLiteral("format")).toString() !=
            UserPresetStore::storeFormat() ||
        !isInteger(version) ||
        version.toInt() != UserPresetStore::CurrentStoreVersion)
    {
        addDiagnostic(diagnostics, QStringLiteral("unsupported-version"),
                      markerFileName,
                      QStringLiteral("the user preset store has an unsupported version"));
        return false;
    }
    return true;
}

template<typename Definition>
QVector<Definition> listPresets(const QString &root,
                                const QString &kind,
                                QVector<PresetValidationDiagnostic> *diagnostics)
{
    QVector<Definition> result;
    if (!storeIsUsable(root, diagnostics))
    {
        return result;
    }
    const QFileInfoList files = QDir(QDir(root).filePath(kind)).entryInfoList(
        {QStringLiteral("*") + definitionSuffix},
        QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &info : files)
    {
        if (result.size() >= UserPresetStore::MaximumPresets)
        {
            addDiagnostic(diagnostics, QStringLiteral("limit-exceeded"), kind,
                          QStringLiteral("the user preset store holds too many presets"));
            break;
        }
        QVector<PresetValidationDiagnostic> found;
        std::optional<Definition> definition;
        const std::optional<QVariantMap> object =
            readPresetDefinitionFile(info.absoluteFilePath(), &found);
        if (object.has_value())
        {
            definition = Definition::fromVariantMap(*object, &found);
        }
        if (definition.has_value() &&
            (definition->identity.builtIn ||
             definition->identity.id != info.completeBaseName()))
        {
            addDiagnostic(&found, QStringLiteral("store-identity-mismatch"),
                          QStringLiteral("/identity/id"),
                          QStringLiteral("definition is not the user preset its file names"));
            definition.reset();
        }
        if (diagnostics)
        {
            for (PresetValidationDiagnostic diagnostic : std::as_const(found))
            {
                diagnostic.jsonPointer = info.fileName() + QLatin1Char('#') +
                    diagnostic.jsonPointer;
                diagnostics->append(diagnostic);
            }
        }
        if (definition.has_value())
        {
            result.append(*definition);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const Definition &left, const Definition &right)
              {
                  const int order = QString::compare(
                      left.identity.name, right.identity.name, Qt::CaseInsensitive);
                  return order != 0 ? order < 0 : left.identity.id < right.identity.id;
              });
    return result;
}

QString newUserId(const QString &root)
{
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        const QString id = PresetIdentity::userIdPrefix() +
            QUuid::createUuid().toString(QUuid::Id128).left(12);
        if (!QFileInfo::exists(definitionPath(root, panelDirectoryName, id)) &&
            !QFileInfo::exists(definitionPath(root, iconDirectoryName, id)))
        {
            return id;
        }
    }
    return {};
}

template<typename Definition>
std::optional<Definition> savePreset(const QString &root,
                                     const QString &kind,
                                     Definition candidate,
                                     QString *errorCode)
{
    QVector<PresetValidationDiagnostic> found;
    if (!storeIsUsable(root, &found))
    {
        setError(errorCode, QStringLiteral("unsupported-store-version"));
        return std::nullopt;
    }

    candidate.identity.builtIn = false;
    const bool stored = PresetIdentity::isValidId(candidate.identity.id) &&
        PresetIdentity::isUserId(candidate.identity.id) &&
        QFileInfo(definitionPath(root, kind, candidate.identity.id)).isFile();
    if (stored)
    {
        // Replacing a user preset advances the revision that is on disk, not
        // whatever revision the caller happened to hold.
        QVector<PresetValidationDiagnostic> ignored;
        const std::optional<QVariantMap> object = readPresetDefinitionFile(
            definitionPath(root, kind, candidate.identity.id), &ignored);
        const std::optional<Definition> previous = object.has_value()
            ? Definition::fromVariantMap(*object, &ignored) : std::nullopt;
        candidate.identity.revision =
            (previous.has_value() ? previous->identity.revision
                                  : candidate.identity.revision) + 1;
    }
    else
    {
        const qsizetype count = QDir(QDir(root).filePath(kind)).entryList(
            {QStringLiteral("*") + definitionSuffix}, QDir::Files).size();
        if (count >= UserPresetStore::MaximumPresets)
        {
            setError(errorCode, QStringLiteral("limit-exceeded"));
            return std::nullopt;
        }
        candidate.identity.id = newUserId(root);
        candidate.identity.revision = 1;
        if (candidate.identity.id.isEmpty())
        {
            setError(errorCode, QStringLiteral("id-unavailable"));
            return std::nullopt;
        }
    }

    // What is written is what the parser accepts and returns: the stored
    // definition is canonical and independently loadable.
    const std::optional<Definition> canonical =
        Definition::fromVariantMap(candidate.toVariantMap(), &found);
    if (!canonical.has_value())
    {
        const auto firstError = std::find_if(
            found.cbegin(), found.cend(), [](const PresetValidationDiagnostic &entry)
            {
                return entry.severity == QStringLiteral("error");
            });
        setError(errorCode, firstError != found.cend()
                     ? firstError->code : QStringLiteral("invalid-definition"));
        return std::nullopt;
    }

    if (!QDir().mkpath(QDir(root).filePath(kind)) ||
        !writeJson(QDir(root).filePath(markerFileName),
                   {{QStringLiteral("format"), UserPresetStore::storeFormat()},
                    {QStringLiteral("version"), UserPresetStore::CurrentStoreVersion}}) ||
        !writeJson(definitionPath(root, kind, canonical->identity.id),
                   canonical->toVariantMap()))
    {
        setError(errorCode, QStringLiteral("store-unwritable"));
        return std::nullopt;
    }
    setError(errorCode, QString{});
    return canonical;
}

template<typename Definition>
Definition derivedPreset(const Definition &source, const QString &name)
{
    Definition result = source;
    result.identity.id.clear();
    result.identity.name = name.trimmed().isEmpty() ? source.identity.name
                                                    : name.trimmed();
    result.identity.builtIn = false;
    result.identity.revision = 1;
    result.identity.derivedFromPresetId = source.identity.id;
    result.identity.sourceRevision = source.identity.revision;
    return result;
}

bool removePreset(const QString &root,
                  const QString &kind,
                  const QString &presetId,
                  QString *errorCode)
{
    if (!PresetIdentity::isValidId(presetId) || !PresetIdentity::isUserId(presetId))
    {
        setError(errorCode, QStringLiteral("not-user-preset"));
        return false;
    }
    const QFileInfo info(definitionPath(root, kind, presetId));
    if (!info.exists() || !info.isFile() || info.isSymLink())
    {
        setError(errorCode, QStringLiteral("not-found"));
        return false;
    }
    if (!QFile::remove(info.absoluteFilePath()))
    {
        setError(errorCode, QStringLiteral("remove-failed"));
        return false;
    }
    setError(errorCode, QString{});
    return true;
}

}

namespace ArchDock
{

UserPresetStore::UserPresetStore(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

QString UserPresetStore::storeFormat()
{
    return QStringLiteral("org.archdock.user-preset-store");
}

QString UserPresetStore::rootDirectory() const
{
    return m_rootDirectory;
}

QVector<PanelPresetDefinition> UserPresetStore::panelPresets(
    QVector<PresetValidationDiagnostic> *diagnostics) const
{
    return listPresets<PanelPresetDefinition>(
        m_rootDirectory, panelDirectoryName, diagnostics);
}

QVector<IconPresetDefinition> UserPresetStore::iconPresets(
    QVector<PresetValidationDiagnostic> *diagnostics) const
{
    return listPresets<IconPresetDefinition>(
        m_rootDirectory, iconDirectoryName, diagnostics);
}

PanelPresetDefinition UserPresetStore::derivedFrom(
    const PanelPresetDefinition &source, const QString &name)
{
    return derivedPreset(source, name);
}

IconPresetDefinition UserPresetStore::derivedFrom(
    const IconPresetDefinition &source, const QString &name)
{
    return derivedPreset(source, name);
}

std::optional<PanelPresetDefinition> UserPresetStore::save(
    const PanelPresetDefinition &definition, QString *errorCode) const
{
    return savePreset(m_rootDirectory, panelDirectoryName, definition, errorCode);
}

std::optional<IconPresetDefinition> UserPresetStore::save(
    const IconPresetDefinition &definition, QString *errorCode) const
{
    return savePreset(m_rootDirectory, iconDirectoryName, definition, errorCode);
}

bool UserPresetStore::removePanelPreset(const QString &presetId,
                                        QString *errorCode) const
{
    return removePreset(m_rootDirectory, panelDirectoryName, presetId, errorCode);
}

bool UserPresetStore::removeIconPreset(const QString &presetId,
                                       QString *errorCode) const
{
    return removePreset(m_rootDirectory, iconDirectoryName, presetId, errorCode);
}

}
