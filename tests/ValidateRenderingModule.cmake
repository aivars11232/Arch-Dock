if(NOT DEFINED ARCHDOCK_SOURCE_DIR)
  message(FATAL_ERROR "ARCHDOCK_SOURCE_DIR is required")
endif()

set(rendering_sources
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/IconScene.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelScene.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelSurfaceLoader.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/RunningIndicator.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/previews/LivePanelPreview.qml"
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

set(preview_source
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/previews/LivePanelPreview.qml")
file(READ "${preview_source}" preview_content)
string(REGEX MATCHALL "PanelScene[ \t\r\n]*\\{" preview_scene_hosts
       "${preview_content}")
list(LENGTH preview_scene_hosts preview_scene_host_count)
if(NOT preview_scene_host_count EQUAL 1)
  message(FATAL_ERROR
          "LivePanelPreview must host exactly one PanelScene")
endif()
foreach(forbidden_preview_implementation
        "LayoutEngine"
        "PanelProcedural2D"
        "Canvas[ \t\r\n]*\\{"
        "Shape[ \t\r\n]*\\{")
  if(preview_content MATCHES "${forbidden_preview_implementation}")
    message(
      FATAL_ERROR
        "Parallel preview renderer found: ${forbidden_preview_implementation}")
  endif()
endforeach()

message(STATUS "Shared rendering sources are host-neutral")
