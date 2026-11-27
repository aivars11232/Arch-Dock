cmake_minimum_required(VERSION 3.21)

project(
  ArchDock
  VERSION 0.1.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Network Qml Quick Widgets)
# The Qt Arch Dock is built with is the lowest Qt it runs with. The package
# recipe reads it from qt-build-version, and the pacman hook compares the
# installed Qt with its minor version.
string(REGEX MATCH "^[0-9]+\\.[0-9]+" ARCHDOCK_QT_MINOR "${Qt6Core_VERSION}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt-build-version" "${Qt6Core_VERSION}\n")

find_package(KF6Kirigami REQUIRED)
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6Notifications REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)

# The release this build is and the one key its Update button trusts
# (data/update-keys.txt, in the form of LCL's trusted_keys.txt). A
# development build has no release and checks no updates; a release build
# must list exactly one key. A test key list or a rehearsal root makes a TEST
# build, which only tests and rehearsals use.
set(ARCHDOCK_RELEASE "" CACHE STRING
    "The release this build is, ${PROJECT_VERSION}-<package release>; empty for a development build")
set(ARCHDOCK_UPDATE_REPOSITORY "aivars11232/Arch-Dock" CACHE STRING
    "The GitHub repository, owner/name, whose releases the Update button looks at")
set(ARCHDOCK_UPDATE_TEST_KEYS "" CACHE FILEPATH
    "TEST builds only: a key list of one test key, used instead of data/update-keys.txt")
set(ARCHDOCK_UPDATE_REHEARSAL_ROOT "" CACHE PATH
    "TEST builds only: the private root a rehearsal build installs into")
set(ARCHDOCK_UPDATE_REHEARSAL_FEED "" CACHE STRING
    "Rehearsal builds only: the local test server standing in for GitHub, http://127.0.0.1:<port>")
if(ARCHDOCK_UPDATE_TEST_KEYS OR ARCHDOCK_UPDATE_REHEARSAL_ROOT)
  set(ARCHDOCK_BUILD_KIND test)
elseif(ARCHDOCK_RELEASE)
  set(ARCHDOCK_BUILD_KIND release)
else()
  set(ARCHDOCK_BUILD_KIND development)
endif()
if(ARCHDOCK_RELEASE)
  string(REPLACE "." "\\." archdock_version_pattern "${PROJECT_VERSION}")
  set(archdock_release_pattern "^${archdock_version_pattern}-[1-9][0-9]*$")
  if(ARCHDOCK_BUILD_KIND STREQUAL "test")
    set(archdock_release_pattern "^${archdock_version_pattern}-[1-9][0-9]*(\\.[1-9][0-9]*)?$")
  endif()
  if(NOT ARCHDOCK_RELEASE MATCHES "${archdock_release_pattern}")
    message(FATAL_ERROR "ARCHDOCK_RELEASE must be ${PROJECT_VERSION}-<n> (a TEST build may add .<m>), "
                        "not \"${ARCHDOCK_RELEASE}\"")
  endif()
endif()
if(NOT ARCHDOCK_UPDATE_REPOSITORY MATCHES "^[A-Za-z0-9](-?[A-Za-z0-9])*/[A-Za-z0-9_.-]+$"
   OR ARCHDOCK_UPDATE_REPOSITORY MATCHES "/\\.\\.?$")
  message(FATAL_ERROR "ARCHDOCK_UPDATE_REPOSITORY must be owner/name, not \"${ARCHDOCK_UPDATE_REPOSITORY}\"")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchDockUpdateKeys.cmake)
set(archdock_key_list "${CMAKE_CURRENT_SOURCE_DIR}/data/update-keys.txt")
if(ARCHDOCK_UPDATE_TEST_KEYS)
  set(archdock_key_list "${ARCHDOCK_UPDATE_TEST_KEYS}")
endif()
archdock_select_update_key("${ARCHDOCK_BUILD_KIND}" "${archdock_key_list}"
  ARCHDOCK_UPDATE_KEY_ID ARCHDOCK_UPDATE_KEY_HEX)
archdock_update_places("${ARCHDOCK_UPDATE_REHEARSAL_ROOT}" "${ARCHDOCK_UPDATE_REHEARSAL_FEED}"
  "${ARCHDOCK_UPDATE_TEST_KEYS}" "${CMAKE_INSTALL_PREFIX}"
  ARCHDOCK_TOOL_FOLDER ARCHDOCK_PACMAN_LOCK ARCHDOCK_UPDATE_API ARCHDOCK_UPDATE_DOWNLOADS)
