// The Panel Preset parser.
#include "PanelPresetDefinition.h"

#include "PanelCapabilityResolver.h"
#include "PanelSettingsSchema.h"

#include <QMetaType>

namespace
{

using namespace ArchDock;
using namespace ArchDock::PresetParsing;

struct ValueGroup
{
    QString name;
    QStringList keys;
};

// The specification's panel block, written in settings-schema keys so that a
// preset, the settings transaction and Panel Studio share one vocabulary.
// Nothing here is a host id, an ownership token, a screen or a content list.
const QVector<ValueGroup> &valueGroups()
{
    static const QVector<ValueGroup> groups{
        {QStringLiteral("host"), {QStringLiteral("edge")}},
        {QStringLiteral("content"), {QStringLiteral("type")}},
        {QStringLiteral("placement"),
         {QStringLiteral("alignment"), QStringLiteral("dynamic"),
          QStringLiteral("width"), QStringLiteral("height")}},
        {QStringLiteral("visibility"), {QStringLiteral("visibilityMode")}},
        {QStringLiteral("presentation"),
         {QStringLiteral("presentationMode"),
          QStringLiteral("presentationTrigger"),
          QStringLiteral("collapseMechanism"), QStringLiteral("collapseAxis"),
          QStringLiteral("revealHandle")}},
        {QStringLiteral("layout"),
         {QStringLiteral("layout"), QStringLiteral("layoutScale"),
          QStringLiteral("layoutAngle"), QStringLiteral("layoutRadius"),
          QStringLiteral("layoutRows"), QStringLiteral("layoutPadding"),
          QStringLiteral("pathSides"), QStringLiteral("pathOrientation"),
          QStringLiteral("panelRotationMode"),
          QStringLiteral("panelRotationSpeed"),
          QStringLiteral("panelRotationTrigger"),
          QStringLiteral("panelMotionTarget"), QStringLiteral("panelTravelSpeed"),
          QStringLiteral("scrollSensitivity"), QStringLiteral("iconShape"),
          QStringLiteral("iconSize"), QStringLiteral("spacing")}},
        {QStringLiteral("theme"),
         {QStringLiteral("completeThemeId"), QStringLiteral("rendererTier")}},
        {QStringLiteral("surface"),
         {QStringLiteral("appearance"), QStringLiteral("shape"),
          QStringLiteral("opacity"), QStringLiteral("color"),
          QStringLiteral("glowIntensity"), QStringLiteral("scene3DQuality")}},
        {QStringLiteral("motion"),
         {QStringLiteral("iconAnimation"), QStringLiteral("animationTrigger"),
          QStringLiteral("animationSpeed"), QStringLiteral("animationIntensity"),
          QStringLiteral("magnificationRadius"),
          QStringLiteral("magnificationFalloff")}},
    };
    return groups;
}

QSet<QString> capabilityNames()
{
    QSet<QString> names = rendererTierNames();
    for (int value = 0; value < static_cast<int>(PanelCapability::Count); ++value)
    {
        names.insert(panelCapabilityName(static_cast<PanelCapability>(value)));
    }
    for (int value = 0;
         value < static_cast<int>(PanelPresentationMechanism::Count); ++value)
    {
        names.insert(panelPresentationMechanismName(
            static_cast<PanelPresentationMechanism>(value)));
    }
    return names;
}

QSet<QString> layoutNames()
{
    const PanelSettingsFieldDescriptor *field =
        PanelSettingsSchema::panelDescriptor(QStringLiteral("layout"));
    return field ? QSet<QString>(field->choices.cbegin(), field->choices.cend())
                 : QSet<QString>{};
}

void inconsistent(QVector<PresetValidationDiagnostic> *diagnostics,
                  const QString &pointer,
                  const QString &message)
{
    addDiagnostic(diagnostics, QStringLiteral("inconsistent-declaration"), pointer,
                  message);
}

}

