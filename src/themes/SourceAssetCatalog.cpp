#include "SourceAssetCatalog.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
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

namespace
{

using namespace ArchDock;

constexpr qsizetype maximumCatalogBytes = 2 * 1024 * 1024;
constexpr qsizetype maximumCatalogStringBytes = 4096;

ThemeValidationDiagnostic catalogDiagnostic(const QString &code,
                                            const QString &pointer,
                                            const QString &message)
{
    return {code, pointer, QStringLiteral("error"), message};
}

bool containsErrors(const QVector<ThemeValidationDiagnostic> &diagnostics)
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

bool isSafeRelativePath(const QString &path)
{
    const QStringList parts = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    static const QRegularExpression schemeOrDrive(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    return !path.isEmpty() && !path.contains(QChar::Null) &&
        !path.contains(QLatin1Char('\\')) && !path.startsWith(QLatin1Char('/')) &&
        !QDir::isAbsolutePath(path) && !schemeOrDrive.match(path).hasMatch() &&
        QDir::cleanPath(path) == path &&
        std::none_of(parts.cbegin(), parts.cend(), [](const QString &part)
        {
            return part.isEmpty() || part == QStringLiteral(".") ||
                part == QStringLiteral("..");
        });
}

class CatalogParser
{
public:
    SourceAssetCatalog parse(const QJsonObject &root)
    {
        SourceAssetCatalog catalog;
        rejectUnknown(root, {QStringLiteral("format"), QStringLiteral("version"),
                             QStringLiteral("archive"), QStringLiteral("policy"),
                             QStringLiteral("assets")}, QString{});

        catalog.format = requiredString(root, QStringLiteral("format"), QString{});
        if (catalog.format != QStringLiteral("org.archdock.source-asset-catalog"))
        {
            add(QStringLiteral("unsupported-format"), QStringLiteral("/format"),
                QStringLiteral("source asset catalog format is unsupported"));
        }
        catalog.version = requiredInteger(root, QStringLiteral("version"), QString{});
        if (catalog.version != SourceAssetCatalog::CurrentVersion)
        {
            add(QStringLiteral("unsupported-version"), QStringLiteral("/version"),
                QStringLiteral("source asset catalog version is unsupported"));
        }
        parseArchive(root, &catalog);
        parsePolicy(root, &catalog);
        parseAssets(root, &catalog);
        validateCatalog(catalog);
        return catalog;
    }

    const QVector<ThemeValidationDiagnostic> &diagnostics() const
    {
        return m_diagnostics;
    }

private:
    void add(const QString &code, const QString &pointer, const QString &message)
    {
        m_diagnostics.append(catalogDiagnostic(code, pointer, message));
    }

    static QString child(const QString &pointer, const QString &key)
    {
        return pointer + QLatin1Char('/') + key;
    }

    void rejectUnknown(const QJsonObject &object,
                       const QSet<QString> &allowed,
                       const QString &pointer)
    {
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            if (!allowed.contains(it.key()))
            {
                add(QStringLiteral("unknown-field"), child(pointer, it.key()),
                    QStringLiteral("unknown source catalog member"));
            }
        }
    }

    QString requiredString(const QJsonObject &object,
                           const QString &key,
                           const QString &pointer)
    {
        if (!object.contains(key))
        {
            add(QStringLiteral("missing-field"), child(pointer, key),
                QStringLiteral("required string is missing"));
            return {};
        }
        if (!object.value(key).isString())
        {
            add(QStringLiteral("invalid-type"), child(pointer, key),
                QStringLiteral("value must be a string"));
            return {};
        }
        const QString value = object.value(key).toString();
        if (value.isEmpty())
        {
            add(QStringLiteral("invalid-value"), child(pointer, key),
                QStringLiteral("required string must not be empty"));
        }
        if (value.toUtf8().size() > maximumCatalogStringBytes)
        {
            add(QStringLiteral("limit-exceeded"), child(pointer, key),
                QStringLiteral("string exceeds the catalog byte limit"));
        }
        return value;
    }

    int requiredInteger(const QJsonObject &object,
                        const QString &key,
                        const QString &pointer)
    {
        if (!object.contains(key))
        {
            add(QStringLiteral("missing-field"), child(pointer, key),
                QStringLiteral("required integer is missing"));
            return 0;
        }
        const QJsonValue value = object.value(key);
        const double number = value.toDouble(-1.0);
        if (!value.isDouble() || number < 0.0 || std::trunc(number) != number ||
            number > std::numeric_limits<int>::max())
        {
            add(QStringLiteral("invalid-value"), child(pointer, key),
                QStringLiteral("value must be a non-negative integer"));
            return 0;
        }
        return value.toInt();
    }

    bool requiredBoolean(const QJsonObject &object,
                         const QString &key,
                         const QString &pointer)
    {
        if (!object.contains(key))
        {
            add(QStringLiteral("missing-field"), child(pointer, key),
                QStringLiteral("required boolean is missing"));
            return false;
        }
        if (!object.value(key).isBool())
        {
            add(QStringLiteral("invalid-type"), child(pointer, key),
                QStringLiteral("value must be a boolean"));
            return false;
        }
        return object.value(key).toBool();
    }

    QStringList requiredStringArray(const QJsonObject &object,
                                    const QString &key,
                                    const QString &pointer,
                                    const QSet<QString> &allowed,
                                    bool allowEmpty)
    {
        const QString arrayPointer = child(pointer, key);
        if (!object.contains(key))
        {
            add(QStringLiteral("missing-field"), arrayPointer,
                QStringLiteral("required array is missing"));
            return {};
        }
        if (!object.value(key).isArray())
        {
            add(QStringLiteral("invalid-type"), arrayPointer,
                QStringLiteral("value must be an array"));
            return {};
        }
        const QJsonArray array = object.value(key).toArray();
        if (!allowEmpty && array.isEmpty())
        {
            add(QStringLiteral("invalid-value"), arrayPointer,
                QStringLiteral("array must not be empty"));
        }
        QStringList result;
        QSet<QString> seen;
        for (qsizetype index = 0; index < array.size(); ++index)
        {
            const QString entryPointer = arrayPointer + QLatin1Char('/') +
                QString::number(index);
            if (!array.at(index).isString())
            {
                add(QStringLiteral("invalid-type"), entryPointer,
                    QStringLiteral("array entry must be a string"));
                continue;
            }
            const QString entry = array.at(index).toString();
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
                    QStringLiteral("unsupported catalog value"));
            }
            result.append(entry);
        }
        return result;
    }

    QJsonObject requiredObject(const QJsonObject &root,
                               const QString &key,
                               const QString &pointer)
    {
        if (!root.contains(key))
        {
            add(QStringLiteral("missing-field"), child(pointer, key),
                QStringLiteral("required object is missing"));
            return {};
        }
        if (!root.value(key).isObject())
        {
            add(QStringLiteral("invalid-type"), child(pointer, key),
                QStringLiteral("value must be an object"));
            return {};
        }
        return root.value(key).toObject();
    }

    void parseArchive(const QJsonObject &root, SourceAssetCatalog *catalog)
    {
        const QJsonObject archive = requiredObject(
            root, QStringLiteral("archive"), QString{});
        rejectUnknown(archive, {QStringLiteral("fileName"), QStringLiteral("sha256"),
                                QStringLiteral("contentRoot"),
                                QStringLiteral("expectedPanelCount"),
                                QStringLiteral("expectedIconReferenceCount"),
                                QStringLiteral("expectedAssetCount")},
                      QStringLiteral("/archive"));
        catalog->archiveFileName = requiredString(
            archive, QStringLiteral("fileName"), QStringLiteral("/archive"));
        catalog->archiveSha256 = requiredString(
            archive, QStringLiteral("sha256"), QStringLiteral("/archive"));
        catalog->archiveContentRoot = requiredString(
            archive, QStringLiteral("contentRoot"), QStringLiteral("/archive"));
        catalog->expectedPanelCount = requiredInteger(
            archive, QStringLiteral("expectedPanelCount"), QStringLiteral("/archive"));
        catalog->expectedIconReferenceCount = requiredInteger(
            archive, QStringLiteral("expectedIconReferenceCount"),
            QStringLiteral("/archive"));
        catalog->expectedAssetCount = requiredInteger(
            archive, QStringLiteral("expectedAssetCount"), QStringLiteral("/archive"));
        static const QRegularExpression digest(QStringLiteral("^[0-9a-f]{64}$"));
        if (!digest.match(catalog->archiveSha256).hasMatch())
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/archive/sha256"),
                QStringLiteral("archive digest must be lowercase SHA-256"));
        }
        if (!isSafeRelativePath(catalog->archiveContentRoot))
        {
            add(QStringLiteral("unsafe-path"), QStringLiteral("/archive/contentRoot"),
                QStringLiteral("archive content root must be a safe relative path"));
        }
    }

    void parsePolicy(const QJsonObject &root, SourceAssetCatalog *catalog)
    {
        const QJsonObject policy = requiredObject(
            root, QStringLiteral("policy"), QString{});
        rejectUnknown(policy, {QStringLiteral("sourceOnly"),
                               QStringLiteral("installByDefault"),
                               QStringLiteral("installEligibleStatus")},
                      QStringLiteral("/policy"));
        catalog->sourceOnly = requiredBoolean(
            policy, QStringLiteral("sourceOnly"), QStringLiteral("/policy"));
        catalog->installByDefault = requiredBoolean(
            policy, QStringLiteral("installByDefault"), QStringLiteral("/policy"));
        catalog->installEligibleStatus = requiredString(
            policy, QStringLiteral("installEligibleStatus"),
            QStringLiteral("/policy"));
    }

    SourceAssetProvenance parseProvenance(const QJsonObject &object,
                                          const QString &pointer)
    {
        SourceAssetProvenance provenance;
        rejectUnknown(object, {QStringLiteral("sourceKind"),
                               QStringLiteral("creator"),
                               QStringLiteral("licenseSpdx"),
                               QStringLiteral("redistribution"),
                               QStringLiteral("evidence")}, pointer);
        provenance.sourceKind = requiredString(
            object, QStringLiteral("sourceKind"), pointer);
        provenance.creator = requiredString(
            object, QStringLiteral("creator"), pointer);
        provenance.licenseSpdx = object.value(
            QStringLiteral("licenseSpdx")).toString();
        if (!object.contains(QStringLiteral("licenseSpdx")) ||
            !object.value(QStringLiteral("licenseSpdx")).isString())
        {
            add(QStringLiteral("invalid-type"), child(pointer, QStringLiteral("licenseSpdx")),
                QStringLiteral("licenseSpdx must be a string"));
        }
        provenance.redistribution = requiredString(
            object, QStringLiteral("redistribution"), pointer);
        provenance.evidence = requiredString(
            object, QStringLiteral("evidence"), pointer);
        if (!QSet<QString>{QStringLiteral("unknown"), QStringLiteral("allowed"),
                           QStringLiteral("forbidden")}
                 .contains(provenance.redistribution))
        {
            add(QStringLiteral("invalid-enum"),
                child(pointer, QStringLiteral("redistribution")),
                QStringLiteral("unsupported redistribution status"));
        }
        return provenance;
    }

    void parseAssets(const QJsonObject &root, SourceAssetCatalog *catalog)
    {
        if (!root.value(QStringLiteral("assets")).isArray())
        {
            add(root.contains(QStringLiteral("assets"))
                    ? QStringLiteral("invalid-type") : QStringLiteral("missing-field"),
                QStringLiteral("/assets"), QStringLiteral("assets must be an array"));
            return;
        }
        const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
        QSet<QString> ids;
        QSet<QString> members;
        static const QRegularExpression identifier(
            QStringLiteral("^[a-z0-9][a-z0-9._-]{0,63}$"));
        static const QRegularExpression digest(QStringLiteral("^[0-9a-f]{64}$"));
        const QSet<QString> orientations{QStringLiteral("horizontal"),
                                         QStringLiteral("vertical"),
                                         QStringLiteral("free")};
        const QSet<QString> cleanup{
            QStringLiteral("manual-review"), QStringLiteral("isolate-clean-assets"),
            QStringLiteral("crop"), QStringLiteral("alpha-mask-review"),
            QStringLiteral("remove-placeholder"), QStringLiteral("remove-logo"),
            QStringLiteral("split-layers"), QStringLiteral("derive-state-pair"),
            QStringLiteral("none")};
        for (qsizetype index = 0; index < assets.size(); ++index)
        {
            const QString pointer = QStringLiteral("/assets/") + QString::number(index);
            if (!assets.at(index).isObject())
            {
                add(QStringLiteral("invalid-type"), pointer,
                    QStringLiteral("asset record must be an object"));
                continue;
            }
            const QJsonObject object = assets.at(index).toObject();
            rejectUnknown(object, {
                QStringLiteral("id"), QStringLiteral("archiveMember"),
                QStringLiteral("fileName"), QStringLiteral("sha256"),
                QStringLiteral("byteSize"), QStringLiteral("sampleKind"),
                QStringLiteral("assetClass"), QStringLiteral("intendedUse"),
                QStringLiteral("provenance"), QStringLiteral("targetRendererTier"),
                QStringLiteral("orientations"), QStringLiteral("cleanupRequirements"),
                QStringLiteral("statePairAvailability"), QStringLiteral("status"),
                QStringLiteral("visualGroups"), QStringLiteral("reviewNotes"),
                QStringLiteral("isolatedCleanAsset"),
                QStringLiteral("opaqueScreenshot")
            }, pointer);

            SourceAssetRecord record;
            record.id = requiredString(object, QStringLiteral("id"), pointer);
            if (!identifier.match(record.id).hasMatch())
            {
                add(QStringLiteral("invalid-id"), child(pointer, QStringLiteral("id")),
                    QStringLiteral("source asset ID is invalid"));
            }
            if (ids.contains(record.id))
            {
                add(QStringLiteral("duplicate-id"), child(pointer, QStringLiteral("id")),
                    QStringLiteral("source asset ID is duplicated"));
            }
            ids.insert(record.id);
            record.archiveMember = requiredString(
                object, QStringLiteral("archiveMember"), pointer);
            if (!isSafeRelativePath(record.archiveMember) ||
                !record.archiveMember.startsWith(
                    catalog->archiveContentRoot + QLatin1Char('/')))
            {
                add(QStringLiteral("unsafe-path"),
                    child(pointer, QStringLiteral("archiveMember")),
                    QStringLiteral("archive member escapes the declared content root"));
            }
            if (members.contains(record.archiveMember))
            {
                add(QStringLiteral("duplicate-value"),
                    child(pointer, QStringLiteral("archiveMember")),
                    QStringLiteral("archive member is cataloged more than once"));
            }
            members.insert(record.archiveMember);
            record.fileName = requiredString(
                object, QStringLiteral("fileName"), pointer);
            if (record.fileName != QFileInfo(record.archiveMember).fileName())
            {
                add(QStringLiteral("invalid-value"),
                    child(pointer, QStringLiteral("fileName")),
                    QStringLiteral("fileName does not match archiveMember"));
            }
            record.sha256 = requiredString(
                object, QStringLiteral("sha256"), pointer);
            if (!digest.match(record.sha256).hasMatch())
            {
                add(QStringLiteral("invalid-value"), child(pointer, QStringLiteral("sha256")),
                    QStringLiteral("asset digest must be lowercase SHA-256"));
            }
            record.byteSize = requiredInteger(
                object, QStringLiteral("byteSize"), pointer);
            if (record.byteSize <= 0)
            {
                add(QStringLiteral("invalid-value"), child(pointer, QStringLiteral("byteSize")),
                    QStringLiteral("source asset byte size must be positive"));
            }
            record.sampleKind = requiredString(
                object, QStringLiteral("sampleKind"), pointer);
            if (!QSet<QString>{QStringLiteral("panel"),
                               QStringLiteral("icon-reference")}
                     .contains(record.sampleKind))
            {
                add(QStringLiteral("invalid-enum"),
                    child(pointer, QStringLiteral("sampleKind")),
                    QStringLiteral("unsupported sample kind"));
            }
            record.assetClass = requiredString(
                object, QStringLiteral("assetClass"), pointer);
            record.intendedUse = requiredString(
                object, QStringLiteral("intendedUse"), pointer);
            if (!object.value(QStringLiteral("provenance")).isObject())
            {
                add(QStringLiteral("invalid-type"), child(pointer, QStringLiteral("provenance")),
                    QStringLiteral("provenance must be an object"));
            }
            else
            {
                record.provenance = parseProvenance(
                    object.value(QStringLiteral("provenance")).toObject(),
                    child(pointer, QStringLiteral("provenance")));
            }
            record.targetRendererTier = requiredString(
                object, QStringLiteral("targetRendererTier"), pointer);
            if (!QSet<QString>{QStringLiteral("unknown"),
                               QStringLiteral("procedural2d"),
                               QStringLiteral("skinned2d"),
                               QStringLiteral("baked2.5d"),
                               QStringLiteral("true3d")}
                     .contains(record.targetRendererTier))
            {
                add(QStringLiteral("invalid-enum"),
                    child(pointer, QStringLiteral("targetRendererTier")),
                    QStringLiteral("unsupported target renderer tier"));
            }
            record.orientations = requiredStringArray(
                object, QStringLiteral("orientations"), pointer,
                orientations, true);
            record.cleanupRequirements = requiredStringArray(
                object, QStringLiteral("cleanupRequirements"), pointer,
                cleanup, false);
            record.statePairAvailability = requiredString(
                object, QStringLiteral("statePairAvailability"), pointer);
            if (!QSet<QString>{QStringLiteral("unknown"),
                               QStringLiteral("not-applicable"),
                               QStringLiteral("absent"), QStringLiteral("partial"),
                               QStringLiteral("paired")}
                     .contains(record.statePairAvailability))
            {
                add(QStringLiteral("invalid-enum"),
                    child(pointer, QStringLiteral("statePairAvailability")),
                    QStringLiteral("unsupported state-pair status"));
            }
            record.status = requiredString(
                object, QStringLiteral("status"), pointer);
            if (!QSet<QString>{QStringLiteral("reference-only"),
                               QStringLiteral("production-approved"),
                               QStringLiteral("rejected")}
                     .contains(record.status))
            {
                add(QStringLiteral("invalid-enum"), child(pointer, QStringLiteral("status")),
                    QStringLiteral("unsupported asset status"));
            }
            record.visualGroups = requiredStringArray(
                object, QStringLiteral("visualGroups"), pointer, {}, true);
            record.reviewNotes = requiredString(
                object, QStringLiteral("reviewNotes"), pointer);
            record.isolatedCleanAsset = requiredBoolean(
                object, QStringLiteral("isolatedCleanAsset"), pointer);
            record.opaqueScreenshot = requiredBoolean(
                object, QStringLiteral("opaqueScreenshot"), pointer);
            if (record.status == QStringLiteral("production-approved") &&
                !record.isInstallEligible())
            {
                add(QStringLiteral("invalid-value"), child(pointer, QStringLiteral("status")),
                    QStringLiteral("production status lacks install eligibility evidence"));
            }
            catalog->assets.append(record);
        }
    }

    void validateCatalog(const SourceAssetCatalog &catalog)
    {
        const qsizetype panelCount = std::count_if(
            catalog.assets.cbegin(), catalog.assets.cend(),
            [](const SourceAssetRecord &record)
            {
                return record.sampleKind == QStringLiteral("panel");
            });
        const qsizetype iconCount = std::count_if(
            catalog.assets.cbegin(), catalog.assets.cend(),
            [](const SourceAssetRecord &record)
            {
                return record.sampleKind == QStringLiteral("icon-reference");
            });
        if (catalog.expectedAssetCount != SourceAssetCatalog::ExpectedAssetCount ||
            catalog.assets.size() != SourceAssetCatalog::ExpectedAssetCount)
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/archive/expectedAssetCount"),
                QStringLiteral("catalog must contain exactly 138 source assets"));
        }
        if (catalog.expectedPanelCount != SourceAssetCatalog::ExpectedPanelCount ||
            panelCount != SourceAssetCatalog::ExpectedPanelCount)
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/archive/expectedPanelCount"),
                QStringLiteral("catalog must contain exactly 122 panel samples"));
        }
        if (catalog.expectedIconReferenceCount !=
                SourceAssetCatalog::ExpectedIconReferenceCount ||
            iconCount != SourceAssetCatalog::ExpectedIconReferenceCount)
        {
            add(QStringLiteral("invalid-value"),
                QStringLiteral("/archive/expectedIconReferenceCount"),
                QStringLiteral("catalog must contain exactly 16 icon references"));
        }
        if (!catalog.sourceOnly || catalog.installByDefault ||
            catalog.installEligibleStatus != QStringLiteral("production-approved"))
        {
            add(QStringLiteral("invalid-value"), QStringLiteral("/policy"),
                QStringLiteral("source catalog must remain separate and non-installing"));
        }

        const QList<QPair<QString, qsizetype>> expectedClasses{
            {QStringLiteral("A-desktop-product-reference"), 9},
            {QStringLiteral("B-strong-horizontal-2d-candidate"), 8},
            {QStringLiteral("C-legacy-dock-shelf-reference"), 27},
            {QStringLiteral("D-energy-glow-frame-candidate"), 28},
            {QStringLiteral("E-circular-launcher-reference"), 8},
            {QStringLiteral("F-ring-polygon-platform-candidate"), 30},
            {QStringLiteral("G-arc-freeform-platform-candidate"), 12},
            {QStringLiteral("H-icon-style-reference-sheet"), 16},
        };
        for (const auto &[assetClass, expectedCount] : expectedClasses)
        {
            const qsizetype actualCount = std::count_if(
                catalog.assets.cbegin(), catalog.assets.cend(),
                [&assetClass](const SourceAssetRecord &record)
                {
                    return record.assetClass == assetClass;
                });
            if (actualCount != expectedCount)
            {
                add(QStringLiteral("invalid-value"), QStringLiteral("/assets"),
                    QStringLiteral("reviewed class %1 must contain exactly %2 entries")
                        .arg(assetClass)
                        .arg(expectedCount));
            }
        }

        const auto requiredGroup = [](const QString &assetClass)
        {
            if (assetClass.startsWith(QLatin1Char('B')))
            {
                return QStringLiteral("strong-2d");
            }
            if (assetClass.startsWith(QLatin1Char('D')))
            {
                return QStringLiteral("energy");
            }
            if (assetClass.startsWith(QLatin1Char('E')) ||
                assetClass.startsWith(QLatin1Char('F')))
            {
                return QStringLiteral("ring-polygon");
            }
            if (assetClass.startsWith(QLatin1Char('G')))
            {
                return QStringLiteral("arc");
            }
            if (assetClass.startsWith(QLatin1Char('H')))
            {
                return QStringLiteral("icon-reference");
            }
            return QString{};
        };
        for (qsizetype index = 0; index < catalog.assets.size(); ++index)
        {
            const SourceAssetRecord &record = catalog.assets.at(index);
            const QString pointer = QStringLiteral("/assets/") +
                QString::number(index);
            const bool iconClass = record.assetClass.startsWith(QLatin1Char('H'));
            const bool recognizedClass = std::any_of(
                expectedClasses.cbegin(), expectedClasses.cend(),
                [&record](const auto &entry)
                {
                    return entry.first == record.assetClass;
                });
            if (!recognizedClass ||
                (record.sampleKind == QStringLiteral("icon-reference")) != iconClass)
            {
                add(QStringLiteral("invalid-value"), pointer +
                        QStringLiteral("/assetClass"),
                    QStringLiteral("reviewed class is incompatible with sample kind"));
            }
            const QString group = requiredGroup(record.assetClass);
            if (!group.isEmpty() && !record.visualGroups.contains(group))
            {
                add(QStringLiteral("missing-field"), pointer +
                        QStringLiteral("/visualGroups"),
                    QStringLiteral("reviewed class is missing its searchable group"));
            }
            if (record.reviewNotes.contains(QStringLiteral("Pending"),
                                            Qt::CaseInsensitive) ||
                !record.reviewNotes.contains(QStringLiteral("visually reviewed"),
                                             Qt::CaseInsensitive) ||
                record.statePairAvailability == QStringLiteral("unknown"))
            {
                add(QStringLiteral("invalid-value"), pointer,
                    QStringLiteral("source asset review is incomplete"));
            }
            if (!record.cleanupRequirements.contains(
                    QStringLiteral("manual-review")) ||
                !record.cleanupRequirements.contains(
                    QStringLiteral("isolate-clean-assets")) ||
                !record.cleanupRequirements.contains(
                    QStringLiteral("alpha-mask-review")))
            {
                add(QStringLiteral("invalid-value"), pointer +
                        QStringLiteral("/cleanupRequirements"),
                    QStringLiteral("opaque reference is missing mandatory cleanup blockers"));
            }
            if (record.status != QStringLiteral("reference-only") ||
                record.provenance.redistribution != QStringLiteral("unknown") ||
                !record.provenance.licenseSpdx.isEmpty() ||
                !record.opaqueScreenshot || record.isolatedCleanAsset ||
                record.isInstallEligible())
            {
                add(QStringLiteral("invalid-value"), pointer,
                    QStringLiteral("reviewed screenshot crossed the provenance or installation boundary"));
            }
        }
    }

    QVector<ThemeValidationDiagnostic> m_diagnostics;
};

}

