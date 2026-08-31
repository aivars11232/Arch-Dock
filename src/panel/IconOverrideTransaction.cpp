#include "IconOverrideTransaction.h"

#include "../model/IconEntryIdentity.h"

#include <QFileInfo>
#include <QUrl>

#include <limits>

namespace
{

using ArchDock::IconOverrideTransactionOutcome;
using ArchDock::IconOverrideTransactionStatus;

QVariant revisionValue(quint64 revision)
{
    return QVariant::fromValue<qulonglong>(revision);
}

void fail(IconOverrideTransactionOutcome *outcome,
          IconOverrideTransactionStatus status,
          const QString &errorCode,
          const QString &errorMessage)
{
    if (!outcome)
    {
        return;
    }
    outcome->status = status;
    outcome->errorCode = errorCode;
    outcome->errorMessage = errorMessage;
}

QString availableCustomGlyph(const QString &customGlyph)
{
    const QString source = customGlyph.trimmed();
    if (source.isEmpty())
    {
        return {};
    }

    const QUrl sourceUrl(source);
    QString localPath;
    if (sourceUrl.isValid() && sourceUrl.scheme() == QStringLiteral("file"))
    {
        localPath = sourceUrl.toLocalFile();
    }
    else if (QFileInfo(source).isAbsolute())
    {
        localPath = source;
    }
    else if (sourceUrl.isValid() && !sourceUrl.scheme().isEmpty())
    {
        return {};
    }
    else
    {
        return source;
    }

    const QFileInfo fileInfo(localPath);
    return fileInfo.isFile() && fileInfo.isReadable()
        ? QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString()
        : QString{};
}

}

namespace ArchDock
{

bool IconOverrideTransactionOutcome::success() const
{
    return status == IconOverrideTransactionStatus::Succeeded;
}

QVariantMap IconOverrideTransactionOutcome::toVariantMap() const
{
    return {
        {QStringLiteral("success"), success()},
        {QStringLiteral("status"), IconOverrideTransaction::statusName(status)},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("entryIdentity"), entryIdentity},
        {QStringLiteral("expectedRevision"), revisionValue(expectedRevision)},
        {QStringLiteral("previousRevision"), revisionValue(previousRevision)},
        {QStringLiteral("revision"), revisionValue(revision)},
        {QStringLiteral("reset"), reset},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("errorMessage"), errorMessage},
    };
}

