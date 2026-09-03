#include "AnimationProfileCatalog.h"

#include "../model/PanelCapabilityResolver.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{

using namespace ArchDock;

const QString catalogFormat =
    QStringLiteral("org.archdock.animation-profile-catalog");
const QString profileFormat = QStringLiteral("org.archdock.animation-profile");

const QSet<QString> easingVocabulary{
    QStringLiteral("linear"),      QStringLiteral("in-quad"),
    QStringLiteral("out-quad"),    QStringLiteral("in-out-quad"),
    QStringLiteral("in-cubic"),    QStringLiteral("out-cubic"),
    QStringLiteral("in-out-cubic"), QStringLiteral("in-sine"),
    QStringLiteral("out-sine"),    QStringLiteral("in-out-sine"),
    QStringLiteral("in-back"),     QStringLiteral("out-back"),
    QStringLiteral("in-out-back"), QStringLiteral("out-bounce"),
    QStringLiteral("out-elastic"),
};

const QSet<QString> directionVocabulary{
    QStringLiteral("normal"),
    QStringLiteral("reverse"),
    QStringLiteral("alternate"),
};

const QSet<QString> blendVocabulary{
    QStringLiteral("replace"),
    QStringLiteral("add"),
};

const QSet<QString> reducedMotionModes{
    QStringLiteral("none"),
    QStringLiteral("static"),
    QStringLiteral("substitute"),
};

AnimationValidationDiagnostic diagnostic(const QString &code,
                                         const QString &pointer,
                                         const QString &message)
{
    return {code, pointer, QStringLiteral("error"), message};
}

bool hasErrors(const QVector<AnimationValidationDiagnostic> &diagnostics)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const AnimationValidationDiagnostic &entry)
                       {
                           return entry.severity == QStringLiteral("error");
                       });
}

QString pointerChild(const QString &parent, const QString &child)
{
    return parent + QLatin1Char('/') + child;
}

bool isFiniteNumber(const QVariant &value)
{
    if (value.typeId() == QMetaType::QString || !value.canConvert<double>())
    {
        return false;
    }
    bool converted = false;
    const double number = value.toDouble(&converted);
    return converted && std::isfinite(number);
}

class ProfileValidator
{
public:
    explicit ProfileValidator(QString pointer)
        : m_pointer(std::move(pointer))
    {
    }

    AnimationProfileDefinition validate(const QVariantMap &profile)
    {
        AnimationProfileDefinition definition;

        rejectUnknown(profile, {
            QStringLiteral("format"), QStringLiteral("version"),
            QStringLiteral("id"), QStringLiteral("name"),
            QStringLiteral("description"), QStringLiteral("author"),
            QStringLiteral("category"), QStringLiteral("target"),
            QStringLiteral("trigger"), QStringLiteral("timing"),
            QStringLiteral("tracks"), QStringLiteral("rendererRequirements"),
            QStringLiteral("reducedMotion"), QStringLiteral("legacyNames"),
            QStringLiteral("extensions"),
        }, m_pointer);

        definition.format = stringValue(profile, QStringLiteral("format"),
                                        m_pointer, true, profileFormat);
        if (!definition.format.isEmpty() && definition.format != profileFormat)
        {
            add(QStringLiteral("invalid-format"),
                pointerChild(m_pointer, QStringLiteral("format")),
                QStringLiteral("profile format is not recognized"));
        }
        definition.version = integerValue(
            profile, QStringLiteral("version"), m_pointer, true,
            AnimationProfileDefinition::CurrentVersion, 1,
            AnimationProfileDefinition::CurrentVersion);
        definition.id = identifier(profile, QStringLiteral("id"), m_pointer);
        definition.name = stringValue(profile, QStringLiteral("name"),
                                      m_pointer, true);
        definition.description = stringValue(
            profile, QStringLiteral("description"), m_pointer, false);
        definition.author = stringValue(profile, QStringLiteral("author"),
                                        m_pointer, false);
        definition.category = stringValue(profile, QStringLiteral("category"),
                                          m_pointer, false);

        definition.target = vocabularyValue(
            profile, QStringLiteral("target"), m_pointer,
            animationTargetVocabulary(), QStringLiteral("invalid-target"),
            QStringLiteral("icon"));
        definition.trigger = vocabularyValue(
            profile, QStringLiteral("trigger"), m_pointer,
            animationTriggerVocabulary(), QStringLiteral("invalid-trigger"),
            QStringLiteral("idle"));

        definition.timing = parseTiming(profile);
        definition.rendererRequirements = parseRendererRequirements(profile);
        definition.reducedMotion = parseReducedMotion(profile);
        definition.tracks = parseTracks(profile, definition.target);
        definition.legacyNames = parseLegacyNames(profile);

        const QVariant extensions = profile.value(QStringLiteral("extensions"));
        if (extensions.isValid() && !extensions.isNull())
        {
            if (extensions.typeId() != QMetaType::QVariantMap)
            {
                add(QStringLiteral("invalid-type"),
                    pointerChild(m_pointer, QStringLiteral("extensions")),
                    QStringLiteral("extensions must be an object"));
            }
            else
            {
                definition.extensions = extensions.toMap();
            }
        }

        validateTrackConflicts(definition);
        return definition;
    }