namespace ArchDock
{

QVariantMap SourceAssetProvenance::toVariantMap() const
{
    return {
        {QStringLiteral("creator"), creator},
        {QStringLiteral("evidence"), evidence},
        {QStringLiteral("licenseSpdx"), licenseSpdx},
        {QStringLiteral("redistribution"), redistribution},
        {QStringLiteral("sourceKind"), sourceKind},
    };
}

bool SourceAssetRecord::isInstallEligible() const
{
    return status == QStringLiteral("production-approved") &&
        provenance.redistribution == QStringLiteral("allowed") &&
        isolatedCleanAsset && !opaqueScreenshot &&
        !cleanupRequirements.contains(QStringLiteral("manual-review")) &&
        !cleanupRequirements.contains(QStringLiteral("isolate-clean-assets")) &&
        !cleanupRequirements.contains(QStringLiteral("remove-placeholder")) &&
        !cleanupRequirements.contains(QStringLiteral("remove-logo"));
}

QVariantMap SourceAssetRecord::toVariantMap() const
{
    return {
        {QStringLiteral("archiveMember"), archiveMember},
        {QStringLiteral("assetClass"), assetClass},
        {QStringLiteral("byteSize"), byteSize},
        {QStringLiteral("cleanupRequirements"), cleanupRequirements},
        {QStringLiteral("fileName"), fileName},
        {QStringLiteral("id"), id},
        {QStringLiteral("intendedUse"), intendedUse},
        {QStringLiteral("isolatedCleanAsset"), isolatedCleanAsset},
        {QStringLiteral("opaqueScreenshot"), opaqueScreenshot},
        {QStringLiteral("orientations"), orientations},
        {QStringLiteral("provenance"), provenance.toVariantMap()},
        {QStringLiteral("reviewNotes"), reviewNotes},
        {QStringLiteral("sampleKind"), sampleKind},
        {QStringLiteral("sha256"), sha256},
        {QStringLiteral("statePairAvailability"), statePairAvailability},
        {QStringLiteral("status"), status},
        {QStringLiteral("targetRendererTier"), targetRendererTier},
        {QStringLiteral("visualGroups"), visualGroups},
    };
}

SourceAssetCatalogLoadResult SourceAssetCatalog::load(const QString &catalogPath)
{
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {{}, {catalogDiagnostic(QStringLiteral("missing-asset"), QString{},
                                       QStringLiteral("source catalog could not be read"))}};
    }
    return loadBytes(file.read(maximumCatalogBytes + 1), catalogPath);
}