namespace ArchDock
{

QVariantMap PanelPresetPreview::toVariantMap() const
{
    return {
        {QStringLiteral("deterministicPreviewSeed"), deterministicPreviewSeed},
        {QStringLiteral("fallbackTier"), fallbackTier},
        {QStringLiteral("previewMode"), previewMode},
        {QStringLiteral("rendererTier"), rendererTier},
    };
}

QVariantMap PanelPresetCompatibility::toVariantMap() const
{
    return {
        {QStringLiteral("hostKinds"), hostKinds},
        {QStringLiteral("layouts"), layouts},
        {QStringLiteral("optionalCapabilities"), optionalCapabilities},
        {QStringLiteral("orientations"), orientations},
        {QStringLiteral("requiredCapabilities"), requiredCapabilities},
    };
}

QString PanelPresetPanel::themeId() const
{
    return configuration.surface.completeThemeId.trimmed();
}

QVariantMap PanelPresetFallback::toVariantMap() const
{
    return {
        {QStringLiteral("iconPresetId"), iconPresetId},
        {QStringLiteral("themeId"), themeId},
        {QStringLiteral("unsupportedFieldPolicy"), unsupportedFieldPolicy},
    };
}

QString PanelPresetDefinition::formatName()
{
    return QStringLiteral("org.archdock.panel-preset");
}

QStringList PanelPresetDefinition::panelValueGroups()
{
    QStringList names;
    for (const ValueGroup &group : valueGroups())
    {
        names.append(group.name);
    }
    return names;
}

QStringList PanelPresetDefinition::panelValueKeys(const QString &group)
{
    for (const ValueGroup &candidate : valueGroups())
    {
        if (candidate.name == group)
        {
            return candidate.keys;
        }
    }
    return {};
}

QStringList PanelPresetDefinition::panelValueKeys()
{
    QStringList keys;
    for (const ValueGroup &group : valueGroups())
    {
        keys.append(group.keys);
    }
    return keys;
}

QVariantMap PanelPresetDefinition::panelValues() const
{
    const QVariantMap record = panel.configuration.toLegacyMap();
    QVariantMap values;
    for (const QString &key : panelValueKeys())
    {
        if (record.contains(key))
        {
            values.insert(key, record.value(key));
        }
    }
    return values;
}

QVariantMap PanelPresetDefinition::toVariantMap() const
{
    const QVariantMap record = panel.configuration.toLegacyMap();
    QVariantMap panelObject;
    for (const ValueGroup &group : valueGroups())
    {
        QVariantMap object;
        for (const QString &key : group.keys)
        {
            if (record.contains(key))
            {
                object.insert(key, record.value(key));
            }
        }
        panelObject.insert(group.name, object);
    }
    if (!panel.recommendedIconPresetId.isEmpty())
    {
        panelObject.insert(QStringLiteral("recommendedIconPresetId"),
                           panel.recommendedIconPresetId);
    }
    return {
        {QStringLiteral("compatibility"), compatibility.toVariantMap()},
        {QStringLiteral("fallback"), fallback.toVariantMap()},
        {QStringLiteral("format"), format},
        {QStringLiteral("identity"), identity.toVariantMap()},
        {QStringLiteral("panel"), panelObject},
        {QStringLiteral("preview"), preview.toVariantMap()},
        {QStringLiteral("schemaVersion"), schemaVersion},
    };
}

std::optional<PanelPresetDefinition> PanelPresetDefinition::fromVariantMap(
    const QVariantMap &definition,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    QVector<PresetValidationDiagnostic> found;
    PanelPresetDefinition result;

    rejectUnknownKeys(definition, {
        QStringLiteral("format"), QStringLiteral("schemaVersion"),
        QStringLiteral("identity"), QStringLiteral("preview"),
        QStringLiteral("compatibility"), QStringLiteral("panel"),
        QStringLiteral("fallback"),
    }, QString{}, &found);
    result.identity = headerMember(
        definition, formatName(), CurrentSchemaVersion, &found);

    // The fallback block is read first: its policy decides how a panel field
    // this version does not support is treated.
    const QString fallbackPointer = QStringLiteral("/fallback");
    const QVariantMap fallback = objectMember(
        definition, QStringLiteral("fallback"), QString{}, true, &found);
    rejectUnknownKeys(fallback, {
        QStringLiteral("themeId"), QStringLiteral("iconPresetId"),
        QStringLiteral("unsupportedFieldPolicy"),
    }, fallbackPointer, &found);
    result.fallback.themeId = identifierMember(
        fallback, QStringLiteral("themeId"), fallbackPointer, false, &found);
    result.fallback.iconPresetId = identifierMember(
        fallback, QStringLiteral("iconPresetId"), fallbackPointer, false, &found);
    result.fallback.unsupportedFieldPolicy = stringMember(
        fallback, QStringLiteral("unsupportedFieldPolicy"), fallbackPointer, true,
        PresetIdentity::MaximumIdentifierBytes, &found);
    if (!result.fallback.unsupportedFieldPolicy.isEmpty() &&
        result.fallback.unsupportedFieldPolicy != QStringLiteral("reject") &&
        result.fallback.unsupportedFieldPolicy != QStringLiteral("drop"))
    {
        addDiagnostic(&found, QStringLiteral("invalid-enum"),
                      pointerChild(fallbackPointer,
                                   QStringLiteral("unsupportedFieldPolicy")),
                      QStringLiteral("unsupported-field policy must be reject or drop"));
    }
    const bool dropUnsupported =
        result.fallback.unsupportedFieldPolicy == QStringLiteral("drop");
    const auto unsupportedField = [&found, dropUnsupported](const QString &pointer)
    {
        if (dropUnsupported)
        {
            addDiagnostic(&found, QStringLiteral("unsupported-field"), pointer,
                          QStringLiteral("field is not supported and was dropped"),
                          QStringLiteral("warning"));
            return;
        }
        addDiagnostic(&found, QStringLiteral("unknown-field"), pointer,
                      QStringLiteral("field is not part of the panel preset schema"));
    };

    const QString previewPointer = QStringLiteral("/preview");
    const QVariantMap preview = objectMember(
        definition, QStringLiteral("preview"), QString{}, true, &found);
    rejectUnknownKeys(preview, {
        QStringLiteral("previewMode"), QStringLiteral("rendererTier"),
        QStringLiteral("fallbackTier"),
        QStringLiteral("deterministicPreviewSeed"),
    }, previewPointer, &found);
    static const QSet<QString> orientationNames{
        QStringLiteral("horizontal"), QStringLiteral("vertical"),
        QStringLiteral("free"),
    };
    const auto enumMember = [&found](const QVariantMap &object,
                                     const QString &key,
                                     const QString &pointer,
                                     const QSet<QString> &allowed)
    {
        const QString value = stringMember(
            object, key, pointer, true, PresetIdentity::MaximumIdentifierBytes,
            &found);
        if (!value.isEmpty() && !allowed.contains(value))
        {
            addDiagnostic(&found, QStringLiteral("invalid-enum"),
                          pointerChild(pointer, key),
                          QStringLiteral("value is not part of the allowed vocabulary"));
            return QString{};
        }
        return value;
    };
    result.preview.previewMode = enumMember(
        preview, QStringLiteral("previewMode"), previewPointer, orientationNames);
    result.preview.rendererTier = enumMember(
        preview, QStringLiteral("rendererTier"), previewPointer,
        rendererTierNames());
    result.preview.fallbackTier = enumMember(
        preview, QStringLiteral("fallbackTier"), previewPointer,
        rendererTierNames());
    result.preview.deterministicPreviewSeed = stringMember(
        preview, QStringLiteral("deterministicPreviewSeed"), previewPointer, true,
        PresetIdentity::MaximumIdentifierBytes, &found);

    const QString compatibilityPointer = QStringLiteral("/compatibility");
    const QVariantMap compatibility = objectMember(
        definition, QStringLiteral("compatibility"), QString{}, true, &found);
    rejectUnknownKeys(compatibility, {
        QStringLiteral("hostKinds"), QStringLiteral("orientations"),
        QStringLiteral("layouts"), QStringLiteral("requiredCapabilities"),
        QStringLiteral("optionalCapabilities"),
    }, compatibilityPointer, &found);
    result.compatibility.hostKinds = stringListMember(
        compatibility, QStringLiteral("hostKinds"), compatibilityPointer, true,
        false,
        {PanelDefinition::hostKindName(PanelHostKind::NativeEdge),
         PanelDefinition::hostKindName(PanelHostKind::FreeDesktop)},
        &found);
    result.compatibility.orientations = stringListMember(
        compatibility, QStringLiteral("orientations"), compatibilityPointer, true,
        false, orientationNames, &found);
    result.compatibility.layouts = stringListMember(
        compatibility, QStringLiteral("layouts"), compatibilityPointer, true, false,
        layoutNames(), &found);
    const QSet<QString> capabilities = capabilityNames();
    result.compatibility.requiredCapabilities = stringListMember(
        compatibility, QStringLiteral("requiredCapabilities"),
        compatibilityPointer, true, true, capabilities, &found);
    result.compatibility.optionalCapabilities = stringListMember(
        compatibility, QStringLiteral("optionalCapabilities"),
        compatibilityPointer, true, true, capabilities, &found);
    for (const QString &capability :
         std::as_const(result.compatibility.requiredCapabilities))
    {
        if (result.compatibility.optionalCapabilities.contains(capability))
        {
            addDiagnostic(&found, QStringLiteral("duplicate-value"),
                          pointerChild(compatibilityPointer,
                                       QStringLiteral("optionalCapabilities")),
                          QStringLiteral(
                              "a capability cannot be both required and optional"));
        }
    }

    const QString panelPointer = QStringLiteral("/panel");
    const QVariantMap panel = objectMember(
        definition, QStringLiteral("panel"), QString{}, true, &found);
    const QStringList groupNames = panelValueGroups();
    for (auto it = panel.cbegin(); it != panel.cend(); ++it)
    {
        if (it.key() != QStringLiteral("recommendedIconPresetId") &&
            !groupNames.contains(it.key()))
        {
            unsupportedField(pointerChild(panelPointer, it.key()));
        }
    }

    QVariantMap values;
    QSet<QString> declaredKeys;
    for (const ValueGroup &group : valueGroups())
    {
        const QString groupPointer = pointerChild(panelPointer, group.name);
        const QVariantMap object = objectMember(
            panel, group.name, panelPointer, false, &found);
        for (auto it = object.cbegin(); it != object.cend(); ++it)
        {
            const QString pointer = pointerChild(groupPointer, it.key());
            if (!group.keys.contains(it.key()))
            {
                unsupportedField(pointer);
                continue;
            }
            declaredKeys.insert(it.key());
            if (const auto value = strictPanelValue(
                    it.key(), it.value(), pointer, &found))
            {
                values.insert(it.key(), *value);
            }
        }
    }

    // A preset states its host, content type and layout itself; they are what
    // the preset is, not something to inherit from a default.
    const QVector<QPair<QString, QString>> requiredKeys{
        {QStringLiteral("host"), QStringLiteral("edge")},
        {QStringLiteral("content"), QStringLiteral("type")},
        {QStringLiteral("layout"), QStringLiteral("layout")},
    };
    for (const auto &[group, key] : requiredKeys)
    {
        if (!declaredKeys.contains(key))
        {
            addDiagnostic(&found, QStringLiteral("missing-field"),
                          pointerChild(pointerChild(panelPointer, group), key),
                          QStringLiteral("required panel field is missing"));
        }
    }

    const QString themePointer = pointerChild(panelPointer, QStringLiteral("theme"));
    const QString tier = values.value(QStringLiteral("rendererTier")).toString();
    if (!tier.isEmpty() && !rendererTierNames().contains(tier))
    {
        addDiagnostic(&found, QStringLiteral("invalid-enum"),
                      pointerChild(themePointer, QStringLiteral("rendererTier")),
                      QStringLiteral("unknown renderer tier"));
    }
    const QString themeId = values.value(QStringLiteral("completeThemeId")).toString();
    if (!themeId.isEmpty() && !PresetIdentity::isValidId(themeId))
    {
        addDiagnostic(&found, QStringLiteral("invalid-id"),
                      pointerChild(themePointer, QStringLiteral("completeThemeId")),
                      QStringLiteral("theme reference is malformed"));
    }
    const QString color = values.value(QStringLiteral("color")).toString();
    if (!color.isEmpty() &&
        (color == QStringLiteral("transparent") || !isValidColor(color)))
    {
        addDiagnostic(&found, QStringLiteral("invalid-color"),
                      pointerChild(pointerChild(panelPointer, QStringLiteral("surface")),
                                   QStringLiteral("color")),
                      QStringLiteral("panel color must be #RRGGBB or #AARRGGBB"));
    }

    result.panel.recommendedIconPresetId = identifierMember(
        panel, QStringLiteral("recommendedIconPresetId"), panelPointer, false,
        &found);

    // Resolve the declared values into a complete panel configuration through
    // the panel model itself: defaults for the host, then the preset's values.
    const QString edge = values.value(
        QStringLiteral("edge"), QStringLiteral("bottom")).toString();
    QVariantMap record = PanelDefinition::defaults(
        result.identity.id.isEmpty() ? QStringLiteral("preset") : result.identity.id,
        result.identity.name, edge, false).toLegacyMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        record.insert(it.key(), it.value());
    }
    if (values.contains(QStringLiteral("completeThemeId")))
    {
        record.insert(QStringLiteral("panelThemeId"),
                      values.value(QStringLiteral("completeThemeId")));
    }
    QString definitionError;
    const std::optional<PanelDefinition> configuration =
        PanelDefinition::fromLegacyMap(record, &definitionError);
    if (!configuration.has_value())
    {
        addDiagnostic(&found, QStringLiteral("invalid-panel-definition"),
                      panelPointer, definitionError);
    }
    else
    {
        result.panel.configuration = *configuration;

        const QString hostKind = PanelDefinition::hostKindName(
            configuration->host.kind);
        if (!result.compatibility.hostKinds.contains(hostKind))
        {
            inconsistent(&found,
                         pointerChild(compatibilityPointer,
                                      QStringLiteral("hostKinds")),
                         QStringLiteral("the preset's own host is not listed"));
        }
        if (!result.compatibility.layouts.contains(configuration->layout.pathType))
        {
            inconsistent(&found,
                         pointerChild(compatibilityPointer,
                                      QStringLiteral("layouts")),
                         QStringLiteral("the preset's own layout is not listed"));
        }
        const bool freeHost = configuration->host.kind == PanelHostKind::FreeDesktop;
        if (freeHost != (result.preview.previewMode == QStringLiteral("free")))
        {
            inconsistent(&found,
                         pointerChild(previewPointer, QStringLiteral("previewMode")),
                         QStringLiteral("preview mode does not match the host"));
        }
        if (!result.compatibility.orientations.contains(result.preview.previewMode))
        {
            inconsistent(&found,
                         pointerChild(compatibilityPointer,
                                      QStringLiteral("orientations")),
                         QStringLiteral("the preview orientation is not listed"));
        }
        if (!configuration->surface.rendererTier.isEmpty() &&
            configuration->surface.rendererTier != result.preview.rendererTier)
        {
            inconsistent(&found,
                         pointerChild(previewPointer, QStringLiteral("rendererTier")),
                         QStringLiteral(
                             "preview tier differs from the requested renderer tier"));
        }
    }

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
