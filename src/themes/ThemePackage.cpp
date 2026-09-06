#include "ThemePackage.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

namespace
{

using namespace ArchDock;

const QString manifestFormat = QStringLiteral("org.archdock.theme");
const QString manifestFileName = QStringLiteral("archdock-theme.json");

// Baked 2.5D track bounds. A perspective platform shrinks distant icons; these
// keep a declared shrink, tilt and polygon within values a panel can still be
// used at, so a malformed manifest cannot produce an unusable dock.
constexpr qreal MaximumTrackScale = 4.0;
constexpr qreal MaximumTrackTiltDegrees = 45.0;
constexpr int MinimumTrackSides = 3;
constexpr int MaximumTrackSides = 12;

struct ParsedThemePackage
{
    ThemeDefinition definition;
    QString legacyFit = QStringLiteral("cover");
    QVariantMap sourceManifest;
    QHash<QString, QString> assetPaths;
};

ThemeValidationDiagnostic diagnostic(const QString &code,
                                     const QString &pointer,
                                     const QString &message)
{
    return {code, pointer, QStringLiteral("error"), message};
}

bool hasErrors(const QVector<ThemeValidationDiagnostic> &diagnostics)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const ThemeValidationDiagnostic &entry)
                       {
                           return entry.severity == QStringLiteral("error");
                       });
}

bool isContainedPath(const QString &root, const QString &candidate)
{
    return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
}

bool isFiniteNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool isInteger(const QJsonValue &value)
{
    return isFiniteNumber(value) && std::trunc(value.toDouble()) == value.toDouble();
}

QString normalizedLegacyId(const QString &value)
{
    QString result;
    for (const QChar character : value.trimmed().toLower())
    {
        if ((character >= QLatin1Char('a') && character <= QLatin1Char('z')) ||
            (character >= QLatin1Char('0') && character <= QLatin1Char('9')))
        {
            result.append(character);
        }
        else if (!result.isEmpty() && !result.endsWith(QLatin1Char('-')))
        {
            result.append(QLatin1Char('-'));
        }
    }
    while (result.endsWith(QLatin1Char('-')))
    {
        result.chop(1);
    }
    return result.isEmpty() ? QStringLiteral("theme") : result.left(64);
}

QString pointerChild(const QString &parent, const QString &child)
{
    return parent + QLatin1Char('/') + child;
}

class ManifestParser
{
public:
    ManifestParser(QString packageRoot, QString manifestPath)
        : m_packageRoot(std::move(packageRoot))
        , m_manifestPath(std::move(manifestPath))
    {
    }

    ParsedThemePackage parseVersion2(const QJsonObject &manifest)
    {
        ParsedThemePackage parsed;
        parsed.sourceManifest = manifest.toVariantMap();
        ThemeDefinition &definition = parsed.definition;
        definition.sourceVersion = 2;

        rejectUnknown(manifest, {
            QStringLiteral("format"), QStringLiteral("version"),
            QStringLiteral("id"), QStringLiteral("name"),
            QStringLiteral("packageRevision"), QStringLiteral("author"),
            QStringLiteral("description"), QStringLiteral("license"),
            QStringLiteral("capabilities"), QStringLiteral("assets"),
            QStringLiteral("states"), QStringLiteral("layers"),
            QStringLiteral("slices"), QStringLiteral("contentRegions"),
            QStringLiteral("tracks"),
            QStringLiteral("effectMargins"), QStringLiteral("inputMasks"),
            QStringLiteral("iconStyleRef"), QStringLiteral("animationProfileRefs")
        }, QString{});

        definition.id = identifier(manifest, QStringLiteral("id"), QString{}, true);
        definition.name = stringValue(manifest, QStringLiteral("name"), QString{}, true);
        definition.packageRevision = integerValue(
            manifest, QStringLiteral("packageRevision"), QString{}, false, 1, 1);
        definition.author = stringValue(
            manifest, QStringLiteral("author"), QString{}, false);
        definition.description = stringValue(
            manifest, QStringLiteral("description"), QString{}, false);

        parseLicense(manifest, &definition);
        parseCapabilities(manifest, &definition);
        parseAssets(manifest, &definition, &parsed.assetPaths);
        parseStates(manifest, &definition);
        parseLayers(manifest, &definition);
        parseSlices(manifest, &definition);
        parseContentRegions(manifest, &definition);
        parseTracks(manifest, &definition);
        parseEffectMargins(manifest, &definition);
        parseInputMasks(manifest, &definition);
        parseResourceReferences(manifest, &definition);
        validateReferences(&definition);
        validateRendererAssets(definition);
        return parsed;
    }

    ParsedThemePackage parseVersion1(const QJsonObject &manifest)
    {
        ParsedThemePackage parsed;
        parsed.sourceManifest = manifest.toVariantMap();
        ThemeDefinition &definition = parsed.definition;
        definition.sourceVersion = 1;
        definition.adaptedFromVersion1 = true;

        QString legacySeed = stringValue(
            manifest, QStringLiteral("id"), QString{}, false);
        if (legacySeed.isEmpty())
        {
            legacySeed = QFileInfo(m_manifestPath).dir().dirName();
        }
        definition.id = normalizedLegacyId(legacySeed);
        definition.name = stringValue(
            manifest, QStringLiteral("name"), QString{}, false).trimmed();
        if (definition.name.isEmpty())
        {
            definition.name = definition.id;
        }
        definition.author = stringValue(
            manifest, QStringLiteral("author"), QString{}, false).trimmed();

        const QJsonValue surfaceValue = manifest.value(QStringLiteral("surface"));
        if (!surfaceValue.isObject())
        {
            add(QStringLiteral("missing-field"), QStringLiteral("/surface"),
                QStringLiteral("version 1 requires a surface object"));
            return parsed;
        }
        const QJsonObject surface = surfaceValue.toObject();
        const QString relativePath = stringValue(
            surface, QStringLiteral("asset"), QStringLiteral("/surface"), true);
        const QString resolvedPath = resolveAsset(relativePath,
                                                  QStringLiteral("/surface/asset"));
        if (!resolvedPath.isEmpty())
        {
            const QString suffix = QFileInfo(relativePath).suffix().toLower();
            ThemeAssetDefinition asset;
            asset.id = QStringLiteral("surface");
            asset.path = relativePath;
            asset.kind = suffix == QStringLiteral("svg") || suffix == QStringLiteral("svgz")
                ? QStringLiteral("vector") : QStringLiteral("raster");
            definition.assets.append(asset);
            parsed.assetPaths.insert(asset.id, resolvedPath);

            ThemeLayerDefinition layer;
            layer.id = QStringLiteral("surface");
            layer.asset = asset.id;
            layer.role = QStringLiteral("surface");
            definition.layers.append(layer);
        }

        QString fit = stringValue(surface, QStringLiteral("fit"),
                                  QStringLiteral("/surface"), false,
                                  QStringLiteral("cover")).toLower();
        if (!QSet<QString>{QStringLiteral("cover"), QStringLiteral("contain"),
                           QStringLiteral("stretch"), QStringLiteral("tile")}.contains(fit))
        {
            fit = QStringLiteral("cover");
        }
        parsed.legacyFit = fit;

        ThemeStateDefinition state;
        state.id = QStringLiteral("normal");
        if (!definition.layers.isEmpty())
        {
            state.layers.append(QStringLiteral("surface"));
        }
        definition.states.append(state);

        definition.capabilities.hosts = {
            QStringLiteral("native-edge"), QStringLiteral("free-desktop")};
        definition.capabilities.rendererTiers = {
            QStringLiteral("skinned2d"), QStringLiteral("procedural2d")};
        definition.capabilities.preferredRendererTier = QStringLiteral("skinned2d");
        definition.capabilities.fallbackRendererTiers = {QStringLiteral("procedural2d")};
        definition.capabilities.layouts = {
            QStringLiteral("adaptive"), QStringLiteral("horizontal"),
            QStringLiteral("vertical")};
        definition.capabilities.orientations = {
            QStringLiteral("horizontal"), QStringLiteral("vertical")};
        return parsed;
    }

    const QVector<ThemeValidationDiagnostic> &diagnostics() const
    {
        return m_diagnostics;
    }

private:
    void add(const QString &code, const QString &pointer, const QString &message)
    {
        m_diagnostics.append(diagnostic(code, pointer, message));
    }