SourceAssetCatalogLoadResult SourceAssetCatalog::loadBytes(
    const QByteArray &catalogBytes,
    const QString &)
{
    SourceAssetCatalogLoadResult result;
    if (catalogBytes.size() > maximumCatalogBytes)
    {
        result.diagnostics.append(catalogDiagnostic(
            QStringLiteral("limit-exceeded"), QString{},
            QStringLiteral("source catalog exceeds the byte limit")));
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(catalogBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.diagnostics.append(catalogDiagnostic(
            QStringLiteral("invalid-json"), QString{},
            QStringLiteral("source catalog is not a JSON object")));
        return result;
    }
    CatalogParser parser;
    SourceAssetCatalog catalog = parser.parse(document.object());
    result.diagnostics = parser.diagnostics();
    if (!containsErrors(result.diagnostics))
    {
        result.catalog = std::move(catalog);
    }
    return result;
}

QVector<ThemeValidationDiagnostic> SourceAssetCatalog::validateSourceRoot(
    const QString &sourceRoot) const
{
    QVector<ThemeValidationDiagnostic> diagnostics;
    const QFileInfo rootInfo(sourceRoot);
    const QString root = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || root.isEmpty())
    {
        diagnostics.append(catalogDiagnostic(
            QStringLiteral("unsafe-package-root"), QString{},
            QStringLiteral("source root must be an existing directory")));
        return diagnostics;
    }

    QSet<QString> expectedMembers;
    for (qsizetype index = 0; index < assets.size(); ++index)
    {
        const SourceAssetRecord &asset = assets.at(index);
        const QString pointer = QStringLiteral("/assets/") + QString::number(index) +
            QStringLiteral("/archiveMember");
        expectedMembers.insert(asset.archiveMember);
        const QFileInfo fileInfo(QDir(root).filePath(asset.archiveMember));
        if (!fileInfo.exists())
        {
            diagnostics.append(catalogDiagnostic(
                QStringLiteral("missing-asset"), pointer,
                QStringLiteral("cataloged source asset is missing")));
            continue;
        }
        const QString canonicalPath = fileInfo.canonicalFilePath();
        if (!fileInfo.isFile())
        {
            diagnostics.append(catalogDiagnostic(
                QStringLiteral("asset-not-regular"), pointer,
                QStringLiteral("cataloged source asset is not a regular file")));
            continue;
        }
        if (canonicalPath.isEmpty() || !isContainedPath(root, canonicalPath))
        {
            diagnostics.append(catalogDiagnostic(
                QStringLiteral("unsafe-path"), pointer,
                QStringLiteral("cataloged source asset resolves outside the source root")));
            continue;
        }
        if (fileInfo.size() != asset.byteSize)
        {
            diagnostics.append(catalogDiagnostic(
                QStringLiteral("invalid-value"), pointer,
                QStringLiteral("cataloged source asset size has changed")));
            continue;
        }
        QFile file(canonicalPath);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) ||
            QString::fromLatin1(hash.result().toHex()) != asset.sha256)
        {
            diagnostics.append(catalogDiagnostic(
                QStringLiteral("asset-hash-mismatch"), pointer,
                QStringLiteral("cataloged source asset hash has changed")));
        }
    }

    QSet<QString> observedMembers;
    const QString contentDirectory = QDir(root).filePath(archiveContentRoot);
    QDirIterator iterator(contentDirectory, {QStringLiteral("*.png")},
                          QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString path = QFileInfo(iterator.next()).canonicalFilePath();
        if (!path.isEmpty() && isContainedPath(root, path))
        {
            observedMembers.insert(QDir(root).relativeFilePath(path));
        }
    }
    if (observedMembers != expectedMembers)
    {
        diagnostics.append(catalogDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/assets"),
            QStringLiteral("source root PNG membership differs from the catalog")));
    }
    return diagnostics;
}

