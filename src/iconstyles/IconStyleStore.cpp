#include "IconStyleStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace
{

using namespace ArchDock;

IconStyleValidationDiagnostic diagnostic(const QString &code,
                                         const QString &pointer,
                                         const QString &message)
{
    return {code, pointer, QStringLiteral("error"), message};
}

bool hasErrors(const QVector<IconStyleValidationDiagnostic> &diagnostics)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const IconStyleValidationDiagnostic &entry)
                       {
                           return entry.severity == QStringLiteral("error");
                       });
}

bool isSafeRelativeManifest(const QString &path)
{
    if (path.isEmpty() || path.contains(QChar::Null) ||
        path.contains(QLatin1Char('\\')) || path.startsWith(QLatin1Char('/')) ||
        QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path)
    {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    return std::none_of(parts.cbegin(), parts.cend(), [](const QString &part)
    {
        return part.isEmpty() || part == QStringLiteral(".") ||
            part == QStringLiteral("..");
    });
}

bool isIdentifier(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z0-9][a-z0-9.-]{0,63}$"));
    return pattern.match(value).hasMatch();
}

void appendPackageDiagnostics(
    QVector<IconStyleValidationDiagnostic> *target,
    const QVector<IconStyleValidationDiagnostic> &source,
    const QString &entryPointer)
{
    for (IconStyleValidationDiagnostic entry : source)
    {
        entry.jsonPointer = entryPointer + QStringLiteral("/package") +
            entry.jsonPointer;
        target->append(entry);
    }
}

}

namespace ArchDock
{

IconStyleStoreLoadResult IconStyleStore::loadCatalog(
    const QString &catalogPath,
    const QString &packageRoot)
{
    const QFileInfo catalogInfo(catalogPath);
    if (!catalogInfo.exists())
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("icon-style catalog does not exist"))}};
    }
    if (!catalogInfo.isFile())
    {
        return {{}, {diagnostic(QStringLiteral("asset-not-regular"), QString{},
                                QStringLiteral("icon-style catalog is not a regular file"))}};
    }
    if (catalogInfo.size() > MaximumCatalogBytes)
    {
        return {{}, {diagnostic(QStringLiteral("catalog-too-large"), QString{},
                                QStringLiteral("icon-style catalog exceeds the byte limit"))}};
    }
    QFile file(catalogInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("icon-style catalog could not be read"))}};
    }
    const QString resolvedRoot = packageRoot.isEmpty()
        ? catalogInfo.absolutePath() : packageRoot;
    return loadCatalogBytes(file.read(MaximumCatalogBytes + 1), resolvedRoot,
                            catalogInfo.absoluteFilePath());
}

