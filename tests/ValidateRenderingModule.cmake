if(NOT DEFINED ARCHDOCK_SOURCE_DIR)
  message(FATAL_ERROR "ARCHDOCK_SOURCE_DIR is required")
endif()

set(rendering_sources
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/IconStyleResolver.js"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/AnimationProfileRuntime.js"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/MotionChannels.js"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PresentationStates.js"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/ThemeStateSelection.js"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/IconScene.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/IconMotionController.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/MotionTrackRunner.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelPresentationController.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelMotionController.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/SceneRotationController.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/inputs/GeometryHitRegion.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelScene.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/PanelSurfaceLoader.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/RunningIndicator.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/inputs/AlphaHitMask.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/previews/LivePanelPreview.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/IconStyle2D.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/PanelProcedural2D.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/PanelSkin2D.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/PanelSkinLayer2D.qml"
    "${ARCHDOCK_SOURCE_DIR}/qml/ArchDock/Rendering/renderers/PanelBaked25D.qml")

foreach(rendering_source IN LISTS rendering_sources)
  if(NOT EXISTS "${rendering_source}")
    message(FATAL_ERROR "Missing rendering source: ${rendering_source}")
  endif()
  file(READ "${rendering_source}" rendering_content)
  if(rendering_content MATCHES
     "org\\.kde\\.plasma|Plasmoid|DBus|dbus|SessionBus|(^|[^A-Za-z0-9_])containment([^A-Za-z0-9_]|$)|panelRegistry|callDock")
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

# Baked 2.5D is layered artwork with a depth ordering, not a mesh scene. The
# module must therefore keep working with no Qt Quick 3D module installed, so
# no shared rendering source may import one.
foreach(rendering_source IN LISTS rendering_sources)
  file(READ "${rendering_source}" rendering_content)
  if(rendering_content MATCHES "QtQuick3D|QtQuick\\.Scene3D|Qt3D")
    message(
      FATAL_ERROR
        "Optional 3D module imported by a shared rendering source: ${rendering_source}")
  endif()
endforeach()

message(STATUS "Shared rendering sources are host-neutral")