const SourceAssetRecord *SourceAssetCatalog::recordById(const QString &id) const
{
    const auto match = std::find_if(
        assets.cbegin(), assets.cend(), [&id](const SourceAssetRecord &record)
        {
            return record.id == id;
        });
    return match == assets.cend() ? nullptr : &*match;
}

QVector<SourceAssetRecord> SourceAssetCatalog::installEligibleRecords() const
{
    QVector<SourceAssetRecord> eligible;
    std::copy_if(assets.cbegin(), assets.cend(), std::back_inserter(eligible),
                 [](const SourceAssetRecord &record)
                 {
                     return record.isInstallEligible();
                 });
    return eligible;
}

QVariantMap SourceAssetCatalog::toVariantMap() const
{
    QVariantList serializedAssets;
    serializedAssets.reserve(assets.size());
    for (const SourceAssetRecord &asset : assets)
    {
        serializedAssets.append(asset.toVariantMap());
    }
    return {
        {QStringLiteral("archive"), QVariantMap{
             {QStringLiteral("contentRoot"), archiveContentRoot},
             {QStringLiteral("expectedAssetCount"), expectedAssetCount},
             {QStringLiteral("expectedIconReferenceCount"), expectedIconReferenceCount},
             {QStringLiteral("expectedPanelCount"), expectedPanelCount},
             {QStringLiteral("fileName"), archiveFileName},
             {QStringLiteral("sha256"), archiveSha256},
         }},
        {QStringLiteral("assets"), serializedAssets},
        {QStringLiteral("format"), format},
        {QStringLiteral("policy"), QVariantMap{
             {QStringLiteral("installByDefault"), installByDefault},
             {QStringLiteral("installEligibleStatus"), installEligibleStatus},
             {QStringLiteral("sourceOnly"), sourceOnly},
         }},
        {QStringLiteral("version"), version},
    };
}

bool SourceAssetCatalogLoadResult::isValid() const
{
    return catalog.has_value() && !containsErrors(diagnostics);
}

QString SourceAssetCatalogLoadResult::primaryCode() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().code;
}

QVariantMap SourceAssetCatalogLoadResult::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("diagnostics"), themeDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("valid"), isValid()},
    };
    if (catalog.has_value())
    {
        result.insert(QStringLiteral("catalog"), catalog->toVariantMap());
    }
    return result;
}

}
