// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QRegion>
#include <QVariant>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// Where a window takes the pointer: the rectangles given, in the window's
// own coordinates; everywhere else the pointer passes through to whatever
// lies under the window. On Wayland a window's mask is its surface's input
// region, so a clear window drawn over the desktop stays out of the way
// except where it shows something. With no rectangles, or switched off, the
// whole window takes the pointer again.
class WindowInputShape : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QVariantList rects READ rects WRITE setRects NOTIFY rectsChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit WindowInputShape(QObject *parent = nullptr) : QObject(parent) {}
    ~WindowInputShape() override
    {
        if (m_window)
            m_window->setMask(QRegion());
    }

    QWindow *window() const { return m_window; }
    QVariantList rects() const { return m_rects; }
    bool enabled() const { return m_enabled; }

    void setWindow(QWindow *value)
    {
        if (m_window == value)
            return;
        if (m_window)
            m_window->setMask(QRegion());
        m_window = value;
        apply();
        emit windowChanged();
    }

    void setRects(const QVariantList &value)
    {
        if (m_rects == value)
            return;
        m_rects = value;
        apply();
        emit rectsChanged();
    }

    void setEnabled(bool value)
    {
        if (m_enabled == value)
            return;
        m_enabled = value;
        apply();
        emit enabledChanged();
    }

signals:
    void windowChanged();
    void rectsChanged();
    void enabledChanged();

private:
    void apply()
    {
        if (!m_window)
            return;
        QRegion region;
        if (m_enabled)
        {
            for (const QVariant &value : m_rects)
            {
                const QRect rect = value.toRectF().toAlignedRect();
                if (rect.width() > 0 && rect.height() > 0)
                    region += rect;
            }
        }
        // An empty region would be no mask at all, the whole window; one
        // pixel in a corner keeps the rest clear while nothing is shown.
        if (m_enabled && region.isEmpty())
            region = QRegion(0, 0, 1, 1);
        m_window->setMask(m_enabled ? region : QRegion());
    }

    QPointer<QWindow> m_window;
    QVariantList m_rects;
    bool m_enabled = true;
};
