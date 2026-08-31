#include "IconStyleDefinition.h"

#include <algorithm>

namespace
{

template<typename T>
QVariantList serialized(const QVector<T> &values)
{
    QVariantList result;
    result.reserve(values.size());
    for (const T &value : values)
    {
        result.append(value.toVariantMap());
    }
    return result;
}

QVariantMap serializedReplacements(const QMap<QString, QString> &replacements)
{
    QVariantMap result;
    for (auto it = replacements.cbegin(); it != replacements.cend(); ++it)
    {
        result.insert(it.key(), it.value());
    }
    return result;
}

}

namespace ArchDock
{

QVariantMap IconStyleValidationDiagnostic::toVariantMap() const
{
    return {
        {QStringLiteral("code"), code},
        {QStringLiteral("jsonPointer"), jsonPointer},
        {QStringLiteral("message"), message},
        {QStringLiteral("severity"), severity},
    };
}

QVariantList iconStyleDiagnosticsToVariantList(
    const QVector<IconStyleValidationDiagnostic> &diagnostics)
{
    return serialized(diagnostics);
}

QVariantMap IconStyleLicenseDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("evidence"), evidence},
        {QStringLiteral("redistribution"), redistribution},
        {QStringLiteral("spdx"), spdx},
    };
}

QVariantMap IconStyleGlyphPolicyDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("compatibleOnly"), compatibleOnly},
        {QStringLiteral("mode"), mode},
    };
    if (!tint.isEmpty())
    {
        result.insert(QStringLiteral("tint"), tint);
    }
    return result;
}

QVariantMap IconStyleSafeInset::toVariantMap() const
{
    return {
        {QStringLiteral("bottom"), bottom},
        {QStringLiteral("left"), left},
        {QStringLiteral("right"), right},
        {QStringLiteral("top"), top},
    };
}

QVariantMap IconStyleLayerDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("borderColor"), borderColor},
        {QStringLiteral("borderWidth"), borderWidth},
        {QStringLiteral("id"), id},
        {QStringLiteral("inset"), inset},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("opacity"), opacity},
    };
    if (kind == QStringLiteral("asset"))
    {
        result.insert(QStringLiteral("asset"), asset);
    }
    else
    {
        result.insert(QStringLiteral("color"), color);
        result.insert(QStringLiteral("radius"), radius);
        result.insert(QStringLiteral("shape"), shape);
        if (!secondaryColor.isEmpty())
        {
            result.insert(QStringLiteral("secondaryColor"), secondaryColor);
        }
    }
    return result;
}

QVariantMap IconStyleLayerSet::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("base"), serialized(base)},
        {QStringLiteral("front"), serialized(front)},
        {QStringLiteral("rear"), serialized(rear)},
    };
    if (glow.has_value())
    {
        result.insert(QStringLiteral("glow"), glow->toVariantMap());
    }
    if (mask.has_value())
    {
        result.insert(QStringLiteral("mask"), mask->toVariantMap());
    }
    if (reflection.has_value())
    {
        result.insert(QStringLiteral("reflection"), reflection->toVariantMap());
    }
    if (shadow.has_value())
    {
        result.insert(QStringLiteral("shadow"), shadow->toVariantMap());
    }
    return result;
}

QStringList IconStyleLayerSet::assetPaths() const
{
    QStringList result;
    const auto appendLayer = [&result](const IconStyleLayerDefinition &layer)
    {
        if (!layer.asset.isEmpty())
        {
            result.append(layer.asset);
        }
    };
    for (const IconStyleLayerDefinition &layer : rear)
    {
        appendLayer(layer);
    }
    for (const IconStyleLayerDefinition &layer : base)
    {
        appendLayer(layer);
    }
    for (const IconStyleLayerDefinition &layer : front)
    {
        appendLayer(layer);
    }
    for (const std::optional<IconStyleLayerDefinition> *layer :
         {&mask, &reflection, &shadow, &glow})
    {
        if (layer->has_value())
        {
            appendLayer(layer->value());
        }
    }
    return result;
}

