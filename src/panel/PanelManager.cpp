// PanelManager: forwards main()'s requests to the backend.
#include "PanelManager.h"

#include <QQmlApplicationEngine>
#include "PanelWindow.h"
PanelManager::PanelManager(QQmlApplicationEngine &engine, QObject *parent)
    : QObject(parent), m_engine(engine), m_panelWindow(std::make_unique<PanelWindow>(engine))
{
}
PanelManager::~PanelManager() = default;

void PanelManager::showSettings()
{
    m_panelWindow->showSettings();
}

void PanelManager::toggleAutoHide()
{
    m_panelWindow->toggleAutoHide();
}

void PanelManager::stopIntentionally(const QString &reason)
{
    m_panelWindow->stopIntentionally(reason);
}