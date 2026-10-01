#pragma once

#include "../model/PanelDefinition.h"

namespace ArchDock
{
struct PresetPreviewRecord
{
    QString sessionId;
    QString kind;
    QString panelId;
    QString hostKind;
    QString phase = QStringLiteral("PREPARING");
    bool temporary = false;
    QString previewToken;
    QString managedToken;
    int containmentId = -1;
    int appletId = -1;
    PanelDefinition snapshot;
    QVariantMap hostState;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] static std::optional<PresetPreviewRecord> fromVariantMap(
        const QVariantMap &value);
    bool operator==(const PresetPreviewRecord &) const = default;
};

// A write-ahead cleanup record, never a source of active preset values.
class PresetPreviewRecovery final
{
public:
    static constexpr int MaximumBytes = 65536;
    [[nodiscard]] static QString defaultPath();
    explicit PresetPreviewRecovery(QString filePath = defaultPath());
    [[nodiscard]] QString filePath() const;
    // Missing file: no record and no error. Invalid file: error and retain it.
    [[nodiscard]] std::optional<PresetPreviewRecord> load(QString *errorCode = nullptr) const;
    [[nodiscard]] bool save(const PresetPreviewRecord &record, QString *errorCode = nullptr) const;
    [[nodiscard]] bool clear(QString *errorCode = nullptr) const;
private:
    QString m_filePath;
};
}
