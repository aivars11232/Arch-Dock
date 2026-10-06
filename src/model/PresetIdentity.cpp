// Preset identity parsing and the strict readers both preset parsers use.
#include "PresetIdentity.h"

#include "PanelCapabilityResolver.h"
#include "PanelSettingsSchema.h"

#include <QMetaType>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

using ArchDock::PresetValidationDiagnostic;

bool containsControlCharacter(const QString &value)
{
    return std::any_of(value.cbegin(), value.cend(), [](QChar character)
    {
        return character.category() == QChar::Other_Control;
    });
}

QString memberKind(const QVariantMap &object, const QString &key)
{
    return object.contains(key) ? QStringLiteral("invalid-type")
                                : QStringLiteral("missing-field");
}

}

namespace ArchDock
{

QVariantMap PresetValidationDiagnostic::toVariantMap() const
{
    return {
        {QStringLiteral("code"), code},
        {QStringLiteral("jsonPointer"), jsonPointer},
        {QStringLiteral("message"), message},
        {QStringLiteral("severity"), severity},
    };
}

QVariantList presetDiagnosticsToVariantList(
    const QVector<PresetValidationDiagnostic> &diagnostics)
{
    QVariantList result;
    result.reserve(diagnostics.size());
    for (const PresetValidationDiagnostic &diagnostic : diagnostics)
    {
        result.append(diagnostic.toVariantMap());
    }
    return result;
}

bool presetDiagnosticsHaveErrors(
    const QVector<PresetValidationDiagnostic> &diagnostics)
{
    return std::any_of(
        diagnostics.cbegin(), diagnostics.cend(),
        [](const PresetValidationDiagnostic &diagnostic)
        {
            return diagnostic.severity == QStringLiteral("error");
        });
}

bool PresetIdentity::isValidId(const QString &id)
{
    // The same version 1 grammar as theme and icon-style ids, anchored to the
    // whole string so a trailing newline can never ride along into a file
    // name.
    static const QRegularExpression pattern(
        QStringLiteral("\\A[a-z0-9][a-z0-9.-]{0,63}\\z"));
    return id.toUtf8().size() <= MaximumIdentifierBytes &&
        pattern.match(id).hasMatch();
}

QString PresetIdentity::userIdPrefix()
{
    return QStringLiteral("user-");
}

bool PresetIdentity::isUserId(const QString &id)
{
    return id.startsWith(userIdPrefix());
}

QVariantMap PresetIdentity::toVariantMap() const
{
    QVariantMap result{
        {QStringLiteral("builtIn"), builtIn},
        {QStringLiteral("description"), description},
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("revision"), revision},
    };
    if (!derivedFromPresetId.isEmpty())
    {
        result.insert(QStringLiteral("derivedFromPresetId"), derivedFromPresetId);
        result.insert(QStringLiteral("sourceRevision"), sourceRevision);
    }
    return result;
}

namespace PresetParsing
{

QString pointerChild(const QString &parent, const QString &child)
{
    return parent + QLatin1Char('/') + child;
}

void addDiagnostic(QVector<PresetValidationDiagnostic> *diagnostics,
                   const QString &code,
                   const QString &pointer,
                   const QString &message,
                   const QString &severity)
{
    if (diagnostics)
    {
        diagnostics->append({code, pointer, severity, message});
    }
}

void rejectUnknownKeys(const QVariantMap &object,
                       const QSet<QString> &allowed,
                       const QString &pointer,
                       QVector<PresetValidationDiagnostic> *diagnostics)
{
    for (auto it = object.cbegin(); it != object.cend(); ++it)
    {
        if (!allowed.contains(it.key()))
        {
            addDiagnostic(diagnostics, QStringLiteral("unknown-field"),
                          pointerChild(pointer, it.key()),
                          QStringLiteral("field is not part of the preset schema"));
        }
    }
}

bool isNumber(const QVariant &value)
{
    switch (value.metaType().id())
    {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    case QMetaType::Double:
    case QMetaType::Float:
        return std::isfinite(value.toDouble());
    default:
        return false;
    }
}

bool isInteger(const QVariant &value)
{
    if (!isNumber(value))
    {
        return false;
    }
    const double number = value.toDouble();
    return number == std::floor(number) &&
        number >= std::numeric_limits<int>::min() &&
        number <= std::numeric_limits<int>::max();
}

bool isValidColor(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral(
        "\\A(?:transparent|#[0-9A-Fa-f]{6}|#[0-9A-Fa-f]{8})\\z"));
    return pattern.match(value).hasMatch();
}