    void rejectUnknown(const QJsonObject &object,
                       const QSet<QString> &allowed,
                       const QString &pointer)
    {
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            if (!allowed.contains(it.key()))
            {
                add(QStringLiteral("unknown-field"), pointerChild(pointer, it.key()),
                    QStringLiteral("unknown manifest member"));
            }
        }
    }

    QString stringValue(const QJsonObject &object,
                        const QString &key,
                        const QString &pointer,
                        bool required,
                        const QString &defaultValue = {})
    {
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), pointerChild(pointer, key),
                    QStringLiteral("required string is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!value.isString())
        {
            add(QStringLiteral("invalid-type"), pointerChild(pointer, key),
                QStringLiteral("value must be a string"));
            return defaultValue;
        }
        const QString result = value.toString();
        if (result.toUtf8().size() > ThemePackage::MaximumStringBytes)
        {
            add(QStringLiteral("limit-exceeded"), pointerChild(pointer, key),
                QStringLiteral("string exceeds the UTF-8 byte limit"));
            return defaultValue;
        }
        if (required && result.isEmpty())
        {
            add(QStringLiteral("invalid-value"), pointerChild(pointer, key),
                QStringLiteral("required string must not be empty"));
        }
        return result;
    }

    QString identifier(const QJsonObject &object,
                       const QString &key,
                       const QString &pointer,
                       bool required)
    {
        const QString value = stringValue(object, key, pointer, required);
        static const QRegularExpression pattern(
            QStringLiteral("^[a-z0-9][a-z0-9._-]{0,63}$"));
        if (!value.isEmpty() &&
            (value.toUtf8().size() > ThemePackage::MaximumIdentifierBytes ||
             !pattern.match(value).hasMatch()))
        {
            add(QStringLiteral("invalid-id"), pointerChild(pointer, key),
                QStringLiteral("identifier does not match the version 2 grammar"));
        }
        return value;
    }

    int integerValue(const QJsonObject &object,
                     const QString &key,
                     const QString &pointer,
                     bool required,
                     int defaultValue,
                     int minimum)
    {
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), pointerChild(pointer, key),
                    QStringLiteral("required integer is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!isInteger(value) || value.toDouble() < minimum ||
            value.toDouble() > std::numeric_limits<int>::max())
        {
            add(QStringLiteral("invalid-value"), pointerChild(pointer, key),
                QStringLiteral("value must be a finite integer in range"));
            return defaultValue;
        }
        return value.toInt(defaultValue);
    }

    qreal numberValue(const QJsonObject &object,
                      const QString &key,
                      const QString &pointer,
                      bool required,
                      qreal defaultValue)
    {
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), pointerChild(pointer, key),
                    QStringLiteral("required number is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!isFiniteNumber(value))
        {
            add(QStringLiteral("invalid-value"), pointerChild(pointer, key),
                QStringLiteral("value must be a finite number"));
            return defaultValue;
        }
        return value.toDouble(defaultValue);
    }

    QStringList stringArray(const QJsonObject &object,
                            const QString &key,
                            const QString &pointer,
                            bool required,
                            bool allowEmpty,
                            const QSet<QString> &allowed = {})
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required array is missing"));
            }
            return {};
        }
        const QJsonValue value = object.value(key);
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be an array"));
            return {};
        }
        const QJsonArray array = value.toArray();
        if (!allowEmpty && array.isEmpty())
        {
            add(QStringLiteral("invalid-value"), memberPointer,
                QStringLiteral("array must not be empty"));
        }
        QStringList result;
        QSet<QString> seen;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString entryPointer = memberPointer + QLatin1Char('/') +
                QString::number(index);
            if (!array.at(index).isString())
            {
                add(QStringLiteral("invalid-type"), entryPointer,
                    QStringLiteral("array entry must be a string"));
                continue;
            }
            const QString entry = array.at(index).toString();
            if (entry.toUtf8().size() > ThemePackage::MaximumStringBytes)
            {
                add(QStringLiteral("limit-exceeded"), entryPointer,
                    QStringLiteral("string exceeds the UTF-8 byte limit"));
                continue;
            }
            if (seen.contains(entry))
            {
                add(QStringLiteral("duplicate-value"), entryPointer,
                    QStringLiteral("array values must be unique"));
                continue;
            }
            seen.insert(entry);
            if (!allowed.isEmpty() && !allowed.contains(entry))
            {
                add(QStringLiteral("invalid-enum"), entryPointer,
                    QStringLiteral("unsupported enumerated value"));
            }
            result.append(entry);
        }
        return result;
    }

    bool lexicallySafePath(const QString &path, const QString &pointer)
    {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        static const QRegularExpression schemeOrDrive(
            QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
        if (path.isEmpty() || path.contains(QChar::Null) ||
            path.contains(QLatin1Char('\\')) || path.startsWith(QLatin1Char('/')) ||
            QDir::isAbsolutePath(path) || schemeOrDrive.match(path).hasMatch() ||
            QDir::cleanPath(path) != path ||
            std::any_of(parts.cbegin(), parts.cend(), [](const QString &part)
            {
                return part.isEmpty() || part == QStringLiteral(".") ||
                    part == QStringLiteral("..");
            }))
        {
            add(QStringLiteral("unsafe-path"), pointer,
                QStringLiteral("path must be a clean relative path inside the package"));
            return false;
        }
        return true;
    }

    QString resolveAsset(const QString &path, const QString &pointer)
    {
        if (!lexicallySafePath(path, pointer))
        {
            return {};
        }
        const QFileInfo info(QDir(m_packageRoot).filePath(path));
        if (!info.exists())
        {
            add(QStringLiteral("missing-asset"), pointer,
                QStringLiteral("declared asset does not exist"));
            return {};
        }
        if (!info.isFile())
        {
            add(QStringLiteral("asset-not-regular"), pointer,
                QStringLiteral("declared asset is not a regular file"));
            return {};
        }
        const QString canonicalPath = info.canonicalFilePath();
        if (canonicalPath.isEmpty() || !isContainedPath(m_packageRoot, canonicalPath))
        {
            add(QStringLiteral("unsafe-path"), pointer,
                QStringLiteral("asset resolves outside the package directory"));
            return {};
        }
        if (info.size() > ThemePackage::MaximumAssetBytes)
        {
            add(QStringLiteral("asset-too-large"), pointer,
                QStringLiteral("asset exceeds the per-file byte limit"));
            return {};
        }
        m_totalAssetBytes += info.size();
        if (m_totalAssetBytes > ThemePackage::MaximumPackageAssetBytes)
        {
            add(QStringLiteral("asset-too-large"), pointer,
                QStringLiteral("declared assets exceed the package byte limit"));
            return {};
        }
        return canonicalPath;
    }

    void parseLicense(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("license")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("license"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/license"),
                QStringLiteral("license must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("spdx"),
                               QStringLiteral("redistribution"),
                               QStringLiteral("evidence")},
                      QStringLiteral("/license"));
        definition->license.spdx = stringValue(
            object, QStringLiteral("spdx"), QStringLiteral("/license"), false);
        definition->license.redistribution = stringValue(
            object, QStringLiteral("redistribution"), QStringLiteral("/license"),
            false, QStringLiteral("unknown"));
        definition->license.evidence = stringValue(
            object, QStringLiteral("evidence"), QStringLiteral("/license"), false);
        if (!QSet<QString>{QStringLiteral("unknown"), QStringLiteral("allowed"),
                           QStringLiteral("forbidden")}
                 .contains(definition->license.redistribution))
        {
            add(QStringLiteral("invalid-enum"), QStringLiteral("/license/redistribution"),
                QStringLiteral("unsupported redistribution value"));
        }
    }

    void parseCapabilities(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("capabilities"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("capabilities"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/capabilities"),
                QStringLiteral("version 2 requires a capabilities object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {
            QStringLiteral("hosts"), QStringLiteral("rendererTiers"),
            QStringLiteral("preferredRendererTier"),
            QStringLiteral("fallbackRendererTiers"), QStringLiteral("layouts"),
            QStringLiteral("orientations"), QStringLiteral("features"),
            QStringLiteral("presentationMechanisms"), QStringLiteral("rotation")
        }, QStringLiteral("/capabilities"));

        const QSet<QString> hosts{QStringLiteral("native-edge"),
                                  QStringLiteral("free-desktop")};
        const QSet<QString> tiers{QStringLiteral("procedural2d"),
                                  QStringLiteral("skinned2d"),
                                  QStringLiteral("baked2.5d"),
                                  QStringLiteral("true3d")};
        const QSet<QString> layouts{
            QStringLiteral("adaptive"), QStringLiteral("horizontal"),
            QStringLiteral("vertical"), QStringLiteral("diagonal"),
            QStringLiteral("circular"), QStringLiteral("ellipse"),
            QStringLiteral("ring"), QStringLiteral("radial"),
            QStringLiteral("arc"), QStringLiteral("semicircle"),
            QStringLiteral("fan"), QStringLiteral("spiral"),
            QStringLiteral("ribbon"), QStringLiteral("vertical-curve"),
            QStringLiteral("horizontal-curve"), QStringLiteral("polygon"),
            QStringLiteral("triangle"), QStringLiteral("square"),
            QStringLiteral("pentagon"), QStringLiteral("hexagon"),
            QStringLiteral("octagon"), QStringLiteral("star"),
            QStringLiteral("grid"), QStringLiteral("floating")};
        const QSet<QString> orientations{QStringLiteral("horizontal"),
                                         QStringLiteral("vertical"),
                                         QStringLiteral("free")};
        const QSet<QString> features{
            QStringLiteral("native-edge-placement"),
            QStringLiteral("arbitrary-xy-placement"),
            QStringLiteral("whole-panel-rotation"),
            QStringLiteral("nonrectangular-input"),
            QStringLiteral("native-auto-hide"),
            QStringLiteral("standard-plasma-widgets"),
            QStringLiteral("thickness-mutation"),
            QStringLiteral("length-mutation"),
            QStringLiteral("dynamic-tint"),
            QStringLiteral("dynamic-glow"),
            QStringLiteral("icon-state-styling")};
        const QSet<QString> presentations{
            QStringLiteral("open"), QStringLiteral("collapse-horizontal"),
            QStringLiteral("collapse-vertical"), QStringLiteral("collapse-radial"),
            QStringLiteral("split"), QStringLiteral("shutter")};

        auto &capabilities = definition->capabilities;
        capabilities.hosts = stringArray(object, QStringLiteral("hosts"),
                                          QStringLiteral("/capabilities"), true,
                                          false, hosts);
        capabilities.rendererTiers = stringArray(
            object, QStringLiteral("rendererTiers"), QStringLiteral("/capabilities"),
            true, false, tiers);
        capabilities.preferredRendererTier = stringValue(
            object, QStringLiteral("preferredRendererTier"),
            QStringLiteral("/capabilities"), true);
        if (!tiers.contains(capabilities.preferredRendererTier))
        {
            add(QStringLiteral("invalid-enum"),
                QStringLiteral("/capabilities/preferredRendererTier"),
                QStringLiteral("unsupported renderer tier"));
        }
        else if (!capabilities.rendererTiers.contains(
                     capabilities.preferredRendererTier))
        {
            add(QStringLiteral("invalid-reference"),
                QStringLiteral("/capabilities/preferredRendererTier"),
                QStringLiteral("preferred renderer tier is not declared"));
        }
        capabilities.fallbackRendererTiers = stringArray(
            object, QStringLiteral("fallbackRendererTiers"),
            QStringLiteral("/capabilities"), true, true, tiers);
        for (qsizetype index = 0; index < capabilities.fallbackRendererTiers.size(); ++index)
        {
            if (!capabilities.rendererTiers.contains(
                    capabilities.fallbackRendererTiers.at(index)))
            {
                add(QStringLiteral("invalid-reference"),
                    QStringLiteral("/capabilities/fallbackRendererTiers/") +
                        QString::number(index),
                    QStringLiteral("fallback renderer tier is not declared"));
            }
        }
        capabilities.layouts = stringArray(object, QStringLiteral("layouts"),
                                            QStringLiteral("/capabilities"), true,
                                            false, layouts);
        capabilities.orientations = stringArray(
            object, QStringLiteral("orientations"), QStringLiteral("/capabilities"),
            true, false, orientations);
        capabilities.features = stringArray(object, QStringLiteral("features"),
                                             QStringLiteral("/capabilities"), true,
                                             true, features);
        capabilities.presentationMechanisms = stringArray(
            object, QStringLiteral("presentationMechanisms"),
            QStringLiteral("/capabilities"), true, true, presentations);

        const QJsonValue rotationValue = object.value(QStringLiteral("rotation"));
        if (!rotationValue.isObject())
        {
            add(object.contains(QStringLiteral("rotation"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/capabilities/rotation"),
                QStringLiteral("rotation must be an object"));
            return;
        }
        const QJsonObject rotation = rotationValue.toObject();
        rejectUnknown(rotation, {QStringLiteral("mode"),
                                 QStringLiteral("minimumDegrees"),
                                 QStringLiteral("maximumDegrees")},
                      QStringLiteral("/capabilities/rotation"));
        capabilities.rotation.mode = stringValue(
            rotation, QStringLiteral("mode"),
            QStringLiteral("/capabilities/rotation"), true);
        if (!QSet<QString>{QStringLiteral("none"), QStringLiteral("bounded"),
                           QStringLiteral("free")}.contains(capabilities.rotation.mode))
        {
            add(QStringLiteral("invalid-enum"),
                QStringLiteral("/capabilities/rotation/mode"),
                QStringLiteral("unsupported rotation mode"));
        }
        if (capabilities.rotation.mode == QStringLiteral("bounded"))
        {
            capabilities.rotation.minimumDegrees = numberValue(
                rotation, QStringLiteral("minimumDegrees"),
                QStringLiteral("/capabilities/rotation"), true, 0.0);
            capabilities.rotation.maximumDegrees = numberValue(
                rotation, QStringLiteral("maximumDegrees"),
                QStringLiteral("/capabilities/rotation"), true, 0.0);
            if (capabilities.rotation.minimumDegrees >
                capabilities.rotation.maximumDegrees)
            {
                add(QStringLiteral("invalid-value"),
                    QStringLiteral("/capabilities/rotation"),
                    QStringLiteral("minimum rotation must not exceed maximum"));
            }
        }
    }

    std::optional<ThemeSize> parseSize(const QJsonValue &value,
                                       const QString &pointer)
    {
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("size must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("width"), QStringLiteral("height")},
                      pointer);
        ThemeSize size;
        size.width = integerValue(object, QStringLiteral("width"), pointer, true, 0, 1);
        size.height = integerValue(object, QStringLiteral("height"), pointer, true, 0, 1);
        if (size.width > ThemePackage::MaximumRasterDimension ||
            size.height > ThemePackage::MaximumRasterDimension ||
            qint64(size.width) * qint64(size.height) > ThemePackage::MaximumRasterPixels)
        {
            add(QStringLiteral("limit-exceeded"), pointer,
                QStringLiteral("declared raster dimensions exceed the decode limit"));
        }
        return size;
    }

    void parseAssets(const QJsonObject &manifest,
                     ThemeDefinition *definition,
                     QHash<QString, QString> *assetPaths)
    {
        if (!manifest.contains(QStringLiteral("assets")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("assets"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/assets"),
                QStringLiteral("assets must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumAssets)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/assets"),
                QStringLiteral("too many declared assets"));
        }
        const QSet<QString> kinds{
            QStringLiteral("raster"), QStringLiteral("vector"),
            QStringLiteral("mask"), QStringLiteral("mesh"),
            QStringLiteral("material"), QStringLiteral("animation-data")};
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/assets/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("asset entry must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("path"),
                                   QStringLiteral("kind"), QStringLiteral("mimeType"),
                                   QStringLiteral("sha256"), QStringLiteral("naturalSize")},
                          pointer);
            ThemeAssetDefinition asset;
            asset.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!asset.id.isEmpty() && ids.contains(asset.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("asset identifier is duplicated"));
            }
            ids.insert(asset.id);
            asset.path = stringValue(object, QStringLiteral("path"), pointer, true);
            asset.kind = stringValue(object, QStringLiteral("kind"), pointer, true);
            if (!kinds.contains(asset.kind))
            {
                add(QStringLiteral("invalid-enum"), pointerChild(pointer, QStringLiteral("kind")),
                    QStringLiteral("unsupported asset kind"));
            }
            asset.mimeType = stringValue(
                object, QStringLiteral("mimeType"), pointer, false);
            asset.sha256 = stringValue(
                object, QStringLiteral("sha256"), pointer, false);
            if (!asset.sha256.isEmpty())
            {
                static const QRegularExpression digestPattern(
                    QStringLiteral("^[0-9a-f]{64}$"));
                if (!digestPattern.match(asset.sha256).hasMatch())
                {
                    add(QStringLiteral("invalid-value"),
                        pointerChild(pointer, QStringLiteral("sha256")),
                        QStringLiteral("sha256 must be lowercase hexadecimal"));
                }
            }
            if (object.contains(QStringLiteral("naturalSize")))
            {
                asset.naturalSize = parseSize(
                    object.value(QStringLiteral("naturalSize")),
                    pointerChild(pointer, QStringLiteral("naturalSize")));
            }

            const QString resolvedPath = resolveAsset(
                asset.path, pointerChild(pointer, QStringLiteral("path")));
            if (!resolvedPath.isEmpty())
            {
                assetPaths->insert(asset.id, resolvedPath);
                if (!asset.sha256.isEmpty())
                {
                    QFile file(resolvedPath);
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) ||
                        QString::fromLatin1(hash.result().toHex()) != asset.sha256)
                    {
                        add(QStringLiteral("asset-hash-mismatch"),
                            pointerChild(pointer, QStringLiteral("sha256")),
                            QStringLiteral("asset bytes do not match the declared digest"));
                    }
                }
            }
            definition->assets.append(asset);
        }
    }

    void parseStates(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("states")))
        {
            ThemeStateDefinition state;
            state.id = QStringLiteral("normal");
            definition->states.append(state);
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("states"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/states"),
                QStringLiteral("states must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumStates)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/states"),
                QStringLiteral("too many states"));
        }
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/states/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("state entry must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("inherits"),
                                   QStringLiteral("layers")}, pointer);
            ThemeStateDefinition state;
            state.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!state.id.isEmpty() && ids.contains(state.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("state identifier is duplicated"));
            }
            state.inherits = stringValue(
                object, QStringLiteral("inherits"), pointer, false);
            if (!state.inherits.isEmpty() && !ids.contains(state.inherits))
            {
                add(QStringLiteral("invalid-reference"),
                    pointerChild(pointer, QStringLiteral("inherits")),
                    QStringLiteral("state may inherit only from an earlier state"));
            }
            state.layers = stringArray(object, QStringLiteral("layers"), pointer,
                                       true, true);
            ids.insert(state.id);
            definition->states.append(state);
        }
    }

    std::optional<ThemeRect> parseRect(const QJsonValue &value,
                                       const QString &pointer)
    {
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("rectangle must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("width"), QStringLiteral("height")},
                      pointer);
        ThemeRect rect;
        rect.x = numberValue(object, QStringLiteral("x"), pointer, true, 0.0);
        rect.y = numberValue(object, QStringLiteral("y"), pointer, true, 0.0);
        rect.width = numberValue(object, QStringLiteral("width"), pointer, true, 0.0);
        rect.height = numberValue(object, QStringLiteral("height"), pointer, true, 0.0);
        if (rect.x < 0.0 || rect.y < 0.0 || rect.width < 0.0 || rect.height < 0.0)
        {
            add(QStringLiteral("invalid-bounds"), pointer,
                QStringLiteral("rectangle values must be non-negative"));
        }
        return rect;
    }

    bool rectFitsAsset(const ThemeRect &rect, const ThemeAssetDefinition *asset) const
    {
        return !asset || !asset->naturalSize.has_value() ||
            (rect.x + rect.width <= asset->naturalSize->width &&
             rect.y + rect.height <= asset->naturalSize->height);
    }

    void parseLayers(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("layers")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("layers"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/layers"),
                QStringLiteral("layers must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumLayers)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/layers"),
                QStringLiteral("too many layers"));
        }
        const QSet<QString> roles{
            QStringLiteral("surface"), QStringLiteral("split-start"),
            QStringLiteral("split-center"), QStringLiteral("split-end"),
            QStringLiteral("rear"), QStringLiteral("foreground"),
            QStringLiteral("glow"), QStringLiteral("overlay"),
            QStringLiteral("shadow"), QStringLiteral("reflection"),
            QStringLiteral("mask"), QStringLiteral("mesh"),
            QStringLiteral("material")};
        const QSet<QString> blendModes{
            QStringLiteral("source-over"), QStringLiteral("multiply"),
            QStringLiteral("screen"), QStringLiteral("add")};
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/layers/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("layer entry must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("asset"),
                                   QStringLiteral("role"), QStringLiteral("sourceRect"),
                                   QStringLiteral("opacity"), QStringLiteral("blendMode")},
                          pointer);
            ThemeLayerDefinition layer;
            layer.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!layer.id.isEmpty() && ids.contains(layer.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("layer identifier is duplicated"));
            }
            ids.insert(layer.id);
            layer.asset = stringValue(object, QStringLiteral("asset"), pointer, true);
            layer.role = stringValue(object, QStringLiteral("role"), pointer, true);
            if (!roles.contains(layer.role))
            {
                add(QStringLiteral("invalid-enum"), pointerChild(pointer, QStringLiteral("role")),
                    QStringLiteral("unsupported layer role"));
            }
            if (object.contains(QStringLiteral("sourceRect")))
            {
                layer.sourceRect = parseRect(
                    object.value(QStringLiteral("sourceRect")),
                    pointerChild(pointer, QStringLiteral("sourceRect")));
            }
            layer.opacity = numberValue(object, QStringLiteral("opacity"), pointer,
                                        false, 1.0);
            if (layer.opacity < 0.0 || layer.opacity > 1.0)
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("opacity")),
                    QStringLiteral("opacity must be between zero and one"));
            }
            layer.blendMode = stringValue(
                object, QStringLiteral("blendMode"), pointer, false,
                QStringLiteral("source-over"));
            if (!blendModes.contains(layer.blendMode))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("blendMode")),
                    QStringLiteral("unsupported blend mode"));
            }
            const ThemeAssetDefinition *asset = definition->assetById(layer.asset);
            if (!asset)
            {
                add(QStringLiteral("invalid-reference"),
                    pointerChild(pointer, QStringLiteral("asset")),
                    QStringLiteral("layer references an unknown asset"));
            }
            else
            {
                const bool compatible =
                    (layer.role == QStringLiteral("mesh") && asset->kind == QStringLiteral("mesh")) ||
                    (layer.role == QStringLiteral("material") && asset->kind == QStringLiteral("material")) ||
                    (layer.role == QStringLiteral("mask") && asset->kind == QStringLiteral("mask")) ||
                    (!QSet<QString>{QStringLiteral("mesh"), QStringLiteral("material"),
                                    QStringLiteral("mask")}.contains(layer.role) &&
                     QSet<QString>{QStringLiteral("raster"), QStringLiteral("vector"),
                                   QStringLiteral("mask")}.contains(asset->kind));
                if (!compatible)
                {
                    add(QStringLiteral("renderer-asset-mismatch"),
                        pointerChild(pointer, QStringLiteral("role")),
                        QStringLiteral("layer role is incompatible with its asset kind"));
                }
                if (layer.sourceRect.has_value() &&
                    !rectFitsAsset(*layer.sourceRect, asset))
                {
                    add(QStringLiteral("invalid-bounds"),
                        pointerChild(pointer, QStringLiteral("sourceRect")),
                        QStringLiteral("layer source rectangle exceeds the asset bounds"));
                }
            }
            definition->layers.append(layer);
        }
    }

    void parseSlices(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("slices")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("slices"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/slices"),
                QStringLiteral("slices must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumSlices)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/slices"),
                QStringLiteral("too many slices"));
        }
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/slices/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("slice entry must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("asset"),
                                   QStringLiteral("state"), QStringLiteral("orientation"),
                                   QStringLiteral("sourceRect"), QStringLiteral("fixedStart"),
                                   QStringLiteral("fixedEnd"), QStringLiteral("centerMode")},
                          pointer);
            ThemeSliceDefinition slice;
            slice.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!slice.id.isEmpty() && ids.contains(slice.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("slice identifier is duplicated"));
            }
            ids.insert(slice.id);
            slice.asset = stringValue(object, QStringLiteral("asset"), pointer, true);
            slice.state = stringValue(object, QStringLiteral("state"), pointer, true);
            slice.orientation = stringValue(
                object, QStringLiteral("orientation"), pointer, true);
            if (!QSet<QString>{QStringLiteral("horizontal"), QStringLiteral("vertical")}
                     .contains(slice.orientation))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("orientation")),
                    QStringLiteral("slice orientation must be horizontal or vertical"));
            }
            const auto rect = parseRect(object.value(QStringLiteral("sourceRect")),
                                        pointerChild(pointer, QStringLiteral("sourceRect")));
            if (rect.has_value())
            {
                slice.sourceRect = *rect;
            }
            slice.fixedStart = numberValue(object, QStringLiteral("fixedStart"),
                                           pointer, false, 0.0);
            slice.fixedEnd = numberValue(object, QStringLiteral("fixedEnd"),
                                         pointer, false, 0.0);
            slice.centerMode = stringValue(
                object, QStringLiteral("centerMode"), pointer, false,
                QStringLiteral("stretch"));
            if (slice.fixedStart < 0.0 || slice.fixedEnd < 0.0)
            {
                add(QStringLiteral("invalid-bounds"), pointer,
                    QStringLiteral("slice fixed extents must be non-negative"));
            }
            if (!QSet<QString>{QStringLiteral("stretch"), QStringLiteral("tile")}
                     .contains(slice.centerMode))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("centerMode")),
                    QStringLiteral("unsupported slice center mode"));
            }
            const qreal extent = slice.orientation == QStringLiteral("vertical")
                ? slice.sourceRect.height : slice.sourceRect.width;
            if (slice.fixedStart + slice.fixedEnd > extent)
            {
                add(QStringLiteral("invalid-bounds"), pointer,
                    QStringLiteral("slice fixed extents exceed its source rectangle"));
            }
            const ThemeAssetDefinition *asset = definition->assetById(slice.asset);
            if (!asset)
            {
                add(QStringLiteral("invalid-reference"),
                    pointerChild(pointer, QStringLiteral("asset")),
                    QStringLiteral("slice references an unknown asset"));
            }
            else if (!rectFitsAsset(slice.sourceRect, asset))
            {
                add(QStringLiteral("invalid-bounds"),
                    pointerChild(pointer, QStringLiteral("sourceRect")),
                    QStringLiteral("slice source rectangle exceeds the asset bounds"));
            }
            definition->slices.append(slice);
        }
    }

    void parseContentRegions(const QJsonObject &manifest,
                             ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("contentRegions")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("contentRegions"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/contentRegions"),
                QStringLiteral("contentRegions must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumContentRegions)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/contentRegions"),
                QStringLiteral("too many content regions"));
        }
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/contentRegions/") +
                QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("content region must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("state"),
                                   QStringLiteral("orientation"), QStringLiteral("shape"),
                                   QStringLiteral("rect"), QStringLiteral("maskAsset"),
                                   QStringLiteral("baseline")}, pointer);
            ThemeContentRegionDefinition region;
            region.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!region.id.isEmpty() && ids.contains(region.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("content region identifier is duplicated"));
            }
            ids.insert(region.id);
            region.state = stringValue(object, QStringLiteral("state"), pointer, true);
            region.orientation = stringValue(
                object, QStringLiteral("orientation"), pointer, true);
            if (!QSet<QString>{QStringLiteral("horizontal"), QStringLiteral("vertical"),
                               QStringLiteral("free")}.contains(region.orientation))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("orientation")),
                    QStringLiteral("unsupported content orientation"));
            }
            region.shape = stringValue(object, QStringLiteral("shape"), pointer,
                                       false, QStringLiteral("rect"));
            if (!QSet<QString>{QStringLiteral("rect"), QStringLiteral("path")}
                     .contains(region.shape))
            {
                add(QStringLiteral("invalid-enum"), pointerChild(pointer, QStringLiteral("shape")),
                    QStringLiteral("unsupported content shape"));
            }
            if (object.contains(QStringLiteral("rect")))
            {
                region.rect = parseRect(object.value(QStringLiteral("rect")),
                                        pointerChild(pointer, QStringLiteral("rect")));
            }
            region.maskAsset = stringValue(
                object, QStringLiteral("maskAsset"), pointer, false);
            if (region.shape == QStringLiteral("rect") && !region.rect.has_value())
            {
                add(QStringLiteral("missing-field"), pointerChild(pointer, QStringLiteral("rect")),
                    QStringLiteral("rect content region requires rectangle geometry"));
            }
            if (region.shape == QStringLiteral("path") && region.maskAsset.isEmpty())
            {
                add(QStringLiteral("missing-field"),
                    pointerChild(pointer, QStringLiteral("maskAsset")),
                    QStringLiteral("path content region requires a mask asset"));
            }
            if (object.contains(QStringLiteral("baseline")))
            {
                region.baseline = numberValue(object, QStringLiteral("baseline"),
                                              pointer, true, 0.0);
                if (*region.baseline < 0.0 ||
                    (region.rect.has_value() && *region.baseline > region.rect->height))
                {
                    add(QStringLiteral("invalid-bounds"),
                        pointerChild(pointer, QStringLiteral("baseline")),
                        QStringLiteral("baseline must fit inside the region"));
                }
            }
            definition->contentRegions.append(region);
        }
    }

    std::optional<ThemePoint> parsePoint(const QJsonValue &value,
                                         const QString &pointer)
    {
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("point must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("x"), QStringLiteral("y")}, pointer);
        ThemePoint point;
        point.x = numberValue(object, QStringLiteral("x"), pointer, true, 0.0);
        point.y = numberValue(object, QStringLiteral("y"), pointer, true, 0.0);
        if (point.x < 0.0 || point.y < 0.0)
        {
            add(QStringLiteral("invalid-bounds"), pointer,
                QStringLiteral("point values must be non-negative"));
        }
        return point;
    }

    void parseTrackDepth(const QJsonObject &object,
                         const QString &pointer,
                         ThemeTrackDefinition *track)
    {
        if (!object.contains(QStringLiteral("depth")))
        {
            add(QStringLiteral("missing-field"),
                pointerChild(pointer, QStringLiteral("depth")),
                QStringLiteral("track requires a depth object"));
            return;
        }
        const QString depthPointer = pointerChild(pointer, QStringLiteral("depth"));
        const QJsonValue value = object.value(QStringLiteral("depth"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), depthPointer,
                QStringLiteral("depth must be an object"));
            return;
        }
        const QJsonObject depth = value.toObject();
        rejectUnknown(depth, {QStringLiteral("farScale"), QStringLiteral("nearScale"),
                              QStringLiteral("occlusionDepth")},
                      depthPointer);
        track->depth.farScale = numberValue(
            depth, QStringLiteral("farScale"), depthPointer, true, 1.0);
        track->depth.nearScale = numberValue(
            depth, QStringLiteral("nearScale"), depthPointer, true, 1.0);
        track->depth.occlusionDepth = numberValue(
            depth, QStringLiteral("occlusionDepth"), depthPointer, true, 0.5);
        if (track->depth.farScale <= 0.0 || track->depth.nearScale <= 0.0 ||
            track->depth.farScale > MaximumTrackScale ||
            track->depth.nearScale > MaximumTrackScale ||
            track->depth.farScale > track->depth.nearScale)
        {
            add(QStringLiteral("invalid-value"), depthPointer,
                QStringLiteral("depth scales must be positive, bounded, and near "
                               "must not be smaller than far"));
        }
        if (track->depth.occlusionDepth < 0.0 || track->depth.occlusionDepth > 1.0)
        {
            add(QStringLiteral("invalid-value"),
                pointerChild(depthPointer, QStringLiteral("occlusionDepth")),
                QStringLiteral("occlusion depth must be between zero and one"));
        }
    }

    void parseTrackTilt(const QJsonObject &object,
                        const QString &pointer,
                        ThemeTrackDefinition *track)
    {
        if (!object.contains(QStringLiteral("tilt")))
        {
            return;
        }
        const QString tiltPointer = pointerChild(pointer, QStringLiteral("tilt"));
        const QJsonValue value = object.value(QStringLiteral("tilt"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), tiltPointer,
                QStringLiteral("tilt must be an object"));
            return;
        }
        const QJsonObject tiltObject = value.toObject();
        rejectUnknown(tiltObject, {QStringLiteral("minimumDegrees"),
                                   QStringLiteral("maximumDegrees"),
                                   QStringLiteral("defaultDegrees")},
                      tiltPointer);
        ThemeTrackTiltDefinition tilt;
        tilt.minimumDegrees = numberValue(
            tiltObject, QStringLiteral("minimumDegrees"), tiltPointer, true, 0.0);
        tilt.maximumDegrees = numberValue(
            tiltObject, QStringLiteral("maximumDegrees"), tiltPointer, true, 0.0);
        tilt.defaultDegrees = numberValue(
            tiltObject, QStringLiteral("defaultDegrees"), tiltPointer, false, 0.0);
        if (tilt.minimumDegrees < -MaximumTrackTiltDegrees ||
            tilt.maximumDegrees > MaximumTrackTiltDegrees ||
            tilt.minimumDegrees > tilt.maximumDegrees ||
            tilt.defaultDegrees < tilt.minimumDegrees ||
            tilt.defaultDegrees > tilt.maximumDegrees)
        {
            add(QStringLiteral("invalid-value"), tiltPointer,
                QStringLiteral("tilt must be a bounded range containing its default"));
        }
        track->tilt = tilt;
    }

    // Anchor paths for the baked 2.5D renderer. A track is metadata: it says
    // where real icons sit on the perspective artwork and how they shrink with
    // depth. Nothing here draws, and no track makes a renderer available.
    void parseTracks(const QJsonObject &manifest, ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("tracks")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("tracks"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/tracks"),
                QStringLiteral("tracks must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumTracks)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/tracks"),
                QStringLiteral("too many tracks"));
        }
        const QSet<QString> shapes{QStringLiteral("ellipse"),
                                   QStringLiteral("polygon"),
                                   QStringLiteral("arc")};
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/tracks/") +
                QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("track must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("state"),
                                   QStringLiteral("shape"), QStringLiteral("center"),
                                   QStringLiteral("radiusX"), QStringLiteral("radiusY"),
                                   QStringLiteral("startDegrees"),
                                   QStringLiteral("sweepDegrees"),
                                   QStringLiteral("sides"), QStringLiteral("depth"),
                                   QStringLiteral("tilt")},
                          pointer);
            ThemeTrackDefinition track;
            track.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!track.id.isEmpty() && ids.contains(track.id))
            {
                add(QStringLiteral("duplicate-id"),
                    pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("track identifier is duplicated"));
            }
            ids.insert(track.id);
            track.state = stringValue(object, QStringLiteral("state"), pointer, false);
            track.shape = stringValue(object, QStringLiteral("shape"), pointer, true);
            if (!shapes.contains(track.shape))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("shape")),
                    QStringLiteral("unsupported track shape"));
            }
            if (object.contains(QStringLiteral("center")))
            {
                const std::optional<ThemePoint> center = parsePoint(
                    object.value(QStringLiteral("center")),
                    pointerChild(pointer, QStringLiteral("center")));
                track.center = center.value_or(ThemePoint{});
            }
            else
            {
                add(QStringLiteral("missing-field"),
                    pointerChild(pointer, QStringLiteral("center")),
                    QStringLiteral("track requires a center point"));
            }
            track.radiusX = numberValue(
                object, QStringLiteral("radiusX"), pointer, true, 0.0);
            track.radiusY = numberValue(
                object, QStringLiteral("radiusY"), pointer, true, 0.0);
            if (track.radiusX <= 0.0 || track.radiusY <= 0.0)
            {
                add(QStringLiteral("invalid-bounds"), pointer,
                    QStringLiteral("track radii must be positive"));
            }
            track.startDegrees = numberValue(
                object, QStringLiteral("startDegrees"), pointer, false, 0.0);
            if (track.startDegrees < -360.0 || track.startDegrees > 360.0)
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("startDegrees")),
                    QStringLiteral("start must be within one turn"));
            }
            const bool closedShape = track.shape == QStringLiteral("ellipse") ||
                track.shape == QStringLiteral("polygon");
            track.sweepDegrees = numberValue(
                object, QStringLiteral("sweepDegrees"), pointer,
                !closedShape, 360.0);
            if (closedShape && object.contains(QStringLiteral("sweepDegrees")) &&
                !qFuzzyCompare(track.sweepDegrees, 360.0))
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("sweepDegrees")),
                    QStringLiteral("a closed track sweeps a full turn"));
            }
            if (qFuzzyIsNull(track.sweepDegrees) ||
                std::abs(track.sweepDegrees) > 360.0)
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("sweepDegrees")),
                    QStringLiteral("sweep must be non-zero and within one turn"));
            }
            track.sides = integerValue(
                object, QStringLiteral("sides"), pointer, false, 8, MinimumTrackSides);
            if (track.sides < MinimumTrackSides || track.sides > MaximumTrackSides)
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("sides")),
                    QStringLiteral("sides must be between three and twelve"));
            }
            parseTrackDepth(object, pointer, &track);
            parseTrackTilt(object, pointer, &track);
            definition->tracks.append(track);
        }
    }

    void parseEffectMargins(const QJsonObject &manifest,
                            ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("effectMargins")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("effectMargins"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/effectMargins"),
                QStringLiteral("effectMargins must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("left"), QStringLiteral("top"),
                               QStringLiteral("right"), QStringLiteral("bottom")},
                      QStringLiteral("/effectMargins"));
        auto &margins = definition->effectMargins;
        margins.left = numberValue(object, QStringLiteral("left"),
                                   QStringLiteral("/effectMargins"), false, 0.0);
        margins.top = numberValue(object, QStringLiteral("top"),
                                  QStringLiteral("/effectMargins"), false, 0.0);
        margins.right = numberValue(object, QStringLiteral("right"),
                                    QStringLiteral("/effectMargins"), false, 0.0);
        margins.bottom = numberValue(object, QStringLiteral("bottom"),
                                     QStringLiteral("/effectMargins"), false, 0.0);
        if (margins.left < 0.0 || margins.top < 0.0 || margins.right < 0.0 ||
            margins.bottom < 0.0)
        {
            add(QStringLiteral("invalid-bounds"), QStringLiteral("/effectMargins"),
                QStringLiteral("effect margins must be non-negative"));
        }
    }

    void parseInputMasks(const QJsonObject &manifest,
                         ThemeDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("inputMasks")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("inputMasks"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/inputMasks"),
                QStringLiteral("inputMasks must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > ThemePackage::MaximumInputMasks)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/inputMasks"),
                QStringLiteral("too many input masks"));
        }
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/inputMasks/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("input mask must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {QStringLiteral("id"), QStringLiteral("asset"),
                                   QStringLiteral("state"), QStringLiteral("orientation"),
                                   QStringLiteral("threshold")}, pointer);
            ThemeInputMaskDefinition mask;
            mask.id = identifier(object, QStringLiteral("id"), pointer, true);
            if (!mask.id.isEmpty() && ids.contains(mask.id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("input mask identifier is duplicated"));
            }
            ids.insert(mask.id);
            mask.asset = stringValue(object, QStringLiteral("asset"), pointer, true);
            mask.state = stringValue(object, QStringLiteral("state"), pointer, true);
            mask.orientation = stringValue(
                object, QStringLiteral("orientation"), pointer, true);
            if (!QSet<QString>{QStringLiteral("horizontal"), QStringLiteral("vertical"),
                               QStringLiteral("free")}.contains(mask.orientation))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("orientation")),
                    QStringLiteral("unsupported input-mask orientation"));
            }
            mask.threshold = numberValue(object, QStringLiteral("threshold"),
                                         pointer, false, 0.5);
            if (mask.threshold < 0.0 || mask.threshold > 1.0)
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("threshold")),
                    QStringLiteral("mask threshold must be between zero and one"));
            }
            definition->inputMasks.append(mask);
        }
    }

    std::optional<ThemeResourceReference> parseResourceReference(
        const QJsonValue &value,
        const QString &pointer,
        bool allowStates,
        const ThemeDefinition &definition)
    {
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("resource reference must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        QSet<QString> allowed{QStringLiteral("id"), QStringLiteral("manifest")};
        if (allowStates)
        {
            allowed.insert(QStringLiteral("states"));
        }
        rejectUnknown(object, allowed, pointer);
        ThemeResourceReference reference;
        reference.id = identifier(object, QStringLiteral("id"), pointer, true);
        reference.manifest = stringValue(
            object, QStringLiteral("manifest"), pointer, false);
        if (!reference.manifest.isEmpty())
        {
            const QString manifestPointer = pointerChild(pointer, QStringLiteral("manifest"));
            if (lexicallySafePath(reference.manifest, manifestPointer) &&
                std::none_of(definition.assets.cbegin(), definition.assets.cend(),
                             [&reference](const ThemeAssetDefinition &asset)
                             {
                                 return asset.path == reference.manifest;
                             }))
            {
                add(QStringLiteral("invalid-reference"), manifestPointer,
                    QStringLiteral("resource manifest is not a declared asset"));
            }
        }
        if (allowStates)
        {
            reference.states = stringArray(object, QStringLiteral("states"), pointer,
                                           false, true);
        }
        return reference;
    }

    void parseResourceReferences(const QJsonObject &manifest,
                                 ThemeDefinition *definition)
    {
        if (manifest.contains(QStringLiteral("iconStyleRef")))
        {
            definition->iconStyleRef = parseResourceReference(
                manifest.value(QStringLiteral("iconStyleRef")),
                QStringLiteral("/iconStyleRef"), false, *definition);
        }
        if (!manifest.contains(QStringLiteral("animationProfileRefs")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("animationProfileRefs"));
        if (!value.isArray())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/animationProfileRefs"),
                QStringLiteral("animationProfileRefs must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        QSet<QString> ids;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/animationProfileRefs/") +
                QString::number(index);
            const auto reference = parseResourceReference(array.at(index), pointer,
                                                           true, *definition);
            if (!reference.has_value())
            {
                continue;
            }
            if (!reference->id.isEmpty() && ids.contains(reference->id))
            {
                add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("animation profile reference is duplicated"));
            }
            ids.insert(reference->id);
            definition->animationProfileRefs.append(*reference);
        }
    }

    void validateReferences(const ThemeDefinition *definition)
    {
        QSet<QString> stateIds;
        QSet<QString> layerIds;
        for (const ThemeStateDefinition &state : definition->states)
        {
            stateIds.insert(state.id);
        }
        for (const ThemeLayerDefinition &layer : definition->layers)
        {
            layerIds.insert(layer.id);
        }
        for (qsizetype stateIndex = 0; stateIndex < definition->states.size(); ++stateIndex)
        {
            const ThemeStateDefinition &state = definition->states.at(stateIndex);
            for (qsizetype layerIndex = 0; layerIndex < state.layers.size(); ++layerIndex)
            {
                if (!layerIds.contains(state.layers.at(layerIndex)))
                {
                    add(QStringLiteral("invalid-reference"),
                        QStringLiteral("/states/") + QString::number(stateIndex) +
                            QStringLiteral("/layers/") + QString::number(layerIndex),
                        QStringLiteral("state references an unknown layer"));
                }
            }
        }
        for (qsizetype index = 0; index < definition->slices.size(); ++index)
        {
            if (!stateIds.contains(definition->slices.at(index).state))
            {
                add(QStringLiteral("invalid-reference"),
                    QStringLiteral("/slices/") + QString::number(index) +
                        QStringLiteral("/state"),
                    QStringLiteral("slice references an unknown state"));
            }
        }
        for (qsizetype index = 0; index < definition->contentRegions.size(); ++index)
        {
            const auto &region = definition->contentRegions.at(index);
            const QString pointer = QStringLiteral("/contentRegions/") +
                QString::number(index);
            if (!stateIds.contains(region.state))
            {
                add(QStringLiteral("invalid-reference"), pointer + QStringLiteral("/state"),
                    QStringLiteral("content region references an unknown state"));
            }
            if (!region.maskAsset.isEmpty() && !definition->assetById(region.maskAsset))
            {
                add(QStringLiteral("invalid-reference"), pointer + QStringLiteral("/maskAsset"),
                    QStringLiteral("content region references an unknown mask asset"));
            }
        }
        for (qsizetype index = 0; index < definition->tracks.size(); ++index)
        {
            const auto &track = definition->tracks.at(index);
            // A track without a state applies to every state; one that names a
            // state must name a declared one.
            if (!track.state.isEmpty() && !stateIds.contains(track.state))
            {
                add(QStringLiteral("invalid-reference"),
                    QStringLiteral("/tracks/") + QString::number(index) +
                        QStringLiteral("/state"),
                    QStringLiteral("track references an unknown state"));
            }
        }
        for (qsizetype index = 0; index < definition->inputMasks.size(); ++index)
        {
            const auto &mask = definition->inputMasks.at(index);
            const QString pointer = QStringLiteral("/inputMasks/") +
                QString::number(index);
            if (!stateIds.contains(mask.state))
            {
                add(QStringLiteral("invalid-reference"), pointer + QStringLiteral("/state"),
                    QStringLiteral("input mask references an unknown state"));
            }
            const ThemeAssetDefinition *asset = definition->assetById(mask.asset);
            if (!asset)
            {
                add(QStringLiteral("invalid-reference"), pointer + QStringLiteral("/asset"),
                    QStringLiteral("input mask references an unknown asset"));
            }
            else if (asset->kind != QStringLiteral("mask"))
            {
                add(QStringLiteral("renderer-asset-mismatch"),
                    pointer + QStringLiteral("/asset"),
                    QStringLiteral("input mask must reference a mask asset"));
            }
        }
        for (qsizetype referenceIndex = 0;
             referenceIndex < definition->animationProfileRefs.size(); ++referenceIndex)
        {
            const auto &reference = definition->animationProfileRefs.at(referenceIndex);
            for (qsizetype stateIndex = 0; stateIndex < reference.states.size(); ++stateIndex)
            {
                if (!stateIds.contains(reference.states.at(stateIndex)))
                {
                    add(QStringLiteral("invalid-reference"),
                        QStringLiteral("/animationProfileRefs/") +
                            QString::number(referenceIndex) + QStringLiteral("/states/") +
                            QString::number(stateIndex),
                        QStringLiteral("animation profile references an unknown state"));
                }
            }
        }
    }

    void validateRendererAssets(const ThemeDefinition &definition)
    {
        const bool hasSurface = std::any_of(
            definition.assets.cbegin(), definition.assets.cend(),
            [](const ThemeAssetDefinition &asset)
            {
                return asset.kind == QStringLiteral("raster") ||
                    asset.kind == QStringLiteral("vector");
            });
        const bool hasMesh = std::any_of(
            definition.assets.cbegin(), definition.assets.cend(),
            [](const ThemeAssetDefinition &asset)
            {
                return asset.kind == QStringLiteral("mesh");
            });
        const QStringList &tiers = definition.capabilities.rendererTiers;
        if ((tiers.contains(QStringLiteral("skinned2d")) ||
             tiers.contains(QStringLiteral("baked2.5d"))) && !hasSurface)
        {
            add(QStringLiteral("renderer-asset-mismatch"), QStringLiteral("/assets"),
                QStringLiteral("declared 2D renderer tier requires a surface asset"));
        }
        if (tiers.contains(QStringLiteral("true3d")) && !hasMesh)
        {
            add(QStringLiteral("renderer-asset-mismatch"), QStringLiteral("/assets"),
                QStringLiteral("true3d requires a declared mesh asset"));
        }
        // A baked 2.5D package must say where real icons sit on its
        // perspective artwork. Without a track it is a flat picture claiming a
        // depth renderer, which is exactly the mismatch this code rejects.
        if (tiers.contains(QStringLiteral("baked2.5d")) &&
            definition.tracks.isEmpty())
        {
            add(QStringLiteral("renderer-asset-mismatch"), QStringLiteral("/tracks"),
                QStringLiteral("baked2.5d requires a declared icon track"));
        }
    }

    QString m_packageRoot;
    QString m_manifestPath;
    qint64 m_totalAssetBytes = 0;
    QVector<ThemeValidationDiagnostic> m_diagnostics;
};

