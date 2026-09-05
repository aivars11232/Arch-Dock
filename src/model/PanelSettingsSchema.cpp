#include "PanelSettingsSchema.h"

#include <QMetaType>
#include <QUrl>
#include <QtGlobal>

#include <limits>

namespace
{

using Access = ArchDock::PanelSettingsFieldAccess;
using Descriptor = ArchDock::PanelSettingsFieldDescriptor;
using Editor = ArchDock::PanelSettingsEditorMetadata;
using Normalization = ArchDock::PanelSettingsNormalization;
using Scope = ArchDock::PanelSettingsFieldScope;
using Type = ArchDock::PanelSettingsValueType;

Editor editor(const char *section,
              const char *label,
              const char *control,
              std::initializer_list<const char *> consumers,
              const char *capability = "",
              std::initializer_list<const char *> layouts = {},
              const QVariantMap &properties = {})
{
    Editor result;
    result.section = QString::fromLatin1(section);
    result.label = QString::fromLatin1(label);
    result.control = QString::fromLatin1(control);
    for (const char *consumer : consumers)
    {
        result.consumers.append(QString::fromLatin1(consumer));
    }
    result.capability = QString::fromLatin1(capability);
    for (const char *layout : layouts)
    {
        result.layouts.append(QString::fromLatin1(layout));
    }
    result.properties = properties;
    return result;
}

Descriptor field(const char *key,
                 Scope scope,
                 Access access,
                 Type type,
                 Normalization normalization,
                 const QVariant &defaultValue,
                 const char *persistencePath,
                 const QVariant &minimumValue = {},
                 const QVariant &maximumValue = {},
                 std::initializer_list<const char *> choices = {},
                 bool optional = false,
                 bool runtimeConsumer = false,
                 const Editor &editorMetadata = {})
{
    Descriptor result;
    result.key = QString::fromLatin1(key);
    result.scope = scope;
    result.access = access;
    result.valueType = type;
    result.normalization = normalization;
    result.defaultValue = defaultValue;
    result.minimumValue = minimumValue;
    result.maximumValue = maximumValue;
    for (const char *choice : choices)
    {
        result.choices.append(QString::fromLatin1(choice));
    }
    result.persistencePath = QString::fromLatin1(persistencePath);
    result.optional = optional;
    result.runtimeConsumer = runtimeConsumer;
    result.editor = editorMetadata;
    return result;
}

Descriptor panel(const char *key,
                 Access access,
                 Type type,
                 Normalization normalization,
                 const QVariant &defaultValue,
                 const char *persistencePath,
                 const QVariant &minimumValue = {},
                 const QVariant &maximumValue = {},
                 std::initializer_list<const char *> choices = {},
                 bool optional = false,
                 bool runtimeConsumer = false,
                 const Editor &editorMetadata = {})
{
    return field(key,
                 Scope::Panel,
                 access,
                 type,
                 normalization,
                 defaultValue,
                 persistencePath,
                 minimumValue,
                 maximumValue,
                 choices,
                 optional,
                 runtimeConsumer,
                 editorMetadata);
}

Descriptor global(const char *key,
                  Access access,
                  Type type,
                  Normalization normalization,
                  const QVariant &defaultValue,
                  const QVariant &minimumValue = {},
                  const QVariant &maximumValue = {},
                  std::initializer_list<const char *> choices = {},
                  const Editor &editorMetadata = {})
{
    const QByteArray path = QByteArrayLiteral("dock/") + key;
    return field(key,
                 Scope::Global,
                 access,
                 type,
                 normalization,
                 defaultValue,
                 path.constData(),
                 minimumValue,
                 maximumValue,
                 choices,
                 false,
                 true,
                 editorMetadata);
}

Descriptor exposedTo(Descriptor descriptor,
                     std::initializer_list<const char *> interfaces)
{
    for (const char *interfaceName : interfaces)
    {
        descriptor.mutationInterfaces.append(QString::fromLatin1(interfaceName));
    }
    return descriptor;
}

QString scopeName(Scope scope)
{
    return scope == Scope::Panel ? QStringLiteral("panel") : QStringLiteral("global");
}

QString accessName(Access access)
{
    switch (access)
    {
    case Access::Editor:
        return QStringLiteral("editor");
    case Access::LegacyMutable:
        return QStringLiteral("legacy-mutable");
    case Access::Protected:
        return QStringLiteral("protected");
    case Access::Internal:
        return QStringLiteral("internal");
    case Access::Artifact:
        return QStringLiteral("artifact");
    }
    return QStringLiteral("internal");
}

QString typeName(Type type)
{
    switch (type)
    {
    case Type::Boolean:
        return QStringLiteral("boolean");
    case Type::Integer:
        return QStringLiteral("integer");
    case Type::Real:
        return QStringLiteral("real");
    case Type::String:
        return QStringLiteral("string");
    case Type::StringList:
        return QStringLiteral("string-list");
    case Type::Map:
        return QStringLiteral("map");
    case Type::Revision:
        return QStringLiteral("revision");
    }
    return QStringLiteral("string");
}

QStringList normalizedStringList(const QVariant &value, bool urls)
{
    QStringList candidates;
    if (value.metaType().id() == QMetaType::QStringList)
    {
        candidates = value.toStringList();
    }
    else
    {
        const QVariantList values = value.toList();
        candidates.reserve(values.size());
        for (const QVariant &candidate : values)
        {
            candidates.append(candidate.toString());
        }
    }

    QStringList result;
    for (const QString &candidate : candidates)
    {
        QString normalized = candidate.trimmed();
        if (urls)
        {
            const QUrl url(normalized);
            if (!url.isValid() || url.isEmpty())
            {
                continue;
            }
            normalized = url.toString();
        }
        if (!normalized.isEmpty() && !result.contains(normalized))
        {
            result.append(normalized);
        }
    }
    return result;
}

bool normalizeValue(const Descriptor &descriptor,
                    const QVariant &value,
                    bool rejectInvalidChoice,
                    int maximumScreenIndex,
                    QVariant *normalizedValue,
                    QString *errorMessage)
{
    QVariant normalized;
    switch (descriptor.normalization)
    {
    case Normalization::None:
        normalized = value;
        break;
    case Normalization::Boolean:
        normalized = value.toBool();
        break;
    case Normalization::TrimmedString:
        normalized = value.toString().trimmed();
        break;
    case Normalization::LowerString:
        normalized = value.toString().trimmed().toLower();
        break;
    case Normalization::ChoiceLower:
    case Normalization::ChoiceExact:
    {
        const QString candidate = descriptor.normalization == Normalization::ChoiceLower
            ? value.toString().trimmed().toLower()
            : value.toString().trimmed();
        if (!descriptor.choices.contains(candidate))
        {
            if (rejectInvalidChoice)
            {
                if (errorMessage)
                {
                    *errorMessage = QStringLiteral("invalid value for settings field: %1")
                        .arg(descriptor.key);
                }
                return false;
            }
            normalized = descriptor.defaultValue;
        }
        else
        {
            normalized = candidate;
        }
        break;
    }
    case Normalization::IntegerRange:
        normalized = qBound(descriptor.minimumValue.toInt(),
                            value.toInt(),
                            descriptor.maximumValue.toInt());
        break;
    case Normalization::RealRange:
        normalized = qBound(descriptor.minimumValue.toReal(),
                            value.toReal(),
                            descriptor.maximumValue.toReal());
        break;
    case Normalization::NonNegativeInteger:
        normalized = qMax(0, value.toInt());
        break;
    case Normalization::ScreenIndex:
        normalized = qBound(0, value.toInt(), qMax(0, maximumScreenIndex));
        break;
    case Normalization::HostId:
    {
        bool ok = false;
        const qlonglong candidate = value.toLongLong(&ok);
        normalized = ok && candidate >= 0 &&
                candidate <= std::numeric_limits<int>::max()
            ? static_cast<int>(candidate)
            : -1;
        break;
    }
    case Normalization::StringList:
        normalized = normalizedStringList(value, false);
        break;
    case Normalization::UrlList:
        normalized = normalizedStringList(value, true);
        break;
    case Normalization::Map:
        normalized = value.toMap();
        break;
    case Normalization::Revision:
    {
        bool ok = false;
        const qulonglong revision = value.toString().toULongLong(&ok);
        if (!ok)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("invalid revision for settings field: %1")
                    .arg(descriptor.key);
            }
            return false;
        }
        normalized = QVariant::fromValue<qulonglong>(revision);
        break;
    }
    }

    if (normalizedValue)
    {
        *normalizedValue = normalized;
    }
    return true;
}