QSet<QString> rendererTierNames()
{
    QSet<QString> names;
    for (int value = 0; value < static_cast<int>(RendererTier::Count); ++value)
    {
        names.insert(rendererTierName(static_cast<RendererTier>(value)));
    }
    return names;
}

QVariantMap objectMember(const QVariantMap &object,
                         const QString &key,
                         const QString &pointer,
                         bool required,
                         QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QVariant value = object.value(key);
    if (value.metaType().id() == QMetaType::QVariantMap)
    {
        return value.toMap();
    }
    if (object.contains(key) || required)
    {
        addDiagnostic(diagnostics, memberKind(object, key),
                      pointerChild(pointer, key),
                      QStringLiteral("value must be an object"));
    }
    return {};
}

QString stringMember(const QVariantMap &object,
                     const QString &key,
                     const QString &pointer,
                     bool required,
                     qsizetype maximumBytes,
                     QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QString memberPointer = pointerChild(pointer, key);
    const QVariant value = object.value(key);
    if (value.metaType().id() != QMetaType::QString)
    {
        if (object.contains(key) || required)
        {
            addDiagnostic(diagnostics, memberKind(object, key), memberPointer,
                          QStringLiteral("value must be a string"));
        }
        return {};
    }
    const QString text = value.toString();
    if (text != text.trimmed() || containsControlCharacter(text))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-value"), memberPointer,
                      QStringLiteral(
                          "text must not have surrounding space or control characters"));
        return {};
    }
    if (text.toUtf8().size() > maximumBytes)
    {
        addDiagnostic(diagnostics, QStringLiteral("limit-exceeded"), memberPointer,
                      QStringLiteral("text exceeds the byte limit"));
        return {};
    }
    if (required && text.isEmpty())
    {
        addDiagnostic(diagnostics, QStringLiteral("missing-field"), memberPointer,
                      QStringLiteral("required text is empty"));
    }
    return text;
}

QString identifierMember(const QVariantMap &object,
                         const QString &key,
                         const QString &pointer,
                         bool required,
                         QVector<PresetValidationDiagnostic> *diagnostics)
{
    const qsizetype before = diagnostics ? diagnostics->size() : 0;
    const QString value = stringMember(
        object, key, pointer, required, PresetIdentity::MaximumIdentifierBytes,
        diagnostics);
    const bool alreadyReported = diagnostics && diagnostics->size() != before;
    if (!alreadyReported && !value.isEmpty() && !PresetIdentity::isValidId(value))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-id"),
                      pointerChild(pointer, key),
                      QStringLiteral("identifier does not match the version 1 grammar"));
        return {};
    }
    return value;
}

int integerMember(const QVariantMap &object,
                  const QString &key,
                  const QString &pointer,
                  bool required,
                  int fallback,
                  int minimum,
                  int maximum,
                  QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QString memberPointer = pointerChild(pointer, key);
    const QVariant value = object.value(key);
    if (!isInteger(value))
    {
        if (object.contains(key) || required)
        {
            addDiagnostic(diagnostics, memberKind(object, key), memberPointer,
                          QStringLiteral("value must be an integer"));
        }
        return fallback;
    }
    const int number = value.toInt();
    if (number < minimum || number > maximum)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-value"), memberPointer,
                      QStringLiteral("integer is outside the allowed range"));
        return fallback;
    }
    return number;
}