std::optional<IconOverrideTransactionDraft> IconOverrideTransaction::prepare(
    const PanelDefinition &currentPanel,
    const IconOverrideTransactionRequest &request,
    IconOverrideTransactionOutcome *outcome,
    const StyleValidator &styleValidator)
{
    if (outcome)
    {
        *outcome = {};
        outcome->panelId = request.panelId;
        outcome->entryIdentity = request.entryIdentity;
        outcome->expectedRevision = request.expectedRevision;
        outcome->previousRevision = currentPanel.settingsRevision;
        outcome->revision = currentPanel.settingsRevision;
        outcome->reset = request.reset;
    }

    if (request.panelId.trimmed().isEmpty() ||
        request.panelId != currentPanel.identity.id)
    {
        fail(outcome,
             IconOverrideTransactionStatus::ValidationFailed,
             QStringLiteral("panel-id-mismatch"),
             QStringLiteral("the override panel id does not match the snapshot"));
        return std::nullopt;
    }
    if (request.expectedRevision != currentPanel.settingsRevision)
    {
        fail(outcome,
             IconOverrideTransactionStatus::RevisionConflict,
             QStringLiteral("stale-revision"),
             QStringLiteral("the panel changed after this override was opened"));
        return std::nullopt;
    }
    if (currentPanel.settingsRevision == std::numeric_limits<quint64>::max())
    {
        fail(outcome,
             IconOverrideTransactionStatus::ValidationFailed,
             QStringLiteral("revision-exhausted"),
             QStringLiteral("the panel settings revision cannot be advanced"));
        return std::nullopt;
    }
    const QString identity = request.entryIdentity.trimmed();
    if (!IconEntryIdentity::isValid(identity))
    {
        fail(outcome,
             IconOverrideTransactionStatus::ValidationFailed,
             QStringLiteral("invalid-entry-identity"),
             QStringLiteral("the icon override identity is invalid"));
        return std::nullopt;
    }
    if (request.reset && !request.overrideValues.isEmpty())
    {
        fail(outcome,
             IconOverrideTransactionStatus::ValidationFailed,
             QStringLiteral("reset-has-values"),
             QStringLiteral("a reset transaction cannot contain override values"));
        return std::nullopt;
    }

    PanelDefinition candidate = currentPanel;
    if (request.reset)
    {
        candidate.iconStyle.perEntryOverrides.remove(identity);
    }
    else
    {
        QString overrideError;
        const auto parsed = PanelIconStyleDefinition::EntryOverride::fromVariantMap(
            request.overrideValues, &overrideError);
        if (!parsed.has_value() || parsed->isEmpty())
        {
            fail(outcome,
                 IconOverrideTransactionStatus::ValidationFailed,
                 parsed.has_value()
                     ? QStringLiteral("empty-override")
                     : QStringLiteral("invalid-override"),
                 parsed.has_value()
                     ? QStringLiteral("an empty override must use reset")
                     : overrideError);
            return std::nullopt;
        }
        if (!parsed->styleReference.isEmpty() && styleValidator &&
            !styleValidator(parsed->styleReference))
        {
            fail(outcome,
                 IconOverrideTransactionStatus::ValidationFailed,
                 QStringLiteral("unknown-icon-style"),
                 QStringLiteral("the requested icon style is not available"));
            return std::nullopt;
        }
        candidate.iconStyle.perEntryOverrides.insert(identity, *parsed);
    }
    candidate.settingsRevision = currentPanel.settingsRevision + 1;
    QString candidateError;
    const auto normalizedCandidate = PanelDefinition::fromLegacyMap(
        candidate.toLegacyMap(), &candidateError);
    if (!normalizedCandidate.has_value())
    {
        fail(outcome,
             IconOverrideTransactionStatus::ValidationFailed,
             QStringLiteral("invalid-panel-definition"),
             candidateError);
        return std::nullopt;
    }
    candidate = *normalizedCandidate;

    if (outcome)
    {
        outcome->status = IconOverrideTransactionStatus::Prepared;
        outcome->entryIdentity = identity;
        outcome->revision = candidate.settingsRevision;
        outcome->errorCode.clear();
        outcome->errorMessage.clear();
    }
    return IconOverrideTransactionDraft{
        currentPanel,
        candidate,
        identity,
        request.reset,
    };
}

