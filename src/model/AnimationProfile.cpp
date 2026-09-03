#include "AnimationProfile.h"

#include <QHash>

namespace ArchDock
{

QVariantMap AnimationValidationDiagnostic::toVariantMap() const
{
    return {
        {QStringLiteral("code"), code},
        {QStringLiteral("jsonPointer"), jsonPointer},
        {QStringLiteral("message"), message},
        {QStringLiteral("severity"), severity},
    };
}

QVariantList animationDiagnosticsToVariantList(
    const QVector<AnimationValidationDiagnostic> &diagnostics)
{
    QVariantList result;
    result.reserve(diagnostics.size());
    for (const AnimationValidationDiagnostic &diagnostic : diagnostics)
    {
        result.append(diagnostic.toVariantMap());
    }
    return result;
}

const QStringList &animationTargetVocabulary()
{
    static const QStringList targets = {
        QStringLiteral("glyph"),
        QStringLiteral("tile"),
        QStringLiteral("icon"),
        QStringLiteral("indicator"),
        QStringLiteral("badge"),
        QStringLiteral("panel-surface"),
        QStringLiteral("panel-glow"),
        QStringLiteral("panel-content"),
        QStringLiteral("panel-segment"),
        QStringLiteral("free-scene"),
        QStringLiteral("window-preview"),
    };
    return targets;
}

const QStringList &animationTriggerVocabulary()
{
    static const QStringList triggers = {
        QStringLiteral("idle"),
        QStringLiteral("hover-enter"),
        QStringLiteral("hover-hold"),
        QStringLiteral("hover-exit"),
        QStringLiteral("press"),
        QStringLiteral("click"),
        QStringLiteral("launch-requested"),
        QStringLiteral("launch-succeeded"),
        QStringLiteral("launch-failed"),
        QStringLiteral("running-started"),
        QStringLiteral("running-stopped"),
        QStringLiteral("urgent"),
        QStringLiteral("drop-entered"),
        QStringLiteral("drop-committed"),
        QStringLiteral("panel-reveal"),
        QStringLiteral("panel-conceal"),
        QStringLiteral("panel-open"),
        QStringLiteral("panel-collapse"),
        QStringLiteral("profile-changed"),
        QStringLiteral("command"),
    };
    return triggers;
}

const QStringList &animationPropertyVocabulary()
{
    static const QStringList properties = {
        QStringLiteral("translate-x"),
        QStringLiteral("translate-y"),
        QStringLiteral("translate-z"),
        QStringLiteral("translate-tangent"),
        QStringLiteral("translate-normal"),
        QStringLiteral("scale"),
        QStringLiteral("scale-x"),
        QStringLiteral("scale-y"),
        QStringLiteral("rotate-x"),
        QStringLiteral("rotate-y"),
        QStringLiteral("rotate-z"),
        QStringLiteral("orbit"),
        QStringLiteral("spiral"),
        QStringLiteral("opacity"),
        QStringLiteral("glow"),
        QStringLiteral("tint"),
        QStringLiteral("blur"),
        QStringLiteral("shadow-offset"),
        QStringLiteral("shadow-intensity"),
        QStringLiteral("material-reflection"),
        QStringLiteral("path-rotation"),
        QStringLiteral("clip-open"),
        QStringLiteral("part-offset"),
    };
    return properties;
}

bool animationPropertyIsColor(const QString &property)
{
    return property == QStringLiteral("tint");
}

std::optional<AnimationPropertyRange> animationPropertyRange(
    const QString &property)
{
    static const QHash<QString, AnimationPropertyRange> ranges = {
        {QStringLiteral("translate-x"), {-10000.0, 10000.0}},
        {QStringLiteral("translate-y"), {-10000.0, 10000.0}},
        {QStringLiteral("translate-z"), {-10000.0, 10000.0}},
        {QStringLiteral("translate-tangent"), {-10000.0, 10000.0}},
        {QStringLiteral("translate-normal"), {-10000.0, 10000.0}},
        {QStringLiteral("scale"), {0.0, 16.0}},
        {QStringLiteral("scale-x"), {0.0, 16.0}},
        {QStringLiteral("scale-y"), {0.0, 16.0}},
        {QStringLiteral("rotate-x"), {-3600.0, 3600.0}},
        {QStringLiteral("rotate-y"), {-3600.0, 3600.0}},
        {QStringLiteral("rotate-z"), {-3600.0, 3600.0}},
        {QStringLiteral("orbit"), {-3600.0, 3600.0}},
        {QStringLiteral("spiral"), {-3600.0, 3600.0}},
        {QStringLiteral("opacity"), {0.0, 1.0}},
        {QStringLiteral("glow"), {0.0, 1.0}},
        {QStringLiteral("blur"), {0.0, 1.0}},
        {QStringLiteral("shadow-offset"), {-1000.0, 1000.0}},
        {QStringLiteral("shadow-intensity"), {0.0, 1.0}},
        {QStringLiteral("material-reflection"), {0.0, 1.0}},
        {QStringLiteral("path-rotation"), {-3600.0, 3600.0}},
        {QStringLiteral("clip-open"), {0.0, 1.0}},
        {QStringLiteral("part-offset"), {-10000.0, 10000.0}},
    };
    const auto match = ranges.constFind(property);
    if (match == ranges.constEnd())
    {
        return std::nullopt;
    }
    return *match;
}

QString AnimationTrackDefinition::effectiveTarget(
    const QString &profileTarget) const
{
    return target.isEmpty() ? profileTarget : target;
}

QVariantMap AnimationTrackDefinition::toVariantMap() const
{
    QVariantMap result = {
        {QStringLiteral("blend"), blend},
        {QStringLiteral("delay"), delay},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("duration"), duration},
        {QStringLiteral("id"), id},
        {QStringLiteral("intensityScale"), intensityScale},
        {QStringLiteral("phase"), phase},
        {QStringLiteral("priority"), priority},
        {QStringLiteral("property"), property},
        {QStringLiteral("repeat"), repeat},
        {QStringLiteral("target"), target},
    };
    if (animationPropertyIsColor(property))
    {
        result.insert(QStringLiteral("fromColor"), fromColor);
        result.insert(QStringLiteral("toColor"), toColor);
    }
    else
    {
        result.insert(QStringLiteral("from"), from);
        result.insert(QStringLiteral("to"), to);
    }
    result.insert(QStringLiteral("easing"), easing);
    return result;
}

