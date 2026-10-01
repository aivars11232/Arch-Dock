#include "IconPresetDefinition.h"

#include <QMetaType>

namespace
{

using namespace ArchDock;
using namespace ArchDock::PresetParsing;

bool isTextOverrideField(const QString &field)
{
    return field == QStringLiteral("shape") || field.endsWith(QStringLiteral("Color")) ||
        field == QStringLiteral("color");
}

// Reads `{ "<entry id>": { "<field>": value } }`. Only the shape of the table
// is decided here; the icon-style validator judges the merged values.
QVariantMap overrideTable(const QVariantMap &object,
                          const QString &key,
                          const QString &pointer,
                          const QStringList &allowedFields,
                          const QStringList *allowedEntryIds,
                          QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QString tablePointer = pointerChild(pointer, key);
    const QVariantMap table = objectMember(object, key, pointer, false, diagnostics);
    if (table.size() > IconPresetDefinition::MaximumOverrideEntries)
    {
        addDiagnostic(diagnostics, QStringLiteral("limit-exceeded"), tablePointer,
                      QStringLiteral("too many override entries"));
        return {};
    }

    QVariantMap result;
    for (auto entry = table.cbegin(); entry != table.cend(); ++entry)
    {
        const QString entryPointer = pointerChild(tablePointer, entry.key());
        if (!PresetIdentity::isValidId(entry.key()) ||
            (allowedEntryIds && !allowedEntryIds->contains(entry.key())))
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-reference"),
                          entryPointer,
                          QStringLiteral("override names an unknown entry"));
            continue;
        }
        if (entry.value().metaType().id() != QMetaType::QVariantMap)
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-type"), entryPointer,
                          QStringLiteral("override entry must be an object"));
            continue;
        }
        const QVariantMap fields = entry.value().toMap();
        if (fields.isEmpty())
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-value"), entryPointer,
                          QStringLiteral("override entry replaces nothing"));
            continue;
        }
        QVariantMap normalized;
        for (auto field = fields.cbegin(); field != fields.cend(); ++field)
        {
            const QString fieldPointer = pointerChild(entryPointer, field.key());
            if (!allowedFields.contains(field.key()))
            {
                addDiagnostic(diagnostics, QStringLiteral("unknown-field"),
                              fieldPointer,
                              QStringLiteral("field cannot be overridden"));
                continue;
            }
            const QVariant value = field.value();
            if (isTextOverrideField(field.key()))
            {
                if (value.metaType().id() != QMetaType::QString)
                {
                    addDiagnostic(diagnostics, QStringLiteral("invalid-type"),
                                  fieldPointer,
                                  QStringLiteral("value must be a string"));
                    continue;
                }
                if (field.key() != QStringLiteral("shape") &&
                    !isValidColor(value.toString()))
                {
                    addDiagnostic(diagnostics, QStringLiteral("invalid-color"),
                                  fieldPointer,
                                  QStringLiteral(
                                      "color must be transparent, #RRGGBB, or #AARRGGBB"));
                    continue;
                }
                normalized.insert(field.key(), value.toString());
                continue;
            }
            if (!isNumber(value))
            {
                addDiagnostic(diagnostics, QStringLiteral("invalid-type"),
                              fieldPointer,
                              QStringLiteral("value must be a number"));
                continue;
            }
            normalized.insert(field.key(), value.toDouble());
        }
        result.insert(entry.key(), normalized);
    }
    return result;
}