    QVector<AnimationValidationDiagnostic> diagnostics() const
    {
        return m_diagnostics;
    }

private:
    void add(const QString &code, const QString &pointer,
             const QString &message)
    {
        m_diagnostics.append(diagnostic(code, pointer, message));
    }

    void rejectUnknown(const QVariantMap &object, const QSet<QString> &allowed,
                       const QString &pointer)
    {
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        {
            if (!allowed.contains(it.key()))
            {
                add(QStringLiteral("unknown-field"),
                    pointerChild(pointer, it.key()),
                    QStringLiteral("unknown profile member"));
            }
        }
    }

    QString stringValue(const QVariantMap &object, const QString &key,
                        const QString &pointer, bool required,
                        const QString &defaultValue = {})
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required string is missing"));
            }
            return defaultValue;
        }
        const QVariant value = object.value(key);
        if (value.typeId() != QMetaType::QString)
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be a string"));
            return defaultValue;
        }
        const QString text = value.toString();
        if (text.toUtf8().size() > AnimationProfileCatalog::MaximumStringBytes)
        {
            add(QStringLiteral("string-too-long"), memberPointer,
                QStringLiteral("string exceeds the version 1 size limit"));
            return defaultValue;
        }
        if (required && text.isEmpty())
        {
            add(QStringLiteral("missing-field"), memberPointer,
                QStringLiteral("required string is empty"));
        }
        return text;
    }

    QString identifier(const QVariantMap &object, const QString &key,
                       const QString &pointer)
    {
        const QString value = stringValue(object, key, pointer, true);
        static const QRegularExpression pattern(
            QStringLiteral("^[a-z0-9][a-z0-9.-]{0,63}$"));
        if (!value.isEmpty() &&
            (value.toUtf8().size()
                 > AnimationProfileCatalog::MaximumIdentifierBytes ||
             !pattern.match(value).hasMatch()))
        {
            add(QStringLiteral("invalid-id"), pointerChild(pointer, key),
                QStringLiteral("identifier does not match the version 1 grammar"));
        }
        return value;
    }

    int integerValue(const QVariantMap &object, const QString &key,
                     const QString &pointer, bool required, int defaultValue,
                     int minimum, int maximum)
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required integer is missing"));
            }
            return defaultValue;
        }
        const QVariant value = object.value(key);
        if (!isFiniteNumber(value)
            || std::trunc(value.toDouble()) != value.toDouble())
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be an integer"));
            return defaultValue;
        }
        const int number = value.toInt();
        if (number < minimum || number > maximum)
        {
            add(QStringLiteral("value-out-of-range"), memberPointer,
                QStringLiteral("integer is outside the permitted range"));
            return defaultValue;
        }
        return number;
    }

    qreal realValue(const QVariantMap &object, const QString &key,
                    const QString &pointer, bool required, qreal defaultValue,
                    qreal minimum, qreal maximum)
    {
        const QString memberPointer = pointerChild(pointer, key);
        if (!object.contains(key))
        {
            if (required)
            {
                add(QStringLiteral("missing-field"), memberPointer,
                    QStringLiteral("required number is missing"));
            }
            return defaultValue;
        }
        const QVariant value = object.value(key);
        if (!isFiniteNumber(value))
        {
            add(QStringLiteral("invalid-type"), memberPointer,
                QStringLiteral("value must be a finite number"));
            return defaultValue;
        }
        const qreal number = value.toReal();
        if (number < minimum || number > maximum)
        {
            add(QStringLiteral("value-out-of-range"), memberPointer,
                QStringLiteral("number is outside the permitted range"));
            return defaultValue;
        }
        return number;
    }

    QString vocabularyValue(const QVariantMap &object, const QString &key,
                            const QString &pointer,
                            const QStringList &vocabulary,
                            const QString &code, const QString &defaultValue)
    {
        const QString value = stringValue(object, key, pointer, true,
                                          defaultValue);
        if (!value.isEmpty() && !vocabulary.contains(value))
        {
            add(code, pointerChild(pointer, key),
                QStringLiteral("value is not part of the version 1 vocabulary"));
            return defaultValue;
        }
        return value;
    }

    AnimationTimingDefinition parseTiming(const QVariantMap &profile)
    {
        AnimationTimingDefinition timing;
        const QString pointer = pointerChild(m_pointer,
                                             QStringLiteral("timing"));
        if (!profile.contains(QStringLiteral("timing")))
        {
            add(QStringLiteral("missing-field"), pointer,
                QStringLiteral("required timing object is missing"));
            return timing;
        }
        const QVariant value = profile.value(QStringLiteral("timing"));
        if (value.typeId() != QMetaType::QVariantMap)
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("timing must be an object"));
            return timing;
        }
        const QVariantMap object = value.toMap();
        rejectUnknown(object, {
            QStringLiteral("baseDuration"), QStringLiteral("speedScale"),
            QStringLiteral("startDelay"),
        }, pointer);
        timing.baseDuration = integerValue(
            object, QStringLiteral("baseDuration"), pointer, true, 170, 0,
            AnimationProfileCatalog::MaximumDurationMs);
        timing.speedScale = realValue(
            object, QStringLiteral("speedScale"), pointer, false, 1.0,
            AnimationProfileCatalog::MinimumSpeedScale,
            AnimationProfileCatalog::MaximumSpeedScale);
        timing.startDelay = integerValue(
            object, QStringLiteral("startDelay"), pointer, false, 0, 0,
            AnimationProfileCatalog::MaximumDurationMs);
        return timing;
    }

    QStringList parseRendererRequirements(const QVariantMap &profile)
    {
        QStringList requirements;
        const QString pointer = pointerChild(
            m_pointer, QStringLiteral("rendererRequirements"));
        if (!profile.contains(QStringLiteral("rendererRequirements")))
        {
            add(QStringLiteral("missing-field"), pointer,
                QStringLiteral("required renderer requirement list is missing"));
            return requirements;
        }
        const QVariant value =
            profile.value(QStringLiteral("rendererRequirements"));
        if (value.typeId() != QMetaType::QVariantList
            && value.typeId() != QMetaType::QStringList)
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("renderer requirements must be an array"));
            return requirements;
        }
        const QVariantList entries = value.toList();
        if (entries.isEmpty())
        {
            add(QStringLiteral("missing-field"), pointer,
                QStringLiteral("at least one renderer tier is required"));
            return requirements;
        }
        for (qsizetype index = 0; index < entries.size(); ++index)
        {
            const QString entryPointer =
                pointer + QLatin1Char('/') + QString::number(index);
            const QVariant entry = entries.at(index);
            if (entry.typeId() != QMetaType::QString)
            {
                add(QStringLiteral("invalid-type"), entryPointer,
                    QStringLiteral("renderer tier must be a string"));
                continue;
            }
            const QString tier = entry.toString();
            // One authoritative tier vocabulary, shared with the capability
            // resolver, so a profile cannot require a tier that cannot exist.
            if (!rendererTierFromName(tier).has_value())
            {
                add(QStringLiteral("unsupported-renderer"), entryPointer,
                    QStringLiteral("renderer tier is not a known tier"));
                continue;
            }
            if (requirements.contains(tier))
            {
                add(QStringLiteral("duplicate-renderer"), entryPointer,
                    QStringLiteral("renderer tier is listed twice"));
                continue;
            }
            requirements.append(tier);
        }
        return requirements;
    }

    AnimationReducedMotionDefinition parseReducedMotion(
        const QVariantMap &profile)
    {
        AnimationReducedMotionDefinition reduced;
        const QString pointer = pointerChild(
            m_pointer, QStringLiteral("reducedMotion"));
        if (!profile.contains(QStringLiteral("reducedMotion")))
        {
            add(QStringLiteral("missing-reduced-motion"), pointer,
                QStringLiteral("every profile must declare a reduced-motion substitute"));
            return reduced;
        }
        const QVariant value = profile.value(QStringLiteral("reducedMotion"));
        if (value.typeId() != QMetaType::QVariantMap)
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("reducedMotion must be an object"));
            return reduced;
        }
        const QVariantMap object = value.toMap();
        rejectUnknown(object, {
            QStringLiteral("mode"), QStringLiteral("substituteProfileId"),
            QStringLiteral("property"), QStringLiteral("value"),
            QStringLiteral("color"),
        }, pointer);

        reduced.mode = stringValue(object, QStringLiteral("mode"), pointer,
                                   true);
        if (!reduced.mode.isEmpty() && !reducedMotionModes.contains(reduced.mode))
        {
            add(QStringLiteral("invalid-reduced-motion"),
                pointerChild(pointer, QStringLiteral("mode")),
                QStringLiteral("reduced-motion mode is not part of the vocabulary"));
            reduced.mode.clear();
            return reduced;
        }

        if (reduced.mode == QStringLiteral("substitute"))
        {
            reduced.substituteProfileId = identifier(
                object, QStringLiteral("substituteProfileId"), pointer);
        }
        else if (object.contains(QStringLiteral("substituteProfileId")))
        {
            add(QStringLiteral("invalid-reduced-motion"),
                pointerChild(pointer, QStringLiteral("substituteProfileId")),
                QStringLiteral("substitute id is only valid in substitute mode"));
        }

        if (reduced.mode == QStringLiteral("static"))
        {
            reduced.property = vocabularyValue(
                object, QStringLiteral("property"), pointer,
                animationPropertyVocabulary(),
                QStringLiteral("invalid-property"), QString{});
            if (animationPropertyIsColor(reduced.property))
            {
                reduced.color = stringValue(object, QStringLiteral("color"),
                                            pointer, true);
                validateColor(reduced.color,
                              pointerChild(pointer, QStringLiteral("color")));
            }
            else if (!reduced.property.isEmpty())
            {
                const std::optional<AnimationPropertyRange> range =
                    animationPropertyRange(reduced.property);
                reduced.value = realValue(
                    object, QStringLiteral("value"), pointer, true, 0.0,
                    range ? range->minimum : 0.0,
                    range ? range->maximum : 0.0);
            }
        }
        return reduced;
    }

    void validateColor(const QString &color, const QString &pointer)
    {
        if (color.isEmpty())
        {
            return;
        }
        if (!QColor::isValidColorName(color))
        {
            add(QStringLiteral("invalid-color"), pointer,
                QStringLiteral("value is not a valid color"));
        }
    }

    QVector<AnimationTrackDefinition> parseTracks(const QVariantMap &profile,
                                                  const QString &profileTarget)
    {
        QVector<AnimationTrackDefinition> tracks;
        const QString pointer = pointerChild(m_pointer,
                                             QStringLiteral("tracks"));
        if (!profile.contains(QStringLiteral("tracks")))
        {
            add(QStringLiteral("missing-field"), pointer,
                QStringLiteral("required track array is missing"));
            return tracks;
        }
        const QVariant value = profile.value(QStringLiteral("tracks"));
        if (value.typeId() != QMetaType::QVariantList)
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("tracks must be an array"));
            return tracks;
        }
        const QVariantList entries = value.toList();
        if (entries.size() > AnimationProfileCatalog::MaximumTracksPerProfile)
        {
            add(QStringLiteral("too-many-tracks"), pointer,
                QStringLiteral("profile exceeds the version 1 track limit"));
            return tracks;
        }
        QStringList seenIds;
        for (qsizetype index = 0; index < entries.size(); ++index)
        {
            const QString trackPointer =
                pointer + QLatin1Char('/') + QString::number(index);
            const QVariant entry = entries.at(index);
            if (entry.typeId() != QMetaType::QVariantMap)
            {
                add(QStringLiteral("invalid-type"), trackPointer,
                    QStringLiteral("track must be an object"));
                continue;
            }
            const AnimationTrackDefinition track =
                parseTrack(entry.toMap(), trackPointer, profileTarget);
            if (!track.id.isEmpty())
            {
                if (seenIds.contains(track.id))
                {
                    add(QStringLiteral("duplicate-track"), trackPointer,
                        QStringLiteral("track id is used more than once"));
                    continue;
                }
                seenIds.append(track.id);
            }
            tracks.append(track);
        }
        return tracks;
    }

    AnimationTrackDefinition parseTrack(const QVariantMap &object,
                                        const QString &pointer,
                                        const QString &profileTarget)
    {
        AnimationTrackDefinition track;
        rejectUnknown(object, {
            QStringLiteral("id"), QStringLiteral("target"),
            QStringLiteral("property"), QStringLiteral("from"),
            QStringLiteral("to"), QStringLiteral("fromColor"),
            QStringLiteral("toColor"), QStringLiteral("easing"),
            QStringLiteral("duration"), QStringLiteral("delay"),
            QStringLiteral("phase"), QStringLiteral("direction"),
            QStringLiteral("repeat"), QStringLiteral("intensityScale"),
            QStringLiteral("blend"), QStringLiteral("priority"),
        }, pointer);

        track.id = identifier(object, QStringLiteral("id"), pointer);
        if (object.contains(QStringLiteral("target")))
        {
            track.target = vocabularyValue(
                object, QStringLiteral("target"), pointer,
                animationTargetVocabulary(),
                QStringLiteral("invalid-target"), QString{});
        }
        track.property = vocabularyValue(
            object, QStringLiteral("property"), pointer,
            animationPropertyVocabulary(),
            QStringLiteral("invalid-property"), QString{});

        if (animationPropertyIsColor(track.property))
        {
            track.fromColor = stringValue(object, QStringLiteral("fromColor"),
                                          pointer, true);
            track.toColor = stringValue(object, QStringLiteral("toColor"),
                                        pointer, true);
            validateColor(track.fromColor,
                          pointerChild(pointer, QStringLiteral("fromColor")));
            validateColor(track.toColor,
                          pointerChild(pointer, QStringLiteral("toColor")));
            if (object.contains(QStringLiteral("from"))
                || object.contains(QStringLiteral("to")))
            {
                add(QStringLiteral("invalid-property"), pointer,
                    QStringLiteral("color track must use fromColor and toColor"));
            }
        }
        else if (!track.property.isEmpty())
        {
            const std::optional<AnimationPropertyRange> range =
                animationPropertyRange(track.property);
            const qreal minimum = range ? range->minimum : 0.0;
            const qreal maximum = range ? range->maximum : 0.0;
            track.from = realValue(object, QStringLiteral("from"), pointer,
                                   true, 0.0, minimum, maximum);
            track.to = realValue(object, QStringLiteral("to"), pointer, true,
                                 0.0, minimum, maximum);
            if (object.contains(QStringLiteral("fromColor"))
                || object.contains(QStringLiteral("toColor")))
            {
                add(QStringLiteral("invalid-property"), pointer,
                    QStringLiteral("numeric track must not use color endpoints"));
            }
        }

        track.easing = stringValue(object, QStringLiteral("easing"), pointer,
                                   false, QStringLiteral("linear"));
        if (!track.easing.isEmpty() && !easingVocabulary.contains(track.easing))
        {
            add(QStringLiteral("invalid-easing"),
                pointerChild(pointer, QStringLiteral("easing")),
                QStringLiteral("easing is not part of the version 1 vocabulary"));
            track.easing = QStringLiteral("linear");
        }

        track.duration = integerValue(
            object, QStringLiteral("duration"), pointer, true, 0, 0,
            AnimationProfileCatalog::MaximumDurationMs);
        track.delay = integerValue(
            object, QStringLiteral("delay"), pointer, false, 0, 0,
            AnimationProfileCatalog::MaximumDurationMs);
        track.phase = integerValue(
            object, QStringLiteral("phase"), pointer, false, 0, 0,
            AnimationProfileCatalog::MaximumDurationMs);

        track.direction = stringValue(object, QStringLiteral("direction"),
                                      pointer, false,
                                      QStringLiteral("normal"));
        if (!track.direction.isEmpty()
            && !directionVocabulary.contains(track.direction))
        {
            add(QStringLiteral("invalid-direction"),
                pointerChild(pointer, QStringLiteral("direction")),
                QStringLiteral("direction is not part of the version 1 vocabulary"));
            track.direction = QStringLiteral("normal");
        }

        track.repeat = integerValue(
            object, QStringLiteral("repeat"), pointer, false, 1, -1,
            AnimationProfileCatalog::MaximumRepeatCount);
        if (track.repeat == 0)
        {
            add(QStringLiteral("value-out-of-range"),
                pointerChild(pointer, QStringLiteral("repeat")),
                QStringLiteral("repeat must be -1 for continuous or at least 1"));
            track.repeat = 1;
        }

        track.intensityScale = realValue(
            object, QStringLiteral("intensityScale"), pointer, false, 1.0, 0.0,
            AnimationProfileCatalog::MaximumIntensityScale);

        track.blend = stringValue(object, QStringLiteral("blend"), pointer,
                                  false, QStringLiteral("replace"));
        if (!track.blend.isEmpty() && !blendVocabulary.contains(track.blend))
        {
            add(QStringLiteral("invalid-blend"),
                pointerChild(pointer, QStringLiteral("blend")),
                QStringLiteral("blend is not part of the version 1 vocabulary"));
            track.blend = QStringLiteral("replace");
        }

        track.priority = integerValue(
            object, QStringLiteral("priority"), pointer, false, 0, 0,
            AnimationProfileCatalog::MaximumPriority);

        if (track.effectiveTarget(profileTarget).isEmpty())
        {
            add(QStringLiteral("invalid-target"), pointer,
                QStringLiteral("track has no resolvable target"));
        }
        return track;
    }

    // Two tracks may share a property on one target only when the result is
    // deterministic: distinct priorities pick a winner, and additive blending
    // sums without ordering ambiguity. Anything else is a write race.
    void validateTrackConflicts(const AnimationProfileDefinition &definition)
    {
        const QString pointer = pointerChild(m_pointer,
                                             QStringLiteral("tracks"));
        for (qsizetype outer = 0; outer < definition.tracks.size(); ++outer)
        {
            const AnimationTrackDefinition &first = definition.tracks.at(outer);
            if (first.property.isEmpty())
            {
                continue;
            }
            for (qsizetype inner = outer + 1; inner < definition.tracks.size();
                 ++inner)
            {
                const AnimationTrackDefinition &second =
                    definition.tracks.at(inner);
                if (second.property.isEmpty()
                    || first.property != second.property
                    || first.effectiveTarget(definition.target)
                           != second.effectiveTarget(definition.target))
                {
                    continue;
                }
                const bool additive = first.blend == QStringLiteral("add")
                    && second.blend == QStringLiteral("add");
                if (additive || first.priority != second.priority)
                {
                    continue;
                }
                add(QStringLiteral("track-conflict"),
                    pointer + QLatin1Char('/') + QString::number(inner),
                    QStringLiteral("two tracks write the same property on the "
                                   "same target without a resolvable priority "
                                   "or additive blend"));
            }
        }
    }

    QStringList parseLegacyNames(const QVariantMap &profile)
    {
        QStringList names;
        const QString pointer = pointerChild(m_pointer,
                                             QStringLiteral("legacyNames"));
        if (!profile.contains(QStringLiteral("legacyNames")))
        {
            return names;
        }
        const QVariant value = profile.value(QStringLiteral("legacyNames"));
        if (value.typeId() != QMetaType::QVariantList
            && value.typeId() != QMetaType::QStringList)
        {
            add(QStringLiteral("invalid-type"), pointer,
                QStringLiteral("legacy names must be an array"));
            return names;
        }
        const QVariantList entries = value.toList();
        for (qsizetype index = 0; index < entries.size(); ++index)
        {
            const QString entryPointer =
                pointer + QLatin1Char('/') + QString::number(index);
            const QVariant entry = entries.at(index);
            if (entry.typeId() != QMetaType::QString
                || entry.toString().isEmpty())
            {
                add(QStringLiteral("invalid-type"), entryPointer,
                    QStringLiteral("legacy name must be a non-empty string"));
                continue;
            }
            const QString name = entry.toString();
            if (names.contains(name))
            {
                add(QStringLiteral("duplicate-legacy-name"), entryPointer,
                    QStringLiteral("legacy name is listed twice"));
                continue;
            }
            names.append(name);
        }
        return names;
    }

    QString m_pointer;
    QVector<AnimationValidationDiagnostic> m_diagnostics;
};

}