const QVector<Descriptor> &schemaFields()
{
    static const QVector<Descriptor> result{
        panel("schemaVersion", Access::Protected, Type::Integer, Normalization::IntegerRange, 2,
              "schemaVersion", 2, 2),
        panel("settingsRevision", Access::Protected, Type::Revision, Normalization::Revision,
              QVariant::fromValue<qulonglong>(0), "settingsRevision"),
        panel("extensions", Access::Protected, Type::Map, Normalization::Map, QVariantMap{},
              "extensions", {}, {}, {}, true),
        panel("presetOrigin", Access::Protected, Type::Map, Normalization::Map, QVariantMap{},
              "presetOrigin", {}, {}, {}, true),
        panel("id", Access::Protected, Type::String, Normalization::TrimmedString, QString{},
              "identity.id"),
        panel("name", Access::Protected, Type::String, Normalization::None, QString{},
              "identity.name"),
        panel("builtIn", Access::Protected, Type::Boolean, Normalization::Boolean, false,
              "identity.builtIn"),
        panel("hostKind", Access::Protected, Type::String, Normalization::ChoiceLower,
              QStringLiteral("native-edge"), "host.kind", {}, {},
              {"native-edge", "free-desktop"}),
        exposedTo(panel("screen", Access::Editor, Type::Integer, Normalization::ScreenIndex, 0,
              "host.screenIndex", 0, {}, {}, false, true,
              editor("panels-general", "Display", "screen", {"studio"},
                     "screen-placement")), {"native-placement", "panel-screen"}),
        panel("screenId", Access::Protected, Type::String, Normalization::TrimmedString,
              QString{}, "host.screenId"),
        panel("nativePanelId", Access::Protected, Type::Integer, Normalization::HostId, -1,
              "host.nativePanelId"),
        panel("nativeControlAppletId", Access::Protected, Type::Integer,
              Normalization::HostId, -1, "host.nativeControlAppletId"),
        panel("nativeDockAppletId", Access::Protected, Type::Integer,
              Normalization::HostId, -1, "host.nativeDockAppletId"),
        panel("nativeOwnershipToken", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.nativeOwnershipToken"),
        panel("nativeRecoveryState", Access::Protected, Type::String,
              Normalization::LowerString, QStringLiteral("idle"),
              "host.nativeRecoveryState"),
        panel("nativeRecoveryError", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.nativeRecoveryError"),
        panel("freeDesktopContainmentId", Access::Protected, Type::Integer,
              Normalization::HostId, -1, "host.freeDesktopContainmentId"),
        panel("freeDockAppletId", Access::Protected, Type::Integer,
              Normalization::HostId, -1, "host.freeDockAppletId"),
        panel("freeOwnershipToken", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.freeOwnershipToken"),
        panel("freeHostMode", Access::Protected, Type::String, Normalization::LowerString,
              QStringLiteral("desktop"), "host.freeHostMode"),
        panel("freeHostState", Access::Protected, Type::String, Normalization::LowerString,
              QStringLiteral("unhosted"), "host.freeHostState"),
        panel("freeCreationState", Access::Protected, Type::String,
              Normalization::LowerString, QStringLiteral("idle"),
              "host.freeCreationState"),
        panel("freeCreationError", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.freeCreationError"),
        panel("freeRollbackError", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.freeRollbackError"),
        panel("freeRecoveryError", Access::Protected, Type::String,
              Normalization::TrimmedString, QString{}, "host.freeRecoveryError"),

        exposedTo(panel("type", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("hybrid"), "content.type", {}, {},
              {"empty", "launcher", "tasks", "hybrid"}, false, true,
              editor("panels-general", "Content", "combo", {"studio"},
                     "content-type")), {"native-type"}),
        panel("contentAppIds", Access::Internal, Type::StringList,
              Normalization::StringList, QStringList{}, "content.applicationIds"),
        panel("contentUrls", Access::Internal, Type::StringList, Normalization::UrlList,
              QStringList{}, "content.urls"),
        // The canonical order across application and URL entries. Internal:
        // it changes only through the panel content operations, never through
        // an editor field.
        panel("contentOrder", Access::Internal, Type::StringList,
              Normalization::StringList, QStringList{}, "content.entryOrder"),
        panel("kdeWidgets", Access::Internal, Type::StringList,
              Normalization::StringList, QStringList{}, "content.kdeWidgets"),
        exposedTo(panel("acceptDrops", Access::Editor, Type::Boolean, Normalization::Boolean, true,
              "content.acceptDrops", {}, {}, {}, false, true,
              editor("panels-behavior", "Accept drops", "switch",
                     {"studio", "native"}, "drop-input")), {"dock-configuration"}),
        panel("folderLayout", Access::Internal, Type::String,
              Normalization::ChoiceLower, QStringLiteral("fan"), "content.folderLayout",
              {}, {}, {"fan", "grid", "stack", "arc", "spiral", "circular",
                       "radial", "vertical", "horizontal", "elastic", "physics"}),
        panel("folderSpeed", Access::Internal, Type::Integer,
              Normalization::IntegerRange, 260, "content.folderSpeed", 80, 1200),
        panel("folderEasing", Access::Internal, Type::String,
              Normalization::ChoiceExact, QStringLiteral("outBack"),
              "content.folderEasing", {}, {},
              {"outCubic", "outBack", "outElastic", "spring"}),
        panel("folderExpandOnClick", Access::Internal, Type::Boolean,
              Normalization::Boolean, true, "content.folderExpandOnClick"),

        exposedTo(panel("edge", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("bottom"), "placement.edge", {}, {},
              {"top", "bottom", "left", "right", "free"}, false, true,
              editor("panels-general", "Edge", "combo", {"studio"},
                     "edge-placement")), {"native-placement"}),
        exposedTo(panel("alignment", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("center"), "placement.alignment", {}, {},
              {"start", "center", "end"}, false, true,
              editor("panels-general", "Alignment", "combo", {"studio"},
                     "alignment")), {"native-placement"}),
        exposedTo(panel("dynamic", Access::Editor, Type::Boolean, Normalization::Boolean, false,
              "placement.dynamic", {}, {}, {}, false, true,
              editor("panels-behavior", "Dynamic", "switch", {"studio"},
                     "dynamic-placement")), {"native-placement"}),
        exposedTo(panel("width", Access::Editor, Type::Integer, Normalization::IntegerRange, 720,
              "placement.width", 48, 4096, {}, false, true,
              editor("panels-size", "Width", "spin", {"studio"}, "length-mutation",
                     {}, {{QStringLiteral("step"), 2}})), {"native-placement"}),
        exposedTo(panel("height", Access::Editor, Type::Integer, Normalization::IntegerRange, 76,
              "placement.height", 48, 4096, {}, false, true,
              editor("panels-size", "Height", "spin", {"studio"},
                     "thickness-mutation", {}, {{QStringLiteral("step"), 2}})),
                  {"native-placement"}),
        panel("x", Access::LegacyMutable, Type::Integer,
              Normalization::NonNegativeInteger, 180, "placement.x", {}, {}, {}, false,
              true),
        panel("y", Access::LegacyMutable, Type::Integer,
              Normalization::NonNegativeInteger, 180, "placement.y", {}, {}, {}, false,
              true),
        panel("offset", Access::LegacyMutable, Type::Integer,
              Normalization::NonNegativeInteger, 0, "placement.offset", {}, {}, {}, true,
              true),
        exposedTo(panel("floatingMargin", Access::LegacyMutable, Type::Integer,
              Normalization::NonNegativeInteger, 0, "placement.floatingMargin", {}, {}, {},
              true, true), {"native-placement"}),
        panel("thickness", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 76, "placement.thickness", 48, 4096, {}, true,
              true),
        panel("lengthMode", Access::LegacyMutable, Type::String,
              Normalization::ChoiceLower, QStringLiteral("fixed"), "placement.lengthMode",
              {}, {}, {"fit", "fixed", "fill"}, true, true),
        panel("minimumLength", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 48, "placement.minimumLength", 48, 4096, {},
              true, true),
        panel("maximumLength", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 4096, "placement.maximumLength", 48, 4096, {},
              true, true),

        exposedTo(panel("visible", Access::Editor, Type::Boolean, Normalization::Boolean, false,
              "visibility.visible", {}, {}, {}, false, true,
              editor("panels-behavior", "Visible", "switch",
                     {"studio", "native"}, "visibility")), {"native-visibility"}),
        exposedTo(panel("visibilityMode", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("always"),
              "visibility.hostMode", {}, {}, {"always", "auto-hide", "dodge", "cover"},
              false, true,
              editor("panels-behavior", "Visibility mode", "combo",
                     {"studio", "native"}, "visibility-mode")),
                  {"native-visibility"}),
        panel("revealZone", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 10, "visibility.revealZone", 1, 64, {}, false,
              true),
        panel("openDelay", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 0, "visibility.openDelay", 0, 60000, {}, false,
              true),
        panel("closeDelay", Access::LegacyMutable, Type::Integer,
              Normalization::IntegerRange, 0, "visibility.closeDelay", 0, 60000, {}, false,
              true),
        panel("windowOverlapPolicy", Access::Internal, Type::String,
              Normalization::LowerString, QString{}, "visibility.windowOverlapPolicy", {}, {},
              {}, true),

        // Presentation. These were internal placeholders until the panel had a
        // motion engine to honour them; they are editor fields now that one
        // exists. The mechanism list is narrowed per panel by the resolver, so
        // a panel is never offered a mechanism its host and theme cannot run.
        panel("presentationMode", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("open"),
              "presentation.mode", {}, {}, {"open", "collapsed"}, true, true,
              editor("panels-behavior", "Resting state", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("presentationTrigger", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("hover"),
              "presentation.trigger", {}, {},
              {"hover", "click", "edge", "manual"}, true, true,
              editor("panels-behavior", "Opens on", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("collapseMechanism", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("open"),
              "presentation.collapseMechanism", {}, {},
              {"open", "collapse-horizontal", "collapse-vertical",
               "collapse-radial", "split", "shutter"}, true, true,
              editor("panels-behavior", "Collapse mechanism", "combo",
                     {"studio"}, "presentation-mechanism")),
        panel("collapseAxis", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("horizontal"),
              "presentation.collapseAxis", {}, {},
              {"horizontal", "vertical"}, true, true,
              editor("panels-behavior", "Collapse axis", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("revealHandle", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("edge-strip"),
              "presentation.revealHandle", {}, {},
              {"edge-strip", "bar", "handle", "none"}, true, true,
              editor("panels-behavior", "Reveal handle", "combo", {"studio"},
                     "presentation-mechanism")),

        panel("layout", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("adaptive"), "layout.pathType", {}, {},
              {"adaptive", "horizontal", "vertical", "diagonal", "circular", "ellipse",
               "ring", "radial", "arc", "semicircle", "fan", "spiral", "ribbon",
               "vertical-curve", "horizontal-curve", "polygon", "triangle", "square",
               "pentagon", "hexagon", "octagon", "star", "grid", "floating"},
              false, true,
              editor("panels-layout", "Dock layout", "combo", {"studio"}, "layout")),
        panel("layoutScale", Access::Editor, Type::Real, Normalization::RealRange, 1.0,
              "layout.scale", 0.5, 2.5, {}, false, true,
              editor("panels-layout", "Layout scale", "slider", {"studio"}, "layout",
                     {}, {{QStringLiteral("step"), 0.05},
                          {QStringLiteral("decimals"), 2},
                          {QStringLiteral("suffix"), QStringLiteral("x")}})),
        panel("layoutAngle", Access::Editor, Type::Real, Normalization::RealRange, 0.0,
              "layout.angle", -180.0, 180.0, {}, false, true,
              editor("panels-layout", "Layout angle", "spin", {"studio"},
                     "whole-panel-rotation")),
        panel("layoutRadius", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 150, "layout.radius", 48, 2048, {}, false, true,
              editor("panels-layout", "Radius", "spin", {"studio"}, "layout",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral"}, {{QStringLiteral("step"), 2}})),
        panel("layoutRows", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 2, "layout.rows", 1, 8, {}, false, true,
              editor("panels-layout", "Grid rows", "spin", {"studio"}, "layout",
                     {"grid"})),
        panel("layoutPadding", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 18, "layout.padding", 0, 240, {}, false, true,
              editor("panels-layout", "Panel padding", "spin", {"studio"}, "layout")),
        panel("pathSides", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 6, "layout.polygonSides", 3, 12, {}, false, true,
              editor("panels-layout", "Polygon sides", "spin", {"studio"}, "layout",
                     {"polygon", "triangle", "square", "pentagon", "hexagon",
                      "octagon", "star"})),
        panel("pathOrientation", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("upright"), "layout.orientation",
              {}, {}, {"upright", "tangent", "radial"}, false, true,
              editor("panels-layout", "Icon path orientation", "combo", {"studio"},
                     "layout", {"diagonal", "circular", "ellipse", "ring", "radial",
                                "arc", "semicircle", "fan", "spiral", "ribbon",
                                "vertical-curve", "horizontal-curve", "polygon", "triangle",
                                "square", "pentagon", "hexagon", "octagon", "star"})),
        panel("pathAnchor", Access::Internal, Type::String,
              Normalization::ChoiceLower, QStringLiteral("center"), "layout.anchor", {}, {},
              {"top-left", "top", "top-right", "left", "center", "right",
               "bottom-left", "bottom", "bottom-right"}),
        // Whole-scene rotation. Gated by the same capability as the static
        // layout angle, so a native panel never sees these controls, and
        // offered only for the radial layouts a turning scene makes sense for.
        panel("panelRotationMode", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("none"),
              "layout.rotationMode", {}, {},
              {"none", "clockwise", "counter-clockwise"}, false, true,
              editor("panels-layout", "Panel rotation", "combo", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star"})),
        panel("panelRotationSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              12.0, "layout.rotationSpeed", 1.0, 180.0, {}, false, true,
              editor("panels-layout", "Rotation speed", "spin", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star"},
                     {{QStringLiteral("step"), 1},
                      {QStringLiteral("suffix"), QStringLiteral("°/s")}})),
        panel("panelRotationTrigger", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("idle"),
              "layout.rotationTrigger", {}, {}, {"idle", "hover"}, false, true,
              editor("panels-layout", "Rotation runs", "combo", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star"})),

        panel("rendererTier", Access::Editor, Type::String, Normalization::LowerString,
              QString{}, "surface.rendererTier", {}, {}, {}, true, true),
        panel("panelThemeId", Access::Editor, Type::String,
              Normalization::TrimmedString, QString{}, "surface.panelThemeId", {}, {}, {}, true,
              true),
        panel("completeThemeId", Access::Editor, Type::String,
              Normalization::TrimmedString, QString{}, "surface.completeThemeId", {}, {}, {},
              true, true),
        exposedTo(panel("appearance", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("glass"), "surface.appearance", {}, {},
              {"glass", "crystal", "neon", "minimal", "plasma", "lime",
               "floating-glass", "metallic", "futuristic", "organic", "platform",
               "plate", "pedestal"}, false, true,
              editor("panels-appearance", "Theme", "combo", {"studio"},
                     "procedural-surface")), {"dock-configuration"}),
        exposedTo(panel("shape", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("pill"), "surface.shape", {}, {},
              {"pill", "rounded", "hexagon"}, false, true,
              editor("panels-appearance", "Shape", "combo", {"studio"},
                     "procedural-surface")), {"dock-configuration"}),
        exposedTo(panel("opacity", Access::Editor, Type::Real, Normalization::RealRange, 0.9,
              "surface.opacity", 0.0, 1.0, {}, false, true,
              editor("panels-appearance", "Opacity", "slider", {"studio"},
                     "procedural-surface", {}, {{QStringLiteral("step"), 0.05},
                                                {QStringLiteral("decimals"), 2}})),
                  {"dock-configuration"}),
        panel("color", Access::Editor, Type::String, Normalization::TrimmedString,
              QString{}, "surface.color", {}, {}, {}, false, true,
              editor("panels-appearance", "Color", "color", {"studio"},
                     "dynamic-tint")),
        panel("glowIntensity", Access::Editor, Type::Real,
              Normalization::RealRange, 1.0, "surface.glowIntensity",
              0.0, 2.0, {}, false, true,
              editor("panels-appearance", "Glow intensity", "slider",
                     {"studio"}, "dynamic-glow", {},
                     {{QStringLiteral("step"), 0.05},
                      {QStringLiteral("decimals"), 2}})),
        panel("border", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.border", {}, {}, {}, true),
        panel("glow", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.glow", {}, {}, {}, true),
        panel("shadow", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.shadow", {}, {}, {}, true),
        panel("blur", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.blur", {}, {}, {}, true),
        panel("surface2D", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.parameters2D", {}, {}, {}, true),
        panel("surface2_5D", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.parameters2_5D", {}, {}, {}, true),
        panel("surface3D", Access::Internal, Type::Map, Normalization::Map, QVariantMap{},
              "surface.parameters3D", {}, {}, {}, true),
        panel("themeAsset", Access::Artifact, Type::String, Normalization::TrimmedString,
              QString{}, "surface.themeAsset", {}, {}, {}, false, true),
        panel("themeSource", Access::Artifact, Type::String, Normalization::TrimmedString,
              QString{}, "surface.themeSource"),
        panel("themeFit", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("cover"), "surface.themeFit", {}, {},
              {"cover", "contain", "stretch", "tile"}, false, true,
              editor("panels-appearance", "Artwork fit", "combo", {"studio"},
                     "artwork-fit")),
        panel("themeSourceKind", Access::Artifact, Type::String,
              Normalization::LowerString, QString{}, "surface.themeSourceKind"),
        panel("themeSourceFormat", Access::Artifact, Type::String,
              Normalization::LowerString, QString{}, "surface.themeSourceFormat"),
        panel("themeSourceWidth", Access::Artifact, Type::Integer,
              Normalization::NonNegativeInteger, 0, "surface.themeSourceWidth"),
        panel("themeSourceHeight", Access::Artifact, Type::Integer,
              Normalization::NonNegativeInteger, 0, "surface.themeSourceHeight"),
        panel("themeSourceHasAlpha", Access::Artifact, Type::Boolean,
              Normalization::Boolean, false, "surface.themeSourceHasAlpha"),
        panel("themeSuggestedFit", Access::Artifact, Type::String,
              Normalization::ChoiceLower, QStringLiteral("cover"),
              "surface.themeSuggestedFit", {}, {}, {"cover", "contain", "stretch", "tile"}),
        panel("themePreview", Access::Artifact, Type::String,
              Normalization::TrimmedString, QString{}, "surface.themePreview"),
        panel("themeAnalysisStatus", Access::Artifact, Type::String,
              Normalization::None, QString{}, "surface.themeAnalysisStatus"),
        panel("themeConversionTool", Access::Artifact, Type::String,
              Normalization::TrimmedString, QString{}, "surface.themeConversionTool"),
        panel("themeConversionAvailable", Access::Artifact, Type::Boolean,
              Normalization::Boolean, false, "surface.themeConversionAvailable"),
        panel("themePackageFormat", Access::Artifact, Type::String,
              Normalization::TrimmedString, QString{}, "surface.themePackageFormat"),
        panel("themePackageVersion", Access::Artifact, Type::Integer,
              Normalization::NonNegativeInteger, 0, "surface.themePackageVersion"),
        panel("themePackageId", Access::Artifact, Type::String,
              Normalization::TrimmedString, QString{}, "surface.themePackageId"),
        panel("themePackageName", Access::Artifact, Type::String,
              Normalization::None, QString{}, "surface.themePackageName"),
        panel("themePackageAuthor", Access::Artifact, Type::String,
              Normalization::None, QString{}, "surface.themePackageAuthor"),
        panel("themePackageManifest", Access::Artifact, Type::String,
              Normalization::TrimmedString, QString{}, "surface.themePackageManifest"),
        panel("themeStatus", Access::Artifact, Type::String, Normalization::None,
              QString{}, "surface.themeStatus"),
        panel("themeRenderWidth", Access::Artifact, Type::Integer,
              Normalization::NonNegativeInteger, 0, "surface.themeRenderWidth"),
        panel("themeRenderHeight", Access::Artifact, Type::Integer,
              Normalization::NonNegativeInteger, 0, "surface.themeRenderHeight"),
        panel("themeRenderFit", Access::Artifact, Type::String,
              Normalization::LowerString, QString{}, "surface.themeRenderFit"),
        panel("themeRenderOutcome", Access::Artifact, Type::String,
              Normalization::LowerString, QString{}, "surface.themeRenderOutcome"),

        exposedTo(panel("iconStyle", Access::Editor, Type::String,
              Normalization::ChoiceExact, QStringLiteral("plain-original"),
              "iconStyle.styleReference", {}, {},
              {"plain-original", "metallic-blue", "metallic-red",
               "neon-green", "neon-orange", "dark-orb"}, false, true,
              editor("icons-appearance", "Icon style", "combo", {"studio", "native"},
                     "icon-state-styling")), {"dock-configuration"}),
        panel("iconThemeId", Access::Internal, Type::String,
              Normalization::TrimmedString, QStringLiteral("plain-original"),
              "iconStyle.themeId", {}, {}, {}, false, false),
        exposedTo(panel("iconShape", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("rounded"), "iconStyle.shape", {}, {},
              {"rounded", "square", "squircle", "circle", "hexagon"}, false, true,
              editor("icons-appearance", "Shape", "combo", {"studio"},
                     "icon-state-styling")), {"dock-configuration"}),
        exposedTo(panel("iconSize", Access::Editor, Type::Integer, Normalization::IntegerRange, 52,
              "iconStyle.size", 24, 128, {}, false, true,
              editor("icons-appearance", "Size", "spin", {"studio"},
                     "icon-state-styling", {}, {{QStringLiteral("step"), 2}})),
                  {"dock-configuration"}),
        exposedTo(panel("spacing", Access::Editor, Type::Real, Normalization::RealRange, 8.0,
              "iconStyle.spacing", 0.0, 48.0, {}, false, true,
              editor("icons-appearance", "Spacing", "slider", {"studio"},
                     "icon-state-styling", {}, {{QStringLiteral("step"), 1},
                                                {QStringLiteral("decimals"), 0}})),
                  {"dock-configuration"}),
        panel("iconGlobalDefaults", Access::Internal, Type::Map, Normalization::Map,
              QVariantMap{}, "iconStyle.globalDefaults", {}, {}, {}, true),
        panel("iconOverrides", Access::Internal, Type::Map, Normalization::Map,
              QVariantMap{}, "iconStyle.perEntryOverrides", {}, {}, {}, true),

        exposedTo(panel("iconAnimation", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("scale"), "motion.iconProfile", {}, {},
              {"none", "bounce", "elastic", "pulse", "scale", "spin", "idle-rotate",
               "orbit", "swing", "wobble", "wiggle", "shake", "glow", "breathe",
               "float", "wave", "ripple", "magnetic", "spring", "slow-y-turn",
               "jump", "shake-tangent", "enlarge", "spiral"}, false, true,
              editor("icons-behavior", "Animation", "combo", {"studio"},
                     "icon-state-styling")), {"dock-configuration"}),
        exposedTo(panel("animationTrigger", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("hover"), "motion.trigger", {}, {},
              {"hover", "click", "launch", "launch-succeeded", "launch-failed",
               "running", "drop", "reveal", "idle"}, false,
              true,
              editor("icons-behavior", "Trigger", "combo", {"studio"},
                     "icon-state-styling")), {"dock-configuration"}),
        exposedTo(panel("animationSpeed", Access::Editor, Type::Real,
              Normalization::RealRange, 1.0, "motion.speed", 0.2, 3.0, {}, false, true,
              editor("icons-behavior", "Animation speed", "slider", {"studio"},
                     "icon-state-styling", {}, {{QStringLiteral("step"), 0.1},
                                                {QStringLiteral("decimals"), 1}})),
                  {"dock-configuration"}),
        exposedTo(panel("animationIntensity", Access::Editor, Type::Real,
              Normalization::RealRange, 1.0, "motion.intensity", 0.1, 2.5, {}, false, true,
              editor("icons-behavior", "Motion intensity", "slider", {"studio"},
                     "icon-state-styling", {}, {{QStringLiteral("step"), 0.1},
                                                {QStringLiteral("decimals"), 1}})),
                  {"dock-configuration"}),
        exposedTo(panel("magnificationRadius", Access::Editor, Type::Real,
              Normalization::RealRange, 2.4, "motion.magnifyRadius", 0.5, 6.0, {},
              false, true,
              editor("icons-behavior", "Magnification reach", "slider",
                     {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 0.1},
                      {QStringLiteral("decimals"), 1}})),
                  {"dock-configuration"}),
        exposedTo(panel("magnificationFalloff", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("linear"),
              "motion.magnifyFalloff", {}, {},
              {"linear", "cosine", "gaussian"}, false, true,
              editor("icons-behavior", "Magnification falloff", "combo",
                     {"studio"}, "icon-state-styling")), {"dock-configuration"}),
        panel("physicsEnabled", Access::Internal, Type::Boolean,
              Normalization::Boolean, false, "motion.physicsEnabled"),
        panel("panelMotionProfile", Access::Internal, Type::String,
              Normalization::TrimmedString, QString{}, "motion.panelProfile", {}, {}, {}, true),
        panel("revealMotionProfile", Access::Internal, Type::String,
              Normalization::TrimmedString, QString{}, "motion.revealProfile", {}, {}, {}, true),
        panel("reducedMotion", Access::Internal, Type::Boolean,
              Normalization::Boolean, false, "motion.reducedMotion", {}, {}, {}, false, true),

        global("position", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("bottom"), {}, {},
               {"top", "bottom", "left", "right"}),
        global("alignment", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("center"), {}, {},
               {"start", "center", "end"}),
        global("appearancePreset", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("glass"), {}, {},
               {"glass", "crystal", "neon", "minimal", "plasma", "lime"}),
        global("monitorIndex", Access::LegacyMutable, Type::Integer,
               Normalization::ScreenIndex, 0),
        global("iconSize", Access::LegacyMutable, Type::Integer,
               Normalization::IntegerRange, 56, 32, 96),
        global("spacing", Access::LegacyMutable, Type::Real,
               Normalization::RealRange, 10.0, 0.0, 32.0),
        exposedTo(global("magnification", Access::Editor, Type::Real,
               Normalization::RealRange, 1.65, 1.0, 2.4, {},
               editor("icons-behavior", "Magnification size", "slider",
                      {"studio"}, "global-renderer",
                      {}, {{QStringLiteral("step"), 0.05},
                           {QStringLiteral("decimals"), 2}})), {"dock-configuration"}),
        exposedTo(global("magnificationEnabled", Access::Editor, Type::Boolean,
               Normalization::Boolean, true, {}, {}, {},
               editor("icons-behavior", "Magnification", "switch",
                      {"studio"}, "global-renderer")), {"dock-configuration"}),
        global("panelOpacity", Access::LegacyMutable, Type::Real,
               Normalization::RealRange, 0.88, 0.35, 1.0),
        exposedTo(global("showReflections", Access::Editor, Type::Boolean,
               Normalization::Boolean, true, {}, {}, {},
               editor("icons-appearance", "Reflection", "switch",
                      {"studio"}, "global-renderer")), {"dock-configuration"}),
        exposedTo(global("showIndicators", Access::Editor, Type::Boolean,
               Normalization::Boolean, true, {}, {}, {},
               editor("icons-indicators", "Indicators", "switch",
                      {"studio"}, "global-renderer")), {"dock-configuration"}),
        exposedTo(global("showTooltips", Access::Editor, Type::Boolean,
               Normalization::Boolean, true, {}, {}, {},
               editor("panels-behavior", "Show tooltips", "switch",
                      {"studio", "native"}, "global-renderer")),
                  {"dock-configuration"}),
        global("showStatusModule", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false),
        global("showDate", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false),
        global("showNetworkModule", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, true),
        global("showBatteryModule", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, true),
        global("showPerformanceModule", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false),
        exposedTo(global("animationDuration", Access::Editor, Type::Integer,
               Normalization::IntegerRange, 170, 80, 500, {},
               editor("icons-behavior", "Animation duration", "slider",
                      {"studio"}, "global-renderer",
                      {}, {{QStringLiteral("step"), 10}})), {"dock-configuration"}),
        exposedTo(global("reducedMotion", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false), {"dock-configuration"}),
        global("autoHide", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false),
        global("desktopSuite", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, false),
        global("topLauncherVisible", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, true),
        global("sideRailVisible", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, true),
        global("bottomPanelVisible", Access::LegacyMutable, Type::Boolean,
               Normalization::Boolean, true),
        global("panelShape", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("pill"), {}, {},
               {"pill", "rounded", "hexagon"}),
        global("iconTileShape", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("rounded"), {}, {},
               {"rounded", "circle", "hexagon"}),
        global("topPanelType", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("hybrid"), {}, {},
               {"launcher", "tasks", "hybrid"}),
        global("sidePanelType", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("hybrid"), {}, {},
               {"launcher", "tasks", "hybrid"}),
        global("bottomPanelType", Access::LegacyMutable, Type::String,
               Normalization::ChoiceLower, QStringLiteral("hybrid"), {}, {},
               {"launcher", "tasks", "hybrid"}),
    };
    return result;
}

}

namespace ArchDock
{

bool PanelSettingsEditorMetadata::isPresented() const
{
    return !section.isEmpty() && !control.isEmpty() && !consumers.isEmpty();
}

QVariantMap PanelSettingsFieldDescriptor::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("key"), key},
        {QStringLiteral("scope"), scopeName(scope)},
        {QStringLiteral("access"), accessName(access)},
        {QStringLiteral("type"), typeName(valueType)},
        {QStringLiteral("defaultValue"), defaultValue},
        {QStringLiteral("minimumValue"), minimumValue},
        {QStringLiteral("maximumValue"), maximumValue},
        {QStringLiteral("choices"), choices},
        {QStringLiteral("persistencePath"), persistencePath},
        {QStringLiteral("mutationInterfaces"), mutationInterfaces},
        {QStringLiteral("optional"), optional},
        {QStringLiteral("runtimeConsumer"), runtimeConsumer},
    };
    if (editor.isPresented())
    {
        result.insert(QStringLiteral("section"), editor.section);
        result.insert(QStringLiteral("label"), editor.label);
        result.insert(QStringLiteral("control"), editor.control);
        result.insert(QStringLiteral("consumers"), editor.consumers);
        result.insert(QStringLiteral("capability"), editor.capability);
        result.insert(QStringLiteral("layouts"), editor.layouts);
        for (auto it = editor.properties.cbegin(); it != editor.properties.cend(); ++it)
        {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

const QVector<PanelSettingsFieldDescriptor> &PanelSettingsSchema::fields()
{
    return schemaFields();
}

const PanelSettingsFieldDescriptor *PanelSettingsSchema::descriptor(
    PanelSettingsFieldScope scope,
    const QString &key)
{
    for (const PanelSettingsFieldDescriptor &candidate : fields())
    {
        if (candidate.scope == scope && candidate.key == key)
        {
            return &candidate;
        }
    }
    return nullptr;
}

const PanelSettingsFieldDescriptor *PanelSettingsSchema::panelDescriptor(
    const QString &key)
{
    return descriptor(PanelSettingsFieldScope::Panel, key);
}

const PanelSettingsFieldDescriptor *PanelSettingsSchema::globalDescriptor(
    const QString &key)
{
    return descriptor(PanelSettingsFieldScope::Global, key);
}

QStringList PanelSettingsSchema::keys(PanelSettingsFieldScope scope)
{
    QStringList result;
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope == scope)
        {
            result.append(field.key);
        }
    }
    return result;
}

QStringList PanelSettingsSchema::editorKeys(PanelSettingsFieldScope scope)
{
    QStringList result;
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope == scope && field.access == PanelSettingsFieldAccess::Editor)
        {
            result.append(field.key);
        }
    }
    return result;
}

bool PanelSettingsSchema::isKnownPanelField(const QString &key)
{
    return panelDescriptor(key) != nullptr;
}

bool PanelSettingsSchema::isKnownGlobalField(const QString &key)
{
    return globalDescriptor(key) != nullptr;
}

bool PanelSettingsSchema::isEditorField(PanelSettingsFieldScope scope,
                                        const QString &key)
{
    const PanelSettingsFieldDescriptor *field = descriptor(scope, key);
    return field && field->access == PanelSettingsFieldAccess::Editor;
}

bool PanelSettingsSchema::isTransactionPanelField(const QString &key)
{
    const PanelSettingsFieldDescriptor *field = panelDescriptor(key);
    return field && (field->access == PanelSettingsFieldAccess::Editor ||
                     field->access == PanelSettingsFieldAccess::LegacyMutable);
}

bool PanelSettingsSchema::isTransactionGlobalField(const QString &key)
{
    const PanelSettingsFieldDescriptor *field = globalDescriptor(key);
    return field && (field->access == PanelSettingsFieldAccess::Editor ||
                     field->access == PanelSettingsFieldAccess::LegacyMutable);
}

bool PanelSettingsSchema::supportsMutationInterface(
    PanelSettingsFieldScope scope,
    const QString &key,
    const QString &interfaceName)
{
    const PanelSettingsFieldDescriptor *field = descriptor(scope, key);
    return field && field->mutationInterfaces.contains(
        interfaceName.trimmed().toLower());
}

QVariant PanelSettingsSchema::normalizePanelValue(const QString &key,
                                                  const QVariant &value)
{
    const PanelSettingsFieldDescriptor *field = panelDescriptor(key);
    if (!field)
    {
        return value;
    }
    QVariant normalized;
    if (!normalizeValue(*field, value, false, std::numeric_limits<int>::max(),
                        &normalized, nullptr))
    {
        return field->defaultValue;
    }
    return normalized;
}

bool PanelSettingsSchema::normalizeGlobalValues(
    const QVariantMap &currentValues,
    const QVariantMap &submittedValues,
    int maximumScreenIndex,
    QVariantMap *candidate,
    QString *errorMessage)
{
    if (!candidate)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("the global settings candidate is missing");
        }
        return false;
    }

    QVariantMap normalized;
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope != PanelSettingsFieldScope::Global)
        {
            continue;
        }
        const QVariant source = currentValues.value(field.key, field.defaultValue);
        QVariant value;
        if (!normalizeValue(field, source, false, maximumScreenIndex, &value,
                            errorMessage))
        {
            return false;
        }
        normalized.insert(field.key, value);
    }

    for (auto it = submittedValues.cbegin(); it != submittedValues.cend(); ++it)
    {
        const PanelSettingsFieldDescriptor *field = globalDescriptor(it.key());
        if (!field)
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("unknown global settings field: %1")
                    .arg(it.key());
            }
            return false;
        }
        if (!isTransactionGlobalField(it.key()))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("protected global settings field: %1")
                    .arg(it.key());
            }
            return false;
        }
        QVariant value;
        if (!normalizeValue(*field, it.value(), true, maximumScreenIndex, &value,
                            errorMessage))
        {
            return false;
        }
        normalized.insert(it.key(), value);
    }

    *candidate = std::move(normalized);
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

QVariantMap PanelSettingsSchema::editorValues(PanelSettingsFieldScope scope,
                                              const QVariantMap &record)
{
    QVariantMap result;
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope != scope || field.access != PanelSettingsFieldAccess::Editor)
        {
            continue;
        }
        result.insert(field.key, record.value(field.key, field.defaultValue));
    }
    return result;
}

QVariantMap PanelSettingsSchema::runtimeValues(PanelSettingsFieldScope scope,
                                               const QVariantMap &record)
{
    QVariantMap result;
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope != scope || !field.runtimeConsumer)
        {
            continue;
        }
        result.insert(field.key, record.value(field.key, field.defaultValue));
    }
    return result;
}

QVariantList PanelSettingsSchema::editorDescriptors(
    PanelSettingsFieldScope scope,
    const QString &consumer)
{
    QVariantList result;
    const QString normalizedConsumer = consumer.trimmed().toLower();
    for (const PanelSettingsFieldDescriptor &field : fields())
    {
        if (field.scope != scope || field.access != PanelSettingsFieldAccess::Editor ||
            !field.editor.isPresented() ||
            !field.editor.consumers.contains(normalizedConsumer))
        {
            continue;
        }
        result.append(field.toVariantMap());
    }
    return result;
}

}
