if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/qml/runtime/SettingsEditorModel.js" editor_model)
file(READ "${SOURCE_DIR}/qml/runtime/SettingsPopup.qml" settings_popup)
file(READ "${SOURCE_DIR}/qml/runtime/StudioForm.qml" studio_form)

foreach(required_editor_contract
        "function rendererCandidate(session)"
        "function rendererThemeCandidate(session, theme)"
        "result.capabilityResolution = copyValue")
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

string(REGEX MATCHALL "applyPanelSettingsTransaction[ \t\r\n]*\\("
       settings_apply_calls "${settings_popup}")
list(LENGTH settings_apply_calls settings_apply_call_count)
if(NOT settings_apply_call_count EQUAL 1)
  message(FATAL_ERROR
          "Panel Studio must retain exactly one transactional Apply call")
endif()

message(STATUS "Panel Studio uses isolated shared-renderer previews")
