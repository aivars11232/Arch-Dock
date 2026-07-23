#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "WindowItem.h"

class WindowModel;

class DockModel final : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role
    {
        AppIdRole = Qt::UserRole + 1,
        DesktopFileNameRole,
        IconNameRole,
        DisplayNameRole,
        PinnedRole,
        RunningRole,
        ActiveRole,
        MinimizedRole,
        WindowCountRole,
        WindowIdsRole,
        WindowTitlesRole,
        FolderRole
    };
    Q_ENUM(Role)

    explicit DockModel(WindowModel &windowModel, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void activateWindow(int row, int windowIndex);
    Q_INVOKABLE void toggleMinimized(int row);
    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE void close(int row, int windowIndex = -1);
    Q_INVOKABLE void closeAll(int row);
    Q_INVOKABLE void togglePinned(int row);
    Q_INVOKABLE void pin(int row);
    Q_INVOKABLE void unpin(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void setCustomIcon(int row, const QString &iconName);
    Q_INVOKABLE void clearCustomIcon(int row);
    Q_INVOKABLE bool pinUrl(const QUrl &url);
    Q_INVOKABLE bool isFolder(int row) const;
    Q_INVOKABLE QVariantList folderEntries(int row) const;
    Q_INVOKABLE bool openUrl(const QUrl &url) const;
    Q_INVOKABLE bool panelEntryMatches(int row, const QString &panelType) const;
    Q_INVOKABLE int panelEntryPosition(int row, const QString &panelType) const;
    Q_INVOKABLE int panelEntryCount(const QString &panelType) const;
    [[nodiscard]] QVariantList panelEntries(const QString &panelType) const;
    bool activateApplication(const QString &appId);
    bool activateApplicationWindow(const QString &appId, const QString &windowId);
    bool minimizeApplication(const QString &appId);
    bool closeApplication(const QString &appId);
    bool closeAllApplication(const QString &appId);
    bool togglePinnedApplication(const QString &appId);
    bool moveApplicationBefore(const QString &appId, const QString &beforeAppId);
    [[nodiscard]] QVariantList folderEntriesForApplication(const QString &appId) const;

signals:
    void countChanged();
    void windowActionRequested(const QString &internalId,
                               const QString &action);

private:
    struct PinnedApplication
    {
        QString appId;
        QString desktopFileName;
        QString iconName;
        QString displayName;
        QString launchCommand;
    };

    struct DockApplication
    {
        QString appId;
        QString desktopFileName;
        QString defaultIconName;
        QString iconName;
        QString displayName;
        QString launchCommand;
        QList<WindowItem> windows;
        bool pinned = false;
    };

    static QString applicationIdForWindow(const WindowItem &window);
    static QString normalizedDesktopId(const QString &identifier);
    static QString desktopIdForResourceClass(const QString &resourceClass);
    static void hydrateDesktopEntry(DockApplication &application);
    static QStringList launchArguments(const QString &command);
    static QString folderPath(const DockApplication &application);

    void loadPinnedApplications();
    void savePinnedApplications() const;
    void rebuild();
    void requestAction(const QString &internalId, const QString &action);
    int preferredWindowIndex(const DockApplication &application);
    int indexForApplication(const QString &appId) const;
    static bool matchesPanelType(const DockApplication &application, const QString &panelType);

    WindowModel &m_windowModel;
    QList<PinnedApplication> m_pinnedApplications;
    QList<DockApplication> m_items;
    QStringList m_order;
    QHash<QString, QString> m_customIcons;
    QHash<QString, int> m_nextWindowByApplication;
};