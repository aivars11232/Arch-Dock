#pragma once

#include <QString>
#include <optional>

// The Panel Preset and Icon Preset a new panel starts from, chosen in Panel
// Studio and kept in a small file of their own. Setting a default never
// changes an existing panel.
namespace ArchDock
{
struct PresetDefaults
{
    QString panelPresetId;
    QString iconPresetId;
    bool operator==(const PresetDefaults &) const = default;
};

class PresetDefaultStore final
{
public:
    static constexpr int CurrentVersion = 1;
    static constexpr int MaximumBytes = 4096;
    [[nodiscard]] static QString defaultPath();
    explicit PresetDefaultStore(QString filePath = defaultPath());
    [[nodiscard]] QString filePath() const;
    [[nodiscard]] std::optional<PresetDefaults> load(QString *errorCode = nullptr) const;
    // Empty presetId removes this default. This never visits an active panel.
    [[nodiscard]] bool setDefault(const QString &kind, const QString &presetId,
                                  QString *errorCode = nullptr) const;
private:
    QString m_filePath;
};
}
