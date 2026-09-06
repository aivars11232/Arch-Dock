#include "ThemeDefinition.h"

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

}

namespace ArchDock
{

QVariantMap ThemeValidationDiagnostic::toVariantMap() const
{
    return {
        {QStringLiteral("code"), code},
        {QStringLiteral("jsonPointer"), jsonPointer},
        {QStringLiteral("message"), message},
        {QStringLiteral("severity"), severity},
    };
}

QVariantList themeDiagnosticsToVariantList(
    const QVector<ThemeValidationDiagnostic> &diagnostics)
{
    return serialized(diagnostics);
}

QVariantMap ThemeSize::toVariantMap() const
{
    return {
        {QStringLiteral("height"), height},
        {QStringLiteral("width"), width},
    };
}

QVariantMap ThemeRect::toVariantMap() const
{
    return {
        {QStringLiteral("height"), height},
        {QStringLiteral("width"), width},
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
    };
}

QVariantMap ThemePoint::toVariantMap() const
{
    return {
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
    };
}

QVariantMap ThemeRotationDefinition::toVariantMap() const
{
    QVariantMap result{{QStringLiteral("mode"), mode}};
    if (mode == QStringLiteral("bounded"))
    {
        result.insert(QStringLiteral("maximumDegrees"), maximumDegrees);
        result.insert(QStringLiteral("minimumDegrees"), minimumDegrees);
    }
    return result;
}

QVariantMap ThemeCapabilityDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("fallbackRendererTiers"), fallbackRendererTiers},
        {QStringLiteral("features"), features},
        {QStringLiteral("hosts"), hosts},
        {QStringLiteral("layouts"), layouts},
        {QStringLiteral("orientations"), orientations},
        {QStringLiteral("preferredRendererTier"), preferredRendererTier},
        {QStringLiteral("presentationMechanisms"), presentationMechanisms},
        {QStringLiteral("rendererTiers"), rendererTiers},
        {QStringLiteral("rotation"), rotation.toVariantMap()},
    };
}

QVariantMap ThemeLicenseDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("evidence"), evidence},
        {QStringLiteral("redistribution"), redistribution},
        {QStringLiteral("spdx"), spdx},
    };
}

QVariantMap ThemeAssetDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("mimeType"), mimeType},
        {QStringLiteral("path"), path},
        {QStringLiteral("sha256"), sha256},
    };
    if (naturalSize.has_value())
    {
        result.insert(QStringLiteral("naturalSize"), naturalSize->toVariantMap());
    }
    return result;
}

QVariantMap ThemeStateDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("id"), id},
        {QStringLiteral("layers"), layers},
    };
    if (!inherits.isEmpty())
    {
        result.insert(QStringLiteral("inherits"), inherits);
    }
    return result;
}

QVariantMap ThemeLayerDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("asset"), asset},
        {QStringLiteral("blendMode"), blendMode},
        {QStringLiteral("id"), id},
        {QStringLiteral("opacity"), opacity},
        {QStringLiteral("role"), role},
    };
    if (sourceRect.has_value())
    {
        result.insert(QStringLiteral("sourceRect"), sourceRect->toVariantMap());
    }
    return result;
}

QVariantMap ThemeSliceDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("asset"), asset},
        {QStringLiteral("centerMode"), centerMode},
        {QStringLiteral("fixedEnd"), fixedEnd},
        {QStringLiteral("fixedStart"), fixedStart},
        {QStringLiteral("id"), id},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("sourceRect"), sourceRect.toVariantMap()},
        {QStringLiteral("state"), state},
    };
}

QVariantMap ThemeContentRegionDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("id"), id},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("state"), state},
    };
    if (rect.has_value())
    {
        result.insert(QStringLiteral("rect"), rect->toVariantMap());
    }
    if (!maskAsset.isEmpty())
    {
        result.insert(QStringLiteral("maskAsset"), maskAsset);
    }
    if (baseline.has_value())
    {
        result.insert(QStringLiteral("baseline"), *baseline);
    }
    return result;
}

QVariantMap ThemeTrackDepthDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("farScale"), farScale},
        {QStringLiteral("nearScale"), nearScale},
        {QStringLiteral("occlusionDepth"), occlusionDepth},
    };
}

