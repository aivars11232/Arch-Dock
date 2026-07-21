#include "PanelWindow.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include <LayerShellQt/Window>

PanelWindow::PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent)
    : QObject(parent),
      m_engine(engine),
      m_windowWatcher(m_windowModel)
{
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("windowModel"),
        &m_windowModel);

    m_engine.loadFromModule(
        QStringLiteral("ArchDock"),
        QStringLiteral("Main"));

    const auto rootObjects = m_engine.rootObjects();

    if (rootObjects.isEmpty())
    {
        return;
    }

    auto *window = qobject_cast<QWindow *>(rootObjects.constFirst());

    if (!window)
    {
        return;
    }

    auto *layerWindow = LayerShellQt::Window::get(window);

    LayerShellQt::Window::Anchors anchors;

    switch (m_position)
    {
    case Position::Bottom:
        anchors = LayerShellQt::Window::AnchorBottom;
        break;

    case Position::Top:
        anchors = LayerShellQt::Window::AnchorTop;
        break;

    case Position::Left:
        anchors = LayerShellQt::Window::AnchorLeft;
        break;

    case Position::Right:
        anchors = LayerShellQt::Window::AnchorRight;
        break;

    case Position::Free:
        anchors = {};
        break;
    }

    layerWindow->setAnchors(anchors);

    switch (m_position)
    {
    case Position::Bottom:
    case Position::Top:
        window->setWidth(800);
        window->setHeight(64);
        break;

    case Position::Left:
    case Position::Right:
        window->setWidth(64);
        window->setHeight(800);
        break;

    case Position::Free:
        window->setWidth(800);
        window->setHeight(64);
        break;
    }

    layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    layerWindow->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setExclusiveZone(0);

    window->show();
}