#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include <optional>

// The one list of every panel and global setting: its key, type, default,
// range or choices, where it lives in PanelDefinition, who may change it,
// and how Panel Studio shows it (page, label, control and the capability
// that gates it). Transactions, editor snapshots and migration all read it.
namespace ArchDock
{

enum class PanelSettingsFieldScope
{
    Panel,
    Global
};

enum class PanelSettingsFieldAccess
{
    Editor,
    LegacyMutable,
    Protected,
    Internal,
    Artifact
};

enum class PanelSettingsValueType
{
    Boolean,
    Integer,
    Real,
    String,
    StringList,
    List,
    Map,
    Revision
};

enum class PanelSettingsNormalization
{
    None,
    Boolean,
    TrimmedString,
    LowerString,
    HexColor,
    ChoiceLower,
    ChoiceExact,
    IntegerRange,
    RealRange,
    NonNegativeInteger,
    ScreenIndex,
    HostId,
    StringList,
    UrlList,
    Map,
    Revision
};

// What decides whether Panel Studio offers a field for the selected panel
// (ADREP-TASK-001). Every presented field names one, and Panel Studio has one
// availability rule for each. A name that is not listed here is never offered
// (fail closed), and panel-settings-schema-test fails on a field that uses one.
enum class EditorCapability
{
    ScreenPlacement,
    ContentType,
    Segments,
    ApplicationOverlays,
    LaunchFeedback,
    DropInput,
    FolderContent,
    EdgePlacement,
    Alignment,
    DynamicPlacement,
    LengthMutation,
    ThicknessMutation,
    ArbitraryXyPlacement,
    Visibility,
    VisibilityMode,
    PresentationMechanism,
    Layout,
    WholePanelRotation,
    Scene3DQuality,
    Scene3DShape,
    BakedTilt,
    ProceduralSurface,
    SurfaceOpacity,
    DynamicTint,
    DynamicGlow,
    ArtworkFit,
    IconStateStyling,
    TileShape,
    GlobalRenderer,
    Count
};

[[nodiscard]] QString editorCapabilityName(EditorCapability capability);
[[nodiscard]] std::optional<EditorCapability> editorCapabilityFromName(const QString &name);

struct PanelSettingsEditorMetadata
{
    QString section;
    QString label;
    QString control;
    QStringList consumers;
    QString capability;
    QStringList layouts;
    QVariantMap properties;

    [[nodiscard]] bool isPresented() const;
};

struct PanelSettingsFieldDescriptor
{
    QString key;
    PanelSettingsFieldScope scope = PanelSettingsFieldScope::Panel;
    PanelSettingsFieldAccess access = PanelSettingsFieldAccess::Internal;
    PanelSettingsValueType valueType = PanelSettingsValueType::String;
    PanelSettingsNormalization normalization = PanelSettingsNormalization::None;
    QVariant defaultValue;
    QVariant minimumValue;
    QVariant maximumValue;
    QStringList choices;
    QString persistencePath;
    QStringList mutationInterfaces;
    bool optional = false;
    bool runtimeConsumer = false;
    PanelSettingsEditorMetadata editor;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class PanelSettingsSchema final
{
public:
    static constexpr int CurrentVersion = 1;

    [[nodiscard]] static const QVector<PanelSettingsFieldDescriptor> &fields();
    [[nodiscard]] static const PanelSettingsFieldDescriptor *descriptor(
        PanelSettingsFieldScope scope,
        const QString &key);
    [[nodiscard]] static const PanelSettingsFieldDescriptor *panelDescriptor(
        const QString &key);
    [[nodiscard]] static const PanelSettingsFieldDescriptor *globalDescriptor(
        const QString &key);

    [[nodiscard]] static QStringList keys(PanelSettingsFieldScope scope);
    [[nodiscard]] static QStringList editorKeys(PanelSettingsFieldScope scope);
    [[nodiscard]] static bool isKnownPanelField(const QString &key);
    [[nodiscard]] static bool isKnownGlobalField(const QString &key);
    [[nodiscard]] static bool isEditorField(PanelSettingsFieldScope scope,
                                            const QString &key);
    [[nodiscard]] static bool isTransactionPanelField(const QString &key);
    [[nodiscard]] static bool isTransactionGlobalField(const QString &key);
    [[nodiscard]] static bool supportsMutationInterface(
        PanelSettingsFieldScope scope,
        const QString &key,
        const QString &interfaceName);

    [[nodiscard]] static QVariant normalizePanelValue(const QString &key,
                                                      const QVariant &value);
    [[nodiscard]] static bool normalizeGlobalValues(
        const QVariantMap &currentValues,
        const QVariantMap &submittedValues,
        int maximumScreenIndex,
        QVariantMap *candidate,
        QString *errorMessage = nullptr);

    [[nodiscard]] static QVariantMap editorValues(
        PanelSettingsFieldScope scope,
        const QVariantMap &record);
    [[nodiscard]] static QVariantMap runtimeValues(
        PanelSettingsFieldScope scope,
        const QVariantMap &record);
    [[nodiscard]] static QVariantList editorDescriptors(
        PanelSettingsFieldScope scope,
        const QString &consumer);
};

}
