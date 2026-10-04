cmake_minimum_required(VERSION 3.21)

project(
  ArchDock
  VERSION 0.1.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Network Qml Quick)

find_package(KF6Kirigami REQUIRED)
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KF6GlobalAccel REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

include(GNUInstallDirs)

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
    LayoutEngine.js
    IconStyleResolver.js
    AnimationProfileRuntime.js
    MotionChannels.js
    PresentationStates.js
    ThemeStateSelection.js
    IconScene.qml
    FolderExpansion.qml
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
    previews/WindowPreviewPopup.qml
    renderers/IconStyle2D.qml
    renderers/IconTile.qml
    renderers/PanelProcedural2D.qml
    renderers/PanelSkin2D.qml
    renderers/PanelSkinLayer2D.qml
    renderers/PanelBaked25D.qml)
set(ARCHDOCK_RENDERING_MODULE_SOURCES)
set(ARCHDOCK_OPTIONAL_RENDERING_FILES
    optional3d/RendererImportProbe.qml
    optional3d/PanelScene3D.qml
    optional3d/IconStyle3D.qml)
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
  src/panel/ProfileApplyTransaction.h src/panel/ProfileApplyTransaction.cpp
  src/panel/ProfileManager.h src/panel/ProfileManager.cpp
  src/integration/ProfileShortcutManager.h src/integration/ProfileShortcutManager.cpp
  src/model/PanelDefinition.cpp src/model/PanelSettingsSchema.cpp
  src/model/PanelRuntimeState.cpp src/model/SettingsMigration.cpp
  src/model/ThemeDefinition.cpp src/themes/ThemePackage.cpp)
target_include_directories(archdock-profile-backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(archdock-profile-backend PUBLIC Qt6::Core PRIVATE Qt6::Gui Qt6::DBus KF6::GlobalAccel)

qt_add_executable(
  arch-dock
  src/main.cpp
  src/DockModel.h
  src/DockModel.cpp
  src/model/FolderContentModel.cpp
  src/content/WindowPreviewModel.cpp
  src/DockSettings.h
  src/DockSettings.cpp
  src/PanelRegistry.h
  src/PanelRegistry.cpp
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
  src/WindowItem.h
  src/WindowModel.h
  src/WindowModel.cpp
  src/WindowWatcher.h
  src/WindowWatcher.cpp)
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
  qml/runtime/StudioDraft.js
  qml/runtime/StudioForm.qml
  qml/runtime/StudioNavigation.js
  qml/runtime/SettingsPopup.qml)
qt_add_qml_module(
  arch-dock
  URI ArchDock
  VERSION 1.0
  NO_IMPORT_SCAN
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
          Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::Network
          Qt6::Qml
          Qt6::Quick
          KF6::Kirigami
          KF6::Service
          KF6::KIOGui)

target_compile_definitions(
  arch-dock
  PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js"
          ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
          ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
          ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}"
          ARCHDOCK_SOURCE_PRESET_ROOT="${ARCHDOCK_SOURCE_PRESET_ROOT}")

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
  foreach(profile_test configuration-backup-test profile-store-test profile-apply-transaction-test profile-manager-test profile-shortcut-manager-test)
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
    src/model/ThemeDefinition.cpp src/themes/ThemePackage.cpp)
  target_include_directories(renderer-capability-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(renderer-capability-test PRIVATE Qt6::Quick Qt6::Test)
  target_compile_definitions(renderer-capability-test PRIVATE
    ARCHDOCK_RENDERING_IMPORT_ROOT="${ARCHDOCK_RENDERING_IMPORT_ROOT}"
    ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}")
  add_dependencies(renderer-capability-test archdock-rendering-module)
  add_test(NAME renderer-capability-software-test COMMAND renderer-capability-test)
  set_tests_properties(renderer-capability-software-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  add_test(NAME renderer-capability-missing-module-test COMMAND renderer-capability-test)
  set_tests_properties(renderer-capability-missing-module-test PROPERTIES ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;ARCHDOCK_TEST_MISSING_3D=1")

  qt_add_executable(
    panel-capability-resolver-test
    tests/PanelCapabilityResolverTest.cpp
    src/model/PanelDefinition.h
    src/model/PanelDefinition.cpp
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
    src/WindowWatcher.cpp)
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
    PRIVATE archdock-profile-backend Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Network
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
            Qt6::QuickTest
            KF6::Kirigami
            KF6::Service
            KF6::KIOGui)
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
    src/integration/PlasmaPanelAdapter.cpp)
  target_include_directories(
    plasma-panel-adapter-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
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

  add_test(NAME icon-tiles-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_IconTiles.qml)
  set_tests_properties(icon-tiles-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
    NAME geometry-hit-region-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_GeometryHitRegion.qml)
  set_tests_properties(
    geometry-hit-region-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
    PROPERTIES DEPENDS rendering-module-import-test TIMEOUT 120)

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
    PROPERTIES DEPENDS window-interaction-smoke TIMEOUT 120)

  add_test(NAME runtime-ui-interaction-smoke
    COMMAND ${CMAKE_COMMAND} -E env
            ARCHDOCK_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            ARCHDOCK_QML_INSTALL_DIR=${ARCHDOCK_QML_INSTALL_DIR}
            ARCHDOCK_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>
            ARCHDOCK_RENDERING_INTERACTIONS=1 ARCHDOCK_RUNTIME_UI=1
            bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rendering-import-smoke.sh)
  set_tests_properties(runtime-ui-interaction-smoke PROPERTIES
    DEPENDS rendering-import-smoke RUN_SERIAL TRUE TIMEOUT 120)

  add_test(
    NAME motion-policy-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_MotionPolicy.qml)
  set_tests_properties(
    motion-policy-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_test(
    NAME free-entry-policy-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_FreeEntryPolicy.qml)
  set_tests_properties(
    free-entry-policy-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
  foreach(preset_matrix_group existing temporary icons recovery defaults)
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
    NAME studio-draft-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_StudioDraft.qml)
  set_tests_properties(
    studio-draft-test
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

  add_test(NAME session-startup-metadata-test
    COMMAND ${CMAKE_COMMAND}
      -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
      -DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
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
foreach(startup_descriptor IN ITEMS arch-dock.service org.archdock.ArchDock.service org.archdock.ArchDock.desktop)
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
