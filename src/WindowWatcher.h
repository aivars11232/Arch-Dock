#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QVariantMap>
#include <QRectF>

class QJsonObject;

class WindowModel;

class WindowWatcher final : public QObject
{
    Q_OBJECT
    // Keep the KWin script's interface independent of the executable name.
    Q_CLASSINFO("D-Bus Interface", "local.WindowWatcher")
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit WindowWatcher(WindowModel &windowModel,
                           QObject *parent = nullptr);
    [[nodiscard]] bool available() const;
    [[nodiscard]] QVariantMap nativePanelState(const QRectF &bounds) const;
    // Unloads every KWin watcher instance. The watcher reports each window
    // event to Arch Dock, and a report to a stopped backend activates it.
    static void releaseKWinScripts();

public slots:
    void windowAdded(const QString &internalId,
                     const QString &desktopFileName,
                     const QString &resourceClass,
                     const QString &resourceName,
                     const QString &caption,
                     bool active,
                     bool minimized,
                     const QString &stateJson);

    void windowRemoved(const QString &internalId);

    void windowUpdated(const QString &internalId,
                       const QString &desktopFileName,
                       const QString &resourceClass,
                       const QString &resourceName,
                       const QString &caption,
                       bool active,
                       bool minimized,
                       const QString &stateJson);

signals:
    void availableChanged();
    void nativePanelsChanged();

private:
    bool loadKWinScript();
    void observeNativePanel(const QString &internalId, const QJsonObject &state);

    WindowModel &m_windowModel;
    bool m_available = false;
    QHash<QString, QVariantMap> m_nativePanels;
};