IconStyleGlyphPolicyDefinition glyphPolicyMember(
    const QVariantMap &icon,
    const QString &pointer,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    IconStyleGlyphPolicyDefinition policy;
    const QString policyPointer = pointerChild(pointer, QStringLiteral("glyphPolicy"));
    const QVariantMap object = objectMember(
        icon, QStringLiteral("glyphPolicy"), pointer, true, diagnostics);
    rejectUnknownKeys(object, {
        QStringLiteral("mode"), QStringLiteral("tint"),
        QStringLiteral("compatibleOnly"),
    }, policyPointer, diagnostics);
    policy.mode = stringMember(object, QStringLiteral("mode"), policyPointer, true,
                               PresetIdentity::MaximumIdentifierBytes, diagnostics);
    static const QSet<QString> modes{
        QStringLiteral("original"), QStringLiteral("tinted"),
        QStringLiteral("monochrome"), QStringLiteral("mapped-replacement"),
    };
    if (!policy.mode.isEmpty() && !modes.contains(policy.mode))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-enum"),
                      pointerChild(policyPointer, QStringLiteral("mode")),
                      QStringLiteral("unsupported glyph policy"));
    }
    policy.tint = stringMember(object, QStringLiteral("tint"), policyPointer, false,
                               PresetIdentity::MaximumIdentifierBytes, diagnostics);
    if (!policy.tint.isEmpty() && !isValidColor(policy.tint))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-color"),
                      pointerChild(policyPointer, QStringLiteral("tint")),
                      QStringLiteral("color must be transparent, #RRGGBB, or #AARRGGBB"));
    }
    policy.compatibleOnly = booleanMember(
        object, QStringLiteral("compatibleOnly"), policyPointer, false, true,
        diagnostics);
    if ((policy.mode == QStringLiteral("tinted") ||
         policy.mode == QStringLiteral("monochrome")) && policy.tint.isEmpty())
    {
        addDiagnostic(diagnostics, QStringLiteral("missing-field"),
                      pointerChild(policyPointer, QStringLiteral("tint")),
                      QStringLiteral("selected glyph policy requires a tint"));
    }
    return policy;
}

IconPresetMotion motionMember(const QVariantMap &icon,
                              const QString &pointer,
                              QVector<PresetValidationDiagnostic> *diagnostics)
{
    IconPresetMotion motion;
    const QString motionPointer = pointerChild(pointer, QStringLiteral("motion"));
    const QVariantMap object = objectMember(
        icon, QStringLiteral("motion"), pointer, true, diagnostics);
    rejectUnknownKeys(object, {
        QStringLiteral("profileId"), QStringLiteral("animationTrigger"),
        QStringLiteral("animationSpeed"), QStringLiteral("animationIntensity"),
        QStringLiteral("magnificationRadius"),
        QStringLiteral("magnificationFalloff"),
    }, motionPointer, diagnostics);
    motion.profileId = identifierMember(
        object, QStringLiteral("profileId"), motionPointer, true, diagnostics);

    // The motion defaults are ordinary panel settings, so the settings schema
    // decides their ranges and choices.
    const auto setting = [&](const QString &key) -> std::optional<QVariant>
    {
        if (!object.contains(key))
        {
            return std::nullopt;
        }
        return strictPanelValue(key, object.value(key),
                                pointerChild(motionPointer, key), diagnostics);
    };
    if (const auto value = setting(QStringLiteral("animationTrigger")))
    {
        motion.trigger = value->toString();
    }
    if (const auto value = setting(QStringLiteral("animationSpeed")))
    {
        motion.speed = value->toDouble();
    }
    if (const auto value = setting(QStringLiteral("animationIntensity")))
    {
        motion.intensity = value->toDouble();
    }
    if (const auto value = setting(QStringLiteral("magnificationRadius")))
    {
        motion.magnificationRadius = value->toDouble();
    }
    if (const auto value = setting(QStringLiteral("magnificationFalloff")))
    {
        motion.magnificationFalloff = value->toString();
    }
    return motion;
}

}

