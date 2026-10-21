// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarWindow.h"

#include "ScreenEdge.h"

#include <LayerShellQt/Window>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <QPlatformSurfaceEvent>
#include <QQuickItem>
#include <QScreen>

Q_LOGGING_CATEGORY(archdockBars, "archdock.bars", QtWarningMsg)

namespace ArchDock::Bars
{
namespace
{
// How long a bar that is no longer needed stays before it hides.
constexpr int ConcealDelayMs = 400;

bool sideEdge(const QString &edge)
{
    return edge == QStringLiteral("left") || edge == QStringLiteral("right");
}

QtWayland::kde_screen_edge_manager_v1::border borderOf(const QString &edge)
{
    if (edge == QStringLiteral("top"))
        return QtWayland::kde_screen_edge_manager_v1::border_top;
    if (edge == QStringLiteral("left"))
        return QtWayland::kde_screen_edge_manager_v1::border_left;
    if (edge == QStringLiteral("right"))
        return QtWayland::kde_screen_edge_manager_v1::border_right;
    return QtWayland::kde_screen_edge_manager_v1::border_bottom;
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

    m_concealTimer.setSingleShot(true);
    m_concealTimer.setInterval(ConcealDelayMs);
    connect(&m_concealTimer, &QTimer::timeout, this, [this]
    {
        m_concealed = true;
        qCDebug(archdockBars) << "bar hidden" << geometry() << "through its edge" << bool(m_autoHide);
        if (m_autoHide)
            m_autoHide->hide();
    });
}

BarWindow::~BarWindow() = default;

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
    // An edge belongs to one screen border: a bar moved to another border
    // gets a new one.
    if (edge != m_edge)
    {
        m_edge = edge;
        m_autoHide.reset();
        attachEdge();
    }
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

void BarWindow::setHides(bool hides, ScreenEdgeManager *edges)
{
    m_edges = edges;
    if (hides == m_hides)
    {
        attachEdge();
        return;
    }
    m_hides = hides;
    if (!hides)
    {
        // Letting the edge go shows the bar.
        m_concealTimer.stop();
        m_concealed = false;
        m_autoHide.reset();
        return;
    }
    attachEdge();
}

void BarWindow::attachEdge()
{
    if (!m_hides || m_autoHide)
        return;
    if (!m_roleReady || !m_edges || !m_edges->isActive() || m_edge.isEmpty())
    {
        qCDebug(archdockBars) << "no auto-hide edge yet: role" << m_roleReady << "KWin's edges"
                              << (m_edges && m_edges->isActive()) << "edge" << m_edge;
        return;
    }
    auto *wayland = nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    wl_surface *surface = wayland ? wayland->surface() : nullptr;
    if (!surface)
        return;
    m_autoHide = std::make_unique<AutoHideEdge>(m_edges->get_auto_hide_screen_edge(borderOf(m_edge), surface));
    qCDebug(archdockBars) << "auto-hide edge on the" << m_edge << "edge for" << geometry();
    if (m_concealed)
        m_autoHide->hide();
}

void BarWindow::setConcealWanted(bool conceal)
{
    if (!conceal)
    {
        m_concealTimer.stop();
        if (m_concealed)
        {
            m_concealed = false;
            if (m_autoHide)
                m_autoHide->show();
        }
        return;
    }
    // A bar KWin shows by its edge has the pointer on it at once, which
    // marks it shown again; one still hidden needs nothing more.
    if (!m_concealed && !m_concealTimer.isActive())
        m_concealTimer.start();
}

bool BarWindow::event(QEvent *event)
{
    // KWin takes an edge only for a surface that is a layer-shell surface
    // already, and it goes with the surface: asking any earlier would be a
    // protocol error. A window on the screen has that surface; one being
    // hidden, or losing its platform side, is about to lose it.
    if (event->type() == QEvent::Expose && isExposed() && !m_roleReady)
    {
        m_roleReady = true;
        attachEdge();
    }
    else if (event->type() == QEvent::Hide
             || (event->type() == QEvent::PlatformSurface
                 && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()
                     == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed))
    {
        m_roleReady = false;
        m_autoHide.reset();
    }
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave)
    {
        const bool inside = event->type() == QEvent::Enter;
        if (inside != m_pointerInside)
        {
            m_pointerInside = inside;
            Q_EMIT pointerInsideChanged();
        }
    }
    return PlasmaQuick::ContainmentView::event(event);
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
