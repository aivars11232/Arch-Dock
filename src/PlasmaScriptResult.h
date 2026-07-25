#pragma once

#include <QString>

#include <optional>

namespace ArchDock
{
std::optional<int> parsePlasmaScriptResult(const QString &output);
}