bool booleanMember(const QVariantMap &object,
                   const QString &key,
                   const QString &pointer,
                   bool required,
                   bool fallback,
                   QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QVariant value = object.value(key);
    if (value.metaType().id() == QMetaType::Bool)
    {
        return value.toBool();
    }
    if (object.contains(key) || required)
    {
        addDiagnostic(diagnostics, memberKind(object, key),
                      pointerChild(pointer, key),
                      QStringLiteral("value must be a boolean"));
    }
    return fallback;
}

QStringList stringListMember(const QVariantMap &object,
                             const QString &key,
                             const QString &pointer,
                             bool required,
                             bool allowEmpty,
                             const QSet<QString> &allowedValues,
                             QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QString memberPointer = pointerChild(pointer, key);
    const QVariant value = object.value(key);
    const int type = value.metaType().id();
    if (type != QMetaType::QVariantList && type != QMetaType::QStringList)
    {
        if (object.contains(key) || required)
        {
            addDiagnostic(diagnostics, memberKind(object, key), memberPointer,
                          QStringLiteral("value must be an array of strings"));
        }
        return {};
    }

    QStringList result;
    const QVariantList entries = value.toList();
    for (qsizetype index = 0; index < entries.size(); ++index)
    {
        const QString entryPointer = pointerChild(
            memberPointer, QString::number(index));
        if (entries.at(index).metaType().id() != QMetaType::QString)
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-type"), entryPointer,
                          QStringLiteral("array entry must be a string"));
            continue;
        }
        const QString entry = entries.at(index).toString();
        if (!allowedValues.isEmpty() && !allowedValues.contains(entry))
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-enum"), entryPointer,
                          QStringLiteral("value is not part of the allowed vocabulary"));
            continue;
        }
        if (result.contains(entry))
        {
            addDiagnostic(diagnostics, QStringLiteral("duplicate-value"),
                          entryPointer,
                          QStringLiteral("value is listed more than once"));
            continue;
        }
        result.append(entry);
    }
    if (!allowEmpty && entries.isEmpty())
    {
        addDiagnostic(diagnostics, QStringLiteral("missing-field"), memberPointer,
                      QStringLiteral("array must not be empty"));
    }
    return result;
}