namespace ArchDock
{

QVector<AnimationValidationDiagnostic> AnimationProfileCatalog::validateProfile(
    const QVariantMap &profile,
    AnimationProfileDefinition *definition)
{
    ProfileValidator validator(QString{});
    const AnimationProfileDefinition parsed = validator.validate(profile);
    if (definition != nullptr)
    {
        *definition = parsed;
    }
    return validator.diagnostics();
}

AnimationProfileCatalogLoadResult AnimationProfileCatalog::loadCatalog(
    const QString &catalogPath)
{
    AnimationProfileCatalogLoadResult result;
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("catalog-unreadable"), QString{},
            QStringLiteral("animation profile catalog cannot be opened")));
        return result;
    }
    if (file.size() > MaximumCatalogBytes)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("catalog-too-large"), QString{},
            QStringLiteral("animation profile catalog exceeds the size limit")));
        return result;
    }
    return loadCatalogBytes(file.readAll(), catalogPath);
}

AnimationProfileCatalogLoadResult AnimationProfileCatalog::loadCatalogBytes(
    const QByteArray &catalogBytes,
    const QString &catalogPath)
{
    AnimationProfileCatalogLoadResult result;
    if (catalogBytes.size() > MaximumCatalogBytes)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("catalog-too-large"), QString{},
            QStringLiteral("animation profile catalog exceeds the size limit")));
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(catalogBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-json"), QString{},
            QStringLiteral("animation profile catalog is not a JSON object")));
        return result;
    }

    const QJsonObject root = document.object();
    AnimationProfileCatalog catalog;
    catalog.m_catalogPath = catalogPath;

    if (root.value(QStringLiteral("format")).toString() != catalogFormat)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-format"), QStringLiteral("/format"),
            QStringLiteral("catalog format is not recognized")));
    }
    if (root.value(QStringLiteral("version")).toInt()
        != AnimationProfileDefinition::CurrentVersion)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unsupported-version"), QStringLiteral("/version"),
            QStringLiteral("catalog version is not supported")));
    }
    const QString fallback =
        root.value(QStringLiteral("fallbackProfileId")).toString();
    if (fallback.isEmpty())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("missing-field"),
            QStringLiteral("/fallbackProfileId"),
            QStringLiteral("catalog must declare a fallback profile")));
    }
    catalog.m_fallbackProfileId = fallback;

    const QJsonValue profilesValue =
        root.value(QStringLiteral("animationProfiles"));
    if (!profilesValue.isArray())
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("invalid-type"),
            QStringLiteral("/animationProfiles"),
            QStringLiteral("animationProfiles must be an array")));
        return result;
    }
    const QJsonArray profiles = profilesValue.toArray();
    if (profiles.size() > MaximumProfiles)
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("too-many-profiles"),
            QStringLiteral("/animationProfiles"),
            QStringLiteral("catalog exceeds the version 1 profile limit")));
        return result;
    }

    for (qsizetype index = 0; index < profiles.size(); ++index)
    {
        const QString pointer = QStringLiteral("/animationProfiles/")
            + QString::number(index);
        if (!profiles.at(index).isObject())
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-type"), pointer,
                QStringLiteral("animation profile must be an object")));
            continue;
        }
        ProfileValidator validator(pointer);
        const AnimationProfileDefinition definition =
            validator.validate(profiles.at(index).toObject().toVariantMap());
        result.diagnostics.append(validator.diagnostics());
        if (definition.id.isEmpty())
        {
            continue;
        }
        if (catalog.m_profiles.contains(definition.id))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("duplicate-profile"), pointer,
                QStringLiteral("profile id is declared more than once")));
            continue;
        }
        for (const QString &legacyName : definition.legacyNames)
        {
            if (catalog.m_legacyNames.contains(legacyName))
            {
                result.diagnostics.append(diagnostic(
                    QStringLiteral("duplicate-legacy-name"), pointer,
                    QStringLiteral("legacy name is claimed by two profiles")));
                continue;
            }
            catalog.m_legacyNames.insert(legacyName, definition.id);
        }
        catalog.m_profileOrder.append(definition.id);
        catalog.m_profiles.insert(definition.id, definition);
    }

    // A substitute must exist, or reduced motion would fall off a cliff.
    for (const QString &profileId : std::as_const(catalog.m_profileOrder))
    {
        const AnimationProfileDefinition &definition =
            catalog.m_profiles[profileId];
        if (definition.reducedMotion.mode != QStringLiteral("substitute"))
        {
            continue;
        }
        const QString substitute = definition.reducedMotion.substituteProfileId;
        if (!catalog.m_profiles.contains(substitute))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("unknown-substitute"),
                QStringLiteral("/animationProfiles"),
                QStringLiteral("reduced-motion substitute profile is missing")));
        }
        else if (catalog.m_profiles[substitute].reducedMotion.mode
                 == QStringLiteral("substitute"))
        {
            result.diagnostics.append(diagnostic(
                QStringLiteral("invalid-reduced-motion"),
                QStringLiteral("/animationProfiles"),
                QStringLiteral("reduced-motion substitute must not itself "
                               "substitute")));
        }
    }

    if (!fallback.isEmpty() && !catalog.m_profiles.contains(fallback))
    {
        result.diagnostics.append(diagnostic(
            QStringLiteral("unknown-fallback"),
            QStringLiteral("/fallbackProfileId"),
            QStringLiteral("fallback profile is not present in the catalog")));
    }

    if (hasErrors(result.diagnostics))
    {
        return result;
    }
    result.catalog = std::move(catalog);
    return result;
}

