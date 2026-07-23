#pragma once

#include <QObject>
#include <QString>

class WindowModel;

class WindowWatcher final : public QObject
{
    Q_OBJECT

public:
    explicit WindowWatcher(WindowModel &windowModel,
                           QObject *parent = nullptr);

public slots:
    void windowAdded(const QString &internalId,
                     const QString &desktopFileName,
                     const QString &resourceClass,
                     const QString &resourceName,
                     const QString &caption,
                     bool active,
                     bool minimized,
                     int frameX,
                     int frameY,
                     int frameWidth,
                     int frameHeight,
                     int screenIndex,
                     bool maximized,
                     bool fullScreen);

    void windowRemoved(const QString &internalId);

    void windowUpdated(const QString &internalId,
                       const QString &desktopFileName,
                       const QString &resourceClass,
                       const QString &resourceName,
                       const QString &caption,
                       bool active,
                       bool minimized,
                       int frameX,
                       int frameY,
                       int frameWidth,
                       int frameHeight,
                       int screenIndex,
                       bool maximized,
                       bool fullScreen);

private:
    void loadKWinScript();

    WindowModel &m_windowModel;
};