namespace ArchDock
{

QVariantMap IconPresetCompatibility::toVariantMap() const
{
    return {
        {QStringLiteral("reducedMotionSupport"), reducedMotionSupport},
        {QStringLiteral("rendererTiers"), rendererTiers},
        {QStringLiteral("requiredStyleCapabilities"), requiredStyleCapabilities},
    };
}

QVariantMap IconPresetMotion::toVariantMap() const
{
    return {
        {QStringLiteral("animationIntensity"), intensity},
        {QStringLiteral("animationSpeed"), speed},
        {QStringLiteral("animationTrigger"), trigger},
        {QStringLiteral("magnificationFalloff"), magnificationFalloff},
        {QStringLiteral("magnificationRadius"), magnificationRadius},
        {QStringLiteral("profileId"), profileId},
    };
}

QVariantMap IconPresetIcon::toVariantMap() const
{
    QVariantMap animationOverrides;
    for (auto it = perStateAnimationOverrides.cbegin();
         it != perStateAnimationOverrides.cend(); ++it)
    {
        animationOverrides.insert(it.key(), it.value());
    }
    return {
        {QStringLiteral("glyphPolicy"), glyphPolicy.toVariantMap()},
        {QStringLiteral("iconStyleId"), iconStyleId},
        {QStringLiteral("motion"), motion.toVariantMap()},
        {QStringLiteral("perStateAnimationOverrides"), animationOverrides},
        {QStringLiteral("stateOverrides"), stateOverrides},
        {QStringLiteral("visualOverrides"), visualOverrides},
    };
}

QVariantMap IconPresetFallback::toVariantMap() const
{
    return {
        {QStringLiteral("iconStyleId"), iconStyleId},
        {QStringLiteral("motionProfileId"), motionProfileId},
    };
}

QString IconPresetDefinition::formatName()
{
    return QStringLiteral("org.archdock.icon-preset");
}

const QStringList &IconPresetDefinition::layerOverrideKeys()
{
    static const QStringList keys{
        QStringLiteral("shape"), QStringLiteral("color"),
        QStringLiteral("secondaryColor"), QStringLiteral("borderColor"),
        QStringLiteral("opacity"), QStringLiteral("inset"),
        QStringLiteral("radius"), QStringLiteral("borderWidth"),
    };
    return keys;
}

const QStringList &IconPresetDefinition::stateOverrideKeys()
{
    static const QStringList keys{
        QStringLiteral("rearOpacity"), QStringLiteral("baseOpacity"),
        QStringLiteral("frontOpacity"), QStringLiteral("glyphOpacity"),
        QStringLiteral("glyphScale"), QStringLiteral("borderColor"),
        QStringLiteral("glowColor"), QStringLiteral("glowOpacity"),
        QStringLiteral("reflectionOpacity"), QStringLiteral("indicatorColor"),
        QStringLiteral("indicatorOpacity"),
    };
    return keys;
}

const QStringList &IconPresetDefinition::stateIds()
{
    static const QStringList ids{
        QStringLiteral("normal"), QStringLiteral("hover"),
        QStringLiteral("pressed"), QStringLiteral("active"),
        QStringLiteral("running"), QStringLiteral("minimized"),
        QStringLiteral("urgent"), QStringLiteral("launching"),
        QStringLiteral("drop"), QStringLiteral("edit"),
        QStringLiteral("disabled"),
    };
    return ids;
}

const QStringList &IconPresetDefinition::panelValueKeys()
{
    static const QStringList keys{
        QStringLiteral("iconStyle"), QStringLiteral("iconAnimation"),
        QStringLiteral("animationTrigger"), QStringLiteral("animationSpeed"),
        QStringLiteral("animationIntensity"),
        QStringLiteral("magnificationRadius"),
        QStringLiteral("magnificationFalloff"),
    };
    return keys;
}

QVariantMap IconPresetDefinition::panelValues() const
{
    return {
        {QStringLiteral("iconStyle"), icon.iconStyleId},
        {QStringLiteral("iconAnimation"), icon.motion.profileId},
        {QStringLiteral("animationTrigger"), icon.motion.trigger},
        {QStringLiteral("animationSpeed"), icon.motion.speed},
        {QStringLiteral("animationIntensity"), icon.motion.intensity},
        {QStringLiteral("magnificationRadius"), icon.motion.magnificationRadius},
        {QStringLiteral("magnificationFalloff"), icon.motion.magnificationFalloff},
    };
}

QVariantMap IconPresetDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("compatibility"), compatibility.toVariantMap()},
        {QStringLiteral("fallback"), fallback.toVariantMap()},
        {QStringLiteral("format"), format},
        {QStringLiteral("icon"), icon.toVariantMap()},
        {QStringLiteral("identity"), identity.toVariantMap()},
        {QStringLiteral("schemaVersion"), schemaVersion},
    };
}