QVariantMap IconStyleStateDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("baseOpacity"), baseOpacity},
        {QStringLiteral("borderColor"), borderColor},
        {QStringLiteral("frontOpacity"), frontOpacity},
        {QStringLiteral("glowColor"), glowColor},
        {QStringLiteral("glowOpacity"), glowOpacity},
        {QStringLiteral("glyphOpacity"), glyphOpacity},
        {QStringLiteral("glyphScale"), glyphScale},
        {QStringLiteral("id"), id},
        {QStringLiteral("indicatorColor"), indicatorColor},
        {QStringLiteral("indicatorOpacity"), indicatorOpacity},
        {QStringLiteral("rearOpacity"), rearOpacity},
        {QStringLiteral("reflectionOpacity"), reflectionOpacity},
    };
}

QVariantMap IconStyleCapabilityDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("animationCapabilities"), animationCapabilities},
        {QStringLiteral("features"), features},
        {QStringLiteral("rendererTiers"), rendererTiers},
        {QStringLiteral("supports3D"), supports3D},
    };
}

QVariantMap IconStylePreviewDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("iconName"), iconName},
        {QStringLiteral("seed"), seed},
        {QStringLiteral("state"), state},
        {QStringLiteral("tileSize"), tileSize},
    };
}

QVariantMap IconStyle3DReference::toVariantMap() const
{
    QVariantMap result{{QStringLiteral("fallbackStyleId"), fallbackStyleId}};
    if (!material.isEmpty())
    {
        result.insert(QStringLiteral("material"), material);
    }
    if (!mesh.isEmpty())
    {
        result.insert(QStringLiteral("mesh"), mesh);
    }
    return result;
}

QStringList IconStyle3DReference::assetPaths() const
{
    QStringList result;
    if (!mesh.isEmpty())
    {
        result.append(mesh);
    }
    if (!material.isEmpty())
    {
        result.append(material);
    }
    return result;
}

const IconStyleStateDefinition *IconStyleDefinition::stateById(
    const QString &stateId) const
{
    const auto match = std::find_if(
        states.cbegin(), states.cend(), [&stateId](const IconStyleStateDefinition &state)
        {
            return state.id == stateId;
        });
    return match == states.cend() ? nullptr : &*match;
}

QStringList IconStyleDefinition::assetPaths() const
{
    QStringList result = layers.assetPaths();
    for (auto it = mappedReplacements.cbegin(); it != mappedReplacements.cend(); ++it)
    {
        result.append(it.value());
    }
    if (threeD.has_value())
    {
        result.append(threeD->assetPaths());
    }
    result.removeDuplicates();
    return result;
}

QVariantMap IconStyleDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("author"), author},
        {QStringLiteral("capabilities"), capabilities.toVariantMap()},
        {QStringLiteral("description"), description},
        {QStringLiteral("format"), format},
        {QStringLiteral("glyphPolicy"), glyphPolicy.toVariantMap()},
        {QStringLiteral("id"), id},
        {QStringLiteral("layers"), layers.toVariantMap()},
        {QStringLiteral("license"), license.toVariantMap()},
        {QStringLiteral("mappedReplacements"), serializedReplacements(mappedReplacements)},
        {QStringLiteral("name"), name},
        {QStringLiteral("preview"), preview.toVariantMap()},
        {QStringLiteral("revision"), revision},
        {QStringLiteral("safeGlyphInset"), safeGlyphInset.toVariantMap()},
        {QStringLiteral("states"), serialized(states)},
        {QStringLiteral("version"), version},
    };
    if (!extensions.isEmpty())
    {
        result.insert(QStringLiteral("extensions"), extensions);
    }
    if (threeD.has_value())
    {
        result.insert(QStringLiteral("threeD"), threeD->toVariantMap());
    }
    return result;
}

QVariantMap IconStyleDefinition::toRuntimeProjection(
    const QString &validationStatus,
    const QVector<IconStyleValidationDiagnostic> &diagnostics) const
{
    QVariantMap result = toVariantMap();
    const bool valid = validationStatus == QStringLiteral("valid");
    result.insert(QStringLiteral("diagnostics"),
                  iconStyleDiagnosticsToVariantList(diagnostics));
    result.insert(QStringLiteral("loadable"), valid);
    result.insert(QStringLiteral("status"), validationStatus);
    result.insert(QStringLiteral("valid"), valid);
    result.insert(QStringLiteral("validationStatus"), validationStatus);
    return result;
}

}
