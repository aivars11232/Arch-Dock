# TASK-0030 acceptance: no migrated animation preset may need a dedicated
# conditional branch in DockEntry. Effects are data now, not code, so the
# delegate must not name a single effect or run an animation of its own.
if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(dock_entry "${SOURCE_DIR}/plasma-dock-widget/contents/ui/DockEntry.qml")
if(NOT EXISTS "${dock_entry}")
  message(FATAL_ERROR "Missing DockEntry source: ${dock_entry}")
endif()
file(READ "${dock_entry}" dock_entry_content)

# Effect names owned by the animation-profile catalog. `glow` and `scale` are
# omitted because they are also animation-property names, which the delegate
# legitimately reads as motion channels.
set(migrated_effects
    bounce elastic pulse spin orbit swing wobble wiggle shake breathe
    float wave ripple magnetic spring idle-rotate)
foreach(effect IN LISTS migrated_effects)
  if(dock_entry_content MATCHES "\"${effect}\"")
    message(
      FATAL_ERROR
        "DockEntry still names the migrated effect '${effect}'; "
        "animation presets must be data, not a conditional branch")
  endif()
endforeach()

# The decisive check: no branch may test which effect is selected.
foreach(forbidden_branch
        "motion[ \t]*===" "motion[ \t]*!==" "motion[ \t]*==" "includes\\(root\\.motion")
  if(dock_entry_content MATCHES "${forbidden_branch}")
    message(
      FATAL_ERROR
        "DockEntry branches on the selected effect (${forbidden_branch}); "
        "animation presets must be data, not a conditional branch")
  endif()
endforeach()

# The delegate must not run per-effect animations either.
foreach(forbidden_animation
        "XAnimator"
        "YAnimator"
        "ScaleAnimator"
        "RotationAnimator"
        "SequentialAnimation")
  if(dock_entry_content MATCHES "${forbidden_animation}")
    message(
      FATAL_ERROR
        "DockEntry runs its own ${forbidden_animation}; "
        "IconMotionController is the only animation-profile track runner")
  endif()
endforeach()

message(STATUS "DockEntry contains no per-effect animation branch")
