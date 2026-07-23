cmake_minimum_required(VERSION 3.21)

project(
  ArchDock
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Network Qml Quick)

find_package(KF6Kirigami REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(
  arch-dock
  src/main.cpp
  src/DockModel.h
  src/DockModel.cpp
  src/DockSettings.h
  src/DockSettings.cpp
  src/PanelRegistry.h
  src/PanelRegistry.cpp
  src/NativeContainmentLifecycle.h
  src/NativeContainmentLifecycle.cpp
  src/PanelPlacement.h
  src/PanelPlacement.cpp
  src/PanelVisibility.h
  src/PanelVisibility.cpp
  src/ScreenIdentity.h
  src/ScreenIdentity.cpp
  src/SystemStatus.h
  src/SystemStatus.cpp
  src/KWinActionBridge.h
  src/KWinActionBridge.cpp
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
  qml/runtime/IconProperties.qml
  qml/runtime/IconPropertiesWindow.qml
  qml/runtime/SettingsPopup.qml)

target_link_libraries(
  arch-dock
  PRIVATE Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::Network
          Qt6::Qml
          Qt6::Quick
          KF6::Kirigami)

target_compile_definitions(
  arch-dock
  PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js")

include(CTest)
if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test QmlTools)

  qt_add_executable(
    panel-registry-test
    tests/PanelRegistryTest.cpp
    src/DockSettings.h
    src/DockSettings.cpp
    src/PanelRegistry.h
    src/PanelRegistry.cpp
    src/NativeContainmentLifecycle.h
    src/NativeContainmentLifecycle.cpp
    src/PanelPlacement.h
    src/PanelPlacement.cpp
    src/PanelVisibility.h
    src/PanelVisibility.cpp
    src/ScreenIdentity.h
    src/ScreenIdentity.cpp
    src/WindowItem.h
    src/WindowModel.h
    src/WindowModel.cpp
    src/DockModel.h
    src/DockModel.cpp)
  target_include_directories(
    panel-registry-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-registry-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME panel-registry-test COMMAND panel-registry-test)
  set_tests_properties(
    panel-registry-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME dock-geometry-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_DockGeometry.qml)
  set_tests_properties(
    dock-geometry-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME motion-policy-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_MotionPolicy.qml)
  set_tests_properties(
    motion-policy-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS arch-dock RUNTIME DESTINATION bin)
install(
  DIRECTORY kwin-script/
  DESTINATION share/kwin/scripts/org.archdock.windowwatcher)
install(
  FILES data/org.archdock.ArchDock.desktop
  DESTINATION share/applications)
install(
  DIRECTORY plasma-widget/
  DESTINATION share/plasma/plasmoids/org.archdock.control)
install(
  DIRECTORY plasma-dock-widget/
  DESTINATION share/plasma/plasmoids/org.archdock.dock)
install(
  DIRECTORY plasma-layout-template/
  DESTINATION share/plasma/layout-templates/org.archdock.plasma.desktop.emptyPanel)
install(
  DIRECTORY plasma-layout-template-launcher/
  DESTINATION share/plasma/layout-templates/org.archdock.plasma.desktop.launcherPanel)
install(
  DIRECTORY plasma-layout-template-tasks/
  DESTINATION share/plasma/layout-templates/org.archdock.plasma.desktop.tasksPanel)
install(
  DIRECTORY plasma-layout-template-hybrid/
  DESTINATION share/plasma/layout-templates/org.archdock.plasma.desktop.hybridPanel)
install(
  FILES data/org.archdock.ArchDock.service
  DESTINATION share/dbus-1/services)
install(
  FILES data/arch-dock.service
  DESTINATION share/systemd/user)
