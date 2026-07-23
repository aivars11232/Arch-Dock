#pragma once

#include <QRect>
#include <QString>

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
};