#include "PanelManager.h"

#include <QQmlApplicationEngine>

PanelManager::PanelManager(QQmlApplicationEngine& engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}
