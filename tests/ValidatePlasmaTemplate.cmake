file(READ
  "${SOURCE_DIR}/plasma-layout-template-free/contents/layout.js"
  free_panel_layout)

string(FIND "${free_panel_layout}" "const bridgePanel = new Panel" bridge_panel_position)
if(bridge_panel_position EQUAL -1)
  message(FATAL_ERROR
    "The free-panel Add Panel template must create a native Plasma Panel bridge.")
endif()

string(FIND "${free_panel_layout}" "desktopsForActivity" direct_desktop_position)
if(NOT direct_desktop_position EQUAL -1)
  message(FATAL_ERROR
    "A panel-category template must not directly create a desktop widget.")
endif()

foreach(required_value
    "create-circular-free-panel"
    "bootstrapToken"
    "bootstrapPanelId")
  string(FIND "${free_panel_layout}" "${required_value}" required_value_position)
  if(required_value_position EQUAL -1)
    message(FATAL_ERROR
      "The free-panel bridge is missing ${required_value}.")
  endif()
endforeach()

file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/config/config.qml"
  dock_config_model)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/configBehavior.qml"
  dock_behavior_page)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/ConfigPageBase.qml"
  dock_config_page_base)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/main.qml"
  dock_main)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/configLayout.qml"
  dock_layout_page)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/configAppearance.qml"
  dock_appearance_page)
file(READ
  "${SOURCE_DIR}/plasma-dock-widget/contents/ui/configAnimation.qml"
  dock_animation_page)

string(FIND
  "${dock_config_model}"
  "source: \"configBehavior.qml\""
  behavior_page_position)
if(behavior_page_position EQUAL -1)
  message(FATAL_ERROR
    "The dock configuration model must register the functional Behavior page.")
endif()

string(REGEX MATCHALL
  "source:[ \t]*\"[^\"]+\\.qml\""
  dock_config_pages
  "${dock_config_model}")
list(LENGTH dock_config_pages dock_config_page_count)
if(NOT dock_config_page_count EQUAL 2)
  message(FATAL_ERROR
    "The native dock configuration must remain limited to its two existing pages.")
endif()

foreach(required_runtime_contract
    "panelRendererConfiguration"
    "capabilityResolution"
    "effectiveRendererTier")
  string(FIND
    "${dock_main}"
    "${required_runtime_contract}"
    main_runtime_contract_position)
  if(main_runtime_contract_position EQUAL -1)
    message(FATAL_ERROR
      "The dock runtime is missing ${required_runtime_contract}.")
  endif()
endforeach()

foreach(required_behavior_contract
    "capabilityResolution"
    "effectiveRendererTier"
    "nativePanelVisibilityStatus"
    "fallbackApplied"
    "visibleFields"
    "applyDraft"
    "cancelDraft"
    "draftDirty")
  string(FIND
    "${dock_behavior_page}"
    "${required_behavior_contract}"
    behavior_contract_position)
  if(behavior_contract_position EQUAL -1)
    message(FATAL_ERROR
      "The Behavior page is missing ${required_behavior_contract}.")
  endif()
endforeach()

foreach(required_draft_contract
    "panelSettingsEditorSnapshot"
    "applyPanelSettingsTransaction"
    "panelCandidate"
    "globalCandidate"
    "applyInFlight")
  string(FIND
    "${dock_config_page_base}"
    "${required_draft_contract}"
    draft_contract_position)
  if(draft_contract_position EQUAL -1)
    message(FATAL_ERROR
      "The native draft base is missing ${required_draft_contract}.")
  endif()
endforeach()

foreach(forbidden_native_write
    "applyNativePanelVisibilityMode"
    "setPanelVisible"
    "setDockBooleanConfiguration"
    "setDockIntegerConfiguration"
    "setDockRealConfiguration"
    "setDockStringConfiguration")
  string(FIND
    "${dock_behavior_page}${dock_config_page_base}"
    "${forbidden_native_write}"
    forbidden_native_write_position)
  if(NOT forbidden_native_write_position EQUAL -1)
    message(FATAL_ERROR
      "Native configuration still exposes the legacy write bypass ${forbidden_native_write}.")
  endif()
endforeach()

foreach(unregistered_page
    dock_layout_page
    dock_appearance_page
    dock_animation_page)
  string(FIND "${${unregistered_page}}" "setValue(" unregistered_setter_position)
  if(NOT unregistered_setter_position EQUAL -1)
    message(FATAL_ERROR
      "An unregistered native placeholder still contains an editable setter.")
  endif()
endforeach()

string(FIND
  "${dock_behavior_page}"
  "visibility are managed by Plasma's panel Edit Mode"
  stale_visibility_claim_position)
if(NOT stale_visibility_claim_position EQUAL -1)
  message(FATAL_ERROR
    "The Behavior page still contains the obsolete static visibility claim.")
endif()

foreach(token_component
    "bridgePanel.id"
    "currentActivity()"
    "Date.now()"
    "Math.random()")
  string(FIND "${free_panel_layout}" "${token_component}" token_component_position)
  if(token_component_position EQUAL -1)
    message(FATAL_ERROR
      "The free-panel bridge token is missing ${token_component}.")
  endif()
endforeach()
