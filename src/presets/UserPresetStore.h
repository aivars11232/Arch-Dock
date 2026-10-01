#pragma once

#include "../model/IconPresetDefinition.h"
#include "../model/PanelPresetDefinition.h"

#include <QString>
#include <QVector>

#include <optional>

namespace ArchDock
{

// The user's own Panel and Icon presets, kept apart from the installed
// catalog. One definition file per preset lives under `<root>/panels` and
// `<root>/icons`; `<root>/user-presets.json` records the store version.
//
// The store only ever writes user-owned ids inside its own root. It has no
// path to an installed built-in, so a built-in cannot be overwritten through
// it by construction.
class UserPresetStore
{
public:
    static constexpr int CurrentStoreVersion = 1;
    static constexpr qsizetype MaximumPresets = 512;

    explicit UserPresetStore(QString rootDirectory);

    [[nodiscard]] static QString storeFormat();
    [[nodiscard]] QString rootDirectory() const;

    // Every readable, valid user preset, ordered by name. A damaged or
    // foreign file is reported and skipped; it never hides the others.
    [[nodiscard]] QVector<PanelPresetDefinition> panelPresets(
        QVector<PresetValidationDiagnostic> *diagnostics = nullptr) const;
    [[nodiscard]] QVector<IconPresetDefinition> iconPresets(
        QVector<PresetValidationDiagnostic> *diagnostics = nullptr) const;

    // A user-owned copy of `source`: a complete snapshot with no id yet and
    // lineage to the preset and revision it came from. Nothing is written.
    [[nodiscard]] static PanelPresetDefinition derivedFrom(
        const PanelPresetDefinition &source, const QString &name);
    [[nodiscard]] static IconPresetDefinition derivedFrom(
        const IconPresetDefinition &source, const QString &name);

    // Writes a user-owned preset and returns it as stored. A definition that
    // is not already a stored user preset is given a new stable user id; an
    // existing one is replaced with its revision advanced.
    [[nodiscard]] std::optional<PanelPresetDefinition> save(
        const PanelPresetDefinition &definition, QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<IconPresetDefinition> save(
        const IconPresetDefinition &definition, QString *errorCode = nullptr) const;

    [[nodiscard]] bool removePanelPreset(const QString &presetId,
                                         QString *errorCode = nullptr) const;
    [[nodiscard]] bool removeIconPreset(const QString &presetId,
                                        QString *errorCode = nullptr) const;

private:
    QString m_rootDirectory;
};

}
