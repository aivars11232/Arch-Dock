cmake_minimum_required(VERSION 3.21)
# Arch Dock's programs and QML libraries must not import Qt's private
# interface. Such imports tie a build to one exact Qt minor release, and a Qt
# update would then stop Arch Dock from starting. Two kinds count: symbols Qt
# exports under its private version tag, and the QML engine's ahead-of-time
# helpers, which QML compiled to C++ calls.
#
# The bars' one private Wayland window interface is looked up by name while
# they run and imports nothing. It must stay the only private use in the
# sources, so that it is the only thing a Qt update can take away.
foreach(required IN ITEMS BINARIES SOURCE_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT DEFINED READELF)
  find_program(READELF readelf REQUIRED)
endif()

set(failures "")

# The built files, given as one list joined with "|".
string(REPLACE "|" ";" binaries "${BINARIES}")
list(LENGTH binaries binary_count)
if(binary_count EQUAL 0)
  message(FATAL_ERROR "No programs or libraries were given to check")
endif()
foreach(binary IN LISTS binaries)
  if(NOT EXISTS "${binary}")
    list(APPEND failures "${binary} was not built")
    continue()
  endif()
  execute_process(
    COMMAND "${READELF}" --dyn-syms --wide "${binary}"
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE readelf_errors
    RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    list(APPEND failures "readelf could not read ${binary}: ${readelf_errors}")
    continue()
  endif()
  # Names are read in their mangled form: QQmlPrivate::AOT... functions,
  # const functions, vtables and type information all start this way.
  string(REGEX MATCHALL "[^ \n]+@Qt_6_PRIVATE_API" private_imports "${symbols}")
  string(REGEX MATCHALL "_Z(T[VIS])?NK?11QQmlPrivate[0-9]+AOT[^ \n@]*" aot_imports "${symbols}")
  list(LENGTH private_imports private_count)
  list(LENGTH aot_imports aot_count)
  get_filename_component(name "${binary}" NAME)
  if(private_count GREATER 0 OR aot_count GREATER 0)
    list(SUBLIST private_imports 0 3 private_examples)
    list(SUBLIST aot_imports 0 3 aot_examples)
    list(JOIN private_examples ", " private_examples)
    list(JOIN aot_examples ", " aot_examples)
    list(APPEND failures
      "${name}: ${aot_count} ahead-of-time QML imports (${aot_examples}) and ${private_count} imports of Qt's private interface (${private_examples})")
  else()
    message(STATUS "${name}: no private or ahead-of-time imports")
  endif()
endforeach()

# The sources: Qt's private headers and private native interfaces appear only
# where the bars ask KWin's window part to hide.
set(allowed "src/bars/BarWindow.cpp")
file(GLOB_RECURSE sources RELATIVE "${SOURCE_DIR}"
  "${SOURCE_DIR}/src/*.cpp" "${SOURCE_DIR}/src/*.cc" "${SOURCE_DIR}/src/*.h" "${SOURCE_DIR}/src/*.hpp")
foreach(source IN LISTS sources)
  file(STRINGS "${SOURCE_DIR}/${source}" private_uses
    REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*(_p\\.h|/private/|qpa/)|QNativeInterface::Private")
  if(private_uses AND NOT source IN_LIST allowed)
    list(APPEND failures "${source} uses Qt's private interface: ${private_uses}")
  endif()
endforeach()
foreach(source IN LISTS allowed)
  if(NOT EXISTS "${SOURCE_DIR}/${source}")
    list(APPEND failures "${source}, the one place allowed a private use, is missing")
  endif()
endforeach()

if(failures)
  list(JOIN failures "\n  " report)
  message(FATAL_ERROR "Qt's private interface is used:\n  ${report}")
endif()
message(STATUS "Checked ${binary_count} programs and libraries and the sources: no private Qt use beyond ${allowed}")
