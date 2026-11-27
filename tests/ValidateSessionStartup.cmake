cmake_minimum_required(VERSION 3.21)
foreach(required IN ITEMS BUILD_DIR INSTALL_BINDIR INSTALL_LIBDIR SOURCE_DIR QT_VERSION EXPECTED_VERSION)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
find_program(DESKTOP_VALIDATOR desktop-file-validate REQUIRED)
find_program(SYSTEMD_VALIDATOR systemd-analyze REQUIRED)
find_program(OPENSSL openssl REQUIRED)
find_program(STAT stat REQUIRED)
find_program(XMLLINT xmllint REQUIRED)
set(root "${BUILD_DIR}/startup-install-check")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")

function(check_install logical_prefix physical_prefix)
  set(executable "${logical_prefix}/${INSTALL_BINDIR}/arch-dock")
  if(NOT EXISTS "${physical_prefix}/${INSTALL_BINDIR}/arch-dock")
    message(FATAL_ERROR "The startup executable was not installed")
  endif()
  # On-demand activation is marked so a user's Quit can refuse it. The
  # application entry always opens Panel Studio, starting Arch Dock explicitly
  # when it is stopped, and offers Quit.
  foreach(pair IN ITEMS
      "share/dbus-1/services/org.archdock.ArchDock.service|Exec| --dbus-activated"
      "share/applications/org.archdock.ArchDock.desktop|Exec| --settings|Exec=\"${executable}\" --quit"
      "${INSTALL_LIBDIR}/systemd/user/arch-dock.service|ExecStart|")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 relative)
    list(GET fields 1 key)
    list(LENGTH fields field_count)
    set(suffix "")
    if(field_count GREATER 2)
      list(SUBLIST fields 2 -1 suffix_fields)
      list(JOIN suffix_fields ";" suffix)
    endif()
    file(STRINGS "${physical_prefix}/${relative}" values REGEX "^${key}=")
    if(NOT values STREQUAL "${key}=\"${executable}\"${suffix}")
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
  # The bar host's entry names the installed bar host itself: KWin grants it
  # the Wayland interfaces the entry lists only then.
  set(bars_entry "share/applications/org.archdock.ArchDock.Bars.desktop")
  file(STRINGS "${physical_prefix}/${bars_entry}" bars REGEX "^Exec=")
  if(NOT bars STREQUAL "Exec=\"${logical_prefix}/${INSTALL_BINDIR}/arch-dock-bars\"")
    message(FATAL_ERROR "Bar host path mismatch in ${bars_entry}: ${bars}")
  endif()
  if(NOT "${logical_prefix}/${bars_entry}" IN_LIST manifest)
    message(FATAL_ERROR "The bar host entry is absent from the install manifest")
  endif()
  # pacman's Qt hook and its script: pacman reads hooks only from
  # /usr/share/libalpm/hooks, so the hook names the script there.
  foreach(relative IN ITEMS share/libalpm/hooks/arch-dock-qt.hook share/libalpm/scripts/arch-dock-check-qt)
    if(NOT EXISTS "${physical_prefix}/${relative}" OR NOT "${logical_prefix}/${relative}" IN_LIST manifest)
      message(FATAL_ERROR "${relative} is not installed or not in the install manifest")
    endif()
  endforeach()
  file(STRINGS "${physical_prefix}/share/libalpm/hooks/arch-dock-qt.hook" hook_exec REGEX "^Exec")
  if(NOT hook_exec STREQUAL "Exec = /usr/share/libalpm/scripts/arch-dock-check-qt")
    message(FATAL_ERROR "The Qt hook runs ${hook_exec}")
  endif()
  execute_process(COMMAND "${STAT}" -c %a "${physical_prefix}/share/libalpm/scripts/arch-dock-check-qt"
    OUTPUT_VARIABLE script_mode OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT script_mode STREQUAL "755")
    message(FATAL_ERROR "The Qt hook's script has mode ${script_mode}, not 755")
  endif()
  # The password window's action names the helper installed with it, and the
  # helper is a program only root can change.
  set(policy "share/polkit-1/actions/org.archdock.ArchDock.policy")
  set(helper "${INSTALL_LIBDIR}/arch-dock/arch-dock-install-update")
  foreach(relative IN ITEMS "${policy}" "${helper}")
    if(NOT EXISTS "${physical_prefix}/${relative}" OR NOT "${logical_prefix}/${relative}" IN_LIST manifest)
      message(FATAL_ERROR "${relative} is not installed or not in the install manifest")
    endif()
  endforeach()
  execute_process(COMMAND "${XMLLINT}" --noout --nonet --dtdvalid /usr/share/polkit-1/policyconfig-1.dtd
    "${physical_prefix}/${policy}" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "The update action does not validate: ${output}${error}")
  endif()
  file(READ "${physical_prefix}/${policy}" action)
  if(NOT action MATCHES "<annotate key=\"org.freedesktop.policykit.exec.path\">${logical_prefix}/${helper}</annotate>"
     OR NOT action MATCHES "<allow_active>auth_admin</allow_active>")
    message(FATAL_ERROR "The update action does not name ${logical_prefix}/${helper} with auth_admin")
  endif()
  execute_process(COMMAND "${STAT}" -c %a "${physical_prefix}/${helper}"
    OUTPUT_VARIABLE helper_mode OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT helper_mode STREQUAL "755")
    message(FATAL_ERROR "The update helper has mode ${helper_mode}, not 755")
  endif()
  execute_process(COMMAND "${DESKTOP_VALIDATOR}"
    "${physical_prefix}/share/applications/org.archdock.ArchDock.desktop"
    "${physical_prefix}/${bars_entry}"
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

# A rehearsal build installs no password-window action: its helper lies in a
# folder this account can write. The install template, filled in as a
# rehearsal build fills it, still installs everything else.
set(rehearsal "${root}/rehearsal")
file(MAKE_DIRECTORY "${rehearsal}/build")
file(COPY "${BUILD_DIR}/startup-templates" DESTINATION "${rehearsal}/build")
file(READ "${SOURCE_DIR}/cmake/InstallStartupMetadata.cmake.in" template)
string(REPLACE "@ARCHDOCK_UPDATE_REHEARSAL_ROOT@" "${rehearsal}" template "${template}")
string(REPLACE "@CMAKE_CURRENT_BINARY_DIR@" "${rehearsal}/build" template "${template}")
string(REPLACE "@CMAKE_INSTALL_BINDIR@" "bin" template "${template}")
string(REPLACE "@CMAKE_INSTALL_LIBDIR@" "${INSTALL_LIBDIR}" template "${template}")
if(template MATCHES "@[A-Z_]+@")
  message(FATAL_ERROR "The startup install template has a value this check does not fill: ${CMAKE_MATCH_0}")
endif()
file(WRITE "${rehearsal}/install-startup.cmake" "${template}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DCMAKE_INSTALL_PREFIX=${rehearsal}/usr" -P "${rehearsal}/install-startup.cmake"
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0 OR NOT EXISTS "${rehearsal}/usr/share/dbus-1/services/org.archdock.ArchDock.service")
  message(FATAL_ERROR "The rehearsal startup install failed: ${output}${error}")
endif()
if(EXISTS "${rehearsal}/usr/share/polkit-1/actions/org.archdock.ArchDock.policy")
  message(FATAL_ERROR "A rehearsal build installs a password-window action")
endif()

# The Qt the build used, for the package's lowest Qt version.
file(READ "${BUILD_DIR}/qt-build-version" qt_build_version)
if(NOT qt_build_version STREQUAL "${QT_VERSION}\n")
  message(FATAL_ERROR "qt-build-version holds \"${qt_build_version}\", not ${QT_VERSION}")
endif()

# `arch-dock --version` answers with no display, bus or session at all.
execute_process(COMMAND env -i "${BUILD_DIR}/arch-dock" --version
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT status EQUAL 0 OR NOT output STREQUAL "Arch Dock ${EXPECTED_VERSION}\n")
  message(FATAL_ERROR "arch-dock --version gave ${status}: ${output}${error}")
endif()

# The update key list: one P-256 key is read; two keys, a malformed line, a
# long or repeated id, another kind of key, and a release build without a key
# stop the configuration.
set(keys "${root}/keys")
file(MAKE_DIRECTORY "${keys}")
function(public_key_hex curve name out)
  execute_process(COMMAND "${OPENSSL}" ecparam -name ${curve} -genkey -noout -out "${keys}/${name}.key"
    RESULT_VARIABLE made)
  execute_process(COMMAND "${OPENSSL}" pkey -in "${keys}/${name}.key" -pubout -outform DER -out "${keys}/${name}.der"
    RESULT_VARIABLE exported)
  if(NOT made EQUAL 0 OR NOT exported EQUAL 0)
    message(FATAL_ERROR "openssl could not make the test key ${name}")
  endif()
  file(READ "${keys}/${name}.der" hex HEX)
  set(${out} "${hex}" PARENT_SCOPE)
endfunction()
public_key_hex(prime256v1 first first_key)
public_key_hex(prime256v1 second second_key)
public_key_hex(secp384r1 other other_key)
function(key_case name kind text expect)
  file(WRITE "${keys}/${name}.txt" "# a test key list\n${text}")
  file(WRITE "${keys}/${name}.cmake"
    "include(\"${SOURCE_DIR}/cmake/ArchDockUpdateKeys.cmake\")\n"
    "archdock_select_update_key(${kind} \"${keys}/${name}.txt\" id key)\n"
    "message(STATUS \"key <\${id}>\")\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -P "${keys}/${name}.cmake"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(expect STREQUAL "refused")
    if(status EQUAL 0)
      message(FATAL_ERROR "The key list case ${name} was accepted: ${output}")
    endif()
  elseif(NOT status EQUAL 0 OR NOT output MATCHES "key <${expect}>")
    message(FATAL_ERROR "The key list case ${name} gave ${status}: ${output}${error}")
  endif()
endfunction()
key_case(one release "arch-dock-update-1 ${first_key}\n" arch-dock-update-1)
key_case(none-development development "" "")
key_case(none-release release "" refused)
key_case(two release "arch-dock-update-1 ${first_key}\narch-dock-update-2 ${second_key}\n" refused)
key_case(malformed release "arch-dock-update-1\n" refused)
key_case(not-hex release "arch-dock-update-1 zz${first_key}\n" refused)
key_case(capital-id release "Arch-Dock ${first_key}\n" refused)
key_case(long-id release "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ${first_key}\n" refused)
key_case(twice test "arch-dock-test-1 ${first_key}\narch-dock-test-1 ${first_key}\n" refused)
key_case(p384 release "arch-dock-update-1 ${other_key}\n" refused)

# Where the Update button looks: the system's places, or, in a rehearsal
# build, its own root with a test server on this PC for GitHub, its test key
# list and its install inside the root.
function(places_case name rehearsal feed test_keys prefix expect)
  file(WRITE "${keys}/${name}-places.cmake"
    "include(\"${SOURCE_DIR}/cmake/ArchDockUpdateKeys.cmake\")\n"
    "archdock_update_places(\"${rehearsal}\" \"${feed}\" \"${test_keys}\" \"${prefix}\" tools lock api downloads)\n"
    "message(STATUS \"places <\${tools} \${lock} \${api} \${downloads}>\")\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -P "${keys}/${name}-places.cmake"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(expect STREQUAL "refused")
    if(status EQUAL 0)
      message(FATAL_ERROR "The places case ${name} was accepted: ${output}")
    endif()
    return()
  endif()
  string(FIND "${output}" "places <${expect}>" found)
  if(NOT status EQUAL 0 OR found EQUAL -1)
    message(FATAL_ERROR "The places case ${name} gave ${status}: ${output}${error}")
  endif()
endfunction()
set(feed "http://127.0.0.1:47055")
places_case(system "" "" "" /usr "/usr/bin /var/lib/pacman/db.lck https://api.github.com https://github.com")
places_case(rehearsal /r "${feed}" /k.txt /r/usr "/r/tools /r/var/lib/pacman/db.lck ${feed} ${feed}")
places_case(rehearsal-without-test-keys /r "${feed}" "" /r/usr refused)
places_case(rehearsal-with-github /r "https://api.github.com" /k.txt /r/usr refused)
places_case(rehearsal-with-another-host /r "http://localhost:47055" /k.txt /r/usr refused)
places_case(rehearsal-installed-outside /r "${feed}" /k.txt /usr refused)
places_case(relative-rehearsal-root r "${feed}" /k.txt r/usr refused)
places_case(feed-without-rehearsal "" "${feed}" "" /usr refused)

file(REMOVE_RECURSE "${root}")
message(STATUS "Startup metadata and manifests match both prefix and DESTDIR installations; "
               "the Qt version, --version, the update key list and a rehearsal build's places are as built")
