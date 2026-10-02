cmake_minimum_required(VERSION 3.21)
foreach(required IN ITEMS BUILD_DIR INSTALL_BINDIR INSTALL_LIBDIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
find_program(DESKTOP_VALIDATOR desktop-file-validate REQUIRED)
find_program(SYSTEMD_VALIDATOR systemd-analyze REQUIRED)
set(root "${BUILD_DIR}/startup-install-check")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")

function(check_install logical_prefix physical_prefix)
  set(executable "${logical_prefix}/${INSTALL_BINDIR}/arch-dock")
  if(NOT EXISTS "${physical_prefix}/${INSTALL_BINDIR}/arch-dock")
    message(FATAL_ERROR "The startup executable was not installed")
  endif()
  foreach(pair IN ITEMS
      "share/dbus-1/services/org.archdock.ArchDock.service|Exec"
      "share/applications/org.archdock.ArchDock.desktop|Exec"
      "${INSTALL_LIBDIR}/systemd/user/arch-dock.service|ExecStart")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 relative)
    list(GET fields 1 key)
    file(STRINGS "${physical_prefix}/${relative}" values REGEX "^${key}=")
    if(NOT values STREQUAL "${key}=\"${executable}\"")
      message(FATAL_ERROR "Startup path mismatch in ${relative}: ${values}")
    endif()
    file(STRINGS "${BUILD_DIR}/install_manifest.txt" manifest)
    if(NOT "${logical_prefix}/${relative}" IN_LIST manifest)
      message(FATAL_ERROR "Startup metadata is absent from the install manifest: ${relative}")
    endif()
  endforeach()
  file(READ "${physical_prefix}/${INSTALL_LIBDIR}/systemd/user/arch-dock.service" unit)
  if(NOT unit MATCHES "Type=dbus\nBusName=org.archdock.ArchDock\n" OR unit MATCHES "\\[Install\\]")
    message(FATAL_ERROR "The manual user unit must track the bus name without automatic enablement")
  endif()
  file(READ "${physical_prefix}/share/dbus-1/services/org.archdock.ArchDock.service" dbus)
  if(NOT dbus MATCHES "Name=org.archdock.ArchDock\n" OR dbus MATCHES "SystemdService=")
    message(FATAL_ERROR "The on-demand service must use direct session D-Bus activation")
  endif()
  execute_process(COMMAND "${DESKTOP_VALIDATOR}"
    "${physical_prefix}/share/applications/org.archdock.ArchDock.desktop"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Desktop metadata validation failed: ${output}${error}")
  endif()
endfunction()

set(prefix "${root}/prefix with spaces")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=DESTDIR
  "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${prefix}"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Prefix stage install failed: ${output}${error}")
endif()
check_install("${prefix}" "${prefix}")
execute_process(COMMAND "${SYSTEMD_VALIDATOR}" --user verify --man=no
  "${prefix}/${INSTALL_LIBDIR}/systemd/user/arch-dock.service"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "User unit validation failed: ${output}${error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "DESTDIR=${root}/destdir"
  "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix /usr
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "DESTDIR stage install failed: ${output}${error}")
endif()
check_install(/usr "${root}/destdir/usr")
file(REMOVE_RECURSE "${root}")
message(STATUS "Startup metadata and manifests match both prefix and DESTDIR installations")
