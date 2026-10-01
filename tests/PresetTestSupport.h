#pragma once

#include "model/PresetIdentity.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <algorithm>

// Fixtures and helpers shared by the preset test programs. Both fixtures name
// resources that really ship (theme, icon style, layer id, motion profile), so
// the same definitions serve the parser, catalog and library tests.
namespace PresetTestSupport
{

inline bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return QDir().mkpath(QFileInfo(path).absolutePath()) &&
        file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

// A digest of every file name and byte below a directory. Two equal digests
// mean nothing was added, removed or rewritten.
inline QByteArray directoryDigest(const QString &directory)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QStringList pending{directory};
    while (!pending.isEmpty())
    {
        const QDir current(pending.takeFirst());
        const QFileInfoList entries = current.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries)
        {
            hash.addData(entry.absoluteFilePath().toUtf8());
            if (entry.isDir())
            {
                pending.append(entry.absoluteFilePath());
                continue;
            }
            hash.addData(readBytes(entry.absoluteFilePath()));
        }
    }
    return hash.result();
}

inline QVariantMap parseJson(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        qFatal("fixture is not a JSON object: %s", qPrintable(error.errorString()));
    }
    return document.object().toVariantMap();
}

inline QByteArray toJson(const QVariantMap &map)
{
    return QJsonDocument(QJsonObject::fromVariantMap(map)).toJson();
}

// Serializes to JSON text and back, so every value has the type a definition
// file on disk would give it.
inline QVariantMap throughJson(const QVariantMap &map)
{
    return parseJson(toJson(map));
}

inline QVariantMap panelPresetMap()
{
    return parseJson(R"json({
        "format": "org.archdock.panel-preset",
        "schemaVersion": 1,
        "identity": {
            "id": "fixture-chassis",
            "name": "Fixture Chassis",
            "description": "A collapsible chassis fixture.",
            "builtIn": true,
            "revision": 3
        },
        "preview": {
            "previewMode": "horizontal",
            "rendererTier": "skinned2d",
            "fallbackTier": "procedural2d",
            "deterministicPreviewSeed": "fixture-chassis-v1"
        },
        "compatibility": {
            "hostKinds": ["native-edge", "free-desktop"],
            "orientations": ["horizontal"],
            "layouts": ["horizontal"],
            "requiredCapabilities": ["split"],
            "optionalCapabilities": ["skinned2d"]
        },
        "panel": {
            "host": {"edge": "bottom"},
            "content": {"type": "hybrid"},
            "placement": {"alignment": "center", "dynamic": true,
                          "width": 860, "height": 88},
            "visibility": {"visibilityMode": "auto-hide"},
            "presentation": {"presentationMode": "collapsed",
                             "presentationTrigger": "hover",
                             "collapseMechanism": "split",
                             "collapseAxis": "horizontal",
                             "revealHandle": "bar"},
            "layout": {"layout": "horizontal", "layoutScale": 1.0,
                       "layoutPadding": 20, "iconShape": "rounded",
                       "iconSize": 48, "spacing": 9.5},
            "theme": {"completeThemeId": "sci-fi-chassis-dark",
                      "rendererTier": "skinned2d"},
            "surface": {"appearance": "glass", "shape": "pill", "opacity": 0.92,
                        "color": "#44ddea", "glowIntensity": 1.15},
            "motion": {"iconAnimation": "pulse", "animationTrigger": "hover",
                       "animationSpeed": 0.9, "animationIntensity": 1.1,
                       "magnificationRadius": 3.0,
                       "magnificationFalloff": "cosine"},
            "recommendedIconPresetId": "fixture-pedestal"
        },
        "fallback": {
            "themeId": "sci-fi-chassis-dark",
            "iconPresetId": "fixture-pedestal",
            "unsupportedFieldPolicy": "reject"
        }
    })json");
}

inline QVariantMap iconPresetMap()
{
    return parseJson(R"json({
        "format": "org.archdock.icon-preset",
        "schemaVersion": 1,
        "identity": {
            "id": "fixture-pedestal",
            "name": "Fixture Pedestal",
            "description": "A recoloured pedestal fixture.",
            "builtIn": true,
            "revision": 2
        },
        "compatibility": {
            "rendererTiers": ["procedural2d"],
            "requiredStyleCapabilities": ["tile", "glow"],
            "reducedMotionSupport": true
        },
        "icon": {
            "iconStyleId": "dark-orb",
            "visualOverrides": {
                "orb-pedestal": {"color": "#08101c", "borderColor": "#4a9dff",
                                 "opacity": 0.9}
            },
            "stateOverrides": {
                "normal": {"glowColor": "#4a9dff", "glowOpacity": 0.2}
            },
            "glyphPolicy": {"mode": "original", "compatibleOnly": true},
            "motion": {"profileId": "slow-y-turn", "animationTrigger": "idle",
                       "animationSpeed": 0.8, "animationIntensity": 1.2,
                       "magnificationRadius": 3.0,
                       "magnificationFalloff": "gaussian"},
            "perStateAnimationOverrides": {"urgent": "glow"}
        },
        "fallback": {"iconStyleId": "plain-original", "motionProfileId": "none"}
    })json");
}

// Replaces the value at a `/a/b/c` pointer. An invalid QVariant removes it.
inline QVariantMap withValue(const QVariantMap &source,
                             const QStringList &path,
                             const QVariant &value)
{
    QVariantMap result = source;
    if (path.size() == 1)
    {
        if (value.isValid())
        {
            result.insert(path.first(), value);
        }
        else
        {
            result.remove(path.first());
        }
        return result;
    }
    result.insert(path.first(),
                  withValue(result.value(path.first()).toMap(), path.mid(1), value));
    return result;
}

inline QVariantMap withValue(const QVariantMap &source,
                             const QString &pointer,
                             const QVariant &value)
{
    return withValue(source, pointer.split(QLatin1Char('/'), Qt::SkipEmptyParts),
                     value);
}

// True when a diagnostic with this code is reported at a pointer ending in
// `pointerSuffix`. Catalog diagnostics prefix the pointer with a file name.
inline bool hasDiagnostic(
    const QVector<ArchDock::PresetValidationDiagnostic> &diagnostics,
    const QString &code,
    const QString &pointerSuffix)
{
    return std::any_of(
        diagnostics.cbegin(), diagnostics.cend(),
        [&](const ArchDock::PresetValidationDiagnostic &entry)
        {
            return entry.code == code && entry.jsonPointer.endsWith(pointerSuffix);
        });
}

inline QString describe(
    const QVector<ArchDock::PresetValidationDiagnostic> &diagnostics)
{
    QStringList lines;
    for (const ArchDock::PresetValidationDiagnostic &entry : diagnostics)
    {
        lines.append(entry.severity + QLatin1Char(' ') + entry.code +
                     QLatin1Char(' ') + entry.jsonPointer + QStringLiteral(": ") +
                     entry.message);
    }
    return lines.join(QLatin1Char('\n'));
}

}