IconStyleStoreLoadResult IconStyleStore::loadCatalogBytes(
    const QByteArray &catalogBytes,
    const QString &packageRoot,
    const QString &catalogPath)
{
    IconStyleStoreLoadResult result;
    if (catalogBytes.size() > MaximumCatalogBytes)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("catalog-too-large"), QString{},
            QStringLiteral("icon-style catalog exceeds the byte limit")));
        return result;
    }
    const QFileInfo rootInfo(packageRoot);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || canonicalRoot.isEmpty())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsafe-package-root"), QString{},
            QStringLiteral("icon-style package root must be an existing directory")));
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(catalogBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-json"), QString{},
            QStringLiteral("icon-style catalog is not a JSON object")));
        return result;
    }
    const QJsonObject root = document.object();
    const QSet<QString> allowedRoot{
        QStringLiteral("format"), QStringLiteral("version"),
        QStringLiteral("fallbackStyleId"), QStringLiteral("iconStyles")};
    for (auto it = root.constBegin(); it != root.constEnd(); ++it)
    {
        if (!allowedRoot.contains(it.key()))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("unknown-field"), QLatin1Char('/') + it.key(),
                QStringLiteral("unknown icon-style catalog member")));
        }
    }
    if (root.value(QStringLiteral("format")).toString() !=
        QStringLiteral("org.archdock.icon-style-catalog"))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-format"), QStringLiteral("/format"),
            QStringLiteral("icon-style catalog format is unsupported")));
    }
    if (!root.value(QStringLiteral("version")).isDouble() ||
        root.value(QStringLiteral("version")).toDouble() != 1.0)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-version"), QStringLiteral("/version"),
            QStringLiteral("icon-style catalog version is unsupported")));
    }

    IconStyleStore store;
    store.m_catalogPath = catalogPath;
    store.m_packageRoot = canonicalRoot;
    const QJsonValue fallbackValue = root.value(QStringLiteral("fallbackStyleId"));
    if (!fallbackValue.isString() || !isIdentifier(fallbackValue.toString()))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-id"), QStringLiteral("/fallbackStyleId"),
            QStringLiteral("fallbackStyleId must be a version 1 identifier")));
    }
    else
    {
        store.m_fallbackStyleId = fallbackValue.toString();
    }

    const QJsonValue stylesValue = root.value(QStringLiteral("iconStyles"));
    if (!stylesValue.isArray())
    {
        result.diagnostics.append(diagnostic(
            root.contains(QStringLiteral("iconStyles"))
                ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
            QStringLiteral("/iconStyles"),
            QStringLiteral("iconStyles must be an array")));
        return result;
    }
    const QJsonArray styles = stylesValue.toArray();
    if (styles.isEmpty() || styles.size() > MaximumPackages)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("limit-exceeded"), QStringLiteral("/iconStyles"),
            QStringLiteral("iconStyles must contain a bounded non-empty package list")));
    }

    QSet<QString> ids;
    const QSet<QString> allowedEntry{
        QStringLiteral("id"), QStringLiteral("name"),
        QStringLiteral("description"), QStringLiteral("author"),
        QStringLiteral("category"), QStringLiteral("packageManifest"),
        QStringLiteral("capabilities"), QStringLiteral("preview")};
    const QSet<QString> categories{
        QStringLiteral("fallback"), QStringLiteral("metallic"),
        QStringLiteral("neon"), QStringLiteral("orb")};
    for (qsizetype index = 0; index < styles.size(); ++index)
    {
        const QString pointer = QStringLiteral("/iconStyles/") +
            QString::number(index);
        if (!styles.at(index).isObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-type"), pointer,
                QStringLiteral("icon-style catalog entry must be an object")));
            continue;
        }
        const QJsonObject entry = styles.at(index).toObject();
        for (auto it = entry.constBegin(); it != entry.constEnd(); ++it)
        {
            if (!allowedEntry.contains(it.key()))
            {
                result.diagnostics.append(diagnostic(
                    QStringLiteral("unknown-field"), pointer + QLatin1Char('/') + it.key(),
                    QStringLiteral("unknown icon-style entry member")));
            }
        }
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (!entry.value(QStringLiteral("id")).isString() || !isIdentifier(id))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-id"), pointer + QStringLiteral("/id"),
                QStringLiteral("catalog style ID is invalid")));
        }
        if (ids.contains(id))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("duplicate-id"), pointer + QStringLiteral("/id"),
                QStringLiteral("catalog style ID is duplicated")));
        }
        ids.insert(id);
        if (!entry.value(QStringLiteral("name")).isString() ||
            entry.value(QStringLiteral("name")).toString().isEmpty())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-value"), pointer + QStringLiteral("/name"),
                QStringLiteral("catalog style name must be a non-empty string")));
        }
        if (!entry.value(QStringLiteral("category")).isString() ||
            !categories.contains(entry.value(QStringLiteral("category")).toString()))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-enum"), pointer + QStringLiteral("/category"),
                QStringLiteral("catalog style category is unsupported")));
        }
        if (!entry.value(QStringLiteral("capabilities")).isObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-type"), pointer + QStringLiteral("/capabilities"),
                QStringLiteral("catalog capabilities must be an object")));
        }
        if (!entry.value(QStringLiteral("preview")).isObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-type"), pointer + QStringLiteral("/preview"),
                QStringLiteral("catalog preview must be an object")));
        }

        const QString manifest = entry.value(QStringLiteral("packageManifest")).toString();
        const QString expectedManifest = id + QStringLiteral("/archdock-icon-style.json");
        if (!entry.value(QStringLiteral("packageManifest")).isString() ||
            !isSafeRelativeManifest(manifest) || manifest != expectedManifest)
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-package-manifest"),
                pointer + QStringLiteral("/packageManifest"),
                QStringLiteral("package manifest must match the catalog style ID")));
            continue;
        }

        IconStylePackageLoadResult packageResult = IconStylePackage::load(
            QDir(canonicalRoot).filePath(manifest));
        if (!packageResult.isValid())
        {
            appendPackageDiagnostics(&result.diagnostics, packageResult.diagnostics,
                                     pointer);
            continue;
        }
        const IconStyleDefinition &definition = packageResult.package->definition();
        if (definition.id != id)
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("identity-mismatch"), pointer + QStringLiteral("/id"),
                QStringLiteral("catalog and package style IDs differ")));
        }
        if (definition.name != entry.value(QStringLiteral("name")).toString())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("identity-mismatch"), pointer + QStringLiteral("/name"),
                QStringLiteral("catalog and package style names differ")));
        }
        if (QJsonObject::fromVariantMap(definition.capabilities.toVariantMap()) !=
            entry.value(QStringLiteral("capabilities")).toObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("capability-mismatch"),
                pointer + QStringLiteral("/capabilities"),
                QStringLiteral("catalog and package capabilities differ")));
        }
        if (QJsonObject::fromVariantMap(definition.preview.toVariantMap()) !=
            entry.value(QStringLiteral("preview")).toObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("preview-mismatch"),
                pointer + QStringLiteral("/preview"),
                QStringLiteral("catalog and package previews differ")));
        }
        store.m_catalogEntries.append(entry.toVariantMap());
        store.m_packages.insert(id, std::move(*packageResult.package));
    }

    if (!store.m_packages.contains(store.m_fallbackStyleId))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("missing-fallback"), QStringLiteral("/fallbackStyleId"),
            QStringLiteral("fallback icon style is absent or invalid")));
    }
    if (hasErrors(result.diagnostics))
    {
        return result;
    }
    result.store = std::move(store);
    return result;
}

