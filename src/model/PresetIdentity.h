#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct PresetValidationDiagnostic
{
    QString code;
    QString jsonPointer;
    QString severity = QStringLiteral("error");
    QString message;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const PresetValidationDiagnostic &) const = default;
};

[[nodiscard]] QVariantList presetDiagnosticsToVariantList(
    const QVector<PresetValidationDiagnostic> &diagnostics);
[[nodiscard]] bool presetDiagnosticsHaveErrors(
    const QVector<PresetValidationDiagnostic> &diagnostics);

// Identity and lineage shared by Panel Presets and Icon Presets. A built-in is
// an immutable installed original. A user preset is a complete snapshot that
// records which preset and revision it was derived from, without depending on
// that source continuing to exist.
struct PresetIdentity
{
    static constexpr qsizetype MaximumIdentifierBytes = 64;
    static constexpr qsizetype MaximumNameBytes = 256;
    static constexpr qsizetype MaximumDescriptionBytes = 2048;

    QString id;
    QString name;
    QString description;
    bool builtIn = false;
    int revision = 1;
    QString derivedFromPresetId;
    int sourceRevision = 0;

    [[nodiscard]] static bool isValidId(const QString &id);
    // User-owned presets live in their own id namespace, so a user preset can
    // never shadow, replace or be mistaken for an installed built-in.
    [[nodiscard]] static QString userIdPrefix();
    [[nodiscard]] static bool isUserId(const QString &id);

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const PresetIdentity &) const = default;
};

// Strict readers shared by the Panel Preset and Icon Preset parsers. A value
// is either exactly valid or reported with its JSON pointer; nothing is
// silently clamped, defaulted or lower-cased on the way in.
namespace PresetParsing
{

[[nodiscard]] QString pointerChild(const QString &parent, const QString &child);
void addDiagnostic(QVector<PresetValidationDiagnostic> *diagnostics,
                   const QString &code,
                   const QString &pointer,
                   const QString &message,
                   const QString &severity = QStringLiteral("error"));
void rejectUnknownKeys(const QVariantMap &object,
                       const QSet<QString> &allowed,
                       const QString &pointer,
                       QVector<PresetValidationDiagnostic> *diagnostics);

[[nodiscard]] bool isNumber(const QVariant &value);
[[nodiscard]] bool isInteger(const QVariant &value);
// `transparent`, `#RRGGBB` or `#AARRGGBB`: the grammar the icon-style
// validator and the panel surface both accept.
[[nodiscard]] bool isValidColor(const QString &value);
// Renderer tier names, taken from the capability resolver so a preset can
// never name a tier the resolver does not know.
[[nodiscard]] QSet<QString> rendererTierNames();

[[nodiscard]] QVariantMap objectMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    QVector<PresetValidationDiagnostic> *diagnostics);
[[nodiscard]] QString stringMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    qsizetype maximumBytes,
    QVector<PresetValidationDiagnostic> *diagnostics);
[[nodiscard]] QString identifierMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    QVector<PresetValidationDiagnostic> *diagnostics);
[[nodiscard]] int integerMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    int fallback,
    int minimum,
    int maximum,
    QVector<PresetValidationDiagnostic> *diagnostics);
[[nodiscard]] bool booleanMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    bool fallback,
    QVector<PresetValidationDiagnostic> *diagnostics);
// An empty vocabulary accepts any string; entries are still type-checked and
// may not repeat.
[[nodiscard]] QStringList stringListMember(
    const QVariantMap &object,
    const QString &key,
    const QString &pointer,
    bool required,
    bool allowEmpty,
    const QSet<QString> &allowedValues,
    QVector<PresetValidationDiagnostic> *diagnostics);

// Reads the `format`, `schemaVersion` and `identity` header every preset
// definition starts with.
[[nodiscard]] PresetIdentity headerMember(
    const QVariantMap &definition,
    const QString &expectedFormat,
    int expectedSchemaVersion,
    QVector<PresetValidationDiagnostic> *diagnostics);

// Validates one panel-settings value against the settings schema. Only
// user-editable schema fields are accepted, which is what keeps host ids,
// ownership tokens and content lists out of every preset. Returns the value
// in its schema type, or nothing when it was rejected.
[[nodiscard]] std::optional<QVariant> strictPanelValue(
    const QString &key,
    const QVariant &value,
    const QString &pointer,
    QVector<PresetValidationDiagnostic> *diagnostics);

}

}
