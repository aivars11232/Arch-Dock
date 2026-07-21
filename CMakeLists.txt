cmake_minimum_required(VERSION 3.21)

project(
  ArchDock
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Qml Quick)

find_package(LayerShellQt REQUIRED)

find_package(KF6Kirigami REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(
  arch-dock
  src/main.cpp
  src/panel/PanelManager.h
  src/panel/PanelManager.cpp
  src/panel/PanelWindow.h
  src/panel/PanelWindow.cpp
  src/WindowItem.h
  src/WindowModel.h
  src/WindowModel.cpp
  src/WindowWatcher.h
  src/WindowWatcher.cpp)
qt_add_qml_module(
  arch-dock
  URI
  ArchDock
  VERSION
  1.0
  NO_IMPORT_SCAN
  QML_FILES
  qml/runtime/Main.qml
  qml/runtime/Panel.qml
  qml/runtime/PanelBackground.qml
  qml/runtime/DockIcon.qml
  qml/runtime/PanelContextMenu.qml
  qml/runtime/PanelPopupWindow.qml
  qml/runtime/EditModeOverlay.qml
  qml/runtime/IconPopupWindow.qml
  qml/runtime/IconProperties.qml
  qml/runtime/IconPropertiesWindow.qml
  qml/runtime/IconContextMenu.qml)

target_link_libraries(
  arch-dock
  PRIVATE Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::Qml
          Qt6::Quick
          LayerShellQt::Interface
          KF6::Kirigami)
install(TARGETS arch-dock RUNTIME DESTINATION bin)