bool IconStyleStore::contains(const QString &styleId) const
{
    return m_packages.contains(styleId);
}

QStringList IconStyleStore::styleIds() const
{
    QStringList result = m_packages.keys();
    std::sort(result.begin(), result.end());
    return result;
}

QString IconStyleStore::fallbackStyleId() const
{
    return m_fallbackStyleId;
}

const IconStylePackage *IconStyleStore::packageById(const QString &styleId) const
{
    const auto match = m_packages.constFind(styleId);
    return match == m_packages.cend() ? nullptr : &match.value();
}

QVariantList IconStyleStore::catalogEntries() const
{
    return m_catalogEntries;
}

QVariantMap IconStyleStore::resolve(const QString &requestedStyleId) const
{
    const QString normalized = requestedStyleId.trimmed();
    const bool defaulted = normalized.isEmpty();
    const bool known = !defaulted && contains(normalized);
    const QString resolvedId = known ? normalized : m_fallbackStyleId;
    const IconStylePackage *package = packageById(resolvedId);
    if (!package)
    {
        return {
            {QStringLiteral("diagnostics"), QVariantList{
                 diagnostic(QStringLiteral("missing-fallback"), QString{},
                            QStringLiteral("fallback icon style is unavailable"))
                     .toVariantMap()}},
            {QStringLiteral("loadable"), false},
            {QStringLiteral("requestedStyleId"), normalized},
            {QStringLiteral("resolvedStyleId"), QString{}},
            {QStringLiteral("selectionStatus"), QStringLiteral("invalid")},
            {QStringLiteral("valid"), false},
        };
    }

    QVariantMap projection = package->runtimeProjection();
    projection.insert(QStringLiteral("fellBack"), !known);
    projection.insert(QStringLiteral("requestedStyleId"), normalized);
    projection.insert(QStringLiteral("resolvedStyleId"), resolvedId);
    projection.insert(QStringLiteral("selectionStatus"),
                      known ? QStringLiteral("selected")
                            : defaulted ? QStringLiteral("defaulted")
                                        : QStringLiteral("fallback"));
    projection.insert(QStringLiteral("fallbackReason"),
                      known ? QString{}
                            : defaulted ? QStringLiteral("no-style-selected")
                                        : QStringLiteral("unknown-style"));
    return projection;
}

QVariantMap IconStyleStore::toVariantMap() const
{
    QVariantMap packages;
    for (const QString &styleId : styleIds())
    {
        packages.insert(styleId, m_packages.value(styleId).runtimeProjection());
    }
    return {
        {QStringLiteral("catalogEntries"), m_catalogEntries},
        {QStringLiteral("catalogPath"), m_catalogPath},
        {QStringLiteral("fallbackStyleId"), m_fallbackStyleId},
        {QStringLiteral("packageRoot"), m_packageRoot},
        {QStringLiteral("packages"), packages},
        {QStringLiteral("styleIds"), styleIds()},
        {QStringLiteral("valid"), contains(m_fallbackStyleId)},
    };
}

bool IconStyleStoreLoadResult::isValid() const
{
    return store.has_value() && !hasErrors(diagnostics);
}

QString IconStyleStoreLoadResult::primaryCode() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().code;
}

QString IconStyleStoreLoadResult::primaryMessage() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().message;
}

QVariantMap IconStyleStoreLoadResult::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("diagnostics"),
         iconStyleDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("status"), isValid() ? QStringLiteral("valid")
                                              : QStringLiteral("invalid")},
        {QStringLiteral("valid"), isValid()},
    };
    if (store.has_value())
    {
        result.insert(QStringLiteral("store"), store->toVariantMap());
    }
    return result;
}

}
