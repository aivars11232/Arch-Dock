cmake_minimum_required(VERSION 3.21)

project(ArchDock
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(arch-dock
    src/main.cpp
)

qt_add_qml_module(arch-dock
    URI ArchDock
    VERSION 1.0
    QML_FILES
        qml/runtime/Main.qml
        qml/runtime/Panel.qml
        qml/runtime/PanelBackground.qml
)

target_link_libraries(arch-dock PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

install(TARGETS arch-dock
    RUNTIME DESTINATION bin
)