QVariantMap IconOverrideTransaction::resolve(
    const PanelDefinition &panel,
    const QString &entryIdentity,
    const QString &baseGlyph,
    const QString &baseLabel,
    const StyleResolver &styleResolver)
{
    const QString identity = entryIdentity.trimmed();
    const auto match = panel.iconStyle.perEntryOverrides.constFind(identity);
    const bool overridden = IconEntryIdentity::isValid(identity) &&
        match != panel.iconStyle.perEntryOverrides.cend();
    const PanelIconStyleDefinition::EntryOverride override = overridden
        ? match.value() : PanelIconStyleDefinition::EntryOverride{};

    const QString fallbackGlyph = baseGlyph.trimmed().isEmpty()
        ? QStringLiteral("application-x-executable") : baseGlyph.trimmed();
    const QString customGlyph = availableCustomGlyph(override.customGlyph);
    const bool glyphFallbackApplied = overridden &&
        !override.customGlyph.isEmpty() && customGlyph.isEmpty();
    const QString panelStyle = panel.iconStyle.styleReference.trimmed().isEmpty()
        ? QStringLiteral("plain-original")
        : panel.iconStyle.styleReference.trimmed();
    const bool hasStyleOverride = overridden &&
        !override.styleReference.isEmpty();
    const QString requestedStyle = hasStyleOverride
        ? override.styleReference : panelStyle;
    QVariantMap styleDefinition = styleResolver
        ? styleResolver(requestedStyle) : QVariantMap{};
    bool resolvingPanelStyle = !hasStyleOverride;
    const auto resolvedStyleId = [](const QVariantMap &definition)
    {
        return definition.value(
            QStringLiteral("resolvedStyleId"),
            definition.value(QStringLiteral("id"))).toString();
    };
    const auto exactStyle = [&resolvedStyleId](
        const QVariantMap &definition,
        const QString &styleId)
    {
        return definition.value(QStringLiteral("valid")).toBool() &&
            !definition.value(QStringLiteral("fellBack")).toBool() &&
            resolvedStyleId(definition) == styleId;
    };
    QString styleFallbackReason;
    if (hasStyleOverride && !exactStyle(styleDefinition, requestedStyle) &&
        styleResolver)
    {
        styleFallbackReason = styleDefinition.value(
            QStringLiteral("fallbackReason"),
            QStringLiteral("override-style-unavailable")).toString();
        styleDefinition = styleResolver(panelStyle);
        resolvingPanelStyle = true;
    }
    if (resolvingPanelStyle && !exactStyle(styleDefinition, panelStyle) &&
        resolvedStyleId(styleDefinition) != QStringLiteral("plain-original") &&
        styleResolver)
    {
        if (styleFallbackReason.isEmpty())
        {
            styleFallbackReason = styleDefinition.value(
                QStringLiteral("fallbackReason"),
                QStringLiteral("panel-style-unavailable")).toString();
        }
        styleDefinition = styleResolver(QStringLiteral("plain-original"));
    }
    if (!styleDefinition.value(QStringLiteral("valid")).toBool() && styleResolver)
    {
        if (styleFallbackReason.isEmpty())
        {
            styleFallbackReason = QStringLiteral("style-unavailable");
        }
        styleDefinition = styleResolver(QStringLiteral("plain-original"));
    }
    const QString resolvedStyle = resolvedStyleId(styleDefinition);
    const bool styleFallbackApplied =
        !styleFallbackReason.isEmpty() ||
        styleDefinition.value(QStringLiteral("fellBack")).toBool() ||
        resolvedStyle != requestedStyle;

    return {
        {QStringLiteral("entryIdentity"), identity},
        {QStringLiteral("overrideApplied"), overridden},
        {QStringLiteral("override"), override.toVariantMap()},
        {QStringLiteral("resolvedGlyph"),
         customGlyph.isEmpty() ? fallbackGlyph : customGlyph},
        {QStringLiteral("resolvedLabel"),
         override.customLabel.isEmpty() ? baseLabel : override.customLabel},
        {QStringLiteral("tileEnabled"),
         override.tileEnabled.value_or(true)},
        {QStringLiteral("requestedStyleReference"), requestedStyle},
        {QStringLiteral("panelStyleReference"), panelStyle},
        {QStringLiteral("styleReference"),
         resolvedStyle.isEmpty() ? QStringLiteral("plain-original")
                                 : resolvedStyle},
        {QStringLiteral("animationProfileReference"),
         override.animationProfileReference},
        {QStringLiteral("glyphFallbackApplied"), glyphFallbackApplied},
        {QStringLiteral("glyphFallbackReason"),
         glyphFallbackApplied ? QStringLiteral("custom-glyph-unavailable")
                              : QString{}},
        {QStringLiteral("styleFallbackApplied"), styleFallbackApplied},
        {QStringLiteral("styleFallbackReason"),
         styleFallbackApplied
             ? styleFallbackReason.isEmpty()
                 ? styleDefinition.value(
                     QStringLiteral("fallbackReason")).toString()
                 : styleFallbackReason
             : QString{}},
        {QStringLiteral("iconStyleDefinition"), styleDefinition},
    };
}

QString IconOverrideTransaction::statusName(
    IconOverrideTransactionStatus status)
{
    switch (status)
    {
    case IconOverrideTransactionStatus::Prepared:
        return QStringLiteral("prepared");
    case IconOverrideTransactionStatus::Succeeded:
        return QStringLiteral("succeeded");
    case IconOverrideTransactionStatus::ValidationFailed:
        return QStringLiteral("validation-failed");
    case IconOverrideTransactionStatus::RevisionConflict:
        return QStringLiteral("revision-conflict");
    case IconOverrideTransactionStatus::PersistenceFailed:
        return QStringLiteral("persistence-failed");
    }
    return QStringLiteral("validation-failed");
}

}
