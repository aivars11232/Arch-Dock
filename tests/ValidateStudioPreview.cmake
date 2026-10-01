if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/qml/runtime/SettingsEditorModel.js" editor_model)
file(READ "${SOURCE_DIR}/qml/runtime/SettingsPopup.qml" settings_popup)
file(READ "${SOURCE_DIR}/qml/runtime/StudioForm.qml" studio_form)
file(READ "${SOURCE_DIR}/qml/runtime/PresetCard.qml" preset_card)
file(READ "${SOURCE_DIR}/qml/runtime/PresetBrowser.qml" preset_browser)

foreach(required_editor_contract
        "function rendererCandidate(session)"
        "function rendererThemeCandidate(session, theme)"
        "result.capabilityResolution = copyValue"
        "result.iconStyleDefinition = copyValue")
  string(FIND "${editor_model}" "${required_editor_contract}"
         editor_contract_position)
  if(editor_contract_position EQUAL -1)
    message(FATAL_ERROR
            "The renderer draft model is missing ${required_editor_contract}")
  endif()
endforeach()

foreach(required_studio_contract
        "selectedRendererCandidate:"
        "EditorModel.rendererCandidate(editorSession)"
        "LivePanelPreview {"
        "panelDefinition: root.selectedRendererCandidate"
        "hostCapabilities:"
        "root.selectedCapabilityResolution"
        ".iconStyleDefinition || ({})"
        "Draft only — desktop unchanged"
        "applyPanelSettingsTransaction")
  string(FIND "${settings_popup}" "${required_studio_contract}"
         studio_contract_position)
  if(studio_contract_position EQUAL -1)
    message(FATAL_ERROR
            "Panel Studio is missing ${required_studio_contract}")
  endif()
endforeach()

foreach(required_theme_card_contract
        "LivePanelPreview {"
        "root.studio.themeRendererCandidate(modelData)"
        "themeSample.rendererCandidate"
        ".iconStyleDefinition || ({})"
        "themePreview.rendererStatusText")
  string(FIND "${studio_form}" "${required_theme_card_contract}"
         theme_card_contract_position)
  if(theme_card_contract_position EQUAL -1)
    message(FATAL_ERROR
            "The theme card is missing ${required_theme_card_contract}")
  endif()
endforeach()

foreach(forbidden_parallel_preview
        "PanelScene {"
        "LayoutEngine."
        "Canvas {"
        "model: 6"
        "themeSample.panelStyle"
        "themeSample.iconStyle")
  string(FIND "${settings_popup}${studio_form}"
         "${forbidden_parallel_preview}" parallel_preview_position)
  if(NOT parallel_preview_position EQUAL -1)
    message(FATAL_ERROR
            "Parallel Studio preview remains: ${forbidden_parallel_preview}")
  endif()
endforeach()

# A preset card is drawn exactly as a theme card is: by the shared preview,
# from the record the editor model hands it.
foreach(required_preset_card_contract
        "LivePanelPreview {"
        "EditorModel.presetRendererCandidate(preset, !motionEnabled)"
        "panelDefinition: root.rendererCandidate"
        "root.rendererCandidate.capabilityResolution || ({})"
        "root.rendererCandidate.iconStyleDefinition || ({})"
        "animationProfiles: root.rendererCandidate"
        "root.livePreview.rendererStatusText"
        "active: root.presetState !== \"incompatible\"")
  string(FIND "${preset_card}" "${required_preset_card_contract}"
         preset_card_contract_position)
  if(preset_card_contract_position EQUAL -1)
    message(FATAL_ERROR
            "The preset card is missing ${required_preset_card_contract}")
  endif()
endforeach()

foreach(required_preset_studio_contract
        "function presetRendererCandidate(card, still)"
        "function presetState(card)")
  string(FIND "${editor_model}" "${required_preset_studio_contract}"
         preset_model_contract_position)
  if(preset_model_contract_position EQUAL -1)
    message(FATAL_ERROR
            "The renderer draft model is missing ${required_preset_studio_contract}")
  endif()
endforeach()

foreach(required_preset_studio_contract
        "PresetBrowser {"
        "StudioNavigation.presetPage(mainTabIndex, subTabIndex)"
        "presetLibrary.panelPresets(page.scope)"
        "presetLibrary.iconPresets(page.scope)"
        "panelDefinition: root.selectedPresetCandidate"
        "Preset preview only — no panel is changed")
  string(FIND "${settings_popup}" "${required_preset_studio_contract}"
         preset_studio_contract_position)
  if(preset_studio_contract_position EQUAL -1)
    message(FATAL_ERROR
            "Panel Studio is missing ${required_preset_studio_contract}")
  endif()
endforeach()

# No second renderer and no picture standing in for one: a card either shows
# the shared scene or says that it cannot be drawn.
foreach(forbidden_preset_preview
        "PanelScene {"
        "IconScene {"
        "LayoutEngine."
        "Canvas {"
        "Image {"
        "ShaderEffect {")
  string(FIND "${preset_card}${preset_browser}"
         "${forbidden_preset_preview}" preset_preview_position)
  if(NOT preset_preview_position EQUAL -1)
    message(FATAL_ERROR
            "Parallel preset preview remains: ${forbidden_preset_preview}")
  endif()
endforeach()

# The preset components receive data and emit requests. They hold no route to
# a panel, to the preset store or to an Apply action.
foreach(forbidden_preset_access
        "panelController"
        "panelRegistry"
        "presetLibrary"
        "dockSettings"
        "applyPanelSettingsTransaction"
        "performStudioAction"
        "Set as Default"
        "Preview on Desktop"
        "qsTr(\"Apply")
  string(FIND "${preset_card}${preset_browser}"
         "${forbidden_preset_access}" preset_access_position)
  if(NOT preset_access_position EQUAL -1)
    message(FATAL_ERROR
            "Preset browsing must stay read-only: ${forbidden_preset_access}")
  endif()
endforeach()

string(REGEX MATCHALL "applyPanelSettingsTransaction[ \t\r\n]*\\("
       settings_apply_calls "${settings_popup}")
list(LENGTH settings_apply_calls settings_apply_call_count)
if(NOT settings_apply_call_count EQUAL 1)
  message(FATAL_ERROR
          "Panel Studio must retain exactly one transactional Apply call")
endif()

message(STATUS "Panel Studio uses isolated shared-renderer previews")
