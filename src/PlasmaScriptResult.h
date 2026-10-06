#pragma once

#include <QString>

#include <optional>

// Plasma's evaluateScript returns only printed text. Arch Dock's scripts
// print "ARCHDOCK_RESULT:<number>"; this reads that number back.
namespace ArchDock
{
std::optional<int> parsePlasmaScriptResult(const QString &output);
}