bool AnimationProfileCatalog::contains(const QString &profileId) const
{
    return m_profiles.contains(profileId);
}

QStringList AnimationProfileCatalog::profileIds() const
{
    return m_profileOrder;
}

QString AnimationProfileCatalog::fallbackProfileId() const
{
    return m_fallbackProfileId;
}

const AnimationProfileDefinition *AnimationProfileCatalog::profileById(
    const QString &profileId) const
{
    const auto match = m_profiles.constFind(profileId);
    if (match == m_profiles.constEnd())
    {
        return nullptr;
    }
    return &match.value();
}

QString AnimationProfileCatalog::profileIdForLegacyName(
    const QString &legacyName) const
{
    if (m_profiles.contains(legacyName))
    {
        return legacyName;
    }
    return m_legacyNames.value(legacyName);
}

QMap<QString, QString> AnimationProfileCatalog::legacyNameMap() const
{
    return m_legacyNames;
}

QVariantList AnimationProfileCatalog::profileProjections() const
{
    QVariantList result;
    result.reserve(m_profileOrder.size());
    for (const QString &profileId : m_profileOrder)
    {
        result.append(m_profiles.value(profileId).toRuntimeProjection());
    }
    return result;
}

QVariantMap AnimationProfileCatalog::resolve(
    const QString &requestedProfileId) const
{
    const QString resolvedId = profileIdForLegacyName(requestedProfileId);
    const bool matched = !resolvedId.isEmpty()
        && m_profiles.contains(resolvedId);
    const QString effectiveId = matched ? resolvedId : m_fallbackProfileId;
    const auto match = m_profiles.constFind(effectiveId);
    if (match == m_profiles.constEnd())
    {
        return {
            {QStringLiteral("fallbackApplied"), true},
            {QStringLiteral("profile"), QVariantMap{}},
            {QStringLiteral("profileId"), QString{}},
            {QStringLiteral("requestedProfileId"), requestedProfileId},
            {QStringLiteral("resolved"), false},
        };
    }
    return {
        {QStringLiteral("fallbackApplied"), !matched},
        {QStringLiteral("profile"), match.value().toRuntimeProjection()},
        {QStringLiteral("profileId"), effectiveId},
        {QStringLiteral("requestedProfileId"), requestedProfileId},
        {QStringLiteral("resolved"), true},
    };
}

