#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct AnimationValidationDiagnostic
{
    QString code;
    QString jsonPointer;
    QString severity = QStringLiteral("error");
    QString message;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const AnimationValidationDiagnostic &) const = default;
};

[[nodiscard]] QVariantList animationDiagnosticsToVariantList(
    const QVector<AnimationValidationDiagnostic> &diagnostics);

// Motion targets from master plan 14.2. A profile names the layer it moves so
// a glyph can turn while its pedestal stays still.
[[nodiscard]] const QStringList &animationTargetVocabulary();

// Triggers from master plan 14.3. Click and launch-succeeded are deliberately
// distinct members: a click may never be reported as a successful launch.
[[nodiscard]] const QStringList &animationTriggerVocabulary();

// Primitive track properties from master plan 14.4.
[[nodiscard]] const QStringList &animationPropertyVocabulary();

[[nodiscard]] bool animationPropertyIsColor(const QString &property);

struct AnimationPropertyRange
{
    qreal minimum = 0.0;
    qreal maximum = 0.0;
};

[[nodiscard]] std::optional<AnimationPropertyRange> animationPropertyRange(
    const QString &property);

struct AnimationTrackDefinition
{
    QString id;
    // Empty means "inherit the profile target"; a value overrides it so one
    // profile can compose motion across several layers.
    QString target;
    QString property;
    qreal from = 0.0;
    qreal to = 0.0;
    QString fromColor;
    QString toColor;
    QString easing = QStringLiteral("linear");
    int duration = 0;
    int delay = 0;
    // Per-entry stagger in milliseconds, multiplied by the entry index.
    int phase = 0;
    QString direction = QStringLiteral("normal");
    // -1 repeats forever; otherwise a bounded cycle count.
    int repeat = 1;
    qreal intensityScale = 1.0;
    QString blend = QStringLiteral("replace");
    int priority = 0;

    [[nodiscard]] QString effectiveTarget(const QString &profileTarget) const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const AnimationTrackDefinition &) const = default;
};

struct AnimationTimingDefinition
{
    int baseDuration = 170;
    qreal speedScale = 1.0;
    int startDelay = 0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const AnimationTimingDefinition &) const = default;
};

// Every profile must state what it does when the user asks for reduced motion.
// There is no implicit default: an absent declaration is a validation error.
struct AnimationReducedMotionDefinition
{
    QString mode;
    QString substituteProfileId;
    QString property;
    qreal value = 0.0;
    QString color;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const AnimationReducedMotionDefinition &) const = default;
};

struct AnimationProfileDefinition
{
    static constexpr int CurrentVersion = 1;

    QString format = QStringLiteral("org.archdock.animation-profile");
    int version = CurrentVersion;
    QString id;
    QString name;
    QString description;
    QString author;
    QString category;
    QString target = QStringLiteral("icon");
    QString trigger = QStringLiteral("idle");
    AnimationTimingDefinition timing;
    QVector<AnimationTrackDefinition> tracks;
    QStringList rendererRequirements;
    AnimationReducedMotionDefinition reducedMotion;
    // Legacy `iconAnimation` values this profile replaces, so an existing
    // configuration keeps working after migration.
    QStringList legacyNames;
    QVariantMap extensions;

    [[nodiscard]] const AnimationTrackDefinition *trackById(
        const QString &trackId) const;
    [[nodiscard]] QStringList effectiveTargets() const;
    [[nodiscard]] bool isContinuous() const;
    [[nodiscard]] int totalDuration() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QVariantMap toRuntimeProjection(
        const QString &validationStatus = QStringLiteral("valid"),
        const QVector<AnimationValidationDiagnostic> &diagnostics = {}) const;
    bool operator==(const AnimationProfileDefinition &) const = default;
};

}
