#include "PanelManager.h"

#include <QQmlApplicationEngine>
#include "PanelWindow.h"
PanelManager::PanelManager(QQmlApplicationEngine& engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_panelWindow(std::make_unique<PanelWindow>())
{
}
PanelManager::~PanelManager() = default;