#pragma once

#include <QRect>
#include <QString>

// One application window as the KWin watcher reports it: identity, title,
// geometry and state.
class WindowItem
{
public:
    QString internalId;
    QString desktopFileName;
    QString iconName;
    QString resourceClass;
    QString resourceName;
    QString caption;
    QRect frameGeometry;

    int screenIndex = -1;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool fullScreen = false;
    // On the virtual desktop and activity in use (a window elsewhere covers
    // nothing on the screen).
    bool onCurrentDesktop = true;
    // Kept out of sight by KWin: it covers nothing. (Show Desktop, which
    // KWin's scripts do not see, is heard from KWin itself: PanelWindow.)
    bool hidden = false;
    // How solid KWin draws it, 1 for not see-through at all.
    double opacity = 1.0;
    bool canActivate = false;
    bool canMinimize = false;
    bool canClose = false;
};