QByteArray packageDigest(const ParsedThemePackage &parsed)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray manifestBytes = QJsonDocument(
        QJsonObject::fromVariantMap(parsed.sourceManifest)).toJson(QJsonDocument::Compact);
    hash.addData(manifestBytes);

    QVector<std::pair<QString, QString>> assets;
    assets.reserve(parsed.definition.assets.size());
    for (const ThemeAssetDefinition &asset : parsed.definition.assets)
    {
        const QString path = parsed.assetPaths.value(asset.id);
        if (!path.isEmpty())
        {
            assets.append({asset.path + QLatin1Char('|') + asset.id, path});
        }
    }
    std::sort(assets.begin(), assets.end());
    for (const auto &[identity, path] : assets)
    {
        hash.addData(QByteArrayView("\0", 1));
        hash.addData(identity.toUtf8());
        hash.addData(QByteArrayView("\0", 1));
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
        {
            hash.addData(&file);
        }
    }
    return hash.result();
}

QVector<ThemeValidationDiagnostic> materializationError(const QString &message)
{
    return {diagnostic(QStringLiteral("invalid-value"), QString{}, message)};
}

}

namespace ArchDock
{

ThemePackageLoadResult ThemePackage::load(const QString &manifestPath)
{
    QFileInfo manifestInfo(manifestPath);
    if (!manifestInfo.exists())
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("theme manifest does not exist"))}};
    }
    if (!manifestInfo.isFile())
    {
        return {{}, {diagnostic(QStringLiteral("asset-not-regular"), QString{},
                                QStringLiteral("theme manifest is not a regular file"))}};
    }
    if (manifestInfo.size() > MaximumManifestBytes)
    {
        return {{}, {diagnostic(QStringLiteral("manifest-too-large"), QString{},
                                QStringLiteral("theme manifest exceeds the byte limit"))}};
    }
    QFile file(manifestInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("theme manifest could not be read"))}};
    }
    const QByteArray bytes = file.read(MaximumManifestBytes + 1);
    return loadBytes(bytes, manifestInfo.absolutePath(),
                     manifestInfo.absoluteFilePath());
}

