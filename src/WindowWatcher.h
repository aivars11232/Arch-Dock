#pragma once

#include <QObject>
#include <QString>

class WindowModel;

class WindowWatcher final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit WindowWatcher(WindowModel &windowModel,
                           QObject *parent = nullptr);
    [[nodiscard]] bool available() const;

public slots:
    void windowAdded(const QString &internalId,
                     const QString &desktopFileName,
                     const QString &resourceClass,
                     const QString &resourceName,
                     const QString &caption,
                     bool active,
                     bool minimized,
                     const QString &stateJson);

    void windowRemoved(const QString &internalId);

    void windowUpdated(const QString &internalId,
                       const QString &desktopFileName,
                       const QString &resourceClass,
                       const QString &resourceName,
                       const QString &caption,
                       bool active,
                       bool minimized,
                       const QString &stateJson);

signals:
    void availableChanged();

private:
    bool loadKWinScript();

    WindowModel &m_windowModel;
    bool m_available = false;
};
