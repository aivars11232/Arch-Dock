// WindowModel: the window list and its roles.
#include "WindowModel.h"

WindowModel::WindowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void WindowModel::setWindows(const QList<WindowItem> &windows)
{
    beginResetModel();
    m_windows = windows;
    endResetModel();
}

void WindowModel::addWindow(const WindowItem &window)
{
    for (const WindowItem &existingWindow : m_windows)
    {
        if (existingWindow.internalId == window.internalId)
        {
            return;
        }
    }

    const int newRow = m_windows.size();

    beginInsertRows(QModelIndex(), newRow, newRow);
    m_windows.append(window);
    endInsertRows();
}
bool WindowModel::removeWindow(const QString &internalId)
{
    for (int row = 0; row < m_windows.size(); ++row)
    {
        if (m_windows.at(row).internalId != internalId)
        {
            continue;
        }

        beginRemoveRows(QModelIndex(), row, row);
        m_windows.removeAt(row);
        endRemoveRows();

        return true;
    }

    return false;
}
bool WindowModel::updateWindow(const WindowItem &window)
{
    for (int row = 0; row < m_windows.size(); ++row)
    {
        if (m_windows.at(row).internalId != window.internalId)
        {
            continue;
        }

        // Only the roles that changed are reported: a moving window changes
        // its geometry on every frame, and consumers that show no geometry
        // (the dock's entries) can ignore such updates.
        const WindowItem &current = m_windows.at(row);
        QList<int> changed;
        const auto compare = [&changed](bool same, int role)
        {
            if (!same)
                changed.append(role);
        };
        compare(current.desktopFileName == window.desktopFileName, DesktopFileNameRole);
        compare(current.iconName == window.iconName, IconNameRole);
        compare(current.resourceClass == window.resourceClass, ResourceClassRole);
        compare(current.resourceName == window.resourceName, ResourceNameRole);
        compare(current.caption == window.caption, CaptionRole);
        compare(current.frameGeometry == window.frameGeometry, FrameGeometryRole);
        compare(current.screenIndex == window.screenIndex, ScreenIndexRole);
        compare(current.active == window.active, ActiveRole);
        compare(current.minimized == window.minimized, MinimizedRole);
        compare(current.maximized == window.maximized, MaximizedRole);
        compare(current.fullScreen == window.fullScreen, FullScreenRole);
        compare(current.canActivate == window.canActivate, CanActivateRole);
        compare(current.canMinimize == window.canMinimize, CanMinimizeRole);
        compare(current.canClose == window.canClose, CanCloseRole);
        if (changed.isEmpty())
        {
            return true;
        }

        m_windows[row] = window;

        const QModelIndex modelIndex = index(row);
        emit dataChanged(modelIndex, modelIndex, changed);

        return true;
    }

    return false;
}
void WindowModel::clearWindows()
{
    beginResetModel();
    m_windows.clear();
    endResetModel();
}

int WindowModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_windows.size();
}

QVariant WindowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_windows.size())
    {
        return {};
    }

    const WindowItem &window = m_windows.at(index.row());

    switch (role)
    {
    case InternalIdRole:
        return window.internalId;

    case DesktopFileNameRole:
        return window.desktopFileName;

    case IconNameRole:
        return window.iconName;

    case ResourceClassRole:
        return window.resourceClass;

    case ResourceNameRole:
        return window.resourceName;

    case CaptionRole:
        return window.caption;

    case FrameGeometryRole:
        return window.frameGeometry;

    case ScreenIndexRole:
        return window.screenIndex;

    case ActiveRole:
        return window.active;

    case MinimizedRole:
        return window.minimized;

    case MaximizedRole:
        return window.maximized;

    case FullScreenRole:
        return window.fullScreen;

    case CanActivateRole:
        return window.canActivate;

    case CanMinimizeRole:
        return window.canMinimize;

    case CanCloseRole:
        return window.canClose;

    default:
        return {};
    }
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    return {
        {InternalIdRole, "internalId"},
        {DesktopFileNameRole, "desktopFileName"},
        {IconNameRole, "iconName"},
        {ResourceClassRole, "resourceClass"},
        {ResourceNameRole, "resourceName"},
        {CaptionRole, "caption"},
        {FrameGeometryRole, "frameGeometry"},
        {ScreenIndexRole, "screenIndex"},
        {ActiveRole, "active"},
        {MinimizedRole, "minimized"},
        {MaximizedRole, "maximized"},
        {FullScreenRole, "fullScreen"},
        {CanActivateRole, "canActivate"},
        {CanMinimizeRole, "canMinimize"},
        {CanCloseRole, "canClose"}};
}
