// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWheelEvent>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// QML's WheelEvent exposes deltas and device but not QWheelEvent::source().
// On Wayland the primary device identifies the seat, not the actual wheel.
// Observe that native metadata without accepting or modifying any event.
class WheelSource : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool discrete READ discrete NOTIFY discreteChanged)

public:
    explicit WheelSource(QObject *parent = nullptr) : QObject(parent) {}
    QWindow *window() const { return m_window; }
    bool discrete() const { return m_discrete; }

    void setWindow(QWindow *value)
    {
        if (m_window == value) return;
        if (m_window) m_window->removeEventFilter(this);
        disconnect(m_destroyed);
        m_window = value;
        setDiscrete(false);
        if (value) {
            value->installEventFilter(this);
            m_destroyed = connect(value, &QObject::destroyed, this, [this] {
                m_window = nullptr;
                setDiscrete(false);
                emit windowChanged();
            });
        }
        emit windowChanged();
    }

signals:
    void windowChanged();
    void discreteChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_window && event->type() == QEvent::Wheel)
            setDiscrete(static_cast<QWheelEvent *>(event)->source() == Qt::MouseEventNotSynthesized);
        return false;
    }

private:
    void setDiscrete(bool value)
    {
        if (m_discrete == value) return;
        m_discrete = value;
        emit discreteChanged();
    }

    QPointer<QWindow> m_window;
    QMetaObject::Connection m_destroyed;
    bool m_discrete = false;
};
