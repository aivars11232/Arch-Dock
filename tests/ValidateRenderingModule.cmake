if(NOT DEFINED ARCHDOCK_SOURCE_DIR)
  message(FATAL_ERROR "ARCHDOCK_SOURCE_DIR is required")
endif()

set(rendering_sources
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelScene.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelSurfaceLoader.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/PanelProcedural2D.qml")

foreach(rendering_source IN LISTS rendering_sources)
  if(NOT EXISTS "${rendering_source}")
    message(FATAL_ERROR "Missing rendering source: ${rendering_source}")
  endif()
  file(READ "${rendering_source}" rendering_content)
  if(rendering_content MATCHES
     "org\\.kde\\.plasma|Plasmoid|DBus|dbus|SessionBus|containment|panelRegistry|callDock")
    message(
      FATAL_ERROR
        "Host mutation API found in shared rendering source: ${rendering_source}")
  endif()
endforeach()

message(STATUS "Shared rendering sources are host-neutral")
