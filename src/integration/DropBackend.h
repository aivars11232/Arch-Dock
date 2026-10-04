#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// A Qt drop event must be answered during delivery. Plasma's asynchronous
// QML D-Bus API cannot report the persistence result before that event expires.
class DropBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DropBackend(QObject *parent = nullptr) : QObject(parent) {}
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE bool addUrls(const QString &panelId, bool freeSurface,
                            const QStringList &urls)
    {
        if (urls.isEmpty() || (freeSurface && panelId.isEmpty()))
            return fail(QStringLiteral("invalid-drop"));
        return commit(freeSurface ? QStringLiteral("addPanelEntries")
                                  : QStringLiteral("pinDockUrls"),
                      freeSurface ? QVariantList{panelId, urls} : QVariantList{urls});
    }

    Q_INVOKABLE bool moveEntry(const QString &panelId, const QString &entryId,
                              const QString &beforeEntryId)
    {
        if (panelId.isEmpty() || entryId.isEmpty() || entryId == beforeEntryId)
            return fail(QStringLiteral("invalid-reorder"));
        return commit(QStringLiteral("movePanelEntryBefore"),
                      {panelId, entryId, beforeEntryId});
    }

signals:
    void lastErrorChanged();

private:
    bool fail(const QString &error)
    {
        if (m_lastError != error) {
            m_lastError = error;
            emit lastErrorChanged();
        }
        return false;
    }

    bool commit(const QString &method, const QVariantList &arguments)
    {
        const auto bus = QDBusConnection::sessionBus();
        // Address the current unique owner. Do not activate an absent service
        // or deliver the mutation to a replacement process during a drag.
        auto ownerMessage = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
        ownerMessage << QStringLiteral("org.archdock.ArchDock");
        const auto owner = bus.call(ownerMessage, QDBus::Block, 1000);
        if (owner.type() != QDBusMessage::ReplyMessage || owner.arguments().size() != 1)
            return fail(QStringLiteral("service-unavailable"));
        auto message = QDBusMessage::createMethodCall(owner.arguments().first().toString(),
            QStringLiteral("/Control"), QStringLiteral("local.PanelWindow"), method);
        message.setArguments(arguments);
        const auto reply = bus.call(message, QDBus::Block, 1000);
        if (reply.type() != QDBusMessage::ReplyMessage)
            return fail(reply.errorName());
        if (reply.arguments().size() != 1 || reply.arguments().first().metaType() != QMetaType::fromType<bool>()
            || !reply.arguments().first().toBool())
            return fail(QStringLiteral("backend-rejected"));
        if (!m_lastError.isEmpty()) {
            m_lastError.clear();
            emit lastErrorChanged();
        }
        return true;
    }

    QString m_lastError;
};
