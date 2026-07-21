#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "WindowItem.h"

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
        ActiveRole,
        MinimizedRole
    };
    Q_ENUM(Role)

    explicit WindowModel(QObject *parent = nullptr);

    void setWindows(const QList<WindowItem> &windows);
    void addWindow(const WindowItem &window);
    bool removeWindow(const QString &internalId);
    bool updateWindow(const WindowItem &window);
    void clearWindows();

    int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    QList<WindowItem> m_windows;
};