QVariantMap AnimationTimingDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("baseDuration"), baseDuration},
        {QStringLiteral("speedScale"), speedScale},
        {QStringLiteral("startDelay"), startDelay},
    };
}

QVariantMap AnimationReducedMotionDefinition::toVariantMap() const
{
    return {
        {QStringLiteral("color"), color},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("property"), property},
        {QStringLiteral("substituteProfileId"), substituteProfileId},
        {QStringLiteral("value"), value},
    };
}

const AnimationTrackDefinition *AnimationProfileDefinition::trackById(
    const QString &trackId) const
{
    for (const AnimationTrackDefinition &track : tracks)
    {
        if (track.id == trackId)
        {
            return &track;
        }
    }
    return nullptr;
}

QStringList AnimationProfileDefinition::effectiveTargets() const
{
    QStringList result;
    for (const AnimationTrackDefinition &track : tracks)
    {
        const QString resolved = track.effectiveTarget(target);
        if (!resolved.isEmpty() && !result.contains(resolved))
        {
            result.append(resolved);
        }
    }
    if (result.isEmpty() && !target.isEmpty())
    {
        result.append(target);
    }
    return result;
}

bool AnimationProfileDefinition::isContinuous() const
{
    for (const AnimationTrackDefinition &track : tracks)
    {
        if (track.repeat < 0)
        {
            return true;
        }
    }
    return false;
}

int AnimationProfileDefinition::totalDuration() const
{
    if (isContinuous())
    {
        return -1;
    }
    int longest = 0;
    for (const AnimationTrackDefinition &track : tracks)
    {
        const int cycles = track.repeat > 0 ? track.repeat : 1;
        const int span = track.delay + track.duration * cycles;
        longest = std::max(longest, span);
    }
    return timing.startDelay + longest;
}

QVariantMap AnimationProfileDefinition::toVariantMap() const
{
    QVariantList serializedTracks;
    serializedTracks.reserve(tracks.size());
    for (const AnimationTrackDefinition &track : tracks)
    {
        serializedTracks.append(track.toVariantMap());
    }
    return {
        {QStringLiteral("author"), author},
        {QStringLiteral("category"), category},
        {QStringLiteral("description"), description},
        {QStringLiteral("extensions"), extensions},
        {QStringLiteral("format"), format},
        {QStringLiteral("id"), id},
        {QStringLiteral("legacyNames"), legacyNames},
        {QStringLiteral("name"), name},
        {QStringLiteral("reducedMotion"), reducedMotion.toVariantMap()},
        {QStringLiteral("rendererRequirements"), rendererRequirements},
        {QStringLiteral("target"), target},
        {QStringLiteral("timing"), timing.toVariantMap()},
        {QStringLiteral("tracks"), serializedTracks},
        {QStringLiteral("trigger"), trigger},
        {QStringLiteral("version"), version},
    };
}

QVariantMap AnimationProfileDefinition::toRuntimeProjection(
    const QString &validationStatus,
    const QVector<AnimationValidationDiagnostic> &diagnostics) const
{
    QVariantMap result = toVariantMap();
    const bool valid = validationStatus == QStringLiteral("valid");
    result.insert(QStringLiteral("continuous"), isContinuous());
    result.insert(QStringLiteral("diagnostics"),
                  animationDiagnosticsToVariantList(diagnostics));
    result.insert(QStringLiteral("effectiveTargets"), effectiveTargets());
    result.insert(QStringLiteral("totalDuration"), totalDuration());
    result.insert(QStringLiteral("validationStatus"), validationStatus);
    result.insert(QStringLiteral("valid"), valid);
    return result;
}

}
