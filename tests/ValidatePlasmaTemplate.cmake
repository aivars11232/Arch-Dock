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

string(FIND
  "${dock_config_model}"
  "source: \"configBehavior.qml\""
  behavior_page_position)
if(behavior_page_position EQUAL -1)
  message(FATAL_ERROR
    "The dock configuration model must register the functional Behavior page.")
endif()

foreach(required_visibility_contract
    "nativePanelVisibilityStatus"
    "applyNativePanelVisibilityMode"
    "setPanelVisible"
    "supportedModes"
    "fallbackApplied"
    "visible: root.visibilityOptions.length > 0")
  string(FIND
    "${dock_behavior_page}"
    "${required_visibility_contract}"
    visibility_contract_position)
  if(visibility_contract_position EQUAL -1)
    message(FATAL_ERROR
      "The Behavior page is missing ${required_visibility_contract}.")
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
