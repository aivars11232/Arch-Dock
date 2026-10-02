#include "ProfileShortcutManager.h"
#include <KGlobalAccel>
#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <memory>

namespace ArchDock
{
namespace
{
const QString component = QStringLiteral("org.archdock.ArchDock");
bool safePath(const QString &path)
{
    QFileInfo info(QFileInfo(path).absoluteFilePath());
    while (true)
    {
        if (info.isSymLink()) return false;
        const QString parent = info.absolutePath();
        if (parent == info.absoluteFilePath()) return true;
        info.setFile(parent);
    }
}
QStringList nativeKeys(const QString &id)
{
    QStringList keys;
    for (const auto &key : KGlobalAccel::self()->globalShortcut(component, id))
        if (!key.isEmpty()) keys.append(key.toString(QKeySequence::PortableText));
    return keys;
}
}
ProfileShortcutManager::NativeOperations ProfileShortcutManager::nativeOperations(QObject *owner)
{
    struct Actions { QMap<QString, QAction *> actions; };
    auto state = std::make_shared<Actions>();
    NativeOperations operations;
    operations.available = [] {
        const auto bus = QDBusConnection::sessionBus();
        return bus.isConnected() && bus.interface() &&
            bus.interface()->isServiceRegistered(QStringLiteral("org.kde.kglobalaccel")).value();
    };
    operations.keys = nativeKeys;
    operations.conflicts = [](const QString &id, const QString &text) {
        const QKeySequence sequence(text, QKeySequence::PortableText);
        QVariantList conflicts;
        QSet<QString> seen;
        bool own = false;
        for (const auto type : {KGlobalAccel::Equal, KGlobalAccel::Shadows, KGlobalAccel::Shadowed})
            for (const auto &info : KGlobalAccel::globalShortcutsByKey(sequence, type))
            {
                if (info.componentUniqueName() == component && info.uniqueName() == id) { own = true; continue; }
                const auto identity = info.componentUniqueName() + QLatin1Char('/') + info.uniqueName();
                if (seen.contains(identity)) continue;
                seen.insert(identity);
                conflicts.append(QVariantMap{{QStringLiteral("component"), info.componentUniqueName()},
                    {QStringLiteral("actionId"), info.uniqueName()}, {QStringLiteral("name"), info.friendlyName()}});
            }
        if (!KGlobalAccel::isGlobalShortcutAvailable(sequence, component) && !own && conflicts.isEmpty())
            conflicts.append(QVariantMap{{QStringLiteral("name"), QStringLiteral("Shortcut is unavailable")}});
        return conflicts;
    };
    operations.assign = [owner, state](const QString &id, const QString &name, const QString &text,
        std::function<void()> activate, QString *error) {
        auto *action = state->actions.value(id);
        if (!action)
        {
            action = new QAction(owner);
            action->setObjectName(id);
            action->setProperty("componentName", component);
            action->setProperty("componentDisplayName", QStringLiteral("Arch Dock"));
            state->actions.insert(id, action);
        }
        action->setText(name);
        QObject::disconnect(action, &QAction::triggered, owner, nullptr);
        QObject::connect(action, &QAction::triggered, owner, [activate = std::move(activate)] { activate(); });
        const bool assigned = KGlobalAccel::self()->setShortcut(action,
            {QKeySequence(text, QKeySequence::PortableText)}, KGlobalAccel::NoAutoloading);
        if (!assigned || nativeKeys(id) != QStringList{text})
        {
            *error = QStringLiteral("profile-shortcut-registration-readback-failed");
            return false;
        }
        return true;
    };
    operations.remove = [state](const QString &id, QString *error) {
        if (auto *action = state->actions.take(id))
        {
            KGlobalAccel::self()->removeAllShortcuts(action);
            delete action;
        }
        // Also remove a known persisted action after a crash, when no local
        // QAction exists. Never clean an entire component or steal a key.
        QDBusInterface remote(QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/kglobalaccel"),
            QStringLiteral("org.kde.KGlobalAccel"), QDBusConnection::sessionBus());
        const QDBusReply<bool> reply = remote.call(QStringLiteral("unregister"), component, id);
        if (!reply.isValid() || !nativeKeys(id).isEmpty())
        {
            *error = QStringLiteral("profile-shortcut-unregister-failed");
            return false;
        }
        return true;
    };
    QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, owner,
        [owner, state](QAction *action, const QKeySequence &) {
            if (state->actions.values().contains(action))
                emit static_cast<ProfileShortcutManager *>(owner)->changed();
        });
    return operations;
}