QVariantMap ThemeTrackTiltDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("defaultDegrees"), defaultDegrees},
        {QStringLiteral("maximumDegrees"), maximumDegrees},
        {QStringLiteral("minimumDegrees"), minimumDegrees},
    };
}

QVariantMap ThemeTrackDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("center"), center.toVariantMap()},
        {QStringLiteral("depth"), depth.toVariantMap()},
        {QStringLiteral("id"), id},
        {QStringLiteral("radiusX"), radiusX},
        {QStringLiteral("radiusY"), radiusY},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("sides"), sides},
        {QStringLiteral("startDegrees"), startDegrees},
        {QStringLiteral("state"), state},
        {QStringLiteral("sweepDegrees"), sweepDegrees},
    };
    if (tilt.has_value())
    {
        result.insert(QStringLiteral("tilt"), tilt->toVariantMap());
    }
    return result;
}

QVariantMap ThemeEffectMargins::toVariantMap() const
{
    return {
        {QStringLiteral("bottom"), bottom},
        {QStringLiteral("left"), left},
        {QStringLiteral("right"), right},
        {QStringLiteral("top"), top},
    };
}

QVariantMap ThemeInputMaskDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("asset"), asset},
        {QStringLiteral("id"), id},
        {QStringLiteral("orientation"), orientation},
        {QStringLiteral("state"), state},
        {QStringLiteral("threshold"), threshold},
    };
}

QVariantMap ThemeResourceReference::toVariantMap() const
{
    QVariantMap result{{QStringLiteral("id"), id}};
    if (!manifest.isEmpty())
    {
        result.insert(QStringLiteral("manifest"), manifest);
    }
    if (!states.isEmpty())
    {
        result.insert(QStringLiteral("states"), states);
    }
    return result;
}

const ThemeAssetDefinition *ThemeDefinition::assetById(const QString &assetId) const
{
    const auto match = std::find_if(
        assets.cbegin(), assets.cend(), [&assetId](const ThemeAssetDefinition &asset)
        {
            return asset.id == assetId;
        });
    return match == assets.cend() ? nullptr : &*match;
}

QVariantMap ThemeDefinition::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("animationProfileRefs"), serialized(animationProfileRefs)},
        {QStringLiteral("assets"), serialized(assets)},
        {QStringLiteral("author"), author},
        {QStringLiteral("capabilities"), capabilities.toVariantMap()},
        {QStringLiteral("contentRegions"), serialized(contentRegions)},
        {QStringLiteral("description"), description},
        {QStringLiteral("effectMargins"), effectMargins.toVariantMap()},
        {QStringLiteral("format"), format},
        {QStringLiteral("id"), id},
        {QStringLiteral("inputMasks"), serialized(inputMasks)},
        {QStringLiteral("layers"), serialized(layers)},
        {QStringLiteral("license"), license.toVariantMap()},
        {QStringLiteral("name"), name},
        {QStringLiteral("packageRevision"), packageRevision},
        {QStringLiteral("slices"), serialized(slices)},
        {QStringLiteral("states"), serialized(states)},
        {QStringLiteral("tracks"), serialized(tracks)},
        {QStringLiteral("version"), version},
    };
    if (iconStyleRef.has_value())
    {
        result.insert(QStringLiteral("iconStyleRef"), iconStyleRef->toVariantMap());
    }
    return result;
}

QVariantMap ThemeDefinition::toRuntimeProjection(
    const QString &validationStatus,
    const QVector<ThemeValidationDiagnostic> &diagnostics) const
{
    QVariantMap result = toVariantMap();
    const bool valid = validationStatus == QStringLiteral("valid");
    result.insert(QStringLiteral("adaptedFromVersion1"), adaptedFromVersion1);
    result.insert(QStringLiteral("diagnostics"),
                  themeDiagnosticsToVariantList(diagnostics));
    result.insert(QStringLiteral("loadable"), valid);
    result.insert(QStringLiteral("sourceVersion"), sourceVersion);
    result.insert(QStringLiteral("status"), validationStatus);
    result.insert(QStringLiteral("valid"), valid);
    result.insert(QStringLiteral("validationStatus"), validationStatus);
    return result;
}

}
