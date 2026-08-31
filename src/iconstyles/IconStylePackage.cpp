#include "IconStylePackage.h"

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
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{

using namespace ArchDock;

const QString manifestFormat = QStringLiteral("org.archdock.icon-style");
const QString manifestFileName = QStringLiteral("archdock-icon-style.json");

const QSet<QString> requiredStates{
    QStringLiteral("normal"), QStringLiteral("hover"),
    QStringLiteral("pressed"), QStringLiteral("active"),
    QStringLiteral("running"), QStringLiteral("minimized"),
    QStringLiteral("urgent"), QStringLiteral("launching"),
    QStringLiteral("drop"), QStringLiteral("edit"),
    QStringLiteral("disabled"),
};

struct ParsedIconStylePackage
{
    IconStyleDefinition definition;
    QVariantMap sourceManifest;
    QHash<QString, QString> assetPaths;
};

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

    ParsedIconStylePackage parse(const QJsonObject &manifest)
    {
        ParsedIconStylePackage parsed;
        parsed.sourceManifest = manifest.toVariantMap();
        m_assetPaths = &parsed.assetPaths;
        IconStyleDefinition &definition = parsed.definition;

        rejectUnknown(manifest, {
            QStringLiteral("format"), QStringLiteral("version"),
            QStringLiteral("id"), QStringLiteral("name"),
            QStringLiteral("revision"), QStringLiteral("author"),
            QStringLiteral("description"), QStringLiteral("license"),
            QStringLiteral("glyphPolicy"), QStringLiteral("safeGlyphInset"),
            QStringLiteral("layers"), QStringLiteral("states"),
            QStringLiteral("capabilities"), QStringLiteral("preview"),
            QStringLiteral("mappedReplacements"), QStringLiteral("threeD"),
            QStringLiteral("extensions")
        }, QString{});

        definition.id = identifier(manifest, QStringLiteral("id"), QString{}, true);
        definition.name = stringValue(manifest, QStringLiteral("name"), QString{}, true);
        definition.revision = integerValue(
            manifest, QStringLiteral("revision"), QString{}, false, 1, 1,
            std::numeric_limits<int>::max());
        definition.author = stringValue(
            manifest, QStringLiteral("author"), QString{}, false);
        definition.description = stringValue(
            manifest, QStringLiteral("description"), QString{}, false);

        parseLicense(manifest, &definition);
        parseGlyphPolicy(manifest, &definition);
        parseSafeGlyphInset(manifest, &definition);
        parseLayers(manifest, &definition);
        parseStates(manifest, &definition);
        parseCapabilities(manifest, &definition);
        parsePreview(manifest, &definition);
        parseMappedReplacements(manifest, &definition);
        parseThreeD(manifest, &definition);
        parseExtensions(manifest, &definition);
        validateDefinition(definition);
        return parsed;
    }

    const QVector<IconStyleValidationDiagnostic> &diagnostics() const
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
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required string is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!value.isString())
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be a string"));
            return defaultValue;
        }
        const QString result = value.toString();
        if (result.toUtf8().size() > IconStylePackage::MaximumStringBytes)
        {
            add(QStringLiteral("limit-exceeded"), memberPointer,
                QStringLiteral("string exceeds the UTF-8 byte limit"));
            return defaultValue;
        }
        if (required && result.isEmpty())
        {
            add(QStringLiteral("invalid-value"), memberPointer,
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
            QStringLiteral("^[a-z0-9][a-z0-9.-]{0,63}$"));
        if (!value.isEmpty() &&
            (value.toUtf8().size() > IconStylePackage::MaximumIdentifierBytes ||
             !pattern.match(value).hasMatch()))
        {
            add(QStringLiteral("invalid-id"), pointerChild(pointer, key),
                QStringLiteral("identifier does not match the version 1 grammar"));
        }
        return value;
    }

    int integerValue(const QJsonObject &object,
                     const QString &key,
                     const QString &pointer,
                     bool required,
                     int defaultValue,
                     int minimum,
                     int maximum)
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required integer is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!isInteger(value) || value.toDouble() < minimum ||
            value.toDouble() > maximum)
        {
            add(QStringLiteral("invalid-value"), memberPointer,
                QStringLiteral("value must be a finite integer in range"));
            return defaultValue;
        }
        return value.toInt(defaultValue);
    }

    qreal numberValue(const QJsonObject &object,
                      const QString &key,
                      const QString &pointer,
                      bool required,
                      qreal defaultValue,
                      qreal minimum,
                      qreal maximum)
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required number is missing"));
            }
            return defaultValue;
        }
        const QJsonValue value = object.value(key);
        if (!isFiniteNumber(value) || value.toDouble() < minimum ||
            value.toDouble() > maximum)
        {
            add(QStringLiteral("invalid-value"), memberPointer,
                QStringLiteral("value must be a finite number in range"));
            return defaultValue;
        }
        return value.toDouble(defaultValue);
    }

    bool boolValue(const QJsonObject &object,
                   const QString &key,
                   const QString &pointer,
                   bool required,
                   bool defaultValue)
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required boolean is missing"));
            }
            return defaultValue;
        }
        if (!object.value(key).isBool())
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be a boolean"));
            return defaultValue;
        }
        return object.value(key).toBool(defaultValue);
    }

    QString colorValue(const QJsonObject &object,
                       const QString &key,
                       const QString &pointer,
                       bool required,
                       const QString &defaultValue)
    {
        const QString result = stringValue(object, key, pointer, required,
                                           defaultValue);
        static const QRegularExpression pattern(
            QStringLiteral("^(transparent|#[0-9A-Fa-f]{6}|#[0-9A-Fa-f]{8})$"));
        if (!result.isEmpty() && !pattern.match(result).hasMatch())
        {
            add(QStringLiteral("invalid-color"), pointerChild(pointer, key),
                QStringLiteral("color must be transparent, #RRGGBB, or #AARRGGBB"));
        }
        return result;
    }

    QStringList stringArray(const QJsonObject &object,
                            const QString &key,
                            const QString &pointer,
                            bool required,
                            bool allowEmpty,
                            const QSet<QString> &allowed,
                            qsizetype maximum = 64)
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
        if (!object.value(key).isArray())
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be an array"));
            return {};
        }
        const QJsonArray array = object.value(key).toArray();
        if (!allowEmpty && array.isEmpty())
        {
            add(QStringLiteral("invalid-value"), memberPointer,
                QStringLiteral("array must not be empty"));
        }
        if (array.size() > maximum)
        {
            add(QStringLiteral("limit-exceeded"), memberPointer,
                QStringLiteral("array exceeds the entry limit"));
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
            if (entry.isEmpty() ||
                entry.toUtf8().size() > IconStylePackage::MaximumStringBytes)
            {
                add(QStringLiteral("invalid-value"), entryPointer,
                    QStringLiteral("array string is empty or exceeds the byte limit"));
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
        if (m_assetPaths->contains(path))
        {
            return m_assetPaths->value(path);
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
        if (info.size() > IconStylePackage::MaximumAssetBytes)
        {
            add(QStringLiteral("asset-too-large"), pointer,
                QStringLiteral("asset exceeds the per-file byte limit"));
            return {};
        }
        m_totalAssetBytes += info.size();
        if (m_totalAssetBytes > IconStylePackage::MaximumPackageAssetBytes)
        {
            add(QStringLiteral("asset-too-large"), pointer,
                QStringLiteral("declared assets exceed the package byte limit"));
            return {};
        }
        m_assetPaths->insert(path, canonicalPath);
        return canonicalPath;
    }

    void parseLicense(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("license")))
        {
            return;
        }
        if (!manifest.value(QStringLiteral("license")).isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/license"),
                QStringLiteral("license must be an object"));
            return;
        }
        const QJsonObject object = manifest.value(QStringLiteral("license")).toObject();
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
            add(QStringLiteral("invalid-enum"),
                QStringLiteral("/license/redistribution"),
                QStringLiteral("unsupported redistribution value"));
        }
    }

    void parseGlyphPolicy(const QJsonObject &manifest,
                          IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("glyphPolicy"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("glyphPolicy"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/glyphPolicy"),
                QStringLiteral("glyphPolicy must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("mode"), QStringLiteral("tint"),
                               QStringLiteral("compatibleOnly")},
                      QStringLiteral("/glyphPolicy"));
        auto &policy = definition->glyphPolicy;
        policy.mode = stringValue(object, QStringLiteral("mode"),
                                  QStringLiteral("/glyphPolicy"), true,
                                  QStringLiteral("original"));
        const QSet<QString> modes{
            QStringLiteral("original"), QStringLiteral("tinted"),
            QStringLiteral("monochrome"), QStringLiteral("mapped-replacement")};
        if (!modes.contains(policy.mode))
        {
            add(QStringLiteral("invalid-enum"), QStringLiteral("/glyphPolicy/mode"),
                QStringLiteral("unsupported glyph policy"));
        }
        policy.tint = colorValue(object, QStringLiteral("tint"),
                                 QStringLiteral("/glyphPolicy"), false, {});
        policy.compatibleOnly = boolValue(object, QStringLiteral("compatibleOnly"),
                                          QStringLiteral("/glyphPolicy"), false, true);
        if ((policy.mode == QStringLiteral("tinted") ||
             policy.mode == QStringLiteral("monochrome")) && policy.tint.isEmpty())
        {
            add(QStringLiteral("missing-field"), QStringLiteral("/glyphPolicy/tint"),
                QStringLiteral("selected glyph policy requires a tint"));
        }
    }

    void parseSafeGlyphInset(const QJsonObject &manifest,
                             IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("safeGlyphInset"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("safeGlyphInset"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/safeGlyphInset"),
                QStringLiteral("safeGlyphInset must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("left"), QStringLiteral("top"),
                               QStringLiteral("right"), QStringLiteral("bottom")},
                      QStringLiteral("/safeGlyphInset"));
        auto &inset = definition->safeGlyphInset;
        inset.left = numberValue(object, QStringLiteral("left"),
                                 QStringLiteral("/safeGlyphInset"), true, 0.0, 0.0, 0.45);
        inset.top = numberValue(object, QStringLiteral("top"),
                                QStringLiteral("/safeGlyphInset"), true, 0.0, 0.0, 0.45);
        inset.right = numberValue(object, QStringLiteral("right"),
                                  QStringLiteral("/safeGlyphInset"), true, 0.0, 0.0, 0.45);
        inset.bottom = numberValue(object, QStringLiteral("bottom"),
                                   QStringLiteral("/safeGlyphInset"), true, 0.0, 0.0, 0.45);
        if (inset.left + inset.right >= 0.9 || inset.top + inset.bottom >= 0.9)
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/safeGlyphInset"),
                QStringLiteral("opposing insets must leave a visible glyph area"));
        }
    }

    std::optional<IconStyleLayerDefinition> parseLayer(
        const QJsonValue &value,
        const QString &pointer,
        QSet<QString> *layerIds)
    {
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("layer must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {
            QStringLiteral("id"), QStringLiteral("kind"),
            QStringLiteral("asset"), QStringLiteral("shape"),
            QStringLiteral("color"), QStringLiteral("secondaryColor"),
            QStringLiteral("borderColor"), QStringLiteral("opacity"),
            QStringLiteral("inset"), QStringLiteral("radius"),
            QStringLiteral("borderWidth")
        }, pointer);
        IconStyleLayerDefinition layer;
        layer.id = identifier(object, QStringLiteral("id"), pointer, true);
        if (!layer.id.isEmpty() && layerIds->contains(layer.id))
        {
            add(QStringLiteral("duplicate-id"), pointerChild(pointer, QStringLiteral("id")),
                QStringLiteral("layer identifier is duplicated"));
        }
        layerIds->insert(layer.id);
        layer.kind = stringValue(object, QStringLiteral("kind"), pointer, true,
                                 QStringLiteral("procedural"));
        if (!QSet<QString>{QStringLiteral("procedural"), QStringLiteral("asset")}
                 .contains(layer.kind))
        {
            add(QStringLiteral("invalid-enum"), pointerChild(pointer, QStringLiteral("kind")),
                QStringLiteral("unsupported layer kind"));
        }
        layer.opacity = numberValue(object, QStringLiteral("opacity"), pointer,
                                    false, 1.0, 0.0, 1.0);
        layer.inset = numberValue(object, QStringLiteral("inset"), pointer,
                                  false, 0.0, 0.0, 0.45);
        layer.borderWidth = numberValue(object, QStringLiteral("borderWidth"), pointer,
                                        false, 0.0, 0.0, 0.25);
        layer.borderColor = colorValue(object, QStringLiteral("borderColor"), pointer,
                                       false, QStringLiteral("transparent"));
        if (layer.kind == QStringLiteral("asset"))
        {
            layer.asset = stringValue(object, QStringLiteral("asset"), pointer, true);
            if (!layer.asset.isEmpty())
            {
                resolveAsset(layer.asset, pointerChild(pointer, QStringLiteral("asset")));
            }
        }
        else
        {
            layer.shape = stringValue(object, QStringLiteral("shape"), pointer, true,
                                      QStringLiteral("rounded-rect"));
            if (!QSet<QString>{QStringLiteral("rounded-rect"), QStringLiteral("circle"),
                               QStringLiteral("diamond"), QStringLiteral("orb"),
                               QStringLiteral("plate"), QStringLiteral("ring")}
                     .contains(layer.shape))
            {
                add(QStringLiteral("invalid-enum"),
                    pointerChild(pointer, QStringLiteral("shape")),
                    QStringLiteral("unsupported procedural shape"));
            }
            layer.color = colorValue(object, QStringLiteral("color"), pointer, true,
                                     QStringLiteral("transparent"));
            layer.secondaryColor = colorValue(object, QStringLiteral("secondaryColor"),
                                              pointer, false, {});
            layer.radius = numberValue(object, QStringLiteral("radius"), pointer,
                                       false, 0.22, 0.0, 1.0);
            if (object.contains(QStringLiteral("asset")))
            {
                add(QStringLiteral("invalid-value"),
                    pointerChild(pointer, QStringLiteral("asset")),
                    QStringLiteral("procedural layers cannot declare an asset"));
            }
        }
        return layer;
    }

    QVector<IconStyleLayerDefinition> parseLayerArray(
        const QJsonObject &layers,
        const QString &key,
        bool required,
        QSet<QString> *layerIds)
    {
        const QString pointer = QStringLiteral("/layers/") + key;
        if (!layers.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), pointer,
                    QStringLiteral("required layer array is missing"));
            }
            return {};
        }
        if (!layers.value(key).isArray())
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("layer role must be an array"));
            return {};
        }
        const QJsonArray array = layers.value(key).toArray();
        if (array.size() > IconStylePackage::MaximumLayersPerRole)
        {
            add(QStringLiteral("limit-exceeded"), pointer,
                QStringLiteral("layer role exceeds the entry limit"));
        }
        QVector<IconStyleLayerDefinition> result;
        result.reserve(array.size());
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const auto layer = parseLayer(
                array.at(index), pointer + QLatin1Char('/') + QString::number(index),
                layerIds);
            if (layer.has_value())
            {
                result.append(*layer);
            }
        }
        return result;
    }

    void parseLayers(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("layers"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("layers"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/layers"), QStringLiteral("layers must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {
            QStringLiteral("rear"), QStringLiteral("base"), QStringLiteral("front"),
            QStringLiteral("mask"), QStringLiteral("reflection"),
            QStringLiteral("shadow"), QStringLiteral("glow")
        }, QStringLiteral("/layers"));
        QSet<QString> layerIds;
        definition->layers.rear = parseLayerArray(
            object, QStringLiteral("rear"), true, &layerIds);
        definition->layers.base = parseLayerArray(
            object, QStringLiteral("base"), true, &layerIds);
        definition->layers.front = parseLayerArray(
            object, QStringLiteral("front"), true, &layerIds);
        for (const QString &key : {QStringLiteral("mask"),
                                   QStringLiteral("reflection"),
                                   QStringLiteral("shadow"),
                                   QStringLiteral("glow")})
        {
            if (!object.contains(key))
            {
                continue;
            }
            const auto layer = parseLayer(object.value(key),
                                          QStringLiteral("/layers/") + key,
                                          &layerIds);
            if (!layer.has_value())
            {
                continue;
            }
            if (key == QStringLiteral("mask"))
            {
                definition->layers.mask = layer;
            }
            else if (key == QStringLiteral("reflection"))
            {
                definition->layers.reflection = layer;
            }
            else if (key == QStringLiteral("shadow"))
            {
                definition->layers.shadow = layer;
            }
            else
            {
                definition->layers.glow = layer;
            }
        }
    }

    void parseStates(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("states"));
        if (!value.isArray())
        {
            add(manifest.contains(QStringLiteral("states"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/states"), QStringLiteral("states must be an array"));
            return;
        }
        const QJsonArray array = value.toArray();
        if (array.size() > IconStylePackage::MaximumStates)
        {
            add(QStringLiteral("limit-exceeded"), QStringLiteral("/states"),
                QStringLiteral("too many states are declared"));
        }
        QSet<QString> seen;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString pointer = QStringLiteral("/states/") + QString::number(index);
            if (!array.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("state must be an object"));
                continue;
            }
            const QJsonObject object = array.at(index).toObject();
            rejectUnknown(object, {
                QStringLiteral("id"), QStringLiteral("rearOpacity"),
                QStringLiteral("baseOpacity"), QStringLiteral("frontOpacity"),
                QStringLiteral("glyphOpacity"), QStringLiteral("glyphScale"),
                QStringLiteral("borderColor"), QStringLiteral("glowColor"),
                QStringLiteral("glowOpacity"), QStringLiteral("reflectionOpacity"),
                QStringLiteral("indicatorColor"), QStringLiteral("indicatorOpacity")
            }, pointer);
            IconStyleStateDefinition state;
            state.id = stringValue(object, QStringLiteral("id"), pointer, true);
            if (!requiredStates.contains(state.id))
            {
                add(QStringLiteral("invalid-state"),
                    pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("state identifier is not part of version 1"));
            }
            if (seen.contains(state.id))
            {
                add(QStringLiteral("duplicate-id"),
                    pointerChild(pointer, QStringLiteral("id")),
                    QStringLiteral("state identifier is duplicated"));
            }
            seen.insert(state.id);
            state.rearOpacity = numberValue(
                object, QStringLiteral("rearOpacity"), pointer, true, 1.0, 0.0, 1.0);
            state.baseOpacity = numberValue(
                object, QStringLiteral("baseOpacity"), pointer, true, 1.0, 0.0, 1.0);
            state.frontOpacity = numberValue(
                object, QStringLiteral("frontOpacity"), pointer, true, 1.0, 0.0, 1.0);
            state.glyphOpacity = numberValue(
                object, QStringLiteral("glyphOpacity"), pointer, true, 1.0, 0.0, 1.0);
            state.glyphScale = numberValue(
                object, QStringLiteral("glyphScale"), pointer, true, 1.0, 0.5, 1.5);
            state.borderColor = colorValue(
                object, QStringLiteral("borderColor"), pointer, true,
                QStringLiteral("transparent"));
            state.glowColor = colorValue(
                object, QStringLiteral("glowColor"), pointer, true,
                QStringLiteral("transparent"));
            state.glowOpacity = numberValue(
                object, QStringLiteral("glowOpacity"), pointer, true, 0.0, 0.0, 1.0);
            state.reflectionOpacity = numberValue(
                object, QStringLiteral("reflectionOpacity"), pointer, true,
                0.0, 0.0, 1.0);
            state.indicatorColor = colorValue(
                object, QStringLiteral("indicatorColor"), pointer, true,
                QStringLiteral("transparent"));
            state.indicatorOpacity = numberValue(
                object, QStringLiteral("indicatorOpacity"), pointer, true,
                0.0, 0.0, 1.0);
            definition->states.append(state);
        }
        for (const QString &state : requiredStates)
        {
            if (!seen.contains(state))
            {
                add(QStringLiteral("missing-state"), QStringLiteral("/states"),
                    QStringLiteral("required state is missing: ") + state);
            }
        }
    }

    void parseCapabilities(const QJsonObject &manifest,
                           IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("capabilities"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("capabilities"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/capabilities"),
                QStringLiteral("capabilities must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {
            QStringLiteral("rendererTiers"), QStringLiteral("features"),
            QStringLiteral("animationCapabilities"), QStringLiteral("supports3D")
        }, QStringLiteral("/capabilities"));
        const QSet<QString> tiers{
            QStringLiteral("procedural2d"), QStringLiteral("skinned2d"),
            QStringLiteral("baked2.5d"), QStringLiteral("true3d")};
        const QSet<QString> features{
            QStringLiteral("tile"), QStringLiteral("reflection"),
            QStringLiteral("shadow"), QStringLiteral("glow"),
            QStringLiteral("mapped-replacement"),
            QStringLiteral("safe-glyph-inset"),
            QStringLiteral("true3d-fallback"),
            QStringLiteral("state-styling")};
        const QSet<QString> animations{
            QStringLiteral("hover-scale"), QStringLiteral("press-scale"),
            QStringLiteral("glow-pulse"), QStringLiteral("urgent-pulse"),
            QStringLiteral("launching-pulse"),
            QStringLiteral("indicator-transition")};
        auto &capabilities = definition->capabilities;
        capabilities.rendererTiers = stringArray(
            object, QStringLiteral("rendererTiers"), QStringLiteral("/capabilities"),
            true, false, tiers);
        capabilities.features = stringArray(
            object, QStringLiteral("features"), QStringLiteral("/capabilities"),
            true, true, features);
        capabilities.animationCapabilities = stringArray(
            object, QStringLiteral("animationCapabilities"),
            QStringLiteral("/capabilities"), true, true, animations);
        capabilities.supports3D = boolValue(
            object, QStringLiteral("supports3D"), QStringLiteral("/capabilities"),
            true, false);
    }

    void parsePreview(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        const QJsonValue value = manifest.value(QStringLiteral("preview"));
        if (!value.isObject())
        {
            add(manifest.contains(QStringLiteral("preview"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/preview"), QStringLiteral("preview must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("seed"), QStringLiteral("iconName"),
                               QStringLiteral("state"), QStringLiteral("tileSize")},
                      QStringLiteral("/preview"));
        auto &preview = definition->preview;
        preview.seed = stringValue(object, QStringLiteral("seed"),
                                   QStringLiteral("/preview"), true);
        preview.iconName = stringValue(object, QStringLiteral("iconName"),
                                       QStringLiteral("/preview"), true,
                                       QStringLiteral("applications-system"));
        preview.state = stringValue(object, QStringLiteral("state"),
                                    QStringLiteral("/preview"), true,
                                    QStringLiteral("normal"));
        preview.tileSize = integerValue(object, QStringLiteral("tileSize"),
                                        QStringLiteral("/preview"), true, 64, 16, 512);
    }

    void parseMappedReplacements(const QJsonObject &manifest,
                                 IconStyleDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("mappedReplacements")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("mappedReplacements"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/mappedReplacements"),
                QStringLiteral("mappedReplacements must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        if (object.size() > IconStylePackage::MaximumMappedReplacements)
        {
            add(QStringLiteral("limit-exceeded"),
                QStringLiteral("/mappedReplacements"),
                QStringLiteral("too many mapped replacements are declared"));
        }
        static const QRegularExpression identityPattern(
            QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,255}$"));
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            const QString pointer = QStringLiteral("/mappedReplacements/") + it.key();
            if (!identityPattern.match(it.key()).hasMatch())
            {
                add(QStringLiteral("invalid-id"), pointer,
                    QStringLiteral("mapped replacement key is not a stable identity"));
            }
            if (!it.value().isString())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("mapped replacement must be an asset path"));
                continue;
            }
            const QString path = it.value().toString();
            if (path.toUtf8().size() > IconStylePackage::MaximumStringBytes)
            {
                add(QStringLiteral("limit-exceeded"), pointer,
                    QStringLiteral("asset path exceeds the UTF-8 byte limit"));
                continue;
            }
            if (!path.isEmpty())
            {
                resolveAsset(path, pointer);
            }
            else
            {
                add(QStringLiteral("unsafe-path"), pointer,
                    QStringLiteral("mapped replacement path must not be empty"));
            }
            definition->mappedReplacements.insert(it.key(), path);
        }
    }

    void parseThreeD(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("threeD")))
        {
            return;
        }
        const QJsonValue value = manifest.value(QStringLiteral("threeD"));
        if (!value.isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/threeD"),
                QStringLiteral("threeD must be an object"));
            return;
        }
        const QJsonObject object = value.toObject();
        rejectUnknown(object, {QStringLiteral("mesh"), QStringLiteral("material"),
                               QStringLiteral("fallbackStyleId")},
                      QStringLiteral("/threeD"));
        IconStyle3DReference threeD;
        threeD.mesh = stringValue(object, QStringLiteral("mesh"),
                                  QStringLiteral("/threeD"), false);
        threeD.material = stringValue(object, QStringLiteral("material"),
                                      QStringLiteral("/threeD"), false);
        threeD.fallbackStyleId = identifier(object, QStringLiteral("fallbackStyleId"),
                                            QStringLiteral("/threeD"), true);
        if (threeD.mesh.isEmpty() && threeD.material.isEmpty())
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/threeD"),
                QStringLiteral("threeD must declare a mesh or material"));
        }
        if (!threeD.mesh.isEmpty())
        {
            resolveAsset(threeD.mesh, QStringLiteral("/threeD/mesh"));
        }
        if (!threeD.material.isEmpty())
        {
            resolveAsset(threeD.material, QStringLiteral("/threeD/material"));
        }
        definition->threeD = threeD;
    }

    void parseExtensions(const QJsonObject &manifest, IconStyleDefinition *definition)
    {
        if (!manifest.contains(QStringLiteral("extensions")))
        {
            return;
        }
        if (!manifest.value(QStringLiteral("extensions")).isObject())
        {
            add(QStringLiteral("invalid-type"), QStringLiteral("/extensions"),
                QStringLiteral("extensions must be an object"));
            return;
        }
        definition->extensions = manifest.value(QStringLiteral("extensions"))
                                     .toObject().toVariantMap();
    }

    void validateDefinition(const IconStyleDefinition &definition)
    {
        if (!requiredStates.contains(definition.preview.state))
        {
            add(QStringLiteral("invalid-reference"), QStringLiteral("/preview/state"),
                QStringLiteral("preview references an unknown state"));
        }
        const bool mapped = definition.glyphPolicy.mode ==
            QStringLiteral("mapped-replacement");
        if (mapped && definition.mappedReplacements.isEmpty())
        {
            add(QStringLiteral("missing-mapping"),
                QStringLiteral("/mappedReplacements"),
                QStringLiteral("mapped-replacement policy requires at least one mapping"));
        }
        if (!mapped && !definition.mappedReplacements.isEmpty())
        {
            add(QStringLiteral("invalid-value"),
                QStringLiteral("/mappedReplacements"),
                QStringLiteral("mappings require the mapped-replacement glyph policy"));
        }
        if (definition.capabilities.supports3D != definition.threeD.has_value())
        {
            add(QStringLiteral("capability-mismatch"), QStringLiteral("/threeD"),
                QStringLiteral("supports3D and threeD reference must agree"));
        }
        if (definition.capabilities.supports3D &&
            !definition.capabilities.rendererTiers.contains(QStringLiteral("true3d")))
        {
            add(QStringLiteral("capability-mismatch"),
                QStringLiteral("/capabilities/rendererTiers"),
                QStringLiteral("3D support requires the true3d renderer tier"));
        }
    }

    QString m_packageRoot;
    QString m_manifestPath;
    qint64 m_totalAssetBytes = 0;
    QHash<QString, QString> *m_assetPaths = nullptr;
    QVector<IconStyleValidationDiagnostic> m_diagnostics;
};

