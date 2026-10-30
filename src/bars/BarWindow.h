// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <PlasmaQuick/ContainmentView>

#include <QLoggingCategory>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>

#include <memory>

class QScreen;

Q_DECLARE_LOGGING_CATEGORY(archdockBars)

namespace LayerShellQt
{
class Window;
}

namespace ArchDock::Bars
{
class AutoHideEdge;
class ScreenEdgeManager;

// One Arch Dock bar on the screen: a window showing the bar's containment, as
// Plasma shows a panel's, but a layer-shell surface of Arch Dock's own above
// windows, placed along its edge by the bar host. It keeps no room for itself
// (other bars may stand beside it on the same edge); the edge's reserved
// strip keeps windows clear of it.
class BarWindow : public PlasmaQuick::ContainmentView
{
    Q_OBJECT

public:
    BarWindow(Plasma::Corona *corona, Plasma::Containment *containment, const QUrl &view);
    ~BarWindow() override;

    // How long the bar's widgets want it to be along its edge.
    [[nodiscard]] int wantedLength() const;
    // How far past the bar's inner side what it shows reaches while the bar
    // hides at times (its dock's stands and holograms): its window is that
    // much deeper, so that they are not cut at the window's edge and hide
    // with the bar. A bar always shown has its dock drawn in the dock's own
    // window past it and asks for none.
    [[nodiscard]] int wantedReach() const;
    // The bar on the screen: its window without the room past its inner side.
    [[nodiscard]] QRect barGeometry() const;
    // Stands the bar on this screen against `edge`, `start` pixels along it
    // from the edge's start (its left, or its top on a side edge), `length`
    // long, `thickness` deep and `margin` pixels off the edge. Shown unless
    // it has no length. Its window reaches `roomBefore` pixels past its start
    // and `roomAfter` past its end, for edit mode's + to stand outside it,
    // and the wanted reach past its inner side.
    void place(QScreen *screen, const QString &edge, int start, int length, int thickness, int margin,
               int roomBefore = 0, int roomAfter = 0);

    // Whether the bar hides at times (it auto-hides or keeps out of windows'
    // way): it then has KWin's auto-hide edge along its screen edge, by which
    // KWin shows it again when the pointer hits that edge, as it shows
    // Plasma's own panels.
    void setHides(bool hides, ScreenEdgeManager *edges);
    // Whether the bar should be hidden now. It shows at once and hides a
    // moment later, so that the pointer passing between the bar and its
    // popups does not make it blink.
    void setConcealWanted(bool conceal);
    [[nodiscard]] bool concealed() const { return m_concealed; }
    [[nodiscard]] bool pointerInside() const { return m_pointerInside; }

Q_SIGNALS:
    void wantedLengthChanged();
    void wantedReachChanged();
    void pointerInsideChanged();

protected:
    bool event(QEvent *event) override;

private:
    void attachEdge();

    LayerShellQt::Window *m_layer = nullptr;
    QPointer<QScreen> m_screen;
    QString m_edge;
    // The room the window reaches past the bar's inner side.
    int m_reach = 0;
    QPointer<ScreenEdgeManager> m_edges;
    std::unique_ptr<AutoHideEdge> m_autoHide;
    QTimer m_concealTimer;
    // The surface has its layer-shell role: only then may it get an edge.
    bool m_roleReady = false;
    bool m_hides = false;
    bool m_concealed = false;
    bool m_pointerInside = false;
};

// The room a screen edge's bars take: a see-through strip along the whole
// edge, under windows, taking no input, whose only work is to reserve the
// depth of the edge's deepest bar so that maximised windows stop above it.
// One window per edge because the compositor keeps no room for a window
// anchored to a corner, as a bar standing beside another must be.
class ReservedStrip : public QQuickWindow
{
    Q_OBJECT

public:
    ReservedStrip(QScreen *screen, const QString &edge);

    void setDepth(int depth);
    [[nodiscard]] QScreen *placedScreen() const { return m_screen; }

private:
    LayerShellQt::Window *m_layer = nullptr;
    QPointer<QScreen> m_screen;
    int m_depth = -1;
};
}
