#pragma once

#include <QString>
#include <QStringList>

// The name a panel goes by until its owner names it: what it is, as Panel
// Studio's choices call it - "Ring panel", "Möbius strip panel", "Left
// panel" - with a number when another panel already has that name. Such a
// name follows the panel when its shape or edge changes; a name its owner
// chose stays.
namespace ArchDock
{
// The plain name for a panel on `edge` ("free" for a free panel) drawn in
// `layout`: "Circle panel", "Top panel".
[[nodiscard]] QString panelBaseName(const QString &edge, const QString &layout);
// Whether nobody chose the name: one these rules give (any shape's or
// edge's, with or without its number), or one an older version gave every
// new panel ("Free panel 3", "Panel 2").
[[nodiscard]] bool panelNameIsAutomatic(const QString &name);
// Whether the name is one an older version gave every new panel.
[[nodiscard]] bool panelNameIsOldDefault(const QString &name);
// The first of `base`, "`base` 2", "`base` 3"... that is not in `taken`.
[[nodiscard]] QString uniquePanelName(const QString &base, const QStringList &taken);
}