QByteArray packageDigest(const ParsedIconStylePackage &parsed)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray manifestBytes = QJsonDocument(
        QJsonObject::fromVariantMap(parsed.sourceManifest)).toJson(QJsonDocument::Compact);
    hash.addData(manifestBytes);

    QStringList assets = parsed.assetPaths.keys();
    std::sort(assets.begin(), assets.end());
    for (const QString &relativePath : assets)
    {
        hash.addData(QByteArrayView("\0", 1));
        hash.addData(relativePath.toUtf8());
        hash.addData(QByteArrayView("\0", 1));
        QFile file(parsed.assetPaths.value(relativePath));
        if (file.open(QIODevice::ReadOnly))
        {
            hash.addData(&file);
        }
    }
    return hash.result();
}

}

namespace ArchDock
{

IconStylePackageLoadResult IconStylePackage::load(const QString &manifestPath)
{
    const QFileInfo manifestInfo(manifestPath);
    if (!manifestInfo.exists())
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("icon-style manifest does not exist"))}};
    }
    if (!manifestInfo.isFile())
    {
        return {{}, {diagnostic(QStringLiteral("asset-not-regular"), QString{},
                                QStringLiteral("icon-style manifest is not a regular file"))}};
    }
    if (manifestInfo.size() > MaximumManifestBytes)
    {
        return {{}, {diagnostic(QStringLiteral("manifest-too-large"), QString{},
                                QStringLiteral("icon-style manifest exceeds the byte limit"))}};
    }
    QFile file(manifestInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        return {{}, {diagnostic(QStringLiteral("missing-asset"), QString{},
                                QStringLiteral("icon-style manifest could not be read"))}};
    }
    const QByteArray bytes = file.read(MaximumManifestBytes + 1);
    return loadBytes(bytes, manifestInfo.absolutePath(), manifestInfo.absoluteFilePath());
}

