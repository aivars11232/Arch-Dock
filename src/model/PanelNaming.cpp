#include "model/PanelNaming.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>

namespace ArchDock
{
namespace
{
QString translated(const char *text)
{
    return QCoreApplication::translate("ArchDock::PanelNaming", text);
}

// Each layout's name as Panel Studio's choices show it.
const QList<QPair<QString, const char *>> &shapeWords()
{
    static const QList<QPair<QString, const char *>> words{
        {QStringLiteral("adaptive"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Dock")},
        {QStringLiteral("horizontal"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Row")},
        {QStringLiteral("vertical"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Column")},
        {QStringLiteral("diagonal"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Diagonal")},
        {QStringLiteral("circular"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Circle")},
        {QStringLiteral("ellipse"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Ellipse")},
        {QStringLiteral("ring"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Ring")},
        {QStringLiteral("radial"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Radial")},
        {QStringLiteral("arc"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Arc")},
        {QStringLiteral("semicircle"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Semicircle")},
        {QStringLiteral("fan"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Fan")},
        {QStringLiteral("spiral"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Spiral")},
        {QStringLiteral("ribbon"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Ribbon")},
        {QStringLiteral("vertical-curve"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Curve")},
        {QStringLiteral("horizontal-curve"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Curve")},
        {QStringLiteral("polygon"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Polygon")},
        {QStringLiteral("triangle"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Triangle")},
        {QStringLiteral("square"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Square")},
        {QStringLiteral("pentagon"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Pentagon")},
        {QStringLiteral("hexagon"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Hexagon")},
        {QStringLiteral("octagon"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Octagon")},
        {QStringLiteral("star"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Star")},
        {QStringLiteral("grid"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Grid")},
        {QStringLiteral("floating"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Floating")},
        {QStringLiteral("mobius"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Möbius strip")},
    };
    return words;
}

const QList<QPair<QString, const char *>> &edgeWords()
{
    static const QList<QPair<QString, const char *>> words{
        {QStringLiteral("top"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Top")},
        {QStringLiteral("bottom"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Bottom")},
        {QStringLiteral("left"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Left")},
        {QStringLiteral("right"), QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Right")},
    };
    return words;
}

QString withPanel(const QString &word)
{
    return translated(QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "%1 panel")).arg(word);
}

// The name without a number at its end ("Ring panel 2" -> "Ring panel"),
// and whether it had one.
QString withoutNumber(const QString &name, bool *numbered)
{
    static const QRegularExpression trailing(QStringLiteral("^(.*\\S)\\s+[0-9]+$"));
    const auto match = trailing.match(name);
    if (numbered) *numbered = match.hasMatch();
    return match.hasMatch() ? match.captured(1) : name;
}
}

QString panelBaseName(const QString &edge, const QString &layout)
{
    if (edge != QStringLiteral("free"))
    {
        for (const auto &word : edgeWords())
            if (word.first == edge) return withPanel(translated(word.second));
        return translated(QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Panel"));
    }
    for (const auto &word : shapeWords())
        if (word.first == layout) return withPanel(translated(word.second));
    return translated(QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Free panel"));
}

bool panelNameIsOldDefault(const QString &name)
{
    const QString trimmed = name.trimmed();
    bool numbered = false;
    const QString stem = withoutNumber(trimmed, &numbered);
    if (!numbered) return false;
    // As older versions wrote them, in English or in the language they ran in.
    for (const QString &old : {QStringLiteral("Free panel"), QStringLiteral("Panel"),
                               QCoreApplication::translate("PanelRegistry", "Free panel %1").arg(QString()).trimmed(),
                               QCoreApplication::translate("PanelRegistry", "Panel %1").arg(QString()).trimmed()})
        if (!old.isEmpty() && stem == old) return true;
    return false;
}

bool panelNameIsAutomatic(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || panelNameIsOldDefault(trimmed)) return true;
    const QString stem = withoutNumber(trimmed, nullptr);
    for (const auto &word : shapeWords())
        if (stem == withPanel(translated(word.second))) return true;
    for (const auto &word : edgeWords())
        if (stem == withPanel(translated(word.second))) return true;
    return stem == translated(QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Free panel")) ||
        stem == translated(QT_TRANSLATE_NOOP("ArchDock::PanelNaming", "Panel"));
}

QString uniquePanelName(const QString &base, const QStringList &taken)
{
    if (!taken.contains(base)) return base;
    for (int number = 2;; ++number)
    {
        const QString candidate = base + QLatin1Char(' ') + QString::number(number);
        if (!taken.contains(candidate)) return candidate;
    }
}
}