ThemePackageLoadResult ThemePackage::loadBytes(const QByteArray &manifestBytes,
                                               const QString &packageRoot,
                                               const QString &manifestPath)
{
    ThemePackageLoadResult result;
    if (manifestBytes.size() > MaximumManifestBytes)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("manifest-too-large"), QString{},
            QStringLiteral("theme manifest exceeds the byte limit")));
        return result;
    }

    const QFileInfo rootInfo(packageRoot);
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || canonicalRoot.isEmpty())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsafe-package-root"), QString{},
            QStringLiteral("theme package root must be an existing directory")));
        return result;
    }
    const QString resolvedManifestPath = manifestPath.isEmpty()
        ? QDir(canonicalRoot).filePath(manifestFileName)
        : QFileInfo(manifestPath).absoluteFilePath();
    const QFileInfo manifestInfo(resolvedManifestPath);
    const QString manifestCanonical = manifestInfo.exists()
        ? manifestInfo.canonicalFilePath() : resolvedManifestPath;
    if (!isContainedPath(canonicalRoot, manifestCanonical))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsafe-path"), QString{},
            QStringLiteral("theme manifest is outside the package root")));
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-json"), QString{},
            QStringLiteral("theme manifest is not a JSON object")));
        return result;
    }
    const QJsonObject manifest = document.object();
    if (!manifest.value(QStringLiteral("format")).isString() ||
        manifest.value(QStringLiteral("format")).toString() != manifestFormat)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-format"), QStringLiteral("/format"),
            QStringLiteral("theme manifest format is unsupported")));
        return result;
    }
    const QJsonValue versionValue = manifest.value(QStringLiteral("version"));
    if (!isInteger(versionValue))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-version"), QStringLiteral("/version"),
            QStringLiteral("theme manifest version is unsupported")));
        return result;
    }
    const int version = versionValue.toInt(-1);
    if (version != 1 && version != ThemeDefinition::CurrentVersion)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-version"), QStringLiteral("/version"),
            QStringLiteral("theme manifest version is unsupported")));
        return result;
    }

    ManifestParser parser(canonicalRoot, resolvedManifestPath);
    ParsedThemePackage parsed = version == 1
        ? parser.parseVersion1(manifest) : parser.parseVersion2(manifest);
    result.diagnostics = parser.diagnostics();
    if (hasErrors(result.diagnostics))
    {
        return result;
    }

    ThemePackage package;
    package.m_definition = parsed.definition;
    package.m_sourceVersion = version;
    package.m_sourceRoot = canonicalRoot;
    package.m_manifestPath = resolvedManifestPath;
    package.m_legacyFit = parsed.legacyFit;
    package.m_sourceManifest = parsed.sourceManifest;
    package.m_assetPaths = parsed.assetPaths;
    package.m_contentDigest = packageDigest(parsed);
    result.package = std::move(package);
    return result;
}