std::optional<IconPresetDefinition> IconPresetDefinition::fromVariantMap(
    const QVariantMap &definition,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    QVector<PresetValidationDiagnostic> found;
    IconPresetDefinition result;

    rejectUnknownKeys(definition, {
        QStringLiteral("format"), QStringLiteral("schemaVersion"),
        QStringLiteral("identity"), QStringLiteral("compatibility"),
        QStringLiteral("icon"), QStringLiteral("fallback"),
    }, QString{}, &found);
    result.identity = headerMember(
        definition, formatName(), CurrentSchemaVersion, &found);

    const QString compatibilityPointer = QStringLiteral("/compatibility");
    const QVariantMap compatibility = objectMember(
        definition, QStringLiteral("compatibility"), QString{}, true, &found);
    rejectUnknownKeys(compatibility, {
        QStringLiteral("rendererTiers"),
        QStringLiteral("requiredStyleCapabilities"),
        QStringLiteral("reducedMotionSupport"),
    }, compatibilityPointer, &found);
    result.compatibility.rendererTiers = stringListMember(
        compatibility, QStringLiteral("rendererTiers"), compatibilityPointer, true,
        false, rendererTierNames(), &found);
    // The referenced style decides which capabilities exist; the catalog
    // checks each name against it. Here they only have to be identifiers.
    result.compatibility.requiredStyleCapabilities = stringListMember(
        compatibility, QStringLiteral("requiredStyleCapabilities"),
        compatibilityPointer, true, true, {}, &found);
    for (const QString &capability :
         std::as_const(result.compatibility.requiredStyleCapabilities))
    {
        if (!PresetIdentity::isValidId(capability))
        {
            addDiagnostic(&found, QStringLiteral("invalid-id"),
                          pointerChild(compatibilityPointer,
                                       QStringLiteral("requiredStyleCapabilities")),
                          QStringLiteral("style capability name is malformed"));
        }
    }
    result.compatibility.reducedMotionSupport = booleanMember(
        compatibility, QStringLiteral("reducedMotionSupport"), compatibilityPointer,
        true, true, &found);

    const QString iconPointer = QStringLiteral("/icon");
    const QVariantMap icon = objectMember(
        definition, QStringLiteral("icon"), QString{}, true, &found);
    rejectUnknownKeys(icon, {
        QStringLiteral("iconStyleId"), QStringLiteral("visualOverrides"),
        QStringLiteral("stateOverrides"), QStringLiteral("glyphPolicy"),
        QStringLiteral("motion"), QStringLiteral("perStateAnimationOverrides"),
    }, iconPointer, &found);
    result.icon.iconStyleId = identifierMember(
        icon, QStringLiteral("iconStyleId"), iconPointer, true, &found);
    result.icon.visualOverrides = overrideTable(
        icon, QStringLiteral("visualOverrides"), iconPointer, layerOverrideKeys(),
        nullptr, &found);
    result.icon.stateOverrides = overrideTable(
        icon, QStringLiteral("stateOverrides"), iconPointer, stateOverrideKeys(),
        &stateIds(), &found);
    result.icon.glyphPolicy = glyphPolicyMember(icon, iconPointer, &found);
    result.icon.motion = motionMember(icon, iconPointer, &found);

    const QString animationPointer = pointerChild(
        iconPointer, QStringLiteral("perStateAnimationOverrides"));
    const QVariantMap animationOverrides = objectMember(
        icon, QStringLiteral("perStateAnimationOverrides"), iconPointer, false,
        &found);
    for (auto it = animationOverrides.cbegin(); it != animationOverrides.cend(); ++it)
    {
        const QString entryPointer = pointerChild(animationPointer, it.key());
        if (!stateIds().contains(it.key()))
        {
            addDiagnostic(&found, QStringLiteral("invalid-reference"), entryPointer,
                          QStringLiteral("override names an unknown icon state"));
            continue;
        }
        if (it.value().metaType().id() != QMetaType::QString ||
            !PresetIdentity::isValidId(it.value().toString()))
        {
            addDiagnostic(&found, QStringLiteral("invalid-id"), entryPointer,
                          QStringLiteral("motion profile id is malformed"));
            continue;
        }
        result.icon.perStateAnimationOverrides.insert(
            it.key(), it.value().toString());
    }

    const QString fallbackPointer = QStringLiteral("/fallback");
    const QVariantMap fallback = objectMember(
        definition, QStringLiteral("fallback"), QString{}, true, &found);
    rejectUnknownKeys(fallback, {
        QStringLiteral("iconStyleId"), QStringLiteral("motionProfileId"),
    }, fallbackPointer, &found);
    result.fallback.iconStyleId = identifierMember(
        fallback, QStringLiteral("iconStyleId"), fallbackPointer, true, &found);
    result.fallback.motionProfileId = identifierMember(
        fallback, QStringLiteral("motionProfileId"), fallbackPointer, true, &found);

    const bool valid = !presetDiagnosticsHaveErrors(found);
    if (diagnostics)
    {
        diagnostics->append(found);
    }
    if (!valid)
    {
        return std::nullopt;
    }
    return result;
}

}
