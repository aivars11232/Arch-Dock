#pragma once

#include "../model/AnimationProfile.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct AnimationProfileCatalogLoadResult;

// Parses and validates the built-in animation-profile catalog. This class only
// decides whether a profile is well formed; executing a profile belongs to the
// QML motion controller.
class AnimationProfileCatalog
{
public:
    static constexpr qsizetype MaximumCatalogBytes = 524288;
    static constexpr qsizetype MaximumStringBytes = 4096;
    static constexpr qsizetype MaximumIdentifierBytes = 64;
    static constexpr qsizetype MaximumProfiles = 256;
    static constexpr qsizetype MaximumTracksPerProfile = 32;
    static constexpr int MaximumDurationMs = 60000;
    static constexpr int MaximumRepeatCount = 10000;
    static constexpr int MaximumPriority = 1000;
    static constexpr qreal MaximumIntensityScale = 10.0;
    static constexpr qreal MinimumSpeedScale = 0.05;
    static constexpr qreal MaximumSpeedScale = 10.0;

    [[nodiscard]] static AnimationProfileCatalogLoadResult loadCatalog(
        const QString &catalogPath);
    [[nodiscard]] static AnimationProfileCatalogLoadResult loadCatalogBytes(
        const QByteArray &catalogBytes,
        const QString &catalogPath = {});

    // Validates a single profile object. Exposed so fixtures can exercise the
    // validator without wrapping every case in a catalog document.
    [[nodiscard]] static QVector<AnimationValidationDiagnostic> validateProfile(
        const QVariantMap &profile,
        AnimationProfileDefinition *definition = nullptr);

    [[nodiscard]] bool contains(const QString &profileId) const;
    [[nodiscard]] QStringList profileIds() const;
    [[nodiscard]] QString fallbackProfileId() const;
    [[nodiscard]] const AnimationProfileDefinition *profileById(
        const QString &profileId) const;
    // Resolves a legacy `iconAnimation` value to its migrated profile id.
    [[nodiscard]] QString profileIdForLegacyName(
        const QString &legacyName) const;
    // Legacy name -> profile id, for compatibility mapping and documentation.
    [[nodiscard]] QMap<QString, QString> legacyNameMap() const;
    [[nodiscard]] QVariantList profileProjections() const;
    [[nodiscard]] QVariantMap resolve(const QString &requestedProfileId) const;
    [[nodiscard]] QVariantMap toVariantMap() const;

private:
    QString m_catalogPath;
    QString m_fallbackProfileId = QStringLiteral("none");
    QStringList m_profileOrder;
    QHash<QString, AnimationProfileDefinition> m_profiles;
    QMap<QString, QString> m_legacyNames;

    friend struct AnimationProfileCatalogLoadResult;
};

struct AnimationProfileCatalogLoadResult
{
    std::optional<AnimationProfileCatalog> catalog;
    QVector<AnimationValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QString primaryMessage() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

}