ThemePackageMaterializeResult ThemePackage::materialize(
    const QString &managedPackagesRoot) const
{
    ThemePackageMaterializeResult result;
    if (m_contentDigest.isEmpty() || m_definition.id.isEmpty())
    {
        result.diagnostics = materializationError(
            QStringLiteral("invalid theme package cannot be materialized"));
        return result;
    }
    if (!QDir().mkpath(managedPackagesRoot))
    {
        result.diagnostics = materializationError(
            QStringLiteral("managed theme directory could not be created"));
        return result;
    }
    const QString canonicalManagedRoot = QFileInfo(managedPackagesRoot).canonicalFilePath();
    if (canonicalManagedRoot.isEmpty() || !QFileInfo(canonicalManagedRoot).isDir())
    {
        result.diagnostics = {diagnostic(
            QStringLiteral("unsafe-package-root"), QString{},
            QStringLiteral("managed theme root is not a canonical directory"))};
        return result;
    }

    const QString token = QString::fromLatin1(m_contentDigest.toHex().left(16));
    const QString finalPath = QDir(canonicalManagedRoot).filePath(
        m_definition.id + QLatin1Char('-') + token);
    if (QFileInfo::exists(finalPath))
    {
        ThemePackageLoadResult existing = ThemePackage::load(
            QDir(finalPath).filePath(manifestFileName));
        if (existing.isValid() &&
            existing.package->contentDigest() == m_contentDigest)
        {
            result.package = std::move(existing.package);
            result.reusedExisting = true;
            return result;
        }
        result.diagnostics = materializationError(
            QStringLiteral("managed theme destination conflicts with different content"));
        return result;
    }

    QTemporaryDir staging(QDir(canonicalManagedRoot).filePath(
        QStringLiteral(".archdock-theme-XXXXXX")));
    if (!staging.isValid())
    {
        result.diagnostics = materializationError(
            QStringLiteral("managed theme staging directory could not be created"));
        return result;
    }
    QSet<QString> copiedRelativePaths;
    for (const ThemeAssetDefinition &asset : m_definition.assets)
    {
        if (copiedRelativePaths.contains(asset.path))
        {
            continue;
        }
        const QString sourcePath = m_assetPaths.value(asset.id);
        if (sourcePath.isEmpty())
        {
            result.diagnostics = materializationError(
                QStringLiteral("declared asset has no validated source path"));
            return result;
        }
        const QString destinationPath = QDir(staging.path()).filePath(asset.path);
        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath()) ||
            !QFile::copy(sourcePath, destinationPath))
        {
            result.diagnostics = materializationError(
                QStringLiteral("declared asset could not be copied"));
            return result;
        }
        copiedRelativePaths.insert(asset.path);
    }

    const QString stagedManifest = QDir(staging.path()).filePath(manifestFileName);
    QSaveFile manifestFile(stagedManifest);
    const QByteArray normalizedManifest = QJsonDocument(
        QJsonObject::fromVariantMap(m_sourceManifest)).toJson(QJsonDocument::Indented);
    if (!manifestFile.open(QIODevice::WriteOnly) ||
        manifestFile.write(normalizedManifest) != normalizedManifest.size() ||
        !manifestFile.commit())
    {
        result.diagnostics = materializationError(
            QStringLiteral("managed theme manifest could not be written"));
        return result;
    }

    ThemePackageLoadResult staged = ThemePackage::load(stagedManifest);
    if (!staged.isValid() || staged.package->contentDigest() != m_contentDigest)
    {
        result.diagnostics = staged.diagnostics;
        if (result.diagnostics.isEmpty())
        {
            result.diagnostics = materializationError(
                QStringLiteral("managed theme validation changed its content digest"));
        }
        return result;
    }
    if (!QDir().rename(staging.path(), finalPath))
    {
        ThemePackageLoadResult concurrent = ThemePackage::load(
            QDir(finalPath).filePath(manifestFileName));
        if (concurrent.isValid() &&
            concurrent.package->contentDigest() == m_contentDigest)
        {
            result.package = std::move(concurrent.package);
            result.reusedExisting = true;
            return result;
        }
        result.diagnostics = materializationError(
            QStringLiteral("managed theme package could not be published atomically"));
        return result;
    }
    staging.setAutoRemove(false);

    ThemePackageLoadResult published = ThemePackage::load(
        QDir(finalPath).filePath(manifestFileName));
    if (!published.isValid() || published.package->contentDigest() != m_contentDigest)
    {
        result.diagnostics = published.diagnostics;
        if (result.diagnostics.isEmpty())
        {
            result.diagnostics = materializationError(
                QStringLiteral("published managed theme failed validation"));
        }
        return result;
    }
    result.package = std::move(published.package);
    return result;
}