IconStylePackageLoadResult IconStylePackage::loadBytes(
    const QByteArray &manifestBytes,
    const QString &packageRoot,
    const QString &manifestPath)
{
    IconStylePackageLoadResult result;
    if (manifestBytes.size() > MaximumManifestBytes)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("manifest-too-large"), QString{},
            QStringLiteral("icon-style manifest exceeds the byte limit")));
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
    const QString resolvedManifestPath = manifestPath.isEmpty()
        ? QDir(canonicalRoot).filePath(manifestFileName)
        : QFileInfo(manifestPath).absoluteFilePath();
    const QFileInfo manifestInfo(resolvedManifestPath);
    const QString manifestCanonical = manifestInfo.exists()
        ? manifestInfo.canonicalFilePath() : QDir::cleanPath(resolvedManifestPath);
    if (!isContainedPath(canonicalRoot, manifestCanonical))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsafe-path"), QString{},
            QStringLiteral("icon-style manifest is outside the package root")));
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-json"), QString{},
            QStringLiteral("icon-style manifest is not a JSON object")));
        return result;
    }
    const QJsonObject manifest = document.object();
    if (!manifest.value(QStringLiteral("format")).isString() ||
        manifest.value(QStringLiteral("format")).toString() != manifestFormat)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-format"), QStringLiteral("/format"),
            QStringLiteral("icon-style manifest format is unsupported")));
        return result;
    }
    const QJsonValue versionValue = manifest.value(QStringLiteral("version"));
    if (!isInteger(versionValue) ||
        versionValue.toInt(-1) != IconStyleDefinition::CurrentVersion)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-version"), QStringLiteral("/version"),
            QStringLiteral("icon-style manifest version is unsupported")));
        return result;
    }

    ManifestParser parser(canonicalRoot, resolvedManifestPath);
    ParsedIconStylePackage parsed = parser.parse(manifest);
    result.diagnostics = parser.diagnostics();
    if (hasErrors(result.diagnostics))
    {
        return result;
    }

    IconStylePackage package;
    package.m_definition = parsed.definition;
    package.m_sourceRoot = canonicalRoot;
    package.m_manifestPath = resolvedManifestPath;
    package.m_assetPaths = parsed.assetPaths;
    package.m_contentDigest = packageDigest(parsed);
    result.package = std::move(package);
    return result;
}

