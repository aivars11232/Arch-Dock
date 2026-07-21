#pragma once

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

    bool active = false;
    bool minimized = false;
};