ProfileShortcutManager::ProfileShortcutManager(Lookup lookup, Apply apply, QString path,
    NativeOperations native, QObject *parent)
    : QObject(parent), m_lookup(std::move(lookup)), m_apply(std::move(apply)),
      m_path(std::move(path)), m_native(std::move(native))
{
    // Constructing the native adapter must not start GlobalAccel while the
    // feature is off. Its factory is deferred until a stored or explicit opt-in.
    QFile file(m_path);
    if (!QFileInfo::exists(m_path)) return;
    if (!safePath(m_path) || !file.open(QIODevice::ReadOnly) || file.size() > 65536)
    { m_error = QStringLiteral("invalid-profile-shortcut-file"); return; }
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    if (object.value("version").toInt() != 1 || !object.value("enabled").isBool() ||
        !object.value("bindings").isObject() || object.value("bindings").toObject().size() > 512 ||
        object.size() != 3)
    { m_error = QStringLiteral("invalid-profile-shortcut-file"); return; }
    const auto bindings = object.value("bindings").toObject();
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
    {
        const QString key = canonicalKey(it.value().toString());
        if (!ProfileDefinition::validId(it.key()) || key.isEmpty())
        { m_bindings.clear(); m_error = QStringLiteral("invalid-profile-shortcut-file"); return; }
        m_bindings.insert(it.key(), key);
    }
    if (!m_native.available) m_native = nativeOperations(this);
    if (m_native.available())
    {
        m_registered = QSet<QString>(m_bindings.keyBegin(), m_bindings.keyEnd());
        QString error;
        if (!install({}, &error)) { m_error = error; return; }
    }
    for (auto it = m_bindings.begin(); it != m_bindings.end();)
        if (!m_lookup || !m_lookup(it.key())) it = m_bindings.erase(it); else ++it;
    if (object.value("enabled").toBool()) configure(true, m_bindings);
}
ProfileShortcutManager::~ProfileShortcutManager()
{
    m_enabled = false;
    if (m_native.available && m_native.available())
    {
        QString error;
        install({}, &error);
    }
}
QString ProfileShortcutManager::canonicalKey(const QString &text) const
{
    if (text.size() > 128) return {};
    const auto sequence = QKeySequence::fromString(text.trimmed(), QKeySequence::PortableText);
    if (sequence.isEmpty() || sequence.count() != 1 ||
        sequence[0].key() == Qt::Key_unknown || sequence[0].key() == Qt::Key(0)) return {};
    return sequence.toString(QKeySequence::PortableText);
}
QVariantMap ProfileShortcutManager::failure(const QString &code, QVariantList conflicts)
{
    m_error = code; m_conflicts = std::move(conflicts); emit changed();
    return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), m_error},
        {QStringLiteral("conflicts"), m_conflicts}};
}
QVariantMap ProfileShortcutManager::status() const
{
    QVariantList bindings;
    QSet<QString> ids(m_bindings.keyBegin(), m_bindings.keyEnd());
    ids.unite(m_registered);
    QStringList ordered(ids.begin(), ids.end());
    ordered.sort();
    for (const auto &id : ordered)
    {
        const auto profile = m_lookup ? m_lookup(id) : std::nullopt;
        const QStringList actual = m_registered.contains(id) && m_native.keys
            ? m_native.keys(id) : QStringList{};
        bindings.append(QVariantMap{{QStringLiteral("profileId"), id},
            {QStringLiteral("name"), profile ? profile->name : QString{}},
            {QStringLiteral("sequence"), m_bindings.value(id)}, {QStringLiteral("registeredKeys"), actual}});
    }
    return {{QStringLiteral("enabled"), m_enabled}, {QStringLiteral("bindings"), bindings},
        {QStringLiteral("errorCode"), m_error}, {QStringLiteral("conflicts"), m_conflicts},
        {QStringLiteral("lastActivation"), m_lastActivation}};
}
bool ProfileShortcutManager::install(const QMap<QString, QString> &bindings, QString *error)
{
    for (const auto &id : std::as_const(m_registered))
        if (!m_native.remove(id, error)) return false;
    m_registered.clear();
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
    {
        const auto profile = m_lookup ? m_lookup(it.key()) : std::nullopt;
        if (!profile) { *error = QStringLiteral("profile-shortcut-invalid-profile"); return false; }
        const auto conflicts = m_native.conflicts(it.key(), it.value());
        if (!conflicts.isEmpty()) { *error = QStringLiteral("profile-shortcut-conflict"); return false; }
        // Include a partially registered action in cleanup even if readback fails.
        m_registered.insert(it.key());
        if (!m_native.assign(it.key(), profile->name, it.value(), [this, id = it.key()] { activate(id); }, error) ||
            m_native.keys(it.key()) != QStringList{it.value()})
        { if (error->isEmpty()) *error = QStringLiteral("profile-shortcut-registration-readback-failed"); return false; }
    }
    return true;
}
bool ProfileShortcutManager::persist(bool enabled, const QMap<QString, QString> &bindings, QString *error)
{
    if (!safePath(m_path) || !QDir().mkpath(QFileInfo(m_path).absolutePath()))
    { *error = QStringLiteral("profile-shortcut-save-failed"); return false; }
    QJsonObject stored;
    for (auto it = bindings.begin(); it != bindings.end(); ++it) stored.insert(it.key(), it.value());
    const auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
        {QStringLiteral("enabled"), enabled}, {QStringLiteral("bindings"), stored}}).toJson();
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    { *error = QStringLiteral("profile-shortcut-save-failed"); return false; }
    QFile readback(m_path);
    if (!readback.open(QIODevice::ReadOnly) || readback.readAll() != bytes)
    { *error = QStringLiteral("profile-shortcut-save-readback-failed"); return false; }
    return true;
}
QVariantMap ProfileShortcutManager::configure(bool enabled, const QMap<QString, QString> &bindings)
{
    if (m_configuring) return failure(QStringLiteral("profile-shortcut-operation-active"));
    QScopedValueRollback<bool> configuring(m_configuring, true);
    QMap<QString, QString> used;
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
    {
        if (!m_lookup || !m_lookup(it.key())) return failure(QStringLiteral("profile-shortcut-invalid-profile"));
        if (used.contains(it.value()))
            return failure(QStringLiteral("profile-shortcut-conflict"),
                {QVariantMap{{QStringLiteral("profileId"), used.value(it.value())}, {QStringLiteral("name"), QStringLiteral("Another saved profile")}}});
        used.insert(it.value(), it.key());
    }
    if (!m_native.available && enabled) m_native = nativeOperations(this);
    const bool available = m_native.available && m_native.available();
    if (enabled && !available) return failure(QStringLiteral("profile-shortcuts-service-unavailable"));
    if (enabled)
        for (auto it = bindings.begin(); it != bindings.end(); ++it)
        {
            const auto conflicts = m_native.conflicts(it.key(), it.value());
            if (!conflicts.isEmpty()) return failure(QStringLiteral("profile-shortcut-conflict"), conflicts);
        }
    const auto previous = m_enabled ? m_bindings : QMap<QString, QString>{};
    QString error;
    if ((!available && !m_registered.isEmpty()) ||
        (available && !install(enabled ? bindings : QMap<QString, QString>{}, &error)) ||
        !persist(enabled, bindings, &error))
    {
        QString rollback;
        if ((!available && !m_registered.isEmpty()) || (available && !install(previous, &rollback)))
        { m_enabled = false; return failure(QStringLiteral("profile-shortcut-cleanup-incomplete")); }
        return failure(error.isEmpty() ? QStringLiteral("profile-shortcuts-service-unavailable") : error);
    }
    m_enabled = enabled; m_bindings = bindings; m_error.clear(); m_conflicts.clear(); emit changed();
    return {{QStringLiteral("success"), true}, {QStringLiteral("errorCode"), QString{}}};
}
QVariantMap ProfileShortcutManager::setEnabled(bool enabled) { return configure(enabled, m_bindings); }
QVariantMap ProfileShortcutManager::setBinding(const QString &id, const QString &text)
{
    if (!ProfileDefinition::validId(id) || !m_lookup || !m_lookup(id))
        return failure(QStringLiteral("profile-shortcut-invalid-profile"));
    const QString key = canonicalKey(text);
    if (key.isEmpty()) return failure(QStringLiteral("profile-shortcut-invalid-sequence"));
    auto bindings = m_bindings; bindings[id] = key;
    return configure(m_enabled, bindings);
}
QVariantMap ProfileShortcutManager::clearBinding(const QString &id)
{
    if (!m_bindings.contains(id) && !m_registered.contains(id))
        return {{QStringLiteral("success"), true}, {QStringLiteral("errorCode"), QString{}}};
    auto bindings = m_bindings; bindings.remove(id);
    return configure(m_enabled, bindings);
}
void ProfileShortcutManager::refreshProfiles()
{
    auto bindings = m_bindings;
    for (auto it = bindings.begin(); it != bindings.end();)
        if (!m_lookup || !m_lookup(it.key())) it = bindings.erase(it); else ++it;
    if (bindings != m_bindings) configure(m_enabled, bindings);
    else emit changed();
}
void ProfileShortcutManager::activate(const QString &id)
{
    if (!m_enabled || m_configuring || !m_bindings.contains(id)) return;
    const auto profile = m_lookup ? m_lookup(id) : std::nullopt;
    m_lastActivation = profile && m_apply ? m_apply(id, profile->revision)
        : QVariantMap{{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-shortcut-invalid-profile")}};
    m_lastActivation.insert(QStringLiteral("profileId"), id);
    emit changed();
}
}
