#include "PlasmaScriptResult.h"

#include <QRegularExpression>

std::optional<int> ArchDock::parsePlasmaScriptResult(const QString &output)
{
    static const QRegularExpression pattern(
        QStringLiteral("\\AARCHDOCK_RESULT:(-?\\d+)\\z"));
    const QRegularExpressionMatch match = pattern.match(output.trimmed());
    if (!match.hasMatch())
    {
        return std::nullopt;
    }

    bool valid = false;
    const int result = match.captured(1).toInt(&valid);
    return valid ? std::optional<int>(result) : std::nullopt;
}
