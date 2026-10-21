// The settings schema: every panel and global field's descriptor.
#include "PanelSettingsSchema.h"

#include <QHash>
#include <QMetaType>
#include <QRegularExpression>
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
    case Type::List:
        return QStringLiteral("list");
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
    case Normalization::HexColor:
    {
        static const QRegularExpression pattern(QStringLiteral("^#(?:[0-9a-f]{6}|[0-9a-f]{8})$"));
        const QString candidate = value.toString().trimmed().toLower();
        normalized = pattern.match(candidate).hasMatch() ? candidate : descriptor.defaultValue;
        break;
    }
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
    case Normalization::PictureUrl:
    {
        // Anything other than a local file, a web address among others, is
        // no picture.
        const QString candidate = value.toString().trimmed();
        const QUrl url(candidate);
        normalized = url.isLocalFile() && url.toLocalFile().startsWith(QLatin1Char('/'))
            ? candidate : QString{};
        break;
    }
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
        // What the panel is called in Panel Studio and its menus. Left as
        // Arch Dock names it, it follows the panel's shape or edge.
        panel("name", Access::Editor, Type::String, Normalization::TrimmedString, QString{},
              "identity.name", {}, {}, {}, true, false,
              editor("panels-general", "Name", "text", {"studio"}, "identity")),
        panel("builtIn", Access::Protected, Type::Boolean, Normalization::Boolean, false,
              "identity.builtIn"),
        panel("hostKind", Access::Protected, Type::String, Normalization::ChoiceLower,
              QStringLiteral("native-edge"), "host.kind", {}, {},
              {"native-edge", "free-desktop"}),
        panel("edgeHost", Access::Protected, Type::String, Normalization::ChoiceLower,
              QStringLiteral("plasma"), "host.edgeHost", {}, {}, {"plasma", "bar"}),
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
        panel("segments", Access::Editor, Type::List, Normalization::None,
              QVariantList{QVariantMap{{QStringLiteral("id"), QStringLiteral("main")}}},
              "segments", {}, {}, {}, false, true,
              editor("panels-segments", "Segments", "segments", {"studio"}, "segments")),
        panel("showBadges", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "content.showBadges", {}, {}, {}, false, true,
              editor("icons-notifications", "Application badges", "switch", {"studio"}, "application-overlays")),
        panel("showProgress", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "content.showProgress", {}, {}, {}, false, true,
              editor("icons-notifications", "Task progress", "switch", {"studio"}, "application-overlays")),
        panel("showTemporaryStatus", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "content.showTemporaryStatus", {}, {}, {}, false, true,
              editor("icons-notifications", "Temporary launch feedback", "switch", {"studio"},
                     "launch-feedback")),
        exposedTo(panel("acceptDrops", Access::Editor, Type::Boolean, Normalization::Boolean, true,
              "content.acceptDrops", {}, {}, {}, false, true,
              editor("panels-behavior", "Accept drops", "switch",
                     {"studio", "native"}, "drop-input")), {"dock-configuration"}),
        panel("folderLayout", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("fan"), "content.folderLayout",
              {}, {}, {"fan", "grid", "stack", "arc", "ring", "mobius", "track", "spiral", "circular",
                       "radial", "vertical", "horizontal", "elastic", "physics"}, false, true,
              editor("panels-behavior", "Folder layout", "combo", {"studio", "native"},
                     "folder-content")),
        // A free panel's folder shapes (ADREP-TASK-003, PD-11, PD-12, PD-14):
        // each is shown while its layout is chosen.
        panel("folderFanOpening", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 90, "content.folderFanOpening", 40, 160, {}, false, true,
              editor("panels-behavior", "Fan opening", "spin", {"studio"}, "folder-content", {},
                     {{QStringLiteral("step"), 5}, {QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("folderStackLength", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 3, "content.folderStackLength", 2, 12, {}, false, true,
              editor("panels-behavior", "Stack length", "spin", {"studio"}, "folder-content")),
        panel("folderRingSize", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("small"), "content.folderRingSize",
              {}, {}, {"small", "panel"}, false, true,
              editor("panels-behavior", "Ring size", "combo", {"studio"}, "folder-content")),
        panel("folderSpeed", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 260, "content.folderSpeed", 80, 1200, {}, false, true,
              editor("panels-behavior", "Folder animation duration (ms)", "spin", {"studio", "native"},
                     "folder-content")),
        panel("folderEasing", Access::Editor, Type::String,
              Normalization::ChoiceExact, QStringLiteral("outBack"),
              "content.folderEasing", {}, {},
              {"outCubic", "outBack", "outElastic", "spring"}, false, true,
              editor("panels-behavior", "Folder easing", "combo", {"studio", "native"},
                     "folder-content")),
        panel("folderExpandOnClick", Access::Editor, Type::Boolean,
              Normalization::Boolean, true, "content.folderExpandOnClick", {}, {}, {}, false, true,
              editor("panels-behavior", "Expand folders on click", "switch", {"studio", "native"},
                     "folder-content")),
        panel("folderShowNames", Access::Editor, Type::Boolean,
              Normalization::Boolean, true, "content.folderShowNames", {}, {}, {}, false, true,
              editor("panels-behavior", "Always show folder item names", "switch", {"studio", "native"},
                     "folder-content")),

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
        panel("x", Access::Editor, Type::Integer,
              Normalization::NonNegativeInteger, 180, "placement.x", 0,
              1000000, {}, false, true,
              editor("panels-general", "Horizontal position", "spin", {"studio"},
                     "arbitrary-xy-placement", {}, {{QStringLiteral("suffix"), QStringLiteral(" px")}})),
        panel("y", Access::Editor, Type::Integer,
              Normalization::NonNegativeInteger, 180, "placement.y", 0,
              1000000, {}, false, true,
              editor("panels-general", "Vertical position", "spin", {"studio"},
                     "arbitrary-xy-placement", {}, {{QStringLiteral("suffix"), QStringLiteral(" px")}})),
        // How far a bar of Arch Dock's own stands from where its alignment
        // puts it: from its corner for Start and End, from the middle for
        // Center (towards the end, or the start below zero), as Plasma moves
        // a panel by its offset.
        exposedTo(panel("offset", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 0, "placement.offset", -4096, 4096, {}, true,
              true, editor("panels-general", "Offset", "spin", {"studio"}, "alignment",
                           {}, {{QStringLiteral("suffix"), QStringLiteral(" px")}})),
                  {"native-placement"}),
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
        panel("openDelay", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 0, "visibility.openDelay", 0, 60000, {}, false,
              true, editor("panels-animations", "Opening delay", "spin", {"studio"},
                           "presentation-mechanism", {}, {{QStringLiteral("suffix"), QStringLiteral(" ms")}})),
        panel("closeDelay", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 0, "visibility.closeDelay", 0, 60000, {}, false,
              true, editor("panels-animations", "Closing delay", "spin", {"studio"},
                           "presentation-mechanism", {}, {{QStringLiteral("suffix"), QStringLiteral(" ms")}})),
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
              editor("panels-animations", "Resting state", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("presentationTrigger", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("hover"),
              "presentation.trigger", {}, {},
              {"hover", "click", "edge", "manual"}, true, true,
              editor("panels-animations", "Opens on", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("collapseMechanism", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("open"),
              "presentation.collapseMechanism", {}, {},
              {"open", "collapse-horizontal", "collapse-vertical",
               "collapse-radial", "split", "shutter"}, true, true,
              editor("panels-animations", "Collapse mechanism", "combo",
                     {"studio"}, "presentation-mechanism")),
        panel("collapseAxis", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("horizontal"),
              "presentation.collapseAxis", {}, {},
              {"horizontal", "vertical"}, true, true,
              editor("panels-animations", "Collapse axis", "combo", {"studio"},
                     "presentation-mechanism")),
        panel("revealHandle", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("edge-strip"),
              "presentation.revealHandle", {}, {},
              {"edge-strip", "bar", "handle", "none"}, true, true,
              editor("panels-animations", "Reveal handle", "combo", {"studio"},
                     "presentation-mechanism")),

        panel("layout", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("adaptive"), "layout.pathType", {}, {},
              {"adaptive", "horizontal", "vertical", "diagonal", "circular", "ellipse",
               "ring", "radial", "arc", "semicircle", "fan", "spiral", "ribbon",
               "vertical-curve", "horizontal-curve", "polygon", "triangle", "square",
               "pentagon", "hexagon", "octagon", "star", "grid", "floating", "mobius"},
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
              // The polygon family is sized from this radius exactly as the
              // round layouts are - LayoutEngine places a polygon's entries on
              // a circle of it - so omitting them left an octagon panel with
              // no way to be sized at all.
              editor("panels-layout", "Radius", "spin", {"studio"}, "layout",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square",
                      "pentagon", "hexagon", "octagon", "star", "mobius"},
                     {{QStringLiteral("step"), 2}})),
        panel("layoutRows", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 2, "layout.rows", 1, 8, {}, false, true,
              editor("panels-layout", "Grid rows", "spin", {"studio"}, "layout",
                     {"grid"})),
        panel("layoutPadding", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 18, "layout.padding", 0, 240, {}, false, true,
              editor("panels-layout", "Panel padding", "spin", {"studio"}, "layout")),
        // Only the free polygon and the star read their number of sides; a
        // triangle, square, pentagon, hexagon or octagon is named by its sides
        // (LayoutEngine.shapeSides), so the control would do nothing there.
        panel("pathSides", Access::Editor, Type::Integer,
              Normalization::IntegerRange, 6, "layout.polygonSides", 3, 12, {}, false, true,
              editor("panels-layout", "Polygon sides", "spin", {"studio"}, "layout",
                     {"polygon", "star"})),
        panel("pathOrientation", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("upright"), "layout.orientation",
              {}, {}, {"upright", "tangent", "radial"}, false, true,
              editor("panels-layout", "Icon path orientation", "combo", {"studio"},
                     "layout", {"diagonal", "circular", "ellipse", "ring", "radial",
                                "arc", "semicircle", "fan", "spiral", "ribbon",
                                "vertical-curve", "horizontal-curve", "polygon", "triangle",
                                "square", "pentagon", "hexagon", "octagon", "star",
                                "mobius"})),
        panel("pathAnchor", Access::Internal, Type::String,
              Normalization::ChoiceLower, QStringLiteral("center"), "layout.anchor", {}, {},
              {"top-left", "top", "top-right", "left", "center", "right",
               "bottom-left", "bottom", "bottom-right"}),
        // Free-panel motion. Gated by the same capability as the static
        // layout angle, so a native panel never sees these controls, and
        // offered only for the radial layouts whose entries stand on a path.
        // Motion lives on the Animations page only (ADREP-TASK-001, PD-08).
        // Continuous motion runs clockwise or counter-clockwise; what it, the
        // wheel and a drag move is chosen apart from it (ADREP-TASK-002,
        // PD-25), each with its own speed.
        panel("panelRotationMode", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("none"),
              "layout.rotationMode", {}, {},
              {"none", "clockwise", "counter-clockwise"}, false, true,
              editor("panels-animations", "Continuous motion", "combo", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"})),
        panel("panelRotationSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              12.0, "layout.rotationSpeed", 1.0, 180.0, {}, false, true,
              editor("panels-animations", "Panel rotation speed", "spin", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"},
                     {{QStringLiteral("step"), 1},
                      {QStringLiteral("suffix"), QStringLiteral("°/s")}})),
        panel("panelRotationTrigger", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("idle"),
              "layout.rotationTrigger", {}, {}, {"idle", "hover"}, false, true,
              editor("panels-animations", "Motion runs", "combo", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"})),
        panel("panelMotionTarget", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("items"),
              "layout.motionTarget", {}, {}, {"items", "panel", "both"}, false, true,
              editor("panels-animations", "Continuous motion moves", "combo", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"})),
        panel("panelTravelSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              0.5, "layout.travelSpeed", 0.05, 5.0, {}, false, true,
              editor("panels-animations", "Item travel speed", "slider", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"},
                     {{QStringLiteral("step"), 0.05},
                      {QStringLiteral("decimals"), 2},
                      {QStringLiteral("suffix"), QStringLiteral(" icons/s")}})),
        panel("scrollSensitivity", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "layout.scrollSensitivity", 0.25, 4.0, {}, false, true,
              editor("panels-animations", "Scroll sensitivity", "slider", {"studio"},
                     "whole-panel-rotation",
                     {"circular", "ellipse", "ring", "radial", "arc", "semicircle",
                      "fan", "spiral", "polygon", "triangle", "square", "pentagon",
                      "hexagon", "octagon", "star", "mobius"},
                     {{QStringLiteral("step"), 0.25},
                      {QStringLiteral("decimals"), 2},
                      {QStringLiteral("suffix"), QStringLiteral("x")}})),

        panel("rendererTier", Access::Editor, Type::String, Normalization::LowerString,
              QString{}, "surface.rendererTier", {}, {}, {}, true, true),
        // Every 3D setting lives on the Panels > 3D page. Pitch and yaw keep
        // their saved meaning (the camera's view of the platform); roll,
        // position and scale are the scene's own transform. Position is a
        // fraction of the room the panel has around the platform, so a value
        // stays valid whatever size the panel is drawn at. Pitch, yaw and
        // roll turn all the way round, and the scale has no practical limit,
        // all ways at once or sideways and up and down on its own (the owner,
        // 2026-10-13: "freely in 360 every direction", "for scaling there
        // shouldn't be a limit", "scale it also sideways or top or down").
        panel("scene3DQuality", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("high"), "surface.parameters3D.quality", {}, {},
              {"low", "medium", "high"}, true, true,
              editor("panels-3d", "Render quality", "combo", {"studio"}, "scene3d-quality")),
        panel("scene3DCameraPitch", Access::Editor, Type::Real, Normalization::RealRange,
              25.0, "surface.parameters3D.cameraPitch", -180.0, 180.0, {}, true, true,
              editor("panels-3d", "Pitch (tilt)", "spin", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("scene3DCameraYaw", Access::Editor, Type::Real, Normalization::RealRange,
              10.0, "surface.parameters3D.cameraYaw", -180.0, 180.0, {}, true, true,
              editor("panels-3d", "Yaw (orientation)", "spin", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("scene3DRoll", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.roll", -180.0, 180.0, {}, true, true,
              editor("panels-3d", "Roll", "spin", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("scene3DPositionX", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.positionX", -1.0, 1.0, {}, true, true,
              editor("panels-3d", "Scene position X", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DPositionY", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.positionY", -1.0, 1.0, {}, true, true,
              editor("panels-3d", "Scene position Y", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DPositionZ", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.positionZ", -1.0, 1.0, {}, true, true,
              editor("panels-3d", "Scene position Z (depth)", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DScale", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.scale", 0.05, 20.0, {}, true, true,
              editor("panels-3d", "Scale", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.01}, {QStringLiteral("decimals"), 2},
                          {QStringLiteral("logarithmic"), true}})),
        panel("scene3DScaleX", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.scaleX", 0.05, 20.0, {}, true, true,
              editor("panels-3d", "Scale sideways", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.01}, {QStringLiteral("decimals"), 2},
                          {QStringLiteral("logarithmic"), true}})),
        panel("scene3DScaleY", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.scaleY", 0.05, 20.0, {}, true, true,
              editor("panels-3d", "Scale up and down", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.01}, {QStringLiteral("decimals"), 2},
                          {QStringLiteral("logarithmic"), true}})),
        panel("scene3DFieldOfView", Access::Editor, Type::Real, Normalization::RealRange,
              40.0, "surface.parameters3D.fieldOfView", 20.0, 70.0, {}, true, true,
              editor("panels-3d", "Field of view", "spin", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("scene3DThickness", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.thickness", 0.05, 20.0, {}, true, true,
              editor("panels-3d", "Platform thickness", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.01}, {QStringLiteral("decimals"), 2},
                          {QStringLiteral("logarithmic"), true}})),
        panel("scene3DIconElevation", Access::Editor, Type::Real, Normalization::RealRange,
              0.3, "surface.parameters3D.iconElevation", 0.0, 2.0, {}, true, true,
              editor("panels-3d", "Pedestal height", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // Real 3D icons: how deep each icon's own picture stands as a solid,
        // as a share of its size (0 keeps it a flat picture), and how far the
        // solid turns about its upright axis so its depth shows.
        panel("scene3DIconDepth", Access::Editor, Type::Real, Normalization::RealRange,
              0.3, "surface.parameters3D.iconDepth", 0.0, 1.0, {}, true, true,
              editor("panels-3d", "Icon depth", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DIconTurn", Access::Editor, Type::Real, Normalization::RealRange,
              15.0, "surface.parameters3D.iconTurn", -60.0, 60.0, {}, true, true,
              editor("panels-3d", "Icon turn", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 1.0}, {QStringLiteral("decimals"), 0}})),
        // Auto: a plate-shaped picture a block, any other its parts rounded;
        // rounded: the picture's parts inflated into rounded volumes; block:
        // the picture cut out as a block with walls.
        panel("scene3DIconRelief", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("auto"), "surface.parameters3D.iconRelief", {}, {},
              {"auto", "rounded", "block"}, true, true,
              editor("panels-3d", "Icon relief", "combo", {"studio"}, "scene3d-quality")),
        // How much the light models the icon's volume: 0 keeps the picture's
        // own brightness, 1 lets the light alone shape it.
        panel("scene3DIconLighting", Access::Editor, Type::Real, Normalization::RealRange,
              0.5, "surface.parameters3D.iconLighting", 0.0, 1.0, {}, true, true,
              editor("panels-3d", "Icon lighting", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // A generated platform's own shape (ADFIX-TASK-002): how wide its flat
        // top is, as half its width in the platform's radius, and how far its
        // top tilts up or down outward of the icons' line.
        panel("scene3DBand", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.band", 0.0, 0.16, {}, true, true,
              editor("panels-3d", "Platform width", "slider", {"studio"}, "scene3d-shape",
                     {}, {{QStringLiteral("step"), 0.01}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DBend", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.bend", -1.0, 1.0, {}, true, true,
              editor("panels-3d", "Edge tilt", "slider", {"studio"}, "scene3d-shape",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DFold", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.fold", -1.0, 1.0, {}, true, true,
              editor("panels-3d", "Bend", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DKeyLight", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.keyLightBrightness", 0.0, 4.0, {}, true, true,
              editor("panels-3d", "Key light", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.1}, {QStringLiteral("decimals"), 1}})),
        panel("scene3DFillLight", Access::Editor, Type::Real, Normalization::RealRange,
              0.4, "surface.parameters3D.fillLightBrightness", 0.0, 2.0, {}, true, true,
              editor("panels-3d", "Fill light", "slider", {"studio"}, "scene3d-quality",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("scene3DTransitions", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "surface.parameters3D.transitions", {}, {}, {}, true, true,
              editor("panels-3d", "Animate orientation changes", "switch", {"studio"},
                     "scene3d-quality")),
        panel("scene3DFloat", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "surface.parameters3D.float", {}, {}, {}, true, true,
              editor("panels-3d", "Gentle float", "switch", {"studio"}, "scene3d-quality")),
        panel("scene3DColor", Access::Editor, Type::String, Normalization::TrimmedString,
              QString{}, "surface.parameters3D.color", {}, {}, {}, true, true,
              editor("panels-3d", "Platform colour", "color", {"studio"}, "scene3d-quality")),
        panel("scene3DMaterial", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("theme"), "surface.parameters3D.material", {}, {},
              {"theme", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime",
               "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("panels-3d", "Material", "combo", {"studio"}, "scene3d-quality")),
        panel("scene3DTexture", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("theme"), "surface.parameters3D.texture", {}, {},
              {"theme", "none", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime",
               "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("panels-3d", "Texture", "combo", {"studio"}, "scene3d-quality")),
        // Pedestals (owner's requests, 2026-10-12): the stands under the
        // icons, on flat and 3D panels alike, facing the viewer with their
        // icon floating centred above. New panels have none; "auto" is the
        // renderer's own column. Ranges wide enough for small plates to tall
        // stands, with their own thickness, texture, opacity and glow.
        panel("pedestalShape", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("none"), "surface.parameters3D.pedestalShape", {}, {},
              {"auto", "none", "column", "plate", "disc", "ring", "saucer", "cube", "hexagon", "octagon", "star", "cone", "pyramid", "dome", "orb", "bowl", "pillar", "tiered", "goblet", "hourglass", "crystal", "halo", "projector", "projector-round"},
              true, true, editor("pedestals", "Shape", "combo", {"studio"}, "pedestals")),
        panel("pedestalSize", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.pedestalSize", 0.2, 4.0, {}, true, true,
              editor("pedestals", "Size", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalThickness", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.pedestalThickness", 0.2, 4.0, {}, true, true,
              editor("pedestals", "Thickness", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalLift", Access::Editor, Type::Real, Normalization::RealRange,
              0.3, "surface.parameters3D.pedestalLift", 0.0, 2.0, {}, true, true,
              editor("pedestals", "Float height", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // How a stand turns on a 3D panel: Panel, the default, lies on the
        // panel at every tilt; Auto lies on a panel tilted back and stays
        // level, facing the viewer, on one seen face-on; Viewer always
        // level. View angle is how far from above a level stand is seen (0
        // side on, 90 straight from above); a flat panel's stands lie level
        // at their View angle, near horizontal by default, whatever their
        // facing.
        panel("pedestalFacing", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("panel"), "surface.parameters3D.pedestalFacing", {}, {},
              {"panel", "auto", "viewer"},
              true, true, editor("pedestals", "Stand facing", "combo", {"studio"}, "pedestals")),
        panel("pedestalViewAngle", Access::Editor, Type::Real, Normalization::RealRange,
              20.0, "surface.parameters3D.pedestalViewAngle", 0.0, 90.0, {}, true, true,
              editor("pedestals", "View angle", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 1.0}, {QStringLiteral("decimals"), 0},
                          {QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("pedestalMaterial", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("panel"), "surface.parameters3D.pedestalMaterial", {}, {},
              {"panel", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("pedestals", "Material", "combo", {"studio"}, "pedestals")),
        panel("pedestalTexture", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("material"), "surface.parameters3D.pedestalTexture", {}, {},
              {"material", "none", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("pedestals", "Texture", "combo", {"studio"}, "pedestals")),
        panel("pedestalPicture", Access::Editor, Type::String, Normalization::PictureUrl,
              QString{}, "surface.parameters3D.pedestalPicture", {}, {}, {}, true, true,
              editor("pedestals", "Imported picture", "picture", {"studio"}, "pedestals")),
        // A 3D stand wearing a picture of its own stands out in relief as the
        // panel does, with the same three choices (see panelPictureDepth).
        panel("pedestalPictureDepth", Access::Editor, Type::Real, Normalization::RealRange,
              0.8, "surface.parameters3D.pedestalPictureDepth", 0.0, 1.0, {}, true, true,
              editor("pedestals", "Relief depth", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalPictureRise", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("light"), "surface.parameters3D.pedestalPictureRise", {}, {}, {"light", "dark"},
              true, true, editor("pedestals", "Raised parts", "combo", {"studio"}, "pedestals")),
        // A stand is a thin plate, so its stones stand out of it by default.
        panel("pedestalPictureDirection", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("outward"), "surface.parameters3D.pedestalPictureDirection", {}, {},
              {"inward", "outward"}, true, true,
              editor("pedestals", "Relief direction", "combo", {"studio"}, "pedestals")),
        // The depth map's regulators: the brightness that is half height,
        // how steeply the height climbs through it, and a blur of the map.
        panel("pedestalPictureLevel", Access::Editor, Type::Real, Normalization::RealRange,
              0.38, "surface.parameters3D.pedestalPictureLevel", 0.0, 1.0, {}, true, true,
              editor("pedestals", "Relief level", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.02}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalPictureContrast", Access::Editor, Type::Real, Normalization::RealRange,
              0.35, "surface.parameters3D.pedestalPictureContrast", 0.0, 1.0, {}, true, true,
              editor("pedestals", "Relief contrast", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalPictureSmoothing", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.pedestalPictureSmoothing", 0.0, 1.0, {}, true, true,
              editor("pedestals", "Relief smoothing", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.pedestalColor", {}, {}, {}, true, true,
              editor("pedestals", "Colour", "color", {"studio"}, "pedestals")),
        panel("pedestalOpacity", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.pedestalOpacity", 0.1, 1.0, {}, true, true,
              editor("pedestals", "Opacity", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // A glow is light round the stand: how strong, how far it reaches out
        // of the stand, and in what colour (the owner, 2026-10-13); each of
        // the panel, its icons, its tiles and its stands has its own.
        panel("pedestalGlow", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.pedestalGlow", 0.0, 1.0, {}, true, true,
              editor("pedestals-glow", "Glow intensity", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.pedestalGlowReach", 0.0, 1.0, {}, true, true,
              editor("pedestals-glow", "Glow reach", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.pedestalGlowColor", {}, {}, {}, true, true,
              editor("pedestals-glow", "Glow colour", "color", {"studio"}, "pedestals")),
        panel("pedestalAnimation", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("none"), "surface.parameters3D.pedestalAnimation", {}, {},
              {"none", "bob", "spin", "pulse", "sway", "wobble", "glow", "float"},
              true, true, editor("pedestals", "Animation", "combo", {"studio"}, "pedestals")),
        panel("pedestalAnimationSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.pedestalAnimationSpeed", 0.1, 5.0, {}, true, true,
              editor("pedestals", "Animation speed", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // The stands under the items of an opened folder: the panel's own,
        // a shape of their own, or none; with Own settings, their own size,
        // thickness, float height, material, texture, colour, opacity, glow
        // and motion (owner's requests, 2026-10-12).
        panel("folderPedestalShape", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("same"), "surface.parameters3D.folderPedestalShape", {}, {},
              {"same", "none", "column", "plate", "disc", "ring", "saucer", "cube", "hexagon", "octagon", "star", "cone", "pyramid", "dome", "orb", "bowl", "pillar", "tiered", "goblet", "hourglass", "crystal", "halo", "projector", "projector-round"},
              true, true, editor("pedestals-folder", "Shape", "combo", {"studio"}, "pedestals")),
        panel("folderPedestalOwnStyle", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "surface.parameters3D.folderPedestalOwnStyle", {}, {}, {}, true, true,
              editor("pedestals-folder", "Own settings", "switch", {"studio"}, "pedestals")),
        panel("folderPedestalSize", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.folderPedestalSize", 0.2, 4.0, {}, true, true,
              editor("pedestals-folder", "Size", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalThickness", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.folderPedestalThickness", 0.2, 4.0, {}, true, true,
              editor("pedestals-folder", "Thickness", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalLift", Access::Editor, Type::Real, Normalization::RealRange,
              0.3, "surface.parameters3D.folderPedestalLift", 0.0, 2.0, {}, true, true,
              editor("pedestals-folder", "Float height", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalViewAngle", Access::Editor, Type::Real, Normalization::RealRange,
              20.0, "surface.parameters3D.folderPedestalViewAngle", 0.0, 90.0, {}, true, true,
              editor("pedestals-folder", "View angle", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 1.0}, {QStringLiteral("decimals"), 0},
                          {QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("folderPedestalMaterial", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("panel"), "surface.parameters3D.folderPedestalMaterial", {}, {},
              {"panel", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("pedestals-folder", "Material", "combo", {"studio"}, "pedestals")),
        panel("folderPedestalTexture", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("material"), "surface.parameters3D.folderPedestalTexture", {}, {},
              {"material", "none", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"},
              true, true, editor("pedestals-folder", "Texture", "combo", {"studio"}, "pedestals")),
        panel("folderPedestalPicture", Access::Editor, Type::String, Normalization::PictureUrl,
              QString{}, "surface.parameters3D.folderPedestalPicture", {}, {}, {}, true, true,
              editor("pedestals-folder", "Imported picture", "picture", {"studio"}, "pedestals")),
        // A 3D stand wearing a picture of its own stands out in relief as the
        // panel does, with the same three choices (see panelPictureDepth).
        panel("folderPedestalPictureDepth", Access::Editor, Type::Real, Normalization::RealRange,
              0.8, "surface.parameters3D.folderPedestalPictureDepth", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Relief depth", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalPictureRise", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("light"), "surface.parameters3D.folderPedestalPictureRise", {}, {}, {"light", "dark"},
              true, true, editor("pedestals-folder", "Raised parts", "combo", {"studio"}, "pedestals")),
        panel("folderPedestalPictureDirection", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("outward"), "surface.parameters3D.folderPedestalPictureDirection", {}, {},
              {"inward", "outward"}, true, true,
              editor("pedestals-folder", "Relief direction", "combo", {"studio"}, "pedestals")),
        // The depth map's regulators: the brightness that is half height,
        // how steeply the height climbs through it, and a blur of the map.
        panel("folderPedestalPictureLevel", Access::Editor, Type::Real, Normalization::RealRange,
              0.38, "surface.parameters3D.folderPedestalPictureLevel", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Relief level", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.02}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalPictureContrast", Access::Editor, Type::Real, Normalization::RealRange,
              0.35, "surface.parameters3D.folderPedestalPictureContrast", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Relief contrast", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalPictureSmoothing", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.folderPedestalPictureSmoothing", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Relief smoothing", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.folderPedestalColor", {}, {}, {}, true, true,
              editor("pedestals-folder", "Colour", "color", {"studio"}, "pedestals")),
        panel("folderPedestalOpacity", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.folderPedestalOpacity", 0.1, 1.0, {}, true, true,
              editor("pedestals-folder", "Opacity", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalGlow", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.folderPedestalGlow", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Glow intensity", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.folderPedestalGlowReach", 0.0, 1.0, {}, true, true,
              editor("pedestals-folder", "Glow reach", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("folderPedestalAnimation", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("none"), "surface.parameters3D.folderPedestalAnimation", {}, {},
              {"none", "bob", "spin", "pulse", "sway", "wobble", "glow", "float"},
              true, true, editor("pedestals-folder", "Animation", "combo", {"studio"}, "pedestals")),
        panel("folderPedestalAnimationSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.folderPedestalAnimationSpeed", 0.1, 5.0, {}, true, true,
              editor("pedestals-folder", "Animation speed", "slider", {"studio"}, "pedestals",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // PD-17: a platform remembers the flat surface and layout it replaced.
        // It is editor data so Preview/Cancel/Apply share the same transaction.
        panel("previousFlatLook", Access::Editor, Type::Map, Normalization::Map,
              QVariantMap{}, "surface.parameters2D.previousFlatLook", {}, {}, {}, true, true),
        panel("bakedTilt", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters2_5D.tilt", -60.0, 60.0, {}, true, true,
              editor("panels-layout", "Perspective tilt", "spin", {"studio"}, "baked-tilt",
                     {}, {{QStringLiteral("suffix"), QStringLiteral("°")}})),
        panel("panelThemeId", Access::Editor, Type::String,
              Normalization::TrimmedString, QString{}, "surface.panelThemeId", {}, {}, {}, true,
              true),
        panel("completeThemeId", Access::Editor, Type::String,
              Normalization::TrimmedString, QString{}, "surface.completeThemeId", {}, {}, {},
              true, true),
        exposedTo(panel("appearance", Access::Editor, Type::String,
              Normalization::ChoiceLower, QStringLiteral("glass"), "surface.appearance", {}, {},
              {"glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime",
               "floating-glass", "metallic", "futuristic", "organic", "platform",
               "plate", "pedestal", "river", "magma", "picture"}, false, true,
              editor("panels-appearance", "Panelpaper", "combo", {"studio"},
                     "procedural-surface")), {"dock-configuration"}),
        // Pictures the user imports become looks of their own: the panel's,
        // its stands', its folder items' stands' and its tiles'. Each is Arch
        // Dock's own copy, by file URL; the icons themselves stay as they are.
        panel("panelPicture", Access::Editor, Type::String, Normalization::PictureUrl,
              QString{}, "surface.picture", {}, {}, {}, true, true,
              editor("panels-appearance", "Imported picture", "picture", {"studio"}, "panel-picture")),
        // How deep a 3D panel's picture stands out of it and sinks into it,
        // its brightness its height.
        panel("panelPictureDepth", Access::Editor, Type::Real, Normalization::RealRange,
              0.8, "surface.parameters3D.panelPictureDepth", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Relief depth", "slider", {"studio"}, "picture-relief",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // Which parts of that picture stand out: its light ones (stones
        // between dark gaps) or its dark ones (dark mountains under a light
        // sky); its brightness alone cannot tell.
        panel("panelPictureRise", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("light"), "surface.parameters3D.panelPictureRise", {}, {}, {"light", "dark"}, true, true,
              editor("panels-appearance", "Raised parts", "combo", {"studio"}, "picture-relief")),
        // Whether those parts rise out of the panel (little mountains), or
        // stay at its surface while the rest sinks in (stones in a cage).
        panel("panelPictureDirection", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("inward"), "surface.parameters3D.panelPictureDirection", {}, {}, {"inward", "outward"},
              true, true, editor("panels-appearance", "Relief direction", "combo", {"studio"}, "picture-relief")),
        // The depth map's regulators: the brightness that is half height,
        // how steeply the height climbs through it, and a blur of the map.
        panel("panelPictureLevel", Access::Editor, Type::Real, Normalization::RealRange,
              0.38, "surface.parameters3D.panelPictureLevel", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Relief level", "slider", {"studio"}, "picture-relief",
                     {}, {{QStringLiteral("step"), 0.02}, {QStringLiteral("decimals"), 2}})),
        panel("panelPictureContrast", Access::Editor, Type::Real, Normalization::RealRange,
              0.35, "surface.parameters3D.panelPictureContrast", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Relief contrast", "slider", {"studio"}, "picture-relief",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("panelPictureSmoothing", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.panelPictureSmoothing", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Relief smoothing", "slider", {"studio"}, "picture-relief",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // A panel decoration remaking a flat panel's outline on every shape
        // and edge (DecorationCatalogue.js; empty for none): a starship's
        // front, a nebula, a branch, a wand. Its lights pulse and its motion
        // plays unless its animation is off, as fast as chosen.
        panel("panelDecoration", Access::Editor, Type::String, Normalization::LowerString,
              QString{}, "surface.parameters3D.panelDecoration", {}, {}, {}, true, true,
              editor("panels-appearance", "Panel decoration", "decoration", {"studio"}, "decoration")),
        panel("panelDecorationAnimation", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "surface.parameters3D.panelDecorationAnimation", {}, {}, {}, true, true,
              editor("panels-appearance", "Animation", "switch", {"studio"}, "decoration")),
        panel("panelDecorationSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.panelDecorationSpeed", 0.1, 3.0, {}, true, true,
              editor("panels-appearance", "Animation speed", "slider", {"studio"}, "decoration",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // Its colours, as designed when empty: its lights' colour and finish
        // (a rainbow), its body's main colour (the material's grain kept)
        // and finish, the colours of a many-coloured one, the metal of its
        // trims, and a picture's hue, strength and brightness (a nebula).
        panel("panelDecorationLight", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.panelDecorationLight", {}, {}, {}, true, true,
              editor("panels-appearance", "Light colour", "color", {"studio"}, "decoration-colours")),
        panel("panelDecorationLightFinish", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("solid"), "surface.parameters3D.panelDecorationLightFinish", {}, {},
              {"solid", "rainbow"}, true, true,
              editor("panels-appearance", "Light finish", "combo", {"studio"}, "decoration-colours")),
        panel("panelDecorationMain", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.panelDecorationMain", {}, {}, {}, true, true,
              editor("panels-appearance", "Main colour", "color", {"studio"}, "decoration-colours")),
        panel("panelDecorationFinish", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("solid"), "surface.parameters3D.panelDecorationFinish", {}, {},
              {"solid", "chameleon", "iridescent", "rainbow", "metallic-rainbow"}, true, true,
              editor("panels-appearance", "Finish", "combo", {"studio"}, "decoration-colours")),
        // The palette as its colours in order, separated by commas.
        panel("panelDecorationPalette", Access::Editor, Type::String, Normalization::TrimmedString,
              QString{}, "surface.parameters3D.panelDecorationPalette", {}, {}, {}, true, true,
              editor("panels-appearance", "Palette", "palette", {"studio"}, "decoration-colours")),
        panel("panelDecorationMetal", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("designed"), "surface.parameters3D.panelDecorationMetal", {}, {},
              {"designed", "polished-gold", "bright-gold", "yellow-gold", "gold-leaf", "white-gold", "rose-gold",
               "red-gold", "silver", "brushed-platinum", "polished-copper"}, true, true,
              editor("panels-appearance", "Metal", "combo", {"studio"}, "decoration-colours")),
        // The icon sitting in the decoration's launcher seat (a raptor's
        // head), by its entry id: put there from the icon's menu or by
        // dropping it on the seat, not from Panel Studio.
        panel("panelDecorationSeat", Access::Editor, Type::String, Normalization::TrimmedString,
              QString{}, "surface.parameters3D.panelDecorationSeat", {}, {}, {}, true, true),
        // The body's material: one of the named materials, or as designed.
        panel("panelDecorationMaterial", Access::Editor, Type::String, Normalization::LowerString,
              QString{}, "surface.parameters3D.panelDecorationMaterial", {}, {}, {}, true, true,
              editor("panels-appearance", "Material", "material", {"studio"}, "decoration-colours")),
        panel("panelDecorationHue", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.panelDecorationHue", -180.0, 180.0, {}, true, true,
              editor("panels-appearance", "Picture hue", "slider", {"studio"}, "decoration-colours",
                     {}, {{QStringLiteral("step"), 1}, {QStringLiteral("decimals"), 0}})),
        panel("panelDecorationStrength", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.panelDecorationStrength", 0.0, 2.0, {}, true, true,
              editor("panels-appearance", "Picture strength", "slider", {"studio"}, "decoration-colours",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("panelDecorationBrightness", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.panelDecorationBrightness", 0.2, 2.0, {}, true, true,
              editor("panels-appearance", "Picture brightness", "slider", {"studio"}, "decoration-colours",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // Holograms (a Hologram tab each for the icons, the panel and its
        // stands): each drawn as light, see-through, edged brighter, crossed
        // by drifting scan lines, flickering as a weak signal does, with its
        // own colour set (by default its own colours, Original, or a set of
        // hologram colours; each part's own colour, when chosen, replaces the
        // set's), glow, flicker and speed. A hologram icon floats over a
        // projector's opening in the middle of its stand's top, or on the
        // panel where it has none, a widening light carrying it (Projector
        // light, in the icon's own main colour by default); the light rises
        // and drifts as Light motion says, and on a 3D panel the icon turns
        // as Turning says.
        panel("hologramIcons", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "surface.parameters3D.hologramIcons", {}, {}, {}, true, true,
              editor("icons-hologram", "Holographic icons", "switch", {"studio"}, "hologram")),
        panel("hologramIconColours", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("original"), "surface.parameters3D.hologramIconColours", {}, {},
              {"original", "cyan", "ice", "emerald", "amber", "ruby", "violet", "rose"}, true, true,
              editor("icons-hologram", "Colour set", "combo", {"studio"}, "hologram")),
        panel("hologramIconOutlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramIconOutlineColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Outline colour", "color", {"studio"}, "hologram")),
        panel("hologramIconInlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramIconInlineColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Inline colour", "color", {"studio"}, "hologram")),
        panel("hologramIconVolumeColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramIconVolumeColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Volume colour", "color", {"studio"}, "hologram")),
        panel("hologramIconGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramIconGlowColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Glow colour", "color", {"studio"}, "hologram")),
        panel("hologramIconGlow", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramIconGlow", 0.0, 1.5, {}, true, true,
              editor("icons-hologram", "Glow intensity", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramIconGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.hologramIconGlowReach", 0.0, 1.0, {}, true, true,
              editor("icons-hologram", "Glow reach", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramIconFlicker", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramIconFlicker", 0.0, 2.0, {}, true, true,
              editor("icons-hologram", "Flicker", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramIconFloat", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "surface.parameters3D.hologramIconFloat", {}, {}, {}, true, true,
              editor("icons-hologram", "Float", "switch", {"studio"}, "hologram")),
        panel("hologramIconTurning", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("turn"), "surface.parameters3D.hologramIconTurning", {}, {},
              {"turn", "sway", "still"}, true, true,
              editor("icons-hologram", "Turning", "combo", {"studio"}, "hologram")),
        panel("hologramIconSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramIconSpeed", 0.1, 3.0, {}, true, true,
              editor("icons-hologram", "Speed", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramBeam", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "surface.parameters3D.hologramBeam", {}, {}, {}, true, true,
              editor("icons-hologram", "Projector light", "switch", {"studio"}, "hologram")),
        panel("hologramLightColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramLightColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Light colour", "color", {"studio"}, "hologram")),
        panel("hologramOpeningColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramOpeningColor", {}, {}, {}, true, true,
              editor("icons-hologram", "Opening colour", "color", {"studio"}, "hologram")),
        panel("hologramBeamStrength", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramBeamStrength", 0.2, 1.6, {}, true, true,
              editor("icons-hologram", "Light strength", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramBeamHeight", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramBeamHeight", 0.2, 2.0, {}, true, true,
              editor("icons-hologram", "Light height", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramBeamMotion", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("rise-drift"), "surface.parameters3D.hologramBeamMotion", {}, {},
              {"rise-drift", "rise", "pulse", "sweep", "still"}, true, true,
              editor("icons-hologram", "Light motion", "combo", {"studio"}, "hologram")),
        panel("hologramPanel", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "surface.parameters3D.hologramPanel", {}, {}, {}, true, true,
              editor("panels-hologram", "Holographic panel", "switch", {"studio"}, "hologram")),
        panel("hologramPanelColours", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("original"), "surface.parameters3D.hologramPanelColours", {}, {},
              {"original", "cyan", "ice", "emerald", "amber", "ruby", "violet", "rose"}, true, true,
              editor("panels-hologram", "Colour set", "combo", {"studio"}, "hologram")),
        panel("hologramPanelOutlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPanelOutlineColor", {}, {}, {}, true, true,
              editor("panels-hologram", "Outline colour", "color", {"studio"}, "hologram")),
        panel("hologramPanelInlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPanelInlineColor", {}, {}, {}, true, true,
              editor("panels-hologram", "Inline colour", "color", {"studio"}, "hologram")),
        panel("hologramPanelVolumeColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPanelVolumeColor", {}, {}, {}, true, true,
              editor("panels-hologram", "Volume colour", "color", {"studio"}, "hologram")),
        panel("hologramPanelGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPanelGlowColor", {}, {}, {}, true, true,
              editor("panels-hologram", "Glow colour", "color", {"studio"}, "hologram")),
        panel("hologramPanelGlow", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPanelGlow", 0.0, 1.5, {}, true, true,
              editor("panels-hologram", "Glow intensity", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPanelGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.hologramPanelGlowReach", 0.0, 1.0, {}, true, true,
              editor("panels-hologram", "Glow reach", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPanelFlicker", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPanelFlicker", 0.0, 2.0, {}, true, true,
              editor("panels-hologram", "Flicker", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPanelSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPanelSpeed", 0.1, 3.0, {}, true, true,
              editor("panels-hologram", "Speed", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPedestals", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "surface.parameters3D.hologramPedestals", {}, {}, {}, true, true,
              editor("pedestals-hologram", "Holographic pedestals", "switch", {"studio"}, "hologram")),
        panel("hologramPedestalColours", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("original"), "surface.parameters3D.hologramPedestalColours", {}, {},
              {"original", "cyan", "ice", "emerald", "amber", "ruby", "violet", "rose"}, true, true,
              editor("pedestals-hologram", "Colour set", "combo", {"studio"}, "hologram")),
        panel("hologramPedestalOutlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPedestalOutlineColor", {}, {}, {}, true, true,
              editor("pedestals-hologram", "Outline colour", "color", {"studio"}, "hologram")),
        panel("hologramPedestalInlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPedestalInlineColor", {}, {}, {}, true, true,
              editor("pedestals-hologram", "Inline colour", "color", {"studio"}, "hologram")),
        panel("hologramPedestalVolumeColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPedestalVolumeColor", {}, {}, {}, true, true,
              editor("pedestals-hologram", "Volume colour", "color", {"studio"}, "hologram")),
        panel("hologramPedestalGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.hologramPedestalGlowColor", {}, {}, {}, true, true,
              editor("pedestals-hologram", "Glow colour", "color", {"studio"}, "hologram")),
        panel("hologramPedestalGlow", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPedestalGlow", 0.0, 1.5, {}, true, true,
              editor("pedestals-hologram", "Glow intensity", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPedestalGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.hologramPedestalGlowReach", 0.0, 1.0, {}, true, true,
              editor("pedestals-hologram", "Glow reach", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPedestalFlicker", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPedestalFlicker", 0.0, 2.0, {}, true, true,
              editor("pedestals-hologram", "Flicker", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("hologramPedestalSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.hologramPedestalSpeed", 0.1, 3.0, {}, true, true,
              editor("pedestals-hologram", "Speed", "slider", {"studio"}, "hologram",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("sparkleIntensity", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters2D.sparkleIntensity", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Sparkle intensity", "slider", {"studio"},
                     "procedural-surface", {}, {{QStringLiteral("step"), 0.05},
                                                {QStringLiteral("decimals"), 2}})),
        // No renderer draws a surface outline from this value: the layout
        // decides a panel's shape. Panel Studio does not offer it (ADREP-TASK-001,
        // PD-04); the saved value is kept.
        exposedTo(panel("shape", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("pill"), "surface.shape", {}, {},
              {"pill", "rounded", "hexagon"}, false, true), {"dock-configuration"}),
        exposedTo(panel("opacity", Access::Editor, Type::Real, Normalization::RealRange, 0.9,
              "surface.opacity", 0.0, 1.0, {}, false, true,
              editor("panels-appearance", "Opacity", "slider", {"studio"},
                     "surface-opacity", {}, {{QStringLiteral("step"), 0.05},
                                                {QStringLiteral("decimals"), 2}})),
                  {"dock-configuration"}),
        panel("color", Access::Editor, Type::String, Normalization::TrimmedString,
              QString{}, "surface.color", {}, {}, {}, false, true,
              editor("panels-appearance", "Color", "color", {"studio"},
                     "dynamic-tint")),
        panel("glowIntensity", Access::Editor, Type::Real,
              Normalization::RealRange, 1.0, "surface.glowIntensity",
              0.0, 2.0, {}, false, true,
              editor("panels-appearance", "Look's own glow", "slider",
                     {"studio"}, "dynamic-glow", {},
                     {{QStringLiteral("step"), 0.05},
                      {QStringLiteral("decimals"), 2}})),
        // A clear-ground look - lightning's bolts, aurora's curtains - is drawn
        // on a clear ground, its dark sky behind as strong as chosen, in a
        // palette of its own (the owner, 2026-10-13); named for the look on
        // the page.
        panel("panelLookBackground", Access::Editor, Type::Real, Normalization::RealRange,
              0.5, "surface.parameters3D.panelLookBackground", 0.0, 1.0, {}, true, true,
              editor("panels-appearance", "Background", "slider", {"studio"}, "lightning",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("pedestalLookBackground", Access::Editor, Type::Real, Normalization::RealRange,
              0.5, "surface.parameters3D.pedestalLookBackground", 0.0, 1.0, {}, true, true,
              editor("pedestals", "Background", "slider", {"studio"}, "lightning",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // A glowing look's colours (the owner, 2026-10-13: lightning in
        // "lightning naturally colors", aurora's, and a rainbow wherever it
        // can be applied): its own ("original"), or another palette of that
        // look, offered and named for the look on the page.
        panel("panelLookColour", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("original"), "surface.parameters3D.panelLookColour", {}, {},
              {"original", "ice", "crimson", "toxic", "solar", "arcane", "void", "aurora", "emerald",
               "australis", "cosmic", "rainbow", "chameleon", "iridescent", "clear", "deep", "jade", "sunset"},
              true, true,
              editor("panels-appearance", "Colour", "combo", {"studio"}, "lightning")),
        panel("pedestalLookColour", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("original"), "surface.parameters3D.pedestalLookColour", {}, {},
              {"original", "ice", "crimson", "toxic", "solar", "arcane", "void", "aurora", "emerald",
               "australis", "cosmic", "rainbow", "chameleon", "iridescent", "clear", "deep", "jade", "sunset"},
              true, true,
              editor("pedestals", "Colour", "combo", {"studio"}, "lightning")),
        // How fast an animated look moves: plasma and circuitry flow,
        // lightning strikes (named for the look on the page).
        panel("lookMotionSpeed", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "surface.parameters3D.lookMotionSpeed", 0.1, 3.0, {}, true, true,
              editor("panels-appearance", "Motion speed", "slider", {"studio"}, "lightning",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // The panel's own glow: light round its shape on every renderer, how
        // strong, how far and in what colour (the owner, 2026-10-13).
        panel("panelGlow", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.panelGlow", 0.0, 1.0, {}, true, true,
              editor("panels-glow", "Glow intensity", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("panelGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.panelGlowReach", 0.0, 1.0, {}, true, true,
              editor("panels-glow", "Glow reach", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("panelGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.panelGlowColor", {}, {}, {}, true, true,
              editor("panels-glow", "Glow colour", "color", {"studio"}, "glow")),
        // Light round each icon's picture.
        panel("iconGlow", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.iconGlow", 0.0, 1.0, {}, true, true,
              editor("icons-glow", "Glow intensity", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("iconGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.iconGlowReach", 0.0, 1.0, {}, true, true,
              editor("icons-glow", "Glow reach", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        // Light round each icon's tile.
        panel("iconTileGlow", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "surface.parameters3D.iconTileGlow", 0.0, 1.0, {}, true, true,
              editor("icon-tiles-glow", "Glow intensity", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("iconTileGlowReach", Access::Editor, Type::Real, Normalization::RealRange,
              0.75, "surface.parameters3D.iconTileGlowReach", 0.0, 1.0, {}, true, true,
              editor("icon-tiles-glow", "Glow reach", "slider", {"studio"}, "glow",
                     {}, {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("iconTileGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "surface.parameters3D.iconTileGlowColor", {}, {}, {}, true, true,
              editor("icon-tiles-glow", "Glow colour", "color", {"studio"}, "glow")),
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
              Normalization::ChoiceLower, QStringLiteral("style-default"), "iconStyle.shape", {}, {},
              {"style-default", "rounded", "square", "squircle", "circle", "hexagon", "diamond"}, false, true,
              // PD-20: every declared built-in silhouette follows this choice.
              editor("icons-appearance", "Shape", "combo", {"studio"},
                     "tile-shape")), {"dock-configuration"}),
        panel("iconDiameter", Access::Editor, Type::Integer, Normalization::IntegerRange,
              100, "iconStyle.diameter", 40, 100, {}, false, true,
              editor("icons-appearance", "Diameter", "spin", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("suffix"), QStringLiteral("%")}})),
        panel("iconLogoSize", Access::Editor, Type::Integer, Normalization::IntegerRange,
              95, "iconStyle.logoSize", 55, 100, {}, false, true,
              editor("icons-appearance", "Logo size", "spin", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("suffix"), QStringLiteral("%")}})),
        panel("iconOutlineWidth", Access::Editor, Type::Integer, Normalization::IntegerRange,
              -1, "iconStyle.outlineWidth", -1, 12, {}, false, true,
              editor("icons-appearance", "Outline thickness", "spin", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("suffix"), QStringLiteral(" px")},
                      {QStringLiteral("specialValueText"), QStringLiteral("Style default")}})),
        panel("iconBodyColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "iconStyle.bodyColor", {}, {}, {}, false, true,
              editor("icons-appearance", "Body colour", "color", {"studio"}, "icon-state-styling")),
        panel("iconOutlineColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "iconStyle.outlineColor", {}, {}, {}, false, true,
              editor("icons-appearance", "Outline colour", "color", {"studio"}, "icon-state-styling")),
        panel("iconGlowColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "iconStyle.glowColor", {}, {}, {}, false, true,
              editor("icons-glow", "Glow colour", "color", {"studio"}, "icon-state-styling")),
        panel("iconPedestalEnabled", Access::Editor, Type::Boolean, Normalization::Boolean,
              false, "iconStyle.pedestalEnabled", {}, {}, {}, false, true,
              editor("icons-appearance", "Pedestal", "switch", {"studio"}, "icon-state-styling")),
        panel("iconPedestalHeight", Access::Editor, Type::Integer, Normalization::IntegerRange,
              20, "iconStyle.pedestalHeight", 5, 50, {}, false, true,
              editor("icons-appearance", "Pedestal height", "spin", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("suffix"), QStringLiteral("%")}})),
        panel("iconPedestalColor", Access::Editor, Type::String, Normalization::HexColor,
              QString{}, "iconStyle.pedestalColor", {}, {}, {}, false, true,
              editor("icons-appearance", "Pedestal colour", "color", {"studio"}, "icon-state-styling")),
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
        panel("iconTilesEnabled", Access::Editor, Type::Boolean, Normalization::Boolean,
              true, "iconStyle.tilesEnabled", {}, {}, {}, false, true,
              editor("icon-tiles", "Show tiles by default", "switch", {"studio"}, "icon-state-styling")),
        panel("iconTileMode", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("style"), "iconStyle.tileMode", {}, {}, {"style", "custom"}, false, true,
              editor("icon-tiles", "Tile appearance", "combo", {"studio"}, "icon-state-styling")),
        panel("iconTileColor", Access::Editor, Type::String, Normalization::HexColor,
              QStringLiteral("#334155"), "iconStyle.tileColor", {}, {}, {}, false, true,
              editor("icon-tiles", "Fill color", "color", {"studio"}, "icon-state-styling")),
        panel("iconTileOpacity", Access::Editor, Type::Real, Normalization::RealRange,
              0.8, "iconStyle.tileOpacity", 0.0, 1.0, {}, false, true,
              editor("icon-tiles", "Tile opacity", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 0.05}, {QStringLiteral("decimals"), 2}})),
        panel("iconTileBorderColor", Access::Editor, Type::String, Normalization::HexColor,
              QStringLiteral("#94a3b8"), "iconStyle.tileBorderColor", {}, {}, {}, false, true,
              editor("icon-tiles", "Border color", "color", {"studio"}, "icon-state-styling")),
        panel("iconTileBorderWidth", Access::Editor, Type::Real, Normalization::RealRange,
              1.0, "iconStyle.tileBorderWidth", 0.0, 8.0, {}, false, true,
              editor("icon-tiles", "Border width", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 0.5}, {QStringLiteral("decimals"), 1}})),
        panel("iconTileTexture", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("none"), "iconStyle.tileTexture", {}, {},
              {"none", "glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass",
               "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma"}, false, true,
              editor("icon-tiles", "Texture", "combo", {"studio"}, "icon-state-styling")),
        panel("iconTileThickness", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "iconStyle.tileThickness", 0.0, 24.0, {}, false, true,
              editor("icon-tiles", "Tile thickness", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 0.5}, {QStringLiteral("decimals"), 1}, {QStringLiteral("suffix"), QStringLiteral(" px")}})),
        panel("iconTileIconOffsetX", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "iconStyle.tileIconOffsetX", -40.0, 40.0, {}, false, true,
              editor("icon-tiles", "Icon horizontal offset", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 1}, {QStringLiteral("suffix"), QStringLiteral(" px")}})),
        panel("iconTileIconOffsetY", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "iconStyle.tileIconOffsetY", -40.0, 40.0, {}, false, true,
              editor("icon-tiles", "Icon vertical offset", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 1}, {QStringLiteral("suffix"), QStringLiteral(" px")}})),
        panel("iconTileIconScale", Access::Editor, Type::Integer, Normalization::IntegerRange,
              100, "iconStyle.tileIconScale", 25, 150, {}, false, true,
              editor("icon-tiles", "Icon scale on tile", "spin", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("suffix"), QStringLiteral("%")}})),
        panel("iconTileBevel", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "iconStyle.tileBevel", 0.0, 12.0, {}, false, true,
              editor("icon-tiles", "Tile bevel", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 0.5}, {QStringLiteral("decimals"), 1}, {QStringLiteral("suffix"), QStringLiteral(" px")}})),
        panel("iconTileMaterial", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("minimal"), "iconStyle.tileMaterial", {}, {},
              {"glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime", "floating-glass",
               "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma"}, false, true,
              editor("icon-tiles", "3D tile material", "combo", {"studio"}, "icon-state-styling")),
        // Flat: the tile as it is; bubble: a glass dome in the tile's shape
        // over the icon, with its outline drawn round it.
        panel("iconTileForm", Access::Editor, Type::String, Normalization::ChoiceLower,
              QStringLiteral("flat"), "iconStyle.tileForm", {}, {},
              {"flat", "bubble"}, false, true,
              editor("icon-tiles", "3D tile form", "combo", {"studio"}, "icon-state-styling")),
        panel("iconTileElevation", Access::Editor, Type::Real, Normalization::RealRange,
              0.0, "iconStyle.tileElevation", -24.0, 96.0, {}, false, true,
              editor("icon-tiles", "3D tile elevation", "slider", {"studio"}, "icon-state-styling", {},
                     {{QStringLiteral("step"), 1}, {QStringLiteral("suffix"), QStringLiteral(" px")}})),
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

QString editorCapabilityName(EditorCapability capability)
{
    switch (capability)
    {
    case EditorCapability::ScreenPlacement: return QStringLiteral("screen-placement");
    case EditorCapability::ContentType: return QStringLiteral("content-type");
    case EditorCapability::Segments: return QStringLiteral("segments");
    case EditorCapability::ApplicationOverlays: return QStringLiteral("application-overlays");
    case EditorCapability::LaunchFeedback: return QStringLiteral("launch-feedback");
    case EditorCapability::DropInput: return QStringLiteral("drop-input");
    case EditorCapability::FolderContent: return QStringLiteral("folder-content");
    case EditorCapability::EdgePlacement: return QStringLiteral("edge-placement");
    case EditorCapability::Alignment: return QStringLiteral("alignment");
    case EditorCapability::DynamicPlacement: return QStringLiteral("dynamic-placement");
    case EditorCapability::LengthMutation: return QStringLiteral("length-mutation");
    case EditorCapability::ThicknessMutation: return QStringLiteral("thickness-mutation");
    case EditorCapability::ArbitraryXyPlacement: return QStringLiteral("arbitrary-xy-placement");
    case EditorCapability::Visibility: return QStringLiteral("visibility");
    case EditorCapability::VisibilityMode: return QStringLiteral("visibility-mode");
    case EditorCapability::PresentationMechanism: return QStringLiteral("presentation-mechanism");
    case EditorCapability::Layout: return QStringLiteral("layout");
    case EditorCapability::WholePanelRotation: return QStringLiteral("whole-panel-rotation");
    case EditorCapability::Scene3DQuality: return QStringLiteral("scene3d-quality");
    case EditorCapability::Scene3DShape: return QStringLiteral("scene3d-shape");
    case EditorCapability::BakedTilt: return QStringLiteral("baked-tilt");
    case EditorCapability::ProceduralSurface: return QStringLiteral("procedural-surface");
    case EditorCapability::SurfaceOpacity: return QStringLiteral("surface-opacity");
    case EditorCapability::DynamicTint: return QStringLiteral("dynamic-tint");
    case EditorCapability::DynamicGlow: return QStringLiteral("dynamic-glow");
    case EditorCapability::ArtworkFit: return QStringLiteral("artwork-fit");
    case EditorCapability::IconStateStyling: return QStringLiteral("icon-state-styling");
    case EditorCapability::TileShape: return QStringLiteral("tile-shape");
    case EditorCapability::GlobalRenderer: return QStringLiteral("global-renderer");
    case EditorCapability::Pedestals: return QStringLiteral("pedestals");
    case EditorCapability::Glow: return QStringLiteral("glow");
    case EditorCapability::Lightning: return QStringLiteral("lightning");
    case EditorCapability::Identity: return QStringLiteral("identity");
    case EditorCapability::PanelPicture: return QStringLiteral("panel-picture");
    case EditorCapability::PictureRelief: return QStringLiteral("picture-relief");
    case EditorCapability::Decoration: return QStringLiteral("decoration");
    case EditorCapability::DecorationColours: return QStringLiteral("decoration-colours");
    case EditorCapability::Hologram: return QStringLiteral("hologram");
    case EditorCapability::Count: break;
    }
    return QString{};
}

std::optional<EditorCapability> editorCapabilityFromName(const QString &name)
{
    for (int value = 0; value < static_cast<int>(EditorCapability::Count); ++value)
    {
        const auto capability = static_cast<EditorCapability>(value);
        if (editorCapabilityName(capability) == name)
            return capability;
    }
    return std::nullopt;
}

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
        {QStringLiteral("choices"), choices},
        {QStringLiteral("persistencePath"), persistencePath},
        {QStringLiteral("mutationInterfaces"), mutationInterfaces},
        {QStringLiteral("optional"), optional},
        {QStringLiteral("runtimeConsumer"), runtimeConsumer},
    };
    // An absent optional bound cannot be encoded as an invalid D-Bus variant.
    if (minimumValue.isValid())
        result.insert(QStringLiteral("minimumValue"), minimumValue);
    if (maximumValue.isValid())
        result.insert(QStringLiteral("maximumValue"), maximumValue);
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
    // A complete flat look adds many fields to a theme candidate. Index the
    // immutable schema once instead of scanning it for every normalized key.
    static const auto indexes = [] {
        QHash<int, QHash<QString, const PanelSettingsFieldDescriptor *>> result;
        for (const auto &field : fields())
            result[static_cast<int>(field.scope)].insert(field.key, &field);
        return result;
    }();
    const auto index = indexes.constFind(static_cast<int>(scope));
    return index == indexes.cend() ? nullptr : index->value(key, nullptr);
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

QVariantMap PanelSettingsSchema::flatLookValues(const QVariantMap &record)
{
    QVariantMap result;
    for (const auto &field : fields()) {
        if (field.scope != PanelSettingsFieldScope::Panel
            || field.access != PanelSettingsFieldAccess::Editor
            || field.key == QStringLiteral("previousFlatLook")
            || !(field.persistencePath.startsWith(QStringLiteral("surface."))
                 || field.persistencePath.startsWith(QStringLiteral("layout."))
                 || field.persistencePath.startsWith(QStringLiteral("iconStyle."))
                 || field.persistencePath.startsWith(QStringLiteral("motion."))))
            continue;
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
