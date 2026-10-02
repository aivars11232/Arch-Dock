#pragma once

#include "persistence/ProfileStore.h"
#include <QObject>
#include <QMap>
#include <functional>

namespace ArchDock
{
class ProfileShortcutManager final : public QObject
{
    Q_OBJECT
public:
    struct NativeOperations
    {
        std::function<bool()> available;
        std::function<QVariantList(const QString &, const QString &)> conflicts;
        std::function<bool(const QString &, const QString &, const QString &,
            std::function<void()>, QString *)> assign;
        std::function<QStringList(const QString &)> keys;
        std::function<bool(const QString &, QString *)> remove;
    };
    using Lookup = std::function<std::optional<ProfileDefinition>(const QString &)>;
    using Apply = std::function<QVariantMap(const QString &, int)>;
    ProfileShortcutManager(Lookup lookup, Apply apply, QString path,
        NativeOperations native = {}, QObject *parent = nullptr);
    ~ProfileShortcutManager() override;
    [[nodiscard]] QVariantMap status() const;
    QVariantMap setEnabled(bool enabled);
    QVariantMap setBinding(const QString &id, const QString &sequence);
    QVariantMap clearBinding(const QString &id);
    void refreshProfiles();
signals:
    void changed();
private:
    static NativeOperations nativeOperations(QObject *owner);
    [[nodiscard]] QString canonicalKey(const QString &text) const;
    QVariantMap configure(bool enabled, const QMap<QString, QString> &bindings);
    QVariantMap failure(const QString &code, QVariantList conflicts = {});
    bool install(const QMap<QString, QString> &bindings, QString *error);
    bool persist(bool enabled, const QMap<QString, QString> &bindings, QString *error);
    void activate(const QString &id);
    Lookup m_lookup;
    Apply m_apply;
    QString m_path;
    NativeOperations m_native;
    QMap<QString, QString> m_bindings;
    QSet<QString> m_registered;
    bool m_enabled = false;
    bool m_configuring = false;
    QString m_error;
    QVariantList m_conflicts;
    QVariantMap m_lastActivation;
};
}