set(ARCHDOCK_BACKEND_PATH "${CMAKE_INSTALL_FULL_BINDIR}/arch-dock")
set(ARCHDOCK_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/arch-dock/arch-dock-install-update")
set(ARCHDOCK_FINISHER_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/arch-dock/arch-dock-finish-update")
set(ARCHDOCK_POLICY_PATH
    "${CMAKE_INSTALL_FULL_DATADIR}/polkit-1/actions/org.archdock.ArchDock.policy")
configure_file(cmake/ArchDockVersion.h.in generated/archdock-version-constants.h @ONLY)
message(STATUS "Arch Dock ${ARCHDOCK_BUILD_KIND} build, release \"${ARCHDOCK_RELEASE}\", "
               "update key \"${ARCHDOCK_UPDATE_KEY_ID}\"")
add_library(archdock-version STATIC src/ArchDockVersion.cpp)
target_include_directories(archdock-version
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(archdock-version PUBLIC Qt6::Core)
# What the Update button and its root helper share: reading a release's
# signed update manifest and checking its signature, pacman's order of
# versions and pacman's form of a dependency (src/updates/UpdateCore).
add_library(archdock-update-core STATIC src/updates/UpdateCore.cpp)
target_include_directories(archdock-update-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(archdock-update-core PUBLIC Qt6::Core)
# The update helper, the one part of Arch Dock that runs as root (through the
# password window). Its package rules read packages with libarchive, as
# libalpm does. A test build, never installed, takes its paths and key from a
# test root.
find_package(LibArchive REQUIRED)
add_library(archdock-package-check STATIC src/updates/PackageCheck.cpp)
target_include_directories(archdock-package-check PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(archdock-package-check PUBLIC Qt6::Core LibArchive::LibArchive)
add_executable(arch-dock-install-update src/updates/helper/main.cpp)
target_link_libraries(arch-dock-install-update PRIVATE archdock-update-core archdock-package-check archdock-version)
install(TARGETS arch-dock-install-update RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/arch-dock)
install(PROGRAMS data/arch-dock-finish-update DESTINATION ${CMAKE_INSTALL_LIBDIR}/arch-dock)
# The Update button's notices: popups only, never a sound.
install(FILES data/arch-dock.notifyrc DESTINATION share/knotifications6)
# The Update button's side in the backend: looking on GitHub, the download
# and the commands it runs, with the fence for builds without a release and
# a key.
add_library(archdock-updater STATIC
  src/updates/CommandRunner.cpp src/updates/ReleaseSource.cpp src/updates/UpdateDownload.cpp)
target_include_directories(archdock-updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(archdock-updater PUBLIC archdock-update-core archdock-version Qt6::Core Qt6::Network)

set(ARCHDOCK_ENABLE_QUICK3D AUTO CACHE STRING
    "Optional Qt Quick 3D support: AUTO, OFF, or ON")
set_property(CACHE ARCHDOCK_ENABLE_QUICK3D PROPERTY STRINGS AUTO OFF ON)
if(NOT ARCHDOCK_ENABLE_QUICK3D MATCHES "^(AUTO|OFF|ON)$")
  message(FATAL_ERROR "ARCHDOCK_ENABLE_QUICK3D must be AUTO, OFF, or ON")
endif()
set(ARCHDOCK_QUICK3D_BUILT 0)
set(ARCHDOCK_QUICK3D_QML_BOOL false)
if(NOT ARCHDOCK_ENABLE_QUICK3D STREQUAL "OFF")
  find_package(Qt6Quick3D QUIET)
  if(Qt6Quick3D_FOUND)
    set(ARCHDOCK_QUICK3D_BUILT 1)
    set(ARCHDOCK_QUICK3D_QML_BOOL true)
  elseif(ARCHDOCK_ENABLE_QUICK3D STREQUAL "ON")
    message(FATAL_ERROR "Quick 3D was requested but Qt6Quick3D is unavailable")
  endif()
endif()
# The scene is packaged only when its optional native module was discovered.
set(ARCHDOCK_SCENE3D_BUILT ${ARCHDOCK_QUICK3D_BUILT})
set(ARCHDOCK_SCENE3D_QML_BOOL ${ARCHDOCK_QUICK3D_QML_BOOL})
configure_file(cmake/RendererBuildConfig.h.in
               generated/RendererBuildConfig.h)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")
message(STATUS "Arch Dock optional Quick 3D build: ${ARCHDOCK_QUICK3D_QML_BOOL}")

set(
  ARCHDOCK_QML_INSTALL_DIR
  "${CMAKE_INSTALL_LIBDIR}/qt6/qml"
  CACHE STRING
        "Relative installation root for Arch Dock QML modules")
if(IS_ABSOLUTE "${ARCHDOCK_QML_INSTALL_DIR}" OR
   ARCHDOCK_QML_INSTALL_DIR MATCHES "(^|/)\.\.(/|$)")
  message(
    FATAL_ERROR
      "ARCHDOCK_QML_INSTALL_DIR must be a safe relative installation path")
endif()

set(ARCHDOCK_RENDERING_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/ArchDock/Rendering")
set(ARCHDOCK_RENDERING_IMPORT_ROOT
    "${CMAKE_CURRENT_BINARY_DIR}/qml-imports")
set(ARCHDOCK_RENDERING_BUILD_DIR
    "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Rendering")
set(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/themes")
set(ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-styles")
set(ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/data/icon-styles/builtin-icon-styles.json")
set(ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/data/animation-profiles/builtin-animation-profiles.json")
set(ARCHDOCK_SOURCE_PRESET_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/data/presets")
set(ARCHDOCK_RENDERING_MODULE_FILES
    qmldir
    RendererCapabilityProbe.qml
    RenderingModuleProbe.qml
    HologramClock.qml
    HologramPointer.qml
    LayoutEngine.js
    GizmoMath.js
    PlatformGeometry.js
    IconSolid.js
    ContourFrames.js
    DecorationCatalogue.js
    DecorationScene.js
    DecorationJob.js
    StandFrames.js
    HologramLook.js
    IconStyleResolver.js
    AnimationProfileRuntime.js
    MotionChannels.js
    PresentationStates.js
    ThemeStateSelection.js
    IconScene.qml
    FolderExpansion.qml
    FolderTrack.qml
    IconMotionController.qml
    MotionTrackRunner.qml
    PanelPresentationController.qml
    PanelMotionController.qml
    SceneRotationController.qml
    PanelScene.qml
    PanelSegment.qml
    PanelSurfaceLoader.qml
    RunningIndicator.qml
    inputs/AlphaHitMask.qml
    inputs/GeometryHitRegion.qml
    inputs/ScrollInput.qml
    previews/LivePanelPreview.qml
    previews/DecorationThumbnail.qml
    previews/WindowPreviewPopup.qml
    renderers/IconStyle2D.qml
    renderers/IconTile.qml
    renderers/GlowEffect.qml
    renderers/HologramEffect.qml
    renderers/PanelProcedural2D.qml
    renderers/PanelDecoration2D.qml
    renderers/PanelSkin2D.qml
    renderers/PanelSkinLayer2D.qml
    renderers/PanelBaked25D.qml)
foreach(material glass crystal neon minimal plasma lime floating-glass metallic
                 futuristic organic platform plate pedestal lightning aurora)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/${material}.svg materials/${material}.png
              materials/${material}-normal.png)
endforeach()
# The clear-ground looks' other palettes share their look's relief map.
foreach(palette ice crimson toxic solar arcane void aurora rainbow)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/lightning-${palette}.png)
endforeach()
foreach(look plasma neon futuristic)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/${look}-rainbow.png)
endforeach()
foreach(look metallic plate)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/${look}-chameleon.png
              materials/${look}-iridescent.png)
endforeach()
foreach(palette emerald australis cosmic solar rainbow)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/aurora-${palette}.png)
endforeach()
# The flowing looks are made from real texture sets, not drawn from a
# swatch: a texture, a relief map and the look's palettes.
foreach(look river magma)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/${look}.png materials/${look}-normal.png)
endforeach()
foreach(palette clear deep jade sunset)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/river-${palette}.png)
endforeach()
foreach(palette ember blaze molten)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/magma-${palette}.png)
endforeach()
# The panel decorations' materials: photographed real surfaces (ambientCG's
# CC0 sets, the Webb telescope's nebulae), each a picture and a height map.
foreach(texture agate alien-metal alien-plastic amber-band azure-tiles bamboo bark-birch
         bark-brown bark-cedar bark-grey bark-mossy bark-oak bark-plane battle-plates
         black-gold-vein black-marble carbon carnelian castle-stone concrete console-plastic
         copper corrugated crystal-neutral dark-rock deep-field diamond-plate gilded-porcelain
         gold gold-bright gold-foil gold-neutral gold-polished gravel hull-metal ice-crystal
         iridescent leather-brown leather-red malachite marble marble-polished metal-black
         mosaic moss moss-agate nebula-carina nebula-pillars nebula-southern-ring
         nebula-tarantula obsidian opal paint-green paint-hazard paint-red paint-white
         paper-aged parchment pebbles perforated plaster plastic-black plastic-yellow platinum
         red-jasper rope rubber rubber-grain serpent silver steel-rope straw straw-grey
         travertine travertine-banded travertine-blue velvet volcanic walnut warm-wood wood-pine)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/deco-${texture}.jpg
              materials/deco-${texture}-height.png)
endforeach()
foreach(texture log-end log-end-pale)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES materials/deco-${texture}.png
              materials/deco-${texture}-height.png)
endforeach()
set(ARCHDOCK_RENDERING_MODULE_SOURCES)
set(ARCHDOCK_OPTIONAL_RENDERING_FILES
    optional3d/RendererImportProbe.qml
    optional3d/PanelScene3D.qml
    optional3d/IconStyle3D.qml
    optional3d/IconSolidView.qml
    optional3d/IconSurface.qml
    optional3d/icon-surface.vert
    optional3d/icon-surface.frag
    optional3d/PictureRelief.qml
    optional3d/picture-relief.vert
    optional3d/picture-relief.frag
    optional3d/FlowingSurface.qml
    optional3d/flowing-surface.vert
    optional3d/flowing-surface.frag
    optional3d/PanelDecoration3D.qml
    optional3d/DecorationSurface.qml
    optional3d/decoration-surface.vert
    optional3d/decoration-surface.frag
    optional3d/DecorationTube.qml
    optional3d/decoration-tube.vert
    optional3d/decoration-tube.frag
    optional3d/HologramSurface.qml
    optional3d/hologram-surface.vert
    optional3d/hologram-folded.vert
    optional3d/hologram-surface.frag
    optional3d/HologramSolid.qml
    optional3d/hologram-solid.vert
    optional3d/hologram-solid.frag
    optional3d/HologramObject.qml
    optional3d/hologram-object.vert
    optional3d/hologram-object.frag
    optional3d/HologramHalo.qml
    optional3d/hologram-halo.frag
    optional3d/HologramLight.qml
    optional3d/hologram-light.frag
    optional3d/HologramOpening.qml
    optional3d/hologram-opening.frag)
# These are owned build outputs. Remove stale optional files on ON -> OFF.
foreach(rendering_file IN LISTS ARCHDOCK_OPTIONAL_RENDERING_FILES)
  if(NOT ARCHDOCK_QUICK3D_BUILT)
    file(REMOVE "${ARCHDOCK_RENDERING_BUILD_DIR}/${rendering_file}")
  endif()
endforeach()
if(ARCHDOCK_QUICK3D_BUILT)
  list(APPEND ARCHDOCK_RENDERING_MODULE_FILES ${ARCHDOCK_OPTIONAL_RENDERING_FILES})
endif()

file(MAKE_DIRECTORY "${ARCHDOCK_RENDERING_BUILD_DIR}")
configure_file(
  "${ARCHDOCK_RENDERING_SOURCE_DIR}/RendererBuildConfig.qml.in"
  "${ARCHDOCK_RENDERING_BUILD_DIR}/RendererBuildConfig.qml" @ONLY)
set(ARCHDOCK_RENDERING_COPY_COMMANDS)
foreach(rendering_file IN LISTS ARCHDOCK_RENDERING_MODULE_FILES)
  list(
    APPEND ARCHDOCK_RENDERING_MODULE_SOURCES
    "${ARCHDOCK_RENDERING_SOURCE_DIR}/${rendering_file}")
  get_filename_component(rendering_directory "${rendering_file}" DIRECTORY)
  file(MAKE_DIRECTORY
       "${ARCHDOCK_RENDERING_BUILD_DIR}/${rendering_directory}")
  configure_file(
    "${ARCHDOCK_RENDERING_SOURCE_DIR}/${rendering_file}"
    "${ARCHDOCK_RENDERING_BUILD_DIR}/${rendering_file}"
    COPYONLY)
  list(APPEND ARCHDOCK_RENDERING_COPY_COMMANDS
       COMMAND ${CMAKE_COMMAND} -E copy_if_different
       "${ARCHDOCK_RENDERING_SOURCE_DIR}/${rendering_file}"
       "${ARCHDOCK_RENDERING_BUILD_DIR}/${rendering_file}")
endforeach()

add_custom_target(
  archdock-rendering-module ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${ARCHDOCK_RENDERING_BUILD_DIR}"
  ${ARCHDOCK_RENDERING_COPY_COMMANDS}
  DEPENDS ${ARCHDOCK_RENDERING_MODULE_SOURCES}
  COMMENT "Preparing the ArchDock.Rendering QML module")

# The holograms' flat shaders (Qt Quick shader effects), compiled into Qt's
# shader packs with qsb at build time, for every graphics backend Qt Quick
# draws with.
find_package(Qt6 REQUIRED COMPONENTS ShaderTools)
set(ARCHDOCK_RENDERING_SHADERS hologram hologram-beam hologram-opening seamless)
set(ARCHDOCK_RENDERING_SHADER_PACKS)
file(MAKE_DIRECTORY "${ARCHDOCK_RENDERING_BUILD_DIR}/shaders")
foreach(shader IN LISTS ARCHDOCK_RENDERING_SHADERS)
  set(shader_pack "${ARCHDOCK_RENDERING_BUILD_DIR}/shaders/${shader}.frag.qsb")
  add_custom_command(
    OUTPUT "${shader_pack}"
    COMMAND Qt6::qsb --glsl "100es,120,150" --hlsl 50 --msl 12 -o "${shader_pack}"
            "${ARCHDOCK_RENDERING_SOURCE_DIR}/shaders/${shader}.frag"
    DEPENDS "${ARCHDOCK_RENDERING_SOURCE_DIR}/shaders/${shader}.frag"
    COMMENT "Compiling the ${shader} shader"
    VERBATIM)
  list(APPEND ARCHDOCK_RENDERING_SHADER_PACKS "${shader_pack}")
endforeach()
add_custom_target(archdock-rendering-shaders ALL DEPENDS ${ARCHDOCK_RENDERING_SHADER_PACKS})
add_dependencies(archdock-rendering-module archdock-rendering-shaders)
install(FILES ${ARCHDOCK_RENDERING_SHADER_PACKS}
        DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Rendering/shaders")

qt_add_qml_module(archdock-input SHARED
  URI ArchDock.Input VERSION 1.0
  OUTPUT_DIRECTORY "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Input"
  SOURCES src/inputs/WheelSource.h src/inputs/WindowInputShape.h)
target_link_libraries(archdock-input PRIVATE Qt6::Core Qt6::Gui Qt6::Qml)
target_include_directories(archdock-input PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/inputs")
set_target_properties(archdock-input archdock-inputplugin
  PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS archdock-input archdock-inputplugin
  LIBRARY DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Input")
install(FILES
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Input/qmldir"
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Input/archdock-input.qmltypes"
  DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Input")
add_dependencies(archdock-rendering-module archdock-inputplugin)

# The icons' 3D objects (IconObject.h): their files, and the steps they are
# made by, with PyMCubes' marching cubes (BSD licence) and meshoptimizer's
# simplifier (MIT licence), kept in thirdparty/ with their licences. Shared
# by the decorations plugin (which reads the objects), the icon helper
# (which makes them) and the tests.
add_library(archdock-iconobject STATIC
  src/decorations/IconObject.h src/decorations/IconObject.cpp
  thirdparty/pymcubes/marchingcubes.h thirdparty/pymcubes/marchingcubes.cpp
  thirdparty/meshoptimizer/meshoptimizer.h thirdparty/meshoptimizer/simplifier.cpp
  thirdparty/meshoptimizer/allocator.cpp)
set_target_properties(archdock-iconobject PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(archdock-iconobject PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/decorations")
target_include_directories(archdock-iconobject SYSTEM PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pymcubes" "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/meshoptimizer")
target_link_libraries(archdock-iconobject PUBLIC Qt6::Core Qt6::Gui)
target_compile_options(archdock-iconobject PRIVATE -O2)

# The decoration lighting (panel decorations lit per pixel), a plugin like
# ArchDock.Input so the dock widget in plasmashell loads it too.
qt_add_qml_module(archdock-decorations SHARED
  URI ArchDock.Decorations VERSION 1.0
  OUTPUT_DIRECTORY "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations"
  SOURCES src/decorations/DecorationLighting.h src/decorations/DecorationLighting.cpp
          src/decorations/DecorationRelief.h src/decorations/DecorationRelief.cpp
          src/decorations/DecorationTubes.h src/decorations/DecorationTubes.cpp
          src/decorations/DecorationWorker.h src/decorations/DecorationWorker.cpp
          src/decorations/DecorationBaker.h src/decorations/DecorationBaker.cpp
          src/decorations/DecorationLayer.h src/decorations/DecorationLayer.cpp
          src/decorations/IconModel.h src/decorations/IconModel.cpp)
target_link_libraries(archdock-decorations PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick archdock-iconobject)
# Where the icon helper is installed, for the icons to find it.
target_compile_definitions(archdock-decorations PRIVATE
  ARCHDOCK_ICON_OBJECT_HELPER="${CMAKE_INSTALL_FULL_BINDIR}/arch-dock-icon-object")
# Per-pixel lighting is unusably slow unoptimised, even in a debug build.
set_source_files_properties(src/decorations/DecorationLighting.cpp src/decorations/DecorationRelief.cpp
  src/decorations/DecorationTubes.cpp
  PROPERTIES COMPILE_OPTIONS "-O2")
target_include_directories(archdock-decorations PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/decorations")
set_target_properties(archdock-decorations archdock-decorationsplugin
  PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS archdock-decorations archdock-decorationsplugin
  LIBRARY DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Decorations")
install(FILES
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations/qmldir"
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations/archdock-decorations.qmltypes"
  DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Decorations")
add_dependencies(archdock-rendering-module archdock-decorationsplugin)

# A decoration in a 3D scene: its solid and surface maps as Qt Quick 3D
# geometry and textures. A module of its own, so a flat panel never loads
# Qt Quick 3D for its decoration.
if(ARCHDOCK_QUICK3D_BUILT)
  qt_add_qml_module(archdock-decorations3d SHARED
    URI ArchDock.Decorations3D VERSION 1.0
    OUTPUT_DIRECTORY "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations3D"
    DEPENDENCIES QtQuick3D
    SOURCES src/decorations/DecorationSolid3D.h src/decorations/DecorationSolid3D.cpp
            src/decorations/IconSolid3D.h src/decorations/IconSolid3D.cpp)
  target_link_libraries(archdock-decorations3d PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D
    archdock-decorations)
  target_include_directories(archdock-decorations3d PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/decorations")
  set_target_properties(archdock-decorations3d archdock-decorations3dplugin
    PROPERTIES INSTALL_RPATH "$ORIGIN:$ORIGIN/../Decorations")
  install(TARGETS archdock-decorations3d archdock-decorations3dplugin
    LIBRARY DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Decorations3D")
  install(FILES
    "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations3D/qmldir"
    "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Decorations3D/archdock-decorations3d.qmltypes"
    DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Decorations3D")
  add_dependencies(archdock-rendering-module archdock-decorations3dplugin)
endif()

qt_add_qml_module(archdock-integration SHARED
  URI ArchDock.Integration VERSION 1.0
  OUTPUT_DIRECTORY "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Integration"
  SOURCES src/integration/DropBackend.h)
target_link_libraries(archdock-integration PRIVATE Qt6::Core Qt6::DBus Qt6::Qml)
target_include_directories(archdock-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/integration")
set_target_properties(archdock-integration archdock-integrationplugin
  PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS archdock-integration archdock-integrationplugin
  LIBRARY DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Integration")
install(FILES
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Integration/qmldir"
  "${ARCHDOCK_RENDERING_IMPORT_ROOT}/ArchDock/Integration/archdock-integration.qmltypes"
  DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Integration")

qt_add_library(archdock-profile-backend STATIC
  src/persistence/ConfigurationBackup.h src/persistence/ConfigurationBackup.cpp
  src/persistence/ProfileStore.h src/persistence/ProfileStore.cpp
  src/persistence/RemovedPanelStore.h src/persistence/RemovedPanelStore.cpp
  src/panel/ProfileApplyTransaction.h src/panel/ProfileApplyTransaction.cpp
  src/panel/ProfileManager.h src/panel/ProfileManager.cpp
  src/integration/ProfileShortcutManager.h src/integration/ProfileShortcutManager.cpp
  src/model/PanelDefinition.cpp src/model/PanelIconCapacity.cpp src/model/PanelNaming.cpp src/model/PanelSettingsSchema.cpp
  src/model/PanelRuntimeState.cpp src/model/SettingsMigration.cpp
  src/model/ThemeDefinition.cpp src/themes/ThemePackage.cpp)
target_include_directories(archdock-profile-backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(archdock-profile-backend PUBLIC Qt6::Core PRIVATE Qt6::Gui Qt6::DBus KF6::GlobalAccel)

qt_add_executable(
  arch-dock
  src/main.cpp
  src/updates/ArchDockUpdates.h
  src/updates/ArchDockUpdates.cpp
  src/updates/UpdateHold.h
  src/DockModel.h
  src/DockModel.cpp
  src/model/FolderContentModel.cpp
  src/content/WindowPreviewModel.cpp
  src/DockSettings.h
  src/DockSettings.cpp
  src/PanelRegistry.h
  src/PanelRegistry.cpp
  src/themes/LookPalette.cpp
  src/model/ThemeDefinition.h
  src/model/ThemeDefinition.cpp
  src/model/IconStyleDefinition.h
  src/model/IconStyleDefinition.cpp
  src/model/AnimationProfile.h
  src/model/AnimationProfile.cpp
  src/model/PresetIdentity.h
  src/model/PresetIdentity.cpp
  src/model/IconPresetDefinition.h
  src/model/IconPresetDefinition.cpp
  src/model/PanelPresetDefinition.h
  src/model/PanelPresetDefinition.cpp
  src/presets/PresetCatalog.h
  src/presets/PresetCatalog.cpp
  src/presets/PresetCapabilityResolver.h
  src/presets/PresetCapabilityResolver.cpp
  src/presets/PresetLibrary.h
  src/presets/PresetLibrary.cpp
  src/presets/UserPresetStore.h
  src/presets/UserPresetStore.cpp
  src/presets/PresetApplication.h
  src/presets/PresetApplication.cpp
  src/presets/PresetDefaultStore.h
  src/presets/PresetDefaultStore.cpp
  src/presets/PresetPreviewRecovery.h
  src/presets/PresetPreviewRecovery.cpp
  src/presets/PresetPreviewSession.h
  src/presets/PresetPreviewSession.cpp
  src/iconstyles/IconStylePackage.h
  src/iconstyles/IconStylePackage.cpp
  src/iconstyles/IconStyleStore.h
  src/iconstyles/IconStyleStore.cpp
  src/animation/AnimationProfileCatalog.h
  src/animation/AnimationProfileCatalog.cpp
  src/themes/ThemeAssetProcessor.h
  src/themes/ThemeAssetProcessor.cpp
  src/themes/ThemePackage.h
  src/themes/ThemePackage.cpp
  src/model/PanelDefinition.h
  src/model/PanelDefinition.cpp
  src/model/PanelIconCapacity.cpp
  src/model/PanelNaming.cpp
  src/model/IconEntryIdentity.h
  src/model/IconEntryIdentity.cpp
  src/model/PanelSettingsSchema.h
  src/model/PanelSettingsSchema.cpp
  src/model/PanelCapabilityResolver.h
  src/model/PanelCapabilityResolver.cpp
  src/model/PanelRuntimeState.h
  src/model/PanelRuntimeState.cpp
  src/model/SettingsMigration.h
  src/model/SettingsMigration.cpp
  src/NativeContainmentLifecycle.h
  src/NativeContainmentLifecycle.cpp
  src/PlasmaScriptResult.h
  src/PlasmaScriptResult.cpp
  src/PanelPlacement.h
  src/PanelPlacement.cpp
  src/integration/PlasmaPanelAdapter.h
  src/integration/PlasmaPanelScreenGuard.h
  src/integration/PlasmaPanelAdapter.cpp
  src/PanelVisibility.h
  src/PanelVisibility.cpp
  src/ScreenIdentity.h
  src/ScreenIdentity.cpp
  src/SystemStatus.h
  src/SystemStatus.cpp
  src/content/OverlayModel.h
  src/content/OverlayModel.cpp
  src/KWinActionBridge.h
  src/KWinActionBridge.cpp
  src/panel/PanelManager.h
  src/panel/PanelManager.cpp
  src/panel/FreePanelController.h
  src/panel/FreePanelController.cpp
  src/panel/PanelSettingsTransaction.h
  src/panel/PanelSettingsTransaction.cpp
  src/panel/PanelContentTransaction.h
  src/panel/PanelContentTransaction.cpp
  src/panel/IconOverrideTransaction.h
  src/panel/IconOverrideTransaction.cpp
  src/panel/PanelWindow.h
  src/panel/PanelWindow.cpp
  src/panel/PanelWindowHelpers.h
  src/panel/PanelWindowEntries.cpp
  src/panel/PanelWindowFreePanels.cpp
  src/panel/PanelWindowNativePanels.cpp
  src/panel/PanelWindowNativePlacement.cpp
  src/panel/FreeGeometrySettle.h
  src/panel/PanelWindowPresets.cpp
  src/panel/PanelWindowScreens.cpp
  src/panel/PanelWindowSettings.cpp
  src/panel/PanelWindowBars.cpp
  src/panel/BarHostSupervisor.h
  src/panel/BarHostSupervisor.cpp
  src/panel/BarAutostart.h
  src/panel/BarAutostart.cpp
  src/bars/BarWidgets.h
  src/bars/BarWidgets.cpp
  src/WindowItem.h
  src/WindowModel.h
  src/WindowModel.cpp
  src/WindowWatcher.h
  src/WindowWatcher.cpp
  src/IntentionalStop.h
  src/IntentionalStop.cpp)
set(ARCHDOCK_APPLICATION_QML_FILES
  qml/runtime/CapabilityModel.js
  qml/runtime/IconPropertiesDraft.js
  qml/runtime/IconProperties.qml
  qml/runtime/IconPropertiesWindow.qml
  qml/runtime/PlacementStatus.js
  qml/runtime/PresetBrowser.qml
  qml/runtime/PresetAuditionBar.qml
  qml/runtime/PresetCard.qml
  qml/runtime/ProfilePage.qml
  qml/runtime/SettingsEditorModel.js
  qml/runtime/VisibilityStatus.js
  qml/runtime/StudioCatalogueTree.qml
  qml/runtime/StudioForm.qml
  qml/runtime/StudioFrame.qml
  qml/runtime/StudioHoneycomb.qml
  qml/runtime/StudioLightStrip.qml
  qml/runtime/StudioNavigation.js
  qml/runtime/StudioThemePreview.qml
  qml/runtime/StudioThemes.js
  qml/runtime/StudioThemesPage.qml
  qml/runtime/StudioUpdatesPage.qml
  qml/runtime/StudioWidgetsPage.qml
  qml/runtime/ThemeSampleCard.qml
  qml/runtime/SettingsPopup.qml)
# The backend's QML is compiled to Qt's portable byte code only. Compiled to
# C++ it would call into the QML engine's private interface, which changes
# with every Qt minor release, and a Qt update would then stop Arch Dock,
# Panel Studio and its Update button from starting at all.
set_target_properties(arch-dock PROPERTIES QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode")
qt_add_qml_module(
  arch-dock
  URI ArchDock
  VERSION 1.0
  NO_IMPORT_SCAN
  # Debug builds otherwise still check lookups through the private interface.
  NO_GENERATE_AOT_VALIDATION
  IMPORT_PATH ${ARCHDOCK_RENDERING_IMPORT_ROOT}
  QML_FILES ${ARCHDOCK_APPLICATION_QML_FILES})
add_dependencies(arch-dock archdock-rendering-module)
qt_add_resources(
  arch-dock
  theme-catalog
  PREFIX "/archdock"
  FILES data/themes/builtin-themes.json)
qt_add_resources(
  arch-dock
  icon-style-catalog
  PREFIX "/archdock"
  FILES data/icon-styles/builtin-icon-styles.json)
qt_add_resources(
  arch-dock
  animation-profile-catalog
  PREFIX "/archdock"
  FILES data/animation-profiles/builtin-animation-profiles.json)

target_link_libraries(
  arch-dock
  PRIVATE archdock-profile-backend
          archdock-updater
          archdock-version
          KF6::Notifications
          Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::Network
          Qt6::Qml
          Qt6::Quick
          Qt6::Widgets
          KF6::Kirigami
          KF6::Service
          KF6::KIOGui
          KF6::Package)

target_compile_definitions(
  arch-dock
  PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js"
          ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
          ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
          ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}"
          ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}")

# Arch Dock's own bars: a program beside the backend that shows each edge bar
# as a Plasma containment (libplasma, as Plasma's plasmoidviewer hosts
# widgets) in a layer-shell window of its own, so that bars can stand side by
# side on one screen edge.
# KF6Package's CMake macros pull in extra-cmake-modules only when these are
# not set already.
if(NOT DEFINED KDE_INSTALL_DATADIR)
  set(KDE_INSTALL_DATADIR share)
endif()
if(NOT DEFINED KDE_INSTALL_METAINFODIR)
  set(KDE_INSTALL_METAINFODIR share/metainfo)
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets WaylandClient GuiPrivate)
find_package(KF6Config REQUIRED)
find_package(KF6Package REQUIRED)
find_package(Plasma 6.8 REQUIRED)
find_package(PlasmaQuick 6.8 REQUIRED)
find_package(LayerShellQt 6.8 REQUIRED)
set(ARCHDOCK_BAR_LOGIC_SOURCES
    src/bars/BarLayout.h src/bars/BarLayout.cpp
    src/bars/BarRecord.h src/bars/BarRecord.cpp
    src/bars/BarWidgets.h src/bars/BarWidgets.cpp)
add_executable(
  arch-dock-bars
  src/bars/main.cpp
  src/bars/BarCorona.h
  src/bars/BarCorona.cpp
  src/bars/BarHost.h
  src/bars/BarHost.cpp
  src/bars/BarWindow.h
  src/bars/BarWindow.cpp
  src/bars/ScreenEdge.h
  src/bars/ScreenEdge.cpp
  ${ARCHDOCK_BAR_LOGIC_SOURCES}
  src/PanelVisibility.h
  src/PanelVisibility.cpp
  src/ScreenIdentity.h
  src/ScreenIdentity.cpp)
# KWin's auto-hide screen edges, by which bars that hide come back: the
# protocol from Plasma's Wayland protocols (MIT-CMU), kept beside the bars.
# Its generated glue is C.
enable_language(C)
qt6_generate_wayland_protocol_client_sources(arch-dock-bars PRIVATE_CODE
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/bars/protocols/kde-screen-edge-v1.xml)
target_include_directories(arch-dock-bars PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(
  arch-dock-bars
  PRIVATE archdock-version
          Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::GuiPrivate
          Qt6::Quick
          Qt6::WaylandClient
          Qt6::Widgets
          KF6::ConfigCore
          KF6::ConfigGui
          KF6::Package
          Plasma::Plasma
          Plasma::PlasmaQuick
          LayerShellQt::Interface)

include(CTest)
if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test QmlTools QuickTest)

  qt_add_executable(drop-delivery-test tests/DropDeliveryTest.cpp)
  target_link_libraries(drop-delivery-test PRIVATE
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
  target_compile_definitions(drop-delivery-test PRIVATE
    ARCHDOCK_RENDERING_IMPORT_ROOT="${ARCHDOCK_RENDERING_IMPORT_ROOT}"
    ARCHDOCK_DOCK_UI_URL="file:${CMAKE_CURRENT_SOURCE_DIR}/plasma-dock-widget/contents/ui")
  add_dependencies(drop-delivery-test archdock-integrationplugin archdock-rendering-module)
  add_test(NAME drop-delivery-test COMMAND dbus-run-session -- $<TARGET_FILE:drop-delivery-test>)
  set_tests_properties(drop-delivery-test PROPERTIES
    TIMEOUT 30 RUN_SERIAL TRUE
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=org.kde.desktop;QT_NO_XDG_DESKTOP_PORTAL=1;QT_QPA_PLATFORMTHEME=generic")

  # The decoration lighting, by itself: what a scene's pictures hold.
  qt_add_executable(decoration-lighting-test tests/DecorationLightingTest.cpp
    src/decorations/DecorationLighting.h src/decorations/DecorationLighting.cpp
    src/decorations/DecorationRelief.h src/decorations/DecorationRelief.cpp
    src/decorations/DecorationTubes.h src/decorations/DecorationTubes.cpp)
  target_include_directories(decoration-lighting-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/decorations)
  target_link_libraries(decoration-lighting-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  target_compile_definitions(decoration-lighting-test PRIVATE
    ARCHDOCK_DECORATION_MATERIALS="${CMAKE_CURRENT_SOURCE_DIR}/qml/ArchDock/Rendering/materials")
  add_test(NAME decoration-lighting-test COMMAND decoration-lighting-test)
  set_tests_properties(decoration-lighting-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # The icons' objects, by themselves: the steps they are made by, on
  # fields drawn for the test, and their files.
  qt_add_executable(icon-object-test tests/IconObjectTest.cpp)
  target_link_libraries(icon-object-test PRIVATE archdock-iconobject Qt6::Test)
  set_source_files_properties(tests/IconObjectTest.cpp PROPERTIES COMPILE_OPTIONS "-O2")
  add_test(NAME icon-object-test COMMAND icon-object-test)
  set_tests_properties(icon-object-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # An icon's object as the 3D scenes take it (IconModel): asked of a
  # stand-in helper, kept in the cache, laid in the picture's frame.
  qt_add_executable(icon-model-test tests/IconModelTest.cpp
    src/decorations/IconModel.h src/decorations/IconModel.cpp
    src/decorations/DecorationWorker.h src/decorations/DecorationWorker.cpp)
  target_include_directories(icon-model-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/decorations)
  target_link_libraries(icon-model-test PRIVATE archdock-iconobject Qt6::Gui Qt6::Qml Qt6::Test)
  add_test(NAME icon-model-test COMMAND icon-model-test)
  set_tests_properties(icon-model-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # Arch Dock's own bars shown by the bar host in a private KWin: side by side
  # on an edge, each where its list puts it, windows kept clear of them.
  add_test(NAME bar-host-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            ARCHDOCK_QML_IMPORT_ROOT=${ARCHDOCK_RENDERING_IMPORT_ROOT}
            ARCHDOCK_QML_TOOL=$<TARGET_FILE_DIR:Qt6::qmltestrunner>/qml
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-bar-host-smoke.sh)
  set_tests_properties(bar-host-smoke PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

  # A bar's window: the bar at its edge, the room past it for its dock's
  # stands and holograms, and the gap at the screen's edge a hiding bar's
  # window covers.
  add_test(
    NAME bar-view-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_BarView.qml)
  set_tests_properties(bar-view-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # Arch Dock's bars, by themselves: where each stands on its edge, and what
  # the backend's list of them says.
  qt_add_executable(bar-autostart-test tests/BarAutostartTest.cpp
    src/panel/BarAutostart.h src/panel/BarAutostart.cpp)
  target_include_directories(bar-autostart-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(bar-autostart-test PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME bar-autostart-test COMMAND bar-autostart-test)

  foreach(bar_test IN ITEMS BarLayout BarRecord BarWidgets)
    string(TOLOWER "${bar_test}" bar_test_name)
    qt_add_executable(${bar_test_name}-test tests/${bar_test}Test.cpp ${ARCHDOCK_BAR_LOGIC_SOURCES})
    target_include_directories(${bar_test_name}-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${bar_test_name}-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME ${bar_test_name}-test COMMAND ${bar_test_name}-test)
  endforeach()

  qt_add_executable(overlay-model-test tests/OverlayModelTest.cpp
    src/content/OverlayModel.h src/content/OverlayModel.cpp
    src/model/IconEntryIdentity.cpp)
  target_include_directories(overlay-model-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(overlay-model-test PRIVATE Qt6::Core Qt6::DBus Qt6::Test KF6::Service)
  add_test(NAME overlay-model-test COMMAND overlay-model-test)
  add_test(NAME overlay-source-smoke
    COMMAND ${CMAKE_COMMAND} -E env ARCHDOCK_OVERLAY_SOURCE_TEST=1
      dbus-run-session -- $<TARGET_FILE:overlay-model-test> supportedDbusSourceUpdatesAndDisconnects)

  qt_add_executable(system-status-test tests/SystemStatusTest.cpp
    src/SystemStatus.h src/SystemStatus.cpp)
  target_include_directories(system-status-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(system-status-test PRIVATE Qt6::Core Qt6::Network Qt6::Test)
  add_test(NAME system-status-test COMMAND system-status-test)

  add_test(NAME content-overlays-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_ContentOverlays.qml)
  set_tests_properties(content-overlays-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  qt_add_executable(
    panel-model-test
    tests/PanelModelTest.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp
    src/model/SettingsMigration.h
    src/model/SettingsMigration.cpp)
  target_include_directories(
    panel-model-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-model-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(NAME panel-model-test COMMAND panel-model-test)

  qt_add_executable(profile-store-test tests/ProfileStoreTest.cpp)
  qt_add_executable(configuration-backup-test tests/ConfigurationBackupTest.cpp)
  qt_add_executable(profile-apply-transaction-test tests/ProfileApplyTransactionTest.cpp)
  qt_add_executable(profile-manager-test tests/ProfileManagerTest.cpp)
  qt_add_executable(profile-shortcut-manager-test tests/ProfileShortcutManagerTest.cpp)
  qt_add_executable(removed-panel-store-test tests/RemovedPanelStoreTest.cpp)
  foreach(profile_test configuration-backup-test profile-store-test profile-apply-transaction-test profile-manager-test profile-shortcut-manager-test removed-panel-store-test)
    target_link_libraries(${profile_test} PRIVATE archdock-profile-backend Qt6::Test)
    add_test(NAME ${profile_test} COMMAND ${profile_test})
  endforeach()

  qt_add_executable(
    preset-definition-test
    tests/PresetDefinitionTest.cpp
    src/model/PresetIdentity.h
    src/model/PresetIdentity.cpp
    src/model/IconPresetDefinition.h
    src/model/IconPresetDefinition.cpp
    src/model/PanelPresetDefinition.h
    src/model/PanelPresetDefinition.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    preset-definition-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    preset-definition-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(NAME preset-definition-test COMMAND preset-definition-test)

  qt_add_executable(
    preset-catalog-test
    tests/PresetCatalogTest.cpp
    src/presets/PresetCatalog.h
    src/presets/PresetCatalog.cpp
    src/presets/PresetCapabilityResolver.h
    src/presets/PresetCapabilityResolver.cpp
    src/presets/UserPresetStore.h
    src/presets/UserPresetStore.cpp
    src/model/PresetIdentity.h
    src/model/PresetIdentity.cpp
    src/model/IconPresetDefinition.h
    src/model/IconPresetDefinition.cpp
    src/model/PanelPresetDefinition.h
    src/model/PanelPresetDefinition.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp
    src/model/AnimationProfile.h
    src/model/AnimationProfile.cpp
    src/animation/AnimationProfileCatalog.h
    src/animation/AnimationProfileCatalog.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    preset-catalog-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(
    preset-catalog-test
    PRIVATE ARCHDOCK_SOURCE_THEME_CATALOG_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/themes/builtin-themes.json"
            ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}")
  target_link_libraries(
    preset-catalog-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME preset-catalog-test COMMAND preset-catalog-test)

  get_target_property(archdock_preset_preview_test_sources preset-catalog-test SOURCES)
  list(REMOVE_ITEM archdock_preset_preview_test_sources tests/PresetCatalogTest.cpp)
  qt_add_executable(
    preset-preview-session-test
    tests/PresetPreviewSessionTest.cpp
    ${archdock_preset_preview_test_sources}
    src/presets/PresetApplication.h
    src/presets/PresetApplication.cpp
    src/presets/PresetDefaultStore.h
    src/presets/PresetDefaultStore.cpp
    src/presets/PresetPreviewRecovery.h
    src/presets/PresetPreviewRecovery.cpp
    src/presets/PresetPreviewSession.h
    src/presets/PresetPreviewSession.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp)
  target_include_directories(preset-preview-session-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(preset-preview-session-test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
  target_compile_definitions(preset-preview-session-test PRIVATE
    ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}")
  add_test(NAME preset-preview-session-test COMMAND preset-preview-session-test)

  qt_add_executable(
    preset-library-test
    tests/PresetLibraryTest.cpp
    src/presets/PresetLibrary.h
    src/presets/PresetLibrary.cpp
    src/presets/PresetCatalog.h
    src/presets/PresetCatalog.cpp
    src/presets/PresetCapabilityResolver.h
    src/presets/PresetCapabilityResolver.cpp
    src/presets/UserPresetStore.h
    src/presets/UserPresetStore.cpp
    src/model/PresetIdentity.h
    src/model/PresetIdentity.cpp
    src/model/IconPresetDefinition.h
    src/model/IconPresetDefinition.cpp
    src/model/PanelPresetDefinition.h
    src/model/PanelPresetDefinition.cpp
    src/PanelRegistry.h
    src/PanelRegistry.cpp
    src/themes/LookPalette.cpp
    src/DockSettings.h
    src/DockSettings.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp
    src/model/AnimationProfile.h
    src/model/AnimationProfile.cpp
    src/animation/AnimationProfileCatalog.h
    src/animation/AnimationProfileCatalog.cpp
    src/themes/ThemeAssetProcessor.h
    src/themes/ThemeAssetProcessor.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp
    src/model/SettingsMigration.h
    src/model/SettingsMigration.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp
    src/panel/IconOverrideTransaction.h
    src/panel/IconOverrideTransaction.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp)
  target_include_directories(
    preset-library-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(
    preset-library-test
    PRIVATE ARCHDOCK_SOURCE_THEME_CATALOG_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/themes/builtin-themes.json"
            ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}"
            ARCHDOCK_RENDERING_IMPORT_ROOT="${ARCHDOCK_RENDERING_IMPORT_ROOT}"
            ARCHDOCK_SOURCE_RUNTIME_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml/runtime")
  target_link_libraries(
    preset-library-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::Test)
  add_dependencies(preset-library-test archdock-rendering-module)
  add_test(NAME preset-library-test COMMAND preset-library-test)
  set_tests_properties(
    preset-library-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/preset-library-config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/preset-library-data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/preset-library-cache")

  qt_add_executable(
    icon-entry-identity-test
    tests/IconEntryIdentityTest.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp)
  target_include_directories(
    icon-entry-identity-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    icon-entry-identity-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME icon-entry-identity-test
    COMMAND icon-entry-identity-test)

  qt_add_executable(
    theme-package-test
    tests/ThemePackageTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp)
  target_include_directories(
    theme-package-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    theme-package-test
    PRIVATE Qt6::Core
            Qt6::Qml
            Qt6::Test)
  add_test(NAME theme-package-test COMMAND theme-package-test)

  qt_add_executable(
    icon-style-package-test
    tests/IconStylePackageTest.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp)
  target_include_directories(
    icon-style-package-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    icon-style-package-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME icon-style-package-test COMMAND icon-style-package-test)

  qt_add_executable(
    icon-style-asset-test
    tests/IconStyleAssetTest.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp)
  target_include_directories(
    icon-style-asset-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    icon-style-asset-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME icon-style-asset-test COMMAND icon-style-asset-test)

  qt_add_executable(
    chassis-asset-test
    tests/ChassisAssetTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp)
  target_include_directories(
    chassis-asset-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    chassis-asset-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME chassis-asset-test COMMAND chassis-asset-test)

  qt_add_executable(
    energy-asset-test
    tests/EnergyAssetTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp)
  target_include_directories(
    energy-asset-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    energy-asset-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME energy-asset-test COMMAND energy-asset-test)

  qt_add_executable(
    baked-25d-asset-test
    tests/Baked25DAssetTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp)
  target_include_directories(
    baked-25d-asset-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    baked-25d-asset-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME baked-25d-asset-test COMMAND baked-25d-asset-test)
  set_tests_properties(
    baked-25d-asset-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  qt_add_executable(
    source-asset-catalog-test
    tests/SourceAssetCatalogTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/SourceAssetCatalog.h
    src/themes/SourceAssetCatalog.cpp)
  target_include_directories(
    source-asset-catalog-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    source-asset-catalog-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME source-asset-catalog-test
    COMMAND source-asset-catalog-test)

  qt_add_executable(
    theme-asset-processor-test
    tests/ThemeAssetProcessorTest.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/themes/ThemeAssetProcessor.h
    src/themes/ThemeAssetProcessor.cpp)
  target_include_directories(
    theme-asset-processor-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    theme-asset-processor-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(
    NAME theme-asset-processor-test
    COMMAND theme-asset-processor-test)

  qt_add_executable(
    panel-settings-schema-test
    tests/PanelSettingsSchemaTest.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    panel-settings-schema-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-settings-schema-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME panel-settings-schema-test
    COMMAND panel-settings-schema-test)

  qt_add_executable(
    animation-profile-test
    tests/AnimationProfileTest.cpp
    src/model/AnimationProfile.h
    src/model/AnimationProfile.cpp
    src/animation/AnimationProfileCatalog.h
    src/animation/AnimationProfileCatalog.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp
    src/model/SettingsMigration.h
    src/model/SettingsMigration.cpp)
  target_include_directories(
    animation-profile-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(
    animation-profile-test
    PRIVATE ARCHDOCK_ANIMATION_PROFILE_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/animation-profile-v1"
            ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}")
  target_link_libraries(
    animation-profile-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
  add_test(NAME animation-profile-test COMMAND animation-profile-test)

  qt_add_executable(renderer-capability-test tests/RendererCapabilityTest.cpp
    src/model/ThemeDefinition.cpp src/themes/ThemePackage.cpp src/themes/LookPalette.cpp)
  target_include_directories(renderer-capability-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(renderer-capability-test PRIVATE Qt6::Quick Qt6::Test)
  target_compile_definitions(renderer-capability-test PRIVATE
    ARCHDOCK_RENDERING_IMPORT_ROOT="${ARCHDOCK_RENDERING_IMPORT_ROOT}"
    ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}")
  add_dependencies(renderer-capability-test archdock-rendering-module)
  add_test(NAME renderer-capability-software-test COMMAND renderer-capability-test)
  # No models for the icons' objects: the tests see the pictures' reliefs.
  set_tests_properties(renderer-capability-software-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ARCHDOCK_ICON_OBJECT_MODELS=/nonexistent")
  add_test(NAME renderer-capability-missing-module-test COMMAND renderer-capability-test)
  set_tests_properties(renderer-capability-missing-module-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ARCHDOCK_TEST_MISSING_3D=1;ARCHDOCK_ICON_OBJECT_MODELS=/nonexistent")

  qt_add_executable(
    panel-capability-resolver-test
    tests/PanelCapabilityResolverTest.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    panel-capability-resolver-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-capability-resolver-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME panel-capability-resolver-test
    COMMAND panel-capability-resolver-test)

  qt_add_executable(
    panel-settings-transaction-test
    tests/PanelSettingsTransactionTest.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    panel-settings-transaction-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-settings-transaction-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME panel-settings-transaction-test
    COMMAND panel-settings-transaction-test)

  qt_add_executable(
    panel-content-transaction-test
    tests/PanelContentTransactionTest.cpp
    src/panel/PanelContentTransaction.h
    src/panel/PanelContentTransaction.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    panel-content-transaction-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-content-transaction-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME panel-content-transaction-test
    COMMAND panel-content-transaction-test)

  qt_add_executable(
    icon-override-transaction-test
    tests/IconOverrideTransactionTest.cpp
    src/panel/IconOverrideTransaction.h
    src/panel/IconOverrideTransaction.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp)
  target_include_directories(
    icon-override-transaction-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    icon-override-transaction-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME icon-override-transaction-test
    COMMAND icon-override-transaction-test)

  qt_add_executable(
    panel-registry-test
    tests/PanelRegistryTest.cpp
    src/model/PresetIdentity.h
    src/model/PresetIdentity.cpp
    src/model/IconPresetDefinition.h
    src/model/IconPresetDefinition.cpp
    src/model/PanelPresetDefinition.h
    src/model/PanelPresetDefinition.cpp
    src/presets/PresetCatalog.h
    src/presets/PresetCatalog.cpp
    src/presets/PresetCapabilityResolver.h
    src/presets/PresetCapabilityResolver.cpp
    src/DockSettings.h
    src/DockSettings.cpp
    src/PanelRegistry.h
    src/PanelRegistry.cpp
    src/themes/LookPalette.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp
    src/model/AnimationProfile.h
    src/model/AnimationProfile.cpp
    src/animation/AnimationProfileCatalog.h
    src/animation/AnimationProfileCatalog.cpp
    src/themes/ThemeAssetProcessor.h
    src/themes/ThemeAssetProcessor.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp
    src/model/SettingsMigration.h
    src/model/SettingsMigration.cpp
    src/NativeContainmentLifecycle.h
    src/NativeContainmentLifecycle.cpp
    src/panel/FreePanelController.h
    src/panel/FreePanelController.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp
    src/panel/IconOverrideTransaction.h
    src/panel/IconOverrideTransaction.cpp
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
    src/DockModel.cpp
    src/model/FolderContentModel.cpp
    src/content/WindowPreviewModel.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp)
  target_include_directories(
    panel-registry-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-registry-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test
            KF6::Service
            KF6::KIOGui)
  target_compile_definitions(
    panel-registry-test
    PRIVATE ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}")
  add_test(NAME panel-registry-test COMMAND panel-registry-test)
  set_tests_properties(
    panel-registry-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/panel-registry")

  qt_add_executable(folder-content-model-test
    tests/FolderContentModelTest.cpp
    src/model/FolderContentModel.cpp)
  target_include_directories(folder-content-model-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(folder-content-model-test PRIVATE Qt6::Core Qt6::Test KF6::KIOCore)
  add_test(NAME folder-content-model-test COMMAND folder-content-model-test)

  qt_add_executable(window-preview-model-test
    tests/WindowPreviewModelTest.cpp
    src/content/WindowPreviewModel.cpp
    src/WindowModel.h
    src/WindowModel.cpp)
  target_include_directories(window-preview-model-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(window-preview-model-test PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME window-preview-model-test COMMAND window-preview-model-test)

  qt_add_executable(
    dock-model-test
    tests/DockModelTest.cpp
    src/DockModel.h
    src/DockModel.cpp
    src/model/FolderContentModel.cpp
    src/content/WindowPreviewModel.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp
    src/WindowItem.h
    src/WindowModel.h
    src/WindowModel.cpp)
  target_include_directories(
    dock-model-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    dock-model-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test
            KF6::Service
            KF6::KIOGui)
  add_test(NAME dock-model-test COMMAND dock-model-test)
  set_tests_properties(
    dock-model-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  qt_add_executable(
    panel-window-capability-test
    tests/PanelWindowCapabilityTest.cpp
    src/DockModel.h
    src/DockModel.cpp
    src/model/FolderContentModel.cpp
    src/content/WindowPreviewModel.cpp
    src/DockSettings.h
    src/DockSettings.cpp
    src/PanelRegistry.h
    src/PanelRegistry.cpp
    src/themes/LookPalette.cpp
    src/model/ThemeDefinition.h
    src/model/ThemeDefinition.cpp
    src/model/IconStyleDefinition.h
    src/model/IconStyleDefinition.cpp
    src/iconstyles/IconStylePackage.h
    src/iconstyles/IconStylePackage.cpp
    src/iconstyles/IconStyleStore.h
    src/iconstyles/IconStyleStore.cpp
    src/model/AnimationProfile.h
    src/model/AnimationProfile.cpp
    src/animation/AnimationProfileCatalog.h
    src/animation/AnimationProfileCatalog.cpp
    src/themes/ThemeAssetProcessor.h
    src/themes/ThemeAssetProcessor.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
    src/model/PanelIconCapacity.cpp
    src/model/PanelNaming.cpp
    src/model/PanelSettingsSchema.h
    src/model/PanelSettingsSchema.cpp
    src/model/PanelCapabilityResolver.h
    src/model/PanelCapabilityResolver.cpp
    src/model/PanelRuntimeState.h
    src/model/PanelRuntimeState.cpp
    src/model/SettingsMigration.h
    src/model/SettingsMigration.cpp
    src/NativeContainmentLifecycle.h
    src/NativeContainmentLifecycle.cpp
    src/PlasmaScriptResult.h
    src/PlasmaScriptResult.cpp
    src/PanelPlacement.h
    src/PanelPlacement.cpp
    src/integration/PlasmaPanelAdapter.h
    src/integration/PlasmaPanelScreenGuard.h
    src/integration/PlasmaPanelAdapter.cpp
    src/PanelVisibility.h
    src/PanelVisibility.cpp
    src/ScreenIdentity.h
    src/ScreenIdentity.cpp
    src/SystemStatus.h
    src/SystemStatus.cpp
    src/content/OverlayModel.h
    src/content/OverlayModel.cpp
    src/KWinActionBridge.h
    src/KWinActionBridge.cpp
    src/panel/PanelManager.h
    src/panel/PanelManager.cpp
    src/panel/FreePanelController.h
    src/panel/FreePanelController.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp
    src/panel/PanelContentTransaction.h
    src/panel/PanelContentTransaction.cpp
    src/panel/IconOverrideTransaction.h
    src/panel/IconOverrideTransaction.cpp
    src/panel/PanelWindow.h
    src/panel/PanelWindow.cpp
    src/panel/PanelWindowHelpers.h
    src/panel/PanelWindowEntries.cpp
    src/panel/PanelWindowFreePanels.cpp
    src/panel/PanelWindowNativePanels.cpp
    src/panel/PanelWindowNativePlacement.cpp
    src/panel/FreeGeometrySettle.h
    src/panel/PanelWindowPresets.cpp
    src/panel/PanelWindowScreens.cpp
    src/panel/PanelWindowSettings.cpp
    src/panel/PanelWindowBars.cpp
    src/panel/BarHostSupervisor.h
    src/panel/BarHostSupervisor.cpp
    src/panel/BarAutostart.h
    src/panel/BarAutostart.cpp
    src/bars/BarWidgets.h
    src/bars/BarWidgets.cpp
    src/model/PresetIdentity.h
    src/model/PresetIdentity.cpp
    src/model/IconPresetDefinition.h
    src/model/IconPresetDefinition.cpp
    src/model/PanelPresetDefinition.h
    src/model/PanelPresetDefinition.cpp
    src/presets/PresetCatalog.h
    src/presets/PresetCatalog.cpp
    src/presets/PresetCapabilityResolver.h
    src/presets/PresetCapabilityResolver.cpp
    src/presets/PresetLibrary.h
    src/presets/PresetLibrary.cpp
    src/presets/UserPresetStore.h
    src/presets/UserPresetStore.cpp
    src/presets/PresetApplication.h
    src/presets/PresetApplication.cpp
    src/presets/PresetDefaultStore.h
    src/presets/PresetDefaultStore.cpp
    src/presets/PresetPreviewRecovery.h
    src/presets/PresetPreviewRecovery.cpp
    src/presets/PresetPreviewSession.h
    src/presets/PresetPreviewSession.cpp
    src/WindowItem.h
    src/WindowModel.h
    src/WindowModel.cpp
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp
    src/WindowWatcher.h
    src/WindowWatcher.cpp
    src/IntentionalStop.h
    src/IntentionalStop.cpp
    src/updates/ArchDockUpdates.h
    src/updates/ArchDockUpdates.cpp)
  target_compile_definitions(panel-window-capability-test PRIVATE ARCHDOCK_TESTING)
  qt_add_resources(
    panel-window-capability-test
    panel-window-capability-theme-catalog
    PREFIX "/archdock"
    FILES data/themes/builtin-themes.json)
  qt_add_resources(
    panel-window-capability-test
    panel-window-capability-icon-properties-runtime
    PREFIX "/qt/qml/ArchDock/qml/runtime"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml/runtime"
    FILES
    qml/runtime/IconPropertiesDraft.js
    qml/runtime/IconProperties.qml
    qml/runtime/IconPropertiesWindow.qml)
  target_include_directories(
    panel-window-capability-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-window-capability-test
    PRIVATE archdock-profile-backend archdock-updater archdock-version KF6::Notifications Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Network
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
            Qt6::QuickTest
            KF6::Kirigami
            KF6::Service
            KF6::KIOGui
            KF6::Package)
  target_compile_definitions(
    panel-window-capability-test
    PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js"
            ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}")
  qt_add_resources(panel-window-capability-test utility-runtime
    PREFIX "/qt/qml/ArchDock" FILES ${ARCHDOCK_APPLICATION_QML_FILES})
  add_dependencies(panel-window-capability-test archdock-rendering-module)
  add_test(
    NAME panel-window-capability-test
    COMMAND panel-window-capability-test)
  # The update guard, from the same backend build.
  add_test(NAME update-guard-test COMMAND panel-window-capability-test updateGuardHoldsEveryChange
    updateControlsStayOffTheBus quitWaitsForTheUpdate)
  set_tests_properties(update-guard-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/update-guard-config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/update-guard-data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/update-guard-cache")
  set_tests_properties(
    panel-window-capability-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/panel-window-config;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/panel-window-data;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data/panel-window-cache")

  qt_add_executable(
    panel-placement-test
    tests/PanelPlacementTest.cpp
    src/PanelPlacement.h
    src/PanelPlacement.cpp)
  target_include_directories(
    panel-placement-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-placement-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(NAME panel-placement-test COMMAND panel-placement-test)

  qt_add_executable(
    panel-visibility-test
    tests/PanelVisibilityTest.cpp
    src/PanelVisibility.h
    src/PanelVisibility.cpp)
  target_include_directories(
    panel-visibility-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-visibility-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(NAME panel-visibility-test COMMAND panel-visibility-test)

  qt_add_executable(
    plasma-panel-adapter-test
    tests/PlasmaPanelAdapterTest.cpp
    src/integration/PlasmaPanelAdapter.h
    src/integration/PlasmaPanelScreenGuard.h
    src/integration/PlasmaPanelAdapter.cpp)
  target_include_directories(
    plasma-panel-adapter-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  # The test reads Arch Dock's other Plasma scripts in the sources.
  target_compile_definitions(
    plasma-panel-adapter-test
    PRIVATE ARCHDOCK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(
    plasma-panel-adapter-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME plasma-panel-adapter-test
    COMMAND plasma-panel-adapter-test)

  qt_add_executable(
    plasma-script-result-test
    tests/PlasmaScriptResultTest.cpp
    src/PlasmaScriptResult.h
    src/PlasmaScriptResult.cpp)
  target_include_directories(
    plasma-script-result-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    plasma-script-result-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME plasma-script-result-test
    COMMAND plasma-script-result-test)

  qt_add_executable(
    intentional-stop-test
    tests/IntentionalStopTest.cpp
    src/IntentionalStop.h
    src/IntentionalStop.cpp)
  target_include_directories(
    intentional-stop-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    intentional-stop-test
    PRIVATE Qt6::Core
            Qt6::Test)
  add_test(
    NAME intentional-stop-test
    COMMAND intentional-stop-test)

  add_test(
    NAME dock-geometry-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_DockGeometry.qml)
  set_tests_properties(
    dock-geometry-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME studio-scroll-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioScroll.qml)
  set_tests_properties(studio-scroll-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=org.kde.desktop;QT_NO_XDG_DESKTOP_PORTAL=1")

  add_test(NAME decoration-scene-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_DecorationScene.qml)
  set_tests_properties(decoration-scene-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME panel-decoration-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelDecoration2D.qml)
  set_tests_properties(panel-decoration-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME icon-tiles-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconTiles.qml)
  set_tests_properties(icon-tiles-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")

  add_test(
    NAME layout-engine-visual-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_LayoutEngineVisual.qml)
  set_tests_properties(
    layout-engine-visual-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-motion-tracks-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelMotionTracks.qml)
  set_tests_properties(
    panel-motion-tracks-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME scene-rotation-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_SceneRotation.qml)
  set_tests_properties(
    scene-rotation-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME gizmo-math-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_GizmoMath.qml)
  set_tests_properties(
    gizmo-math-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME hologram-look-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_HologramLook.qml)
  set_tests_properties(
    hologram-look-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME platform-geometry-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PlatformGeometry.qml)
  set_tests_properties(
    platform-geometry-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME icon-solid-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconSolid.qml)
  set_tests_properties(
    icon-solid-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME geometry-hit-region-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_GeometryHitRegion.qml)
  set_tests_properties(
    geometry-hit-region-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-materials-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelMaterials.qml)
  set_tests_properties(panel-materials-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  add_test(
    NAME panel-surface-integration-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelSurfaceIntegration.qml)
  set_tests_properties(
    panel-surface-integration-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-scene-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelScene.qml)
  set_tests_properties(
    panel-scene-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-skin-2d-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelSkin2D.qml)
  set_tests_properties(
    panel-skin-2d-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-baked-25d-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelBaked25D.qml)
  set_tests_properties(
    panel-baked-25d-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME window-preview-popup-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_WindowPreviewPopup.qml)
  set_tests_properties(window-preview-popup-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME folder-expansion-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_FolderExpansion.qml)
  set_tests_properties(folder-expansion-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # The four folder easings are four different motions in
  # every folder layout, opening and closing, over the folder duration.
  add_test(NAME folder-easing-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_FolderEasing.qml)
  set_tests_properties(folder-easing-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME live-panel-preview-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_LivePanelPreview.qml)
  set_tests_properties(
    live-panel-preview-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME renderer-parity-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_RendererParity.qml)
  set_tests_properties(
    renderer-parity-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME icon-scene-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconScene.qml)
  set_tests_properties(
    icon-scene-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME icon-style-visual-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconStyleVisual.qml)
  set_tests_properties(
    icon-style-visual-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")

  add_test(
    NAME icon-override-resolution-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconOverrideResolution.qml)
  set_tests_properties(
    icon-override-resolution-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME motion-channels-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_MotionChannels.qml)
  set_tests_properties(
    motion-channels-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME animation-profile-runtime-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_AnimationProfileRuntime.qml)
  set_tests_properties(
    animation-profile-runtime-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-presentation-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelPresentation.qml)
  set_tests_properties(
    panel-presentation-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-presentation-guards-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelPresentationGuards.qml)
  set_tests_properties(
    panel-presentation-guards-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME panel-guard-interaction-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PanelGuardInteraction.qml)
  set_tests_properties(
    panel-guard-interaction-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME dock-entry-motion-contract-test
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateDockEntryMotion.cmake)

  add_test(
    NAME icon-motion-migration-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconMotionMigration.qml)
  set_tests_properties(
    icon-motion-migration-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")

  add_test(
    NAME requested-icon-motions-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_RequestedIconMotions.qml)
  set_tests_properties(
    requested-icon-motions-test
    PROPERTIES ENVIRONMENT
               "QT_QPA_PLATFORM=offscreen;QML_XHR_ALLOW_FILE_READ=1")

  add_test(
    NAME dock-entry-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_DockEntry.qml)
  set_tests_properties(
    dock-entry-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME icon-properties-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconProperties.qml)
  set_tests_properties(
    icon-properties-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME rendering-module-host-neutral-test
    COMMAND ${CMAKE_COMMAND}
            -DARCHDOCK_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateRenderingModule.cmake)

  add_test(
    NAME theme-v2-fixture-scaffold-test
    COMMAND ${CMAKE_COMMAND}
            -DARCHDOCK_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateThemeV2Fixtures.cmake)

  add_test(
    NAME rendering-module-import-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_RenderingModuleImport.qml)
  set_tests_properties(
    rendering-module-import-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME rendering-import-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(
    rendering-import-smoke
    # The private session is limited to 240 s; it takes about 190 (the
    # real-RHI generated platform rows about 15 s of it, the 3D preview
    # switch with freed memory overwritten about 8 s, the pictures in relief
    # about 10 s).
    PROPERTIES DEPENDS rendering-module-import-test TIMEOUT 270)

  add_test(
    NAME window-interaction-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(
    window-interaction-smoke
    PROPERTIES DEPENDS rendering-import-smoke TIMEOUT 120)

  add_test(NAME folder-interaction-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1
            ARCHDOCK_RENDERING_FOLDERS=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(folder-interaction-smoke
    PROPERTIES DEPENDS window-interaction-smoke TIMEOUT 300)

  # Owner-visible folder placement on a curved free panel, flat, baked and in
  # true 3D: 90 real openings, every folder layout at five positions of the
  # ring, each checked against the clicked icon, the dock's centre and the
  # Dock's other icons; the fan, arc, stack and ring by their own
  # Geometry. A screen the size of the user's leaves them
  # room.
  add_test(NAME folder-anchor-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1
            ARCHDOCK_RENDERING_FOLDERS=1
            ARCHDOCK_RENDERING_FOLDER_ANCHORS=1
            ARCHDOCK_RENDERING_SCREEN_WIDTH=1920
            ARCHDOCK_RENDERING_SCREEN_HEIGHT=1080
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(folder-anchor-smoke
    PROPERTIES DEPENDS folder-interaction-smoke RUN_SERIAL TRUE TIMEOUT 900)

  # Every opening mechanism a panel offers, per renderer: the drawn panel
  # visibly collapses and comes back; unoffered mechanisms are refused.
  add_test(NAME presentation-mechanism-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1
            ARCHDOCK_RENDERING_MECHANISMS=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(presentation-mechanism-smoke
    # Above its private session's own limit (240 s) and the set-up before it.
    PROPERTIES DEPENDS window-interaction-smoke RUN_SERIAL TRUE TIMEOUT 300)

  # Panel decorations on 3D panels, drawn by a private compositor's real
  # graphics backend: their shaders build and each stands as a solid.
  add_test(NAME decoration-3d-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_DECORATIONS=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(decoration-3d-smoke
    PROPERTIES DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 270)

  # 3D views made anew when their window changes (never carried into another
  # window) and let go in two steps, with real graphics.
  add_test(NAME window-move-3d-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_WINDOW_MOVES=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(window-move-3d-smoke
    PROPERTIES DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 480)

  # The Studio truth matrix drawn with a private compositor's
  # real graphics backend, where shader-drawn effects and true 3D appear.
  add_test(NAME studio-truth-matrix-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_TRUTH_MATRIX=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(studio-truth-matrix-smoke
    PROPERTIES DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 1200)

  add_test(NAME runtime-ui-interaction-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1 ARCHDOCK_RUNTIME_UI=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(runtime-ui-interaction-smoke PROPERTIES
    DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 180)

  # A free panel's entries travel along its own path. Real
  # wheel input on twelve layouts in a private compositor: slots, wrap-around,
  # hit tests, step timing, what motion moves, and Direction; every resting
  # state is captured and every frame recorded.
  add_test(NAME path-travel-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1 ARCHDOCK_RENDERING_TRAVEL=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(path-travel-smoke PROPERTIES
    DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 480)

  add_test(
    NAME free-entry-policy-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_FreeEntryPolicy.qml)
  set_tests_properties(
    free-entry-policy-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # Which window the dock is drawn in, and how a free panel's place is set.
  add_test(
    NAME dock-window-policy-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_DockWindowPolicy.qml)
  set_tests_properties(
    dock-window-policy-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # The window an always-shown Plasma edge panel's dock is drawn in: over
  # its placeholder wherever Plasma moves it, as deep as the panel's window
  # and the dock's reach.
  add_test(
    NAME overflow-host-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_OverflowHost.qml)
  set_tests_properties(
    overflow-host-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  qt_add_executable(free-geometry-settle-test tests/FreeGeometrySettleTest.cpp src/panel/FreeGeometrySettle.h)
  target_include_directories(free-geometry-settle-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(free-geometry-settle-test PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME free-geometry-settle-test COMMAND free-geometry-settle-test)

  add_test(
    NAME bootstrap-coordinator-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_BootstrapCoordinator.qml)
  set_tests_properties(
    bootstrap-coordinator-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME plasma-template-contract-test
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidatePlasmaTemplate.cmake)

  add_test(
    NAME studio-preview-contract-test
    COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateStudioPreview.cmake)

  add_test(
    NAME preset-staged-preview-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-preset-staged-smoke.sh)
  set_tests_properties(
    preset-staged-preview-smoke
    PROPERTIES DEPENDS preset-library-test TIMEOUT 180)

  add_test(
    NAME preset-browser-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PresetBrowser.qml)
  set_tests_properties(
    preset-browser-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME preset-audition-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PresetAudition.qml)
  set_tests_properties(preset-audition-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(NAME profile-page-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_ProfilePage.qml)
  set_tests_properties(profile-page-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME studio-widgets-page-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioWidgetsPage.qml)
  set_tests_properties(studio-widgets-page-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_test(NAME studio-updates-page-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioUpdatesPage.qml)
  set_tests_properties(studio-updates-page-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  foreach(profile_matrix_group apply shortcuts)
    add_test(NAME profile-matrix-${profile_matrix_group}
      COMMAND ${CMAKE_COMMAND} -E env
        ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
        bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-profile-matrix.sh ${profile_matrix_group})
    set_tests_properties(profile-matrix-${profile_matrix_group} PROPERTIES
      RUN_SERIAL TRUE TIMEOUT 420 DEPENDS "profile-apply-transaction-test;profile-shortcut-manager-test")
  endforeach()

  set(preset_matrix_previous preset-audition-test)
  foreach(preset_matrix_group existing temporary icons icons-middle icons-last recovery defaults)
    set(preset_matrix_name "preset-audition-matrix-${preset_matrix_group}")
    add_test(NAME ${preset_matrix_name}
      COMMAND ${CMAKE_COMMAND} -E env
        ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
        bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-preset-audition-matrix.sh ${preset_matrix_group})
    set_tests_properties(${preset_matrix_name} PROPERTIES
      RUN_SERIAL TRUE TIMEOUT 420 DEPENDS ${preset_matrix_previous})
    set(preset_matrix_previous ${preset_matrix_name})
  endforeach()

  add_test(
    NAME studio-navigation-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioNavigation.qml)
  set_tests_properties(
    studio-navigation-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME studio-themes-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioThemes.qml)
  set_tests_properties(
    studio-themes-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME settings-editor-model-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_SettingsEditorModel.qml)
  set_tests_properties(
    settings-editor-model-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME native-config-draft-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_NativeConfigDraft.qml)
  set_tests_properties(
    native-config-draft-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME capability-model-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_CapabilityModel.qml)
  set_tests_properties(
    capability-model-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME placement-status-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_PlacementStatus.qml)
  set_tests_properties(
    placement-status-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME visibility-status-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_VisibilityStatus.qml)
  set_tests_properties(
    visibility-status-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  if(ARCHDOCK_RELEASE)
    set(archdock_expected_version "${ARCHDOCK_RELEASE}")
  else()
    set(archdock_expected_version "${PROJECT_VERSION} (development build)")
  endif()
  add_test(NAME session-startup-metadata-test
    COMMAND ${CMAKE_COMMAND}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
      -DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DQT_VERSION=${Qt6Core_VERSION}
      "-DEXPECTED_VERSION=${archdock_expected_version}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateSessionStartup.cmake)
  set(ARCHDOCK_TEST_TMPDIR "${CMAKE_CURRENT_BINARY_DIR}/test-tmp" CACHE PATH
      "Disk-backed temporary root for disposable runtime fixtures")
  file(MAKE_DIRECTORY "${ARCHDOCK_TEST_TMPDIR}")
  find_program(ARCHDOCK_TEST_PYTHON NAMES python3 python REQUIRED)
  add_test(NAME source-exporter-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_prepare_arch_source.py)
  set_tests_properties(source-exporter-test PROPERTIES RUN_SERIAL TRUE
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME configuration-upgrade-test
    COMMAND ${CMAKE_COMMAND} -E env
      ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      TMPDIR=${ARCHDOCK_TEST_TMPDIR}
      bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-configuration-upgrade-smoke.sh)
  set_tests_properties(configuration-upgrade-test PROPERTIES RUN_SERIAL TRUE TIMEOUT 90)
  add_test(NAME core-accessibility-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
      -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
      -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_CoreAccessibility.qml)
  set_tests_properties(core-accessibility-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" RUN_SERIAL TRUE TIMEOUT 60)
  foreach(hardening_group IN ITEMS scale100 scale125 scale150 scale200 hotplug-recovery resources)
    add_test(NAME wayland-hardening-${hardening_group}-test
      COMMAND ${CMAKE_COMMAND} -E env
        ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
        ARCHDOCK_HARDENING_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
        TMPDIR=${ARCHDOCK_TEST_TMPDIR}
        bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-wayland-hardening-matrix.sh ${hardening_group})
    set_tests_properties(wayland-hardening-${hardening_group}-test PROPERTIES
      RUN_SERIAL TRUE TIMEOUT 420)
  endforeach()
  foreach(startup_mode IN ITEMS diagnostics runtime)
    add_test(NAME session-startup-${startup_mode}-test
      COMMAND ${CMAKE_COMMAND} -E env
        ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
        TMPDIR=${ARCHDOCK_TEST_TMPDIR}
        bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-session-startup-smoke.sh ${startup_mode})
    set_tests_properties(session-startup-${startup_mode}-test PROPERTIES
      RUN_SERIAL TRUE DEPENDS session-startup-metadata-test TIMEOUT 180)
  endforeach()
  set_tests_properties(session-startup-metadata-test PROPERTIES RUN_SERIAL TRUE)
  # These fixtures compile selected production sources directly. Share the
  # persistence service rather than maintaining another backup implementation.
  get_property(archdock_fixture_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(fixture_target IN LISTS archdock_fixture_targets)
    get_target_property(fixture_sources ${fixture_target} SOURCES)
    if(fixture_sources MATCHES "src/(PanelRegistry|presets/UserPresetStore)\\.cpp")
      target_link_libraries(${fixture_target} PRIVATE archdock-profile-backend)
    endif()
  endforeach()
endif()

install(TARGETS arch-dock RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE packaging/LICENSING.md DESTINATION share/licenses/arch-dock)
# After Qt, Plasma or Arch Dock itself is updated, pacman says when the
# installed Arch Dock no longer fits them. pacman reads hooks only from
# /usr/share/libalpm/hooks, so the hook names its script there.
set(ARCHDOCK_HOOK_PACMAN /usr/bin/pacman)
set(ARCHDOCK_HOOK_OD /usr/bin/od)
set(ARCHDOCK_HOOK_ENV /usr/bin/env)
set(ARCHDOCK_HOOK_LOADER /usr/lib/ld-linux-x86-64.so.2)
configure_file(data/arch-dock-check-qt.in arch-dock-check-qt @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/arch-dock-check-qt" DESTINATION share/libalpm/scripts)
install(FILES data/arch-dock-qt.hook.in DESTINATION share/libalpm/hooks RENAME arch-dock-qt.hook)
foreach(startup_descriptor IN ITEMS arch-dock.service org.archdock.ArchDock.service org.archdock.ArchDock.desktop
                                    org.archdock.ArchDock.Bars.desktop org.archdock.ArchDock.policy)
  configure_file("data/${startup_descriptor}.in"
                 "startup-templates/${startup_descriptor}.in" COPYONLY)
endforeach()
configure_file(cmake/InstallStartupMetadata.cmake.in
               InstallStartupMetadata.cmake @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallStartupMetadata.cmake")
install(FILES "${ARCHDOCK_RENDERING_BUILD_DIR}/RendererBuildConfig.qml"
        DESTINATION "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Rendering")
foreach(rendering_file IN LISTS ARCHDOCK_RENDERING_MODULE_FILES)
  get_filename_component(rendering_directory "${rendering_file}" DIRECTORY)
  install(
    FILES "${ARCHDOCK_RENDERING_SOURCE_DIR}/${rendering_file}"
    DESTINATION
      "${ARCHDOCK_QML_INSTALL_DIR}/ArchDock/Rendering/${rendering_directory}")
endforeach()
# The application's icon and logo, from the project's artwork
# (data/icons/arch-dock-logo-source.png), in the hicolor theme.
foreach(icon_size IN ITEMS 16 22 24 32 48 64 128 256 512)
  install(FILES "data/icons/hicolor/${icon_size}x${icon_size}.png"
          DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
          RENAME org.archdock.ArchDock.png)
endforeach()
install(FILES data/themes/builtin-themes.json DESTINATION share/arch-dock/themes)
install(
  FILES data/icon-styles/builtin-icon-styles.json
  DESTINATION share/arch-dock/icon-styles)
install(
  FILES data/animation-profiles/builtin-animation-profiles.json
  DESTINATION share/arch-dock/animation-profiles)
install(
  DIRECTORY data/presets/panels data/presets/icons
  DESTINATION share/arch-dock/presets
  FILES_MATCHING
  PATTERN "*.json")
foreach(packaged_icon_style
        plain-original
        metallic-blue
        metallic-red
        neon-green
        neon-orange
        dark-orb)
  install(
    DIRECTORY "assets/icon-styles/${packaged_icon_style}"
    DESTINATION share/arch-dock/icon-styles)
endforeach()
foreach(packaged_theme
        sci-fi-chassis-dark
        sci-fi-chassis-red
        sci-fi-chassis-blue
        energy-frame-cyan
        energy-frame-green
        energy-frame-orange
        energy-frame-purple
        ring-platform-blue
        octagon-platform-steel
        arc-platform-orange
        mesh-platform-cyan)
  install(
    DIRECTORY "assets/themes/${packaged_theme}"
    DESTINATION share/arch-dock/themes)
endforeach()
install(
  DIRECTORY kwin-script/
  DESTINATION share/kwin/scripts/org.archdock.windowwatcher)
install(
  DIRECTORY plasma-widget/
  DESTINATION share/plasma/plasmoids/org.archdock.control)
install(CODE [[
  file(REMOVE_RECURSE
    "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/plasma/plasmoids/org.archdock.templatebootstrap")
]])
install(
  DIRECTORY plasma-dock-widget/
  DESTINATION share/plasma/plasmoids/org.archdock.dock)
install(TARGETS arch-dock-bars RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The icon helper (arch-dock-icon-object): makes icons into 3D objects with
# TripoSR's network, run by ONNX Runtime (Arch's onnxruntime-cpu), its two
# models installed apart (tools/icon-objects/export-triposr.py makes them).
# Built where ONNX Runtime is; without it the 3D icons keep their pictures'
# own reliefs. ARCHDOCK_ONNXRUNTIME_ROOT names an unpacked ONNX Runtime
# elsewhere.
set(ARCHDOCK_ONNXRUNTIME_ROOT "" CACHE PATH "An ONNX Runtime unpacked outside the system")
find_path(ARCHDOCK_ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h
  HINTS "${ARCHDOCK_ONNXRUNTIME_ROOT}/include" PATH_SUFFIXES onnxruntime)
find_library(ARCHDOCK_ONNXRUNTIME_LIBRARY onnxruntime HINTS "${ARCHDOCK_ONNXRUNTIME_ROOT}/lib")
if(ARCHDOCK_ONNXRUNTIME_INCLUDE AND ARCHDOCK_ONNXRUNTIME_LIBRARY)
  add_executable(arch-dock-icon-object
    src/iconobject/main.cpp
    src/iconobject/IconObjectMaker.h
    src/iconobject/IconObjectMaker.cpp)
  target_include_directories(arch-dock-icon-object SYSTEM PRIVATE "${ARCHDOCK_ONNXRUNTIME_INCLUDE}")
  target_link_libraries(arch-dock-icon-object PRIVATE archdock-iconobject "${ARCHDOCK_ONNXRUNTIME_LIBRARY}")
  target_compile_options(arch-dock-icon-object PRIVATE -O2)
  if(ARCHDOCK_ONNXRUNTIME_ROOT)
    set_target_properties(arch-dock-icon-object PROPERTIES BUILD_RPATH "${ARCHDOCK_ONNXRUNTIME_ROOT}/lib")
  endif()
  install(TARGETS arch-dock-icon-object RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  message(STATUS "Arch Dock: the icon helper is built (ONNX Runtime at ${ARCHDOCK_ONNXRUNTIME_LIBRARY})")
  if(BUILD_TESTING)
    add_test(NAME icon-helper-pictures-test
      COMMAND ${CMAKE_COMMAND}
        -DHELPER=$<TARGET_FILE:arch-dock-icon-object>
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/test-data/icon-helper-pictures
        -DPICTURE=${CMAKE_CURRENT_SOURCE_DIR}/data/icons/hicolor/128x128.png
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/icon-helper-pictures.cmake)
    set_tests_properties(icon-helper-pictures-test PROPERTIES
      ENVIRONMENT "ARCHDOCK_ICON_OBJECT_MODELS=/nonexistent;QT_QPA_PLATFORM=offscreen")
  endif()
else()
  message(STATUS "Arch Dock: no ONNX Runtime, so no icon helper; 3D icons keep their pictures' reliefs")
endif()
install(
  DIRECTORY plasma-bar-containment/
  DESTINATION share/plasma/plasmoids/org.archdock.bar)
install(
  DIRECTORY plasma-bar-shell/
  DESTINATION share/plasma/shells/org.archdock.barshell)
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
  DIRECTORY plasma-layout-template-free/
  DESTINATION share/plasma/layout-templates/org.archdock.plasma.desktop.circularFreeDock)

# Qt's private interface: none in the programs and QML libraries. Checked here
# at the end, where every program exists.
if(BUILD_TESTING)
  find_program(ARCHDOCK_READELF readelf REQUIRED)
  set(ARCHDOCK_PRIVATE_CHECK_FILES "")
  foreach(checked_target IN ITEMS
      arch-dock arch-dock-bars arch-dock-icon-object arch-dock-install-update
      archdock-input archdock-inputplugin
      archdock-decorations archdock-decorationsplugin
      archdock-decorations3d archdock-decorations3dplugin
      archdock-integration archdock-integrationplugin)
    if(TARGET ${checked_target})
      list(APPEND ARCHDOCK_PRIVATE_CHECK_FILES "$<TARGET_FILE:${checked_target}>")
    endif()
  endforeach()
  list(JOIN ARCHDOCK_PRIVATE_CHECK_FILES "|" ARCHDOCK_PRIVATE_CHECK_FILES)
  add_test(NAME qt-private-interface-test
    COMMAND ${CMAKE_COMMAND}
      -DREADELF=${ARCHDOCK_READELF}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      "-DBINARIES=${ARCHDOCK_PRIVATE_CHECK_FILES}"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/QtPrivateInterfaceCheck.cmake)

  add_executable(arch-dock-install-update-test src/updates/helper/main.cpp)
  target_compile_definitions(arch-dock-install-update-test PRIVATE ARCHDOCK_HELPER_TEST_BUILD)
  target_link_libraries(arch-dock-install-update-test PRIVATE archdock-update-core archdock-package-check archdock-version)

  qt_add_executable(update-package-check-test tests/UpdatePackageCheckTest.cpp)
  target_link_libraries(update-package-check-test PRIVATE archdock-package-check Qt6::Test)
  add_test(NAME update-package-check-test COMMAND update-package-check-test)

  qt_add_executable(update-download-test tests/UpdateDownloadTest.cpp)
  target_link_libraries(update-download-test PRIVATE archdock-updater Qt6::Test)
  add_test(NAME update-download-test COMMAND update-download-test)

  qt_add_executable(release-source-test tests/ReleaseSourceTest.cpp)
  target_link_libraries(release-source-test PRIVATE archdock-updater Qt6::Network Qt6::Test)
  target_compile_definitions(release-source-test PRIVATE
    UPDATE_VECTORS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/update-manifest-vectors")
  add_test(NAME release-source-test COMMAND release-source-test)

  # The Update button's whole flow with stand-ins: a local server for GitHub,
  # a script for pkexec and the root helper, a runner for pacman and
  # systemd-run. No session or system bus is reachable from it.
  qt_add_executable(update-installer-test
    tests/UpdateInstallerTest.cpp
    src/updates/ArchDockUpdates.h
    src/updates/ArchDockUpdates.cpp
    src/updates/UpdateHold.h
    src/IntentionalStop.h
    src/IntentionalStop.cpp)
  target_include_directories(update-installer-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(update-installer-test PRIVATE archdock-profile-backend archdock-updater archdock-version
    KF6::Notifications Qt6::Core Qt6::DBus Qt6::Network Qt6::Test)
  target_compile_definitions(update-installer-test PRIVATE
    UPDATE_VECTORS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/update-manifest-vectors"
    ARCHDOCK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME update-installer-test COMMAND update-installer-test)
  set_tests_properties(update-installer-test PROPERTIES ENVIRONMENT
    "DBUS_SESSION_BUS_ADDRESS=unix:path=/dev/null/no-bus;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/dev/null/no-bus")

  qt_add_executable(update-manifest-test tests/UpdateCoreTest.cpp)
  target_link_libraries(update-manifest-test PRIVATE archdock-update-core Qt6::Test)
  target_compile_definitions(update-manifest-test PRIVATE
    UPDATE_VECTORS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/update-manifest-vectors")
  add_test(NAME update-manifest-test COMMAND update-manifest-test)

  qt_add_executable(qt-build-check-test tests/QtBuildCheckTest.cpp)
  target_include_directories(qt-build-check-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(qt-build-check-test PRIVATE Qt6::Core Qt6::Test)
  add_test(NAME qt-build-check-test COMMAND qt-build-check-test)

  # The install route in README and INSTALL.md, and the guide's update line
  # run behind the fence.
  add_test(NAME install-docs-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_install_docs.py ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(install-docs-test PROPERTIES TIMEOUT 120
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME release-files-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_files.py)
  set_tests_properties(release-files-test PROPERTIES RUN_SERIAL TRUE TIMEOUT 300
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")

  add_test(NAME update-helper-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_update_helper.py
      $<TARGET_FILE:arch-dock-install-update-test>)
  set_tests_properties(update-helper-test PROPERTIES RUN_SERIAL TRUE TIMEOUT 600
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME update-finisher-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_update_finisher.py)
  set_tests_properties(update-finisher-test PROPERTIES RUN_SERIAL TRUE TIMEOUT 300
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")

  # The Qt hook's script: a test copy whose pacman and loader are stand-ins
  # the test writes. The installed copy is only read, never run.
  set(ARCHDOCK_HOOK_TEST_TOOLS "${CMAKE_CURRENT_BINARY_DIR}/qt-hook-test/tools")
  set(ARCHDOCK_HOOK_PACMAN "${ARCHDOCK_HOOK_TEST_TOOLS}/pacman")
  set(ARCHDOCK_HOOK_LOADER "${ARCHDOCK_HOOK_TEST_TOOLS}/ld-linux")
  configure_file(data/arch-dock-check-qt.in qt-hook-test/arch-dock-check-qt @ONLY)
  add_test(NAME qt-hook-test
    COMMAND ${ARCHDOCK_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qt_hook.py
      ${CMAKE_CURRENT_BINARY_DIR}/arch-dock-check-qt
      ${CMAKE_CURRENT_BINARY_DIR}/qt-hook-test/arch-dock-check-qt
      ${ARCHDOCK_HOOK_TEST_TOOLS}
      ${CMAKE_CURRENT_SOURCE_DIR}/data/arch-dock-qt.hook.in
      ${ARCHDOCK_QT_MINOR})
  set_tests_properties(qt-hook-test PROPERTIES RUN_SERIAL TRUE TIMEOUT 120
    ENVIRONMENT "TMPDIR=${ARCHDOCK_TEST_TMPDIR};PYTHONDONTWRITEBYTECODE=1")
endif()
