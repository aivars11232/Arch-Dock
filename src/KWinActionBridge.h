#pragma once

#include <QObject>

class KWinActionBridge final : public QObject
{
    Q_OBJECT

public:
    explicit KWinActionBridge(QObject *parent = nullptr);

public slots:
    void requestAction(const QString &internalId, const QString &action);
    void focusWindow(const QString &resourceClass, const QString &caption);

private:
    void runScript(const QString &source);
    static QString commandScript(const QString &internalId, const QString &action);
    static QString focusScript(const QString &resourceClass, const QString &caption);
};