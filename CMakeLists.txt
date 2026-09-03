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

include(GNUInstallDirs)

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
set(ARCHDOCK_RENDERING_MODULE_FILES
    qmldir
    RenderingModuleProbe.qml
    LayoutEngine.js
    IconStyleResolver.js
    AnimationProfileRuntime.js
    IconScene.qml
    IconMotionController.qml
    MotionTrackRunner.qml
    PanelScene.qml
    PanelSurfaceLoader.qml
    RunningIndicator.qml
    inputs/AlphaHitMask.qml
    previews/LivePanelPreview.qml
    renderers/IconStyle2D.qml
    renderers/PanelProcedural2D.qml
    renderers/PanelSkin2D.qml
    renderers/PanelSkinLayer2D.qml)
set(ARCHDOCK_RENDERING_MODULE_SOURCES)

file(MAKE_DIRECTORY "${ARCHDOCK_RENDERING_BUILD_DIR}")
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
endforeach()

add_custom_target(
  archdock-rendering-module ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${ARCHDOCK_RENDERING_BUILD_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ARCHDOCK_RENDERING_SOURCE_DIR}"
          "${ARCHDOCK_RENDERING_BUILD_DIR}"
  DEPENDS ${ARCHDOCK_RENDERING_MODULE_SOURCES}
  COMMENT "Preparing the ArchDock.Rendering QML module")

qt_add_executable(
  arch-dock
  src/main.cpp
  src/DockModel.h
  src/DockModel.cpp
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
  src/KWinActionBridge.h
  src/KWinActionBridge.cpp
  src/panel/PanelManager.h
  src/panel/PanelManager.cpp
  src/panel/FreePanelController.h
  src/panel/FreePanelController.cpp
  src/panel/PanelSettingsTransaction.h
  src/panel/PanelSettingsTransaction.cpp
  src/panel/IconOverrideTransaction.h
  src/panel/IconOverrideTransaction.cpp
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
  IMPORT_PATH
  ${ARCHDOCK_RENDERING_IMPORT_ROOT}
  QML_FILES
  qml/runtime/CapabilityModel.js
  qml/runtime/IconPropertiesDraft.js
  qml/runtime/IconProperties.qml
  qml/runtime/IconPropertiesWindow.qml
  qml/runtime/PlacementStatus.js
  qml/runtime/SettingsEditorModel.js
  qml/runtime/VisibilityStatus.js
  qml/runtime/StudioDraft.js
  qml/runtime/StudioForm.qml
  qml/runtime/StudioNavigation.js
  qml/runtime/SettingsPopup.qml)
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
  PRIVATE Qt6::Core
          Qt6::DBus
          Qt6::Gui
          Qt6::Network
          Qt6::Qml
          Qt6::Quick
          KF6::Kirigami)

target_compile_definitions(
  arch-dock
  PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js"
          ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
          ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}"
          ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH="${ARCHDOCK_SOURCE_ANIMATION_PROFILE_CATALOG_PATH}")

include(CTest)
if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test QmlTools)

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
    src/model/IconEntryIdentity.h
    src/model/IconEntryIdentity.cpp)
  target_include_directories(
    panel-registry-test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(
    panel-registry-test
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Test)
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

  qt_add_executable(
    dock-model-test
    tests/DockModelTest.cpp
    src/DockModel.h
    src/DockModel.cpp
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
            Qt6::Test)
  add_test(NAME dock-model-test COMMAND dock-model-test)
  set_tests_properties(
    dock-model-test
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  qt_add_executable(
    panel-window-capability-test
    tests/PanelWindowCapabilityTest.cpp
    src/DockModel.h
    src/DockModel.cpp
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
    src/KWinActionBridge.h
    src/KWinActionBridge.cpp
    src/panel/PanelManager.h
    src/panel/PanelManager.cpp
    src/panel/FreePanelController.h
    src/panel/FreePanelController.cpp
    src/panel/PanelSettingsTransaction.h
    src/panel/PanelSettingsTransaction.cpp
    src/panel/IconOverrideTransaction.h
    src/panel/IconOverrideTransaction.cpp
    src/panel/PanelWindow.h
    src/panel/PanelWindow.cpp
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
    PRIVATE Qt6::Core
            Qt6::DBus
            Qt6::Gui
            Qt6::Network
            Qt6::Qml
            Qt6::Quick
            Qt6::Test
            KF6::Kirigami)
  target_compile_definitions(
    panel-window-capability-test
    PRIVATE ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/kwin-script/contents/code/main.js"
            ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT="${ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT}"
            ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH="${ARCHDOCK_SOURCE_ICON_STYLE_CATALOG_PATH}"
            ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT="${ARCHDOCK_SOURCE_ICON_STYLE_PACKAGE_ROOT}")
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

  add_test(
    NAME layout-engine-visual-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_LayoutEngineVisual.qml)
  set_tests_properties(
    layout-engine-visual-test
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
    NAME animation-profile-runtime-test
    COMMAND $<TARGET_FILE:Qt6::qmltestrunner>
            -import ${ARCHDOCK_RENDERING_IMPORT_ROOT}
            -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_AnimationProfileRuntime.qml)
  set_tests_properties(
    animation-profile-runtime-test
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
endif()

install(TARGETS arch-dock RUNTIME DESTINATION bin)
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
        energy-frame-purple)
  install(
    DIRECTORY "assets/themes/${packaged_theme}"
    DESTINATION share/arch-dock/themes)
endforeach()
install(
  DIRECTORY kwin-script/
  DESTINATION share/kwin/scripts/org.archdock.windowwatcher)
install(
  FILES data/org.archdock.ArchDock.desktop
  DESTINATION share/applications)
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
install(
  FILES data/org.archdock.ArchDock.service
  DESTINATION share/dbus-1/services)
install(
  FILES data/arch-dock.service
  DESTINATION share/systemd/user)