const ThemeDefinition &ThemePackage::definition() const
{
    return m_definition;
}

int ThemePackage::sourceVersion() const
{
    return m_sourceVersion;
}

QString ThemePackage::sourceRoot() const
{
    return m_sourceRoot;
}

QString ThemePackage::manifestPath() const
{
    return m_manifestPath;
}

QString ThemePackage::assetPath(const QString &assetId) const
{
    return m_assetPaths.value(assetId);
}

QString ThemePackage::primarySurfacePath() const
{
    for (const ThemeLayerDefinition &layer : m_definition.layers)
    {
        if (QSet<QString>{QStringLiteral("surface"), QStringLiteral("split-center"),
                          QStringLiteral("split-start"), QStringLiteral("split-end")}
                .contains(layer.role))
        {
            const QString path = assetPath(layer.asset);
            if (!path.isEmpty())
            {
                return path;
            }
        }
    }
    for (const ThemeAssetDefinition &asset : m_definition.assets)
    {
        if (asset.kind == QStringLiteral("raster") ||
            asset.kind == QStringLiteral("vector"))
        {
            return assetPath(asset.id);
        }
    }
    return {};
}

QString ThemePackage::legacyFit() const
{
    return m_legacyFit;
}

QByteArray ThemePackage::contentDigest() const
{
    return m_contentDigest;
}

