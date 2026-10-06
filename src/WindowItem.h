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
    bool canActivate = false;
    bool canMinimize = false;
    bool canClose = false;
};
