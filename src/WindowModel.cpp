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

        const WindowItem &current = m_windows.at(row);
        if (current.desktopFileName == window.desktopFileName &&
            current.iconName == window.iconName &&
            current.resourceClass == window.resourceClass &&
            current.resourceName == window.resourceName &&
            current.caption == window.caption &&
            current.frameGeometry == window.frameGeometry &&
            current.screenIndex == window.screenIndex &&
            current.active == window.active &&
            current.minimized == window.minimized &&
            current.maximized == window.maximized &&
            current.fullScreen == window.fullScreen)
        {
            return true;
        }

        m_windows[row] = window;

        const QModelIndex modelIndex = index(row);
        emit dataChanged(
            modelIndex,
            modelIndex,
            {DesktopFileNameRole,
             IconNameRole,
             ResourceClassRole,
             ResourceNameRole,
             CaptionRole,
             FrameGeometryRole,
             ScreenIndexRole,
             ActiveRole,
             MinimizedRole,
             MaximizedRole,
             FullScreenRole});

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
        {FullScreenRole, "fullScreen"}};
}
