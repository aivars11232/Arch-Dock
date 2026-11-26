#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QRectF>
#include <QString>

#include "WindowItem.h"

// The application windows open on the desktop, as the KWin watcher reports
// them (WindowWatcher). DockModel groups them by application.
class WindowModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        InternalIdRole = Qt::UserRole + 1,
        DesktopFileNameRole,
        IconNameRole,
        ResourceClassRole,
        ResourceNameRole,
        CaptionRole,
        FrameGeometryRole,
        ScreenIndexRole,
        ActiveRole,
        MinimizedRole,
        MaximizedRole,
        FullScreenRole,
        CanActivateRole,
        CanMinimizeRole,
        CanCloseRole,
        OnCurrentDesktopRole,
        HiddenRole,
        OpacityRole
    };
    Q_ENUM(Role)

    explicit WindowModel(QObject *parent = nullptr);

    void setWindows(const QList<WindowItem> &windows);
    void addWindow(const WindowItem &window);
    bool removeWindow(const QString &internalId);
    bool updateWindow(const WindowItem &window);
    void clearWindows();

    [[nodiscard]] const QList<WindowItem> &windows() const
    {
        return m_windows;
    }

    int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    QList<WindowItem> m_windows;
};

// Whether windows hide all of `area` on the desktop (in its logical pixels).
// Only a window shown on the desktop in use, and solid, hides anything: any
// such window what is drawn under windows (`underWindows`: a free panel on
// the desktop); what is drawn above them (an edge panel, a bar) only a
// full-screen window KWin draws above panels - while it is active, or while
// the active window is on another screen. Windows side by side may hide it
// together. `panels` are Plasma's own panels as shown: drawn above the
// desktop, they hide what lies under them on it too.
[[nodiscard]] bool windowsCoverArea(const QList<WindowItem> &windows, const QRectF &area, bool underWindows,
                                    const QList<QRect> &panels = {});
// Changes whenever what the windows may cover changes; not with a caption,
// nor with which window is active unless that lifts a full-screen window
// above the panels or lets it drop.
[[nodiscard]] size_t windowCoverSignature(const QList<WindowItem> &windows);