PresetIdentity headerMember(const QVariantMap &definition,
                            const QString &expectedFormat,
                            int expectedSchemaVersion,
                            QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QVariant format = definition.value(QStringLiteral("format"));
    if (format.metaType().id() != QMetaType::QString ||
        format.toString() != expectedFormat)
    {
        addDiagnostic(diagnostics, QStringLiteral("unsupported-format"),
                      QStringLiteral("/format"),
                      QStringLiteral("preset definition format is unsupported"));
    }
    const QVariant version = definition.value(QStringLiteral("schemaVersion"));
    if (!isInteger(version) || version.toInt() != expectedSchemaVersion)
    {
        addDiagnostic(diagnostics, QStringLiteral("unsupported-version"),
                      QStringLiteral("/schemaVersion"),
                      QStringLiteral("preset schema version is unsupported"));
    }

    const QString pointer = QStringLiteral("/identity");
    const QVariantMap object = objectMember(
        definition, QStringLiteral("identity"), QString{}, true, diagnostics);
    rejectUnknownKeys(object, {
        QStringLiteral("id"), QStringLiteral("name"),
        QStringLiteral("description"), QStringLiteral("builtIn"),
        QStringLiteral("revision"), QStringLiteral("derivedFromPresetId"),
        QStringLiteral("sourceRevision"),
    }, pointer, diagnostics);

    PresetIdentity identity;
    identity.id = identifierMember(
        object, QStringLiteral("id"), pointer, true, diagnostics);
    identity.name = stringMember(
        object, QStringLiteral("name"), pointer, true,
        PresetIdentity::MaximumNameBytes, diagnostics);
    identity.description = stringMember(
        object, QStringLiteral("description"), pointer, false,
        PresetIdentity::MaximumDescriptionBytes, diagnostics);
    identity.builtIn = booleanMember(
        object, QStringLiteral("builtIn"), pointer, true, false, diagnostics);
    identity.revision = integerMember(
        object, QStringLiteral("revision"), pointer, true, 1, 1,
        std::numeric_limits<int>::max(), diagnostics);
    identity.derivedFromPresetId = identifierMember(
        object, QStringLiteral("derivedFromPresetId"), pointer, false, diagnostics);
    identity.sourceRevision = integerMember(
        object, QStringLiteral("sourceRevision"), pointer, false, 0, 0,
        std::numeric_limits<int>::max(), diagnostics);

    if (!identity.id.isEmpty() &&
        identity.builtIn == PresetIdentity::isUserId(identity.id))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-id"),
                      pointerChild(pointer, QStringLiteral("id")),
                      identity.builtIn
                          ? QStringLiteral(
                                "a built-in preset must not use the user id namespace")
                          : QStringLiteral(
                                "a user preset id must use the user id namespace"));
    }
    if (identity.builtIn && (!identity.derivedFromPresetId.isEmpty() ||
                             identity.sourceRevision != 0))
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-value"),
                      pointerChild(pointer, QStringLiteral("derivedFromPresetId")),
                      QStringLiteral("a built-in preset is an original and has no lineage"));
    }
    if (identity.derivedFromPresetId.isEmpty() && identity.sourceRevision != 0)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-value"),
                      pointerChild(pointer, QStringLiteral("sourceRevision")),
                      QStringLiteral("a source revision needs a source preset id"));
    }
    return identity;
}

std::optional<QVariant> strictPanelValue(
    const QString &key,
    const QVariant &value,
    const QString &pointer,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    const PanelSettingsFieldDescriptor *field =
        PanelSettingsSchema::panelDescriptor(key);
    if (!field || field->access != PanelSettingsFieldAccess::Editor)
    {
        addDiagnostic(diagnostics, QStringLiteral("unknown-field"), pointer,
                      QStringLiteral("field is not a user-editable panel setting"));
        return std::nullopt;
    }

    bool typeMatches = false;
    switch (field->valueType)
    {
    case PanelSettingsValueType::Boolean:
        typeMatches = value.metaType().id() == QMetaType::Bool;
        break;
    case PanelSettingsValueType::Integer:
        typeMatches = isInteger(value);
        break;
    case PanelSettingsValueType::Real:
        typeMatches = isNumber(value);
        break;
    case PanelSettingsValueType::String:
        typeMatches = value.metaType().id() == QMetaType::QString;
        break;
    default:
        break;
    }
    if (!typeMatches)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-type"), pointer,
                      QStringLiteral("value has the wrong type for this panel setting"));
        return std::nullopt;
    }

    // The schema normalizer clamps and substitutes defaults. A preset is only
    // valid when that normalization is the identity, so what a card shows is
    // exactly what the definition declares.
    const QVariant normalized = PanelSettingsSchema::normalizePanelValue(key, value);
    bool unchanged = false;
    QVariant typed;
    switch (field->valueType)
    {
    case PanelSettingsValueType::Boolean:
        typed = value.toBool();
        unchanged = normalized.toBool() == value.toBool();
        break;
    case PanelSettingsValueType::Integer:
        typed = value.toInt();
        unchanged = normalized.toInt() == value.toInt();
        break;
    case PanelSettingsValueType::Real:
        typed = value.toDouble();
        unchanged = std::abs(normalized.toDouble() - value.toDouble()) < 1e-9;
        break;
    default:
        typed = value.toString();
        unchanged = normalized.toString() == value.toString();
        break;
    }
    if (!unchanged)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-value"), pointer,
                      QStringLiteral("value is outside what this panel setting accepts"));
        return std::nullopt;
    }
    return typed;
}

}

}
