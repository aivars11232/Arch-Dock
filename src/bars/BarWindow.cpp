// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarWindow.h"

#include <LayerShellQt/Window>

#include <QLoggingCategory>
#include <QQuickItem>
#include <QScreen>

Q_LOGGING_CATEGORY(archdockBars, "archdock.bars", QtWarningMsg)

namespace ArchDock::Bars
{
namespace
{
bool sideEdge(const QString &edge)
{
    return edge == QStringLiteral("left") || edge == QStringLiteral("right");
}
}

BarWindow::BarWindow(Plasma::Corona *corona, Plasma::Containment *containment, const QUrl &view)
    : PlasmaQuick::ContainmentView(corona)
{
    setColor(Qt::transparent);
    setFlags(flags() | Qt::FramelessWindowHint);
    setSource(view);
    setContainment(containment);

    m_layer = LayerShellQt::Window::get(this);
    m_layer->setScope(QStringLiteral("arch-dock-bar"));
    m_layer->setLayer(LayerShellQt::Window::LayerTop);
    // Standing at the very edge, inside the room the edge's strip reserves.
    m_layer->setExclusiveZone(-1);
    // Widgets that type (a launcher's search) get the keyboard when clicked;
    // showing a bar takes it from nobody.
    m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    m_layer->setActivateOnShow(false);
    // A bar whose screen goes is moved to another by the bar host.
    m_layer->setCloseOnDismissed(false);

    if (QQuickItem *root = rootObject())
        connect(root, SIGNAL(wantedLengthChanged()), this, SIGNAL(wantedLengthChanged()));
}

int BarWindow::wantedLength() const
{
    const QQuickItem *root = rootObject();
    return root ? root->property("wantedLength").toInt() : 0;
}

void BarWindow::place(QScreen *screen, const QString &edge, int start, int length, int thickness, int margin)
{
    using LayerShellQt::Window;
    if (!screen || length <= 0 || thickness <= 0)
    {
        hide();
        return;
    }
    const bool vertical = sideEdge(edge);
    if (QQuickItem *root = rootObject())
        root->setProperty("vertical", vertical);

    // Anchored to its edge and the edge's start, so that its margin along
    // the edge is where it starts.
    const QRect area = screen->geometry();
    Window::Anchors anchors;
    QMargins margins;
    QPoint at;
    if (edge == QStringLiteral("top"))
    {
        anchors = Window::Anchors(Window::AnchorTop | Window::AnchorLeft);
        margins = QMargins(start, margin, 0, 0);
        at = QPoint(area.x() + start, area.y() + margin);
    }
    else if (edge == QStringLiteral("left"))
    {
        anchors = Window::Anchors(Window::AnchorLeft | Window::AnchorTop);
        margins = QMargins(margin, start, 0, 0);
        at = QPoint(area.x() + margin, area.y() + start);
    }
    else if (edge == QStringLiteral("right"))
    {
        anchors = Window::Anchors(Window::AnchorRight | Window::AnchorTop);
        margins = QMargins(0, start, margin, 0);
        at = QPoint(area.x() + area.width() - margin - thickness, area.y() + start);
    }
    else
    {
        anchors = Window::Anchors(Window::AnchorBottom | Window::AnchorLeft);
        margins = QMargins(start, 0, 0, margin);
        at = QPoint(area.x() + start, area.y() + area.height() - margin - thickness);
    }

    // A layer-shell surface belongs to one screen from when it is shown: to
    // move it to another it is shown again there.
    if (m_screen != screen && isVisible())
        hide();
    m_screen = screen;
    setScreen(screen);
    const bool moved = m_layer->anchors() != anchors || m_layer->margins() != margins;
    m_layer->setScreen(screen);
    m_layer->setAnchors(anchors);
    m_layer->setMargins(margins);
    // The compositor places the bar by its anchors and margins. Its place on
    // the screen is kept here too, as Plasma keeps a panel's, for whatever
    // places itself from it: the dock's popups and its window past the bar.
    setGeometry(QRect(at, vertical ? QSize(thickness, length) : QSize(length, thickness)));
    if (!isVisible())
        show();
    // The compositor takes a new place with the bar's next frame, which a bar
    // whose picture stays the same would not draw: one is asked for.
    else if (moved)
        update();
    qCDebug(archdockBars) << "bar placed at" << QRect(at, size()) << "kept as" << geometry()
                          << "on" << screen->name();
}

ReservedStrip::ReservedStrip(QScreen *screen, const QString &edge)
    : m_screen(screen)
{
    using LayerShellQt::Window;
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setScreen(screen);

    m_layer = Window::get(this);
    m_layer->setScope(QStringLiteral("arch-dock-bar-room"));
    m_layer->setLayer(Window::LayerBottom);
    m_layer->setKeyboardInteractivity(Window::KeyboardInteractivityNone);
    m_layer->setActivateOnShow(false);
    m_layer->setCloseOnDismissed(false);
    m_layer->setScreen(screen);
    Window::Anchors anchors;
    if (edge == QStringLiteral("top"))
        anchors = Window::Anchors(Window::AnchorTop | Window::AnchorLeft | Window::AnchorRight);
    else if (edge == QStringLiteral("left"))
        anchors = Window::Anchors(Window::AnchorLeft | Window::AnchorTop | Window::AnchorBottom);
    else if (edge == QStringLiteral("right"))
        anchors = Window::Anchors(Window::AnchorRight | Window::AnchorTop | Window::AnchorBottom);
    else
        anchors = Window::Anchors(Window::AnchorBottom | Window::AnchorLeft | Window::AnchorRight);
    m_layer->setAnchors(anchors);
    // One pixel deep: the compositor stretches it along the edge.
    resize(sideEdge(edge) ? QSize(1, 10) : QSize(10, 1));
}

void ReservedStrip::setDepth(int depth)
{
    if (depth == m_depth && isVisible())
        return;
    m_depth = depth;
    m_layer->setExclusiveZone(depth);
    // As a bar's new place, a new depth comes with the strip's next frame,
    // which an unchanged picture would not draw.
    if (!isVisible())
        show();
    else
        update();
}
}
