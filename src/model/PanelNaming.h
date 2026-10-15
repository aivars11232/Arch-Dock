#pragma once

#include <QString>
#include <QStringList>

// The name a panel goes by until its owner names it: a free panel its shape,
// exactly as Panel Studio's Dock layout list shows it - "Ring", "Möbius
// Strip", "Star" - and an edge panel its edge, "Left panel", with a number
// when another panel already has that name. Such a name follows the panel
// when its shape or edge changes; a name its owner chose stays.
namespace ArchDock
{
// A layout's name as Panel Studio's lists show it: "Circle" for "circular",
// "Möbius Strip" for "mobius".
[[nodiscard]] QString panelShapeName(const QString &layout);
// The plain name for a panel on `edge` ("free" for a free panel) drawn in
// `layout`: "Circle", "Top panel".
[[nodiscard]] QString panelBaseName(const QString &edge, const QString &layout);
// Whether nobody chose the name: one these rules give (any shape's or
// edge's, with or without its number), or one an older version gave
// ("Free panel 3", "Panel 2", "Ring panel").
[[nodiscard]] bool panelNameIsAutomatic(const QString &name);
// Whether the name is one an older version gave a panel nobody named.
[[nodiscard]] bool panelNameIsOldDefault(const QString &name);
// The first of `base`, "`base` 2", "`base` 3"... that is not in `taken`.
[[nodiscard]] QString uniquePanelName(const QString &base, const QStringList &taken);
}