QVariantMap AnimationProfileCatalog::toVariantMap() const
{
    QVariantMap legacy;
    for (auto it = m_legacyNames.constBegin(); it != m_legacyNames.constEnd();
         ++it)
    {
        legacy.insert(it.key(), it.value());
    }
    return {
        {QStringLiteral("animationProfiles"), profileProjections()},
        {QStringLiteral("catalogPath"), m_catalogPath},
        {QStringLiteral("fallbackProfileId"), m_fallbackProfileId},
        {QStringLiteral("format"), catalogFormat},
        {QStringLiteral("legacyNames"), legacy},
        {QStringLiteral("profileIds"), m_profileOrder},
        {QStringLiteral("version"), AnimationProfileDefinition::CurrentVersion},
    };
}

bool AnimationProfileCatalogLoadResult::isValid() const
{
    return catalog.has_value();
}

QString AnimationProfileCatalogLoadResult::primaryCode() const
{
    for (const AnimationValidationDiagnostic &entry : diagnostics)
    {
        if (entry.severity == QStringLiteral("error"))
        {
            return entry.code;
        }
    }
    return QString{};
}

QString AnimationProfileCatalogLoadResult::primaryMessage() const
{
    for (const AnimationValidationDiagnostic &entry : diagnostics)
    {
        if (entry.severity == QStringLiteral("error"))
        {
            return entry.message;
        }
    }
    return QString{};
}

QVariantMap AnimationProfileCatalogLoadResult::toVariantMap() const
{
    return {
        {QStringLiteral("catalog"),
         catalog.has_value() ? catalog->toVariantMap() : QVariantMap{}},
        {QStringLiteral("diagnostics"),
         animationDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("primaryCode"), primaryCode()},
        {QStringLiteral("primaryMessage"), primaryMessage()},
        {QStringLiteral("valid"), isValid()},
    };
}

}