const IconStyleDefinition &IconStylePackage::definition() const
{
    return m_definition;
}

QString IconStylePackage::sourceRoot() const
{
    return m_sourceRoot;
}

QString IconStylePackage::manifestPath() const
{
    return m_manifestPath;
}

QString IconStylePackage::assetPath(const QString &relativePath) const
{
    return m_assetPaths.value(relativePath);
}

QByteArray IconStylePackage::contentDigest() const
{
    return m_contentDigest;
}

QVariantMap IconStylePackage::runtimeProjection() const
{
    QVariantMap projection = m_definition.toRuntimeProjection();
    QVariantMap paths;
    QStringList relativePaths = m_assetPaths.keys();
    std::sort(relativePaths.begin(), relativePaths.end());
    for (const QString &relativePath : relativePaths)
    {
        paths.insert(relativePath, m_assetPaths.value(relativePath));
    }
    projection.insert(QStringLiteral("assetPaths"), paths);
    projection.insert(QStringLiteral("contentDigest"),
                      QString::fromLatin1(m_contentDigest.toHex()));
    projection.insert(QStringLiteral("manifestPath"), m_manifestPath);
    projection.insert(QStringLiteral("sourceRoot"), m_sourceRoot);
    return projection;
}

bool IconStylePackageLoadResult::isValid() const
{
    return package.has_value() && !hasErrors(diagnostics);
}

QString IconStylePackageLoadResult::primaryCode() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().code;
}

QString IconStylePackageLoadResult::primaryMessage() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().message;
}

QVariantMap IconStylePackageLoadResult::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("diagnostics"),
         iconStyleDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("status"), isValid() ? QStringLiteral("valid")
                                              : QStringLiteral("invalid")},
        {QStringLiteral("valid"), isValid()},
    };
    if (package.has_value())
    {
        result.insert(QStringLiteral("iconStyle"), package->runtimeProjection());
    }
    return result;
}

}