QVariantMap ThemePackage::runtimeProjection() const
{
    QVariantMap projection = m_definition.toRuntimeProjection();
    QVariantMap paths;
    QStringList assetIds = m_assetPaths.keys();
    std::sort(assetIds.begin(), assetIds.end());
    for (const QString &assetId : assetIds)
    {
        paths.insert(assetId, m_assetPaths.value(assetId));
    }
    projection.insert(QStringLiteral("assetPaths"), paths);
    projection.insert(QStringLiteral("contentDigest"),
                      QString::fromLatin1(m_contentDigest.toHex()));
    projection.insert(QStringLiteral("legacyFit"), m_legacyFit);
    projection.insert(QStringLiteral("manifestPath"), m_manifestPath);
    projection.insert(QStringLiteral("primarySurfacePath"), primarySurfacePath());
    projection.insert(QStringLiteral("sourceRoot"), m_sourceRoot);
    return projection;
}

bool ThemePackageLoadResult::isValid() const
{
    return package.has_value() && !hasErrors(diagnostics);
}

QString ThemePackageLoadResult::primaryCode() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().code;
}

QString ThemePackageLoadResult::primaryMessage() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().message;
}

QVariantMap ThemePackageLoadResult::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("diagnostics"), themeDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("status"), isValid() ? QStringLiteral("valid")
                                              : QStringLiteral("invalid")},
        {QStringLiteral("valid"), isValid()},
    };
    if (package.has_value())
    {
        result.insert(QStringLiteral("theme"), package->runtimeProjection());
    }
    return result;
}

bool ThemePackageMaterializeResult::isValid() const
{
    return package.has_value() && !hasErrors(diagnostics);
}

QVariantMap ThemePackageMaterializeResult::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("diagnostics"), themeDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("reusedExisting"), reusedExisting},
        {QStringLiteral("status"), isValid() ? QStringLiteral("valid")
                                              : QStringLiteral("invalid")},
        {QStringLiteral("valid"), isValid()},
    };
    if (package.has_value())
    {
        result.insert(QStringLiteral("theme"), package->runtimeProjection());
    }
    return result;
}

}
