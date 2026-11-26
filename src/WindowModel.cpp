// WindowModel: the window list and its roles.
#include "WindowModel.h"

#include <QHash>
#include <QRect>
#include <QtMath>

namespace
{
// A window that hides what lies under it: shown on the desktop in use, and
// solid. One KWin draws see-through hides nothing. (A program can make its
// own window see-through too, which KWin's scripts do not tell; such a
// window counts as solid.)
bool windowHides(const WindowItem &window)
{
    return !window.minimized && !window.hidden && window.onCurrentDesktop
        && window.opacity >= 0.99 && !window.frameGeometry.isEmpty();
}

// The active window among them, if they hold it.
const WindowItem *activeWindow(const QList<WindowItem> &windows)
{
    for (const WindowItem &window : windows)
    {
        if (window.active)
        {
            return &window;
        }
    }
    return nullptr;
}

// Whether KWin draws a full-screen window above panels and bars: only while
// it is active, or while the active window lies wholly off it (on another
// screen), as KWin's own Window::isActiveFullScreen decides. An active window
// they do not hold (Plasma's own, Arch Dock's) is taken to be over it, so the
// panel is not taken for covered.
bool aboveDocks(const WindowItem &window, const WindowItem *active)
{
    if (!window.fullScreen)
    {
        return false;
    }
    if (window.active)
    {
        return true;
    }
    return active && !active->frameGeometry.intersects(window.frameGeometry);
}

// What is left of `rect` with `cut` taken out: up to four rects, the strips
// above and below the cut and the pieces beside it.
QList<QRect> without(const QRect &rect, const QRect &cut)
{
    const QRect overlap = rect.intersected(cut);
    if (overlap.isEmpty())
    {
        return {rect};
    }
    QList<QRect> rest;
    if (overlap.top() > rect.top())
        rest.append(QRect(QPoint(rect.left(), rect.top()), QPoint(rect.right(), overlap.top() - 1)));
    if (overlap.bottom() < rect.bottom())
        rest.append(QRect(QPoint(rect.left(), overlap.bottom() + 1), QPoint(rect.right(), rect.bottom())));
    if (overlap.left() > rect.left())
        rest.append(QRect(QPoint(rect.left(), overlap.top()), QPoint(overlap.left() - 1, overlap.bottom())));
    if (overlap.right() < rect.right())
        rest.append(QRect(QPoint(overlap.right() + 1, overlap.top()), QPoint(rect.right(), overlap.bottom())));
    return rest;
}
}

bool windowsCoverArea(const QList<WindowItem> &windows, const QRectF &area, bool underWindows,
                      const QList<QRect> &panels)
{
    if (!area.isValid() || area.width() < 1 || area.height() < 1)
    {
        return false;
    }
    // The whole pixels the area holds; what shows of a part of a pixel at
    // its edge is not worth drawing for.
    const QRect whole(QPoint(qCeil(area.left()), qCeil(area.top())),
                      QPoint(qFloor(area.right()) - 1, qFloor(area.bottom()) - 1));
    if (whole.isEmpty())
    {
        return false;
    }
    // Windows side by side may hide it together.
    const WindowItem *active = activeWindow(windows);
    QList<QRect> uncovered{whole};
    if (underWindows)
    {
        for (const QRect &panel : panels)
        {
            QList<QRect> left;
            for (const QRect &part : std::as_const(uncovered))
            {
                left.append(without(part, panel));
            }
            uncovered = left;
        }
    }
    for (const WindowItem &window : windows)
    {
        if (!windowHides(window) || (!underWindows && !aboveDocks(window, active)))
        {
            continue;
        }
        QList<QRect> left;
        for (const QRect &part : std::as_const(uncovered))
        {
            left.append(without(part, window.frameGeometry));
        }
        uncovered = left;
        if (uncovered.isEmpty())
        {
            return true;
        }
    }
    return false;
}

size_t windowCoverSignature(const QList<WindowItem> &windows)
{
    const WindowItem *active = activeWindow(windows);
    size_t signature = 0;
    for (const WindowItem &window : windows)
    {
        const QRect frame = window.frameGeometry;
        signature = qHashMulti(signature, frame.x(), frame.y(), frame.width(), frame.height(),
                               windowHides(window), aboveDocks(window, active));
    }
    return signature;
}

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
        compare(current.onCurrentDesktop == window.onCurrentDesktop, OnCurrentDesktopRole);
        compare(current.hidden == window.hidden, HiddenRole);
        compare(qFuzzyCompare(current.opacity, window.opacity), OpacityRole);
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

    case OnCurrentDesktopRole:
        return window.onCurrentDesktop;

    case HiddenRole:
        return window.hidden;

    case OpacityRole:
        return window.opacity;

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
        {CanCloseRole, "canClose"},
        {OnCurrentDesktopRole, "onCurrentDesktop"},
        {HiddenRole, "hidden"},
        {OpacityRole, "opacity"}};
}
