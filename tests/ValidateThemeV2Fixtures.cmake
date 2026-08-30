cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED ARCHDOCK_SOURCE_DIR)
  cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH ARCHDOCK_SOURCE_DIR)
endif()

set(theme_fixture_root "${ARCHDOCK_SOURCE_DIR}/tests/fixtures/theme-v2")

function(read_json_object file_path output_variable)
  if(NOT EXISTS "${file_path}")
    message(FATAL_ERROR "Theme fixture is missing: ${file_path}")
  endif()
  file(SIZE "${file_path}" file_size)
  if(file_size GREATER 262144)
    message(FATAL_ERROR "Theme fixture exceeds the v2 manifest limit: ${file_path}")
  endif()
  file(READ "${file_path}" json_text)
  string(JSON root_type ERROR_VARIABLE json_error TYPE "${json_text}")
  if(NOT json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Theme fixture is not valid JSON: ${file_path}: ${json_error}")
  endif()
  if(NOT root_type STREQUAL "OBJECT")
    message(FATAL_ERROR "Theme fixture root must be an object: ${file_path}")
  endif()
  set(${output_variable} "${json_text}" PARENT_SCOPE)
endfunction()

if(DEFINED MANIFEST)
  cmake_path(ABSOLUTE_PATH MANIFEST BASE_DIRECTORY "${ARCHDOCK_SOURCE_DIR}"
             NORMALIZE OUTPUT_VARIABLE manifest_path)
  read_json_object("${manifest_path}" ignored_manifest)
  message(STATUS "Validated Theme v2 JSON syntax: ${manifest_path}")
  return()
endif()

set(index_path "${theme_fixture_root}/fixture-index.json")
read_json_object("${index_path}" fixture_index)

string(JSON index_format GET "${fixture_index}" format)
string(JSON index_version GET "${fixture_index}" version)
if(NOT index_format STREQUAL "org.archdock.theme-fixture-index" OR
   NOT index_version EQUAL 1)
  message(FATAL_ERROR "Theme fixture index has an unsupported contract")
endif()

string(JSON fixture_count LENGTH "${fixture_index}" fixtures)
if(NOT fixture_count EQUAL 10)
  message(FATAL_ERROR
          "Theme fixture index must contain exactly 10 fixtures; found ${fixture_count}")
endif()

set(seen_paths)
set(valid_count 0)
set(invalid_count 0)
math(EXPR fixture_last "${fixture_count} - 1")
foreach(fixture_index_number RANGE 0 ${fixture_last})
  string(JSON relative_path GET "${fixture_index}" fixtures
         ${fixture_index_number} path)
  string(JSON expected_valid GET "${fixture_index}" fixtures
         ${fixture_index_number} expectValid)
  string(JSON expected_code GET "${fixture_index}" fixtures
         ${fixture_index_number} expectedCode)

  if(relative_path IN_LIST seen_paths)
    message(FATAL_ERROR "Duplicate theme fixture index path: ${relative_path}")
  endif()
  list(APPEND seen_paths "${relative_path}")

  if(relative_path MATCHES "(^|/)\.\.(/|$)" OR
     IS_ABSOLUTE "${relative_path}")
    message(FATAL_ERROR "Unsafe theme fixture index path: ${relative_path}")
  endif()
  set(fixture_path "${theme_fixture_root}/${relative_path}")
  read_json_object("${fixture_path}" fixture_json)

  if(expected_valid)
    math(EXPR valid_count "${valid_count} + 1")
    if(NOT expected_code STREQUAL "")
      message(FATAL_ERROR
              "Valid fixture must not declare an error code: ${relative_path}")
    endif()
    string(JSON manifest_format ERROR_VARIABLE format_error GET
           "${fixture_json}" format)
    string(JSON manifest_version ERROR_VARIABLE version_error GET
           "${fixture_json}" version)
    string(JSON manifest_id ERROR_VARIABLE id_error GET "${fixture_json}" id)
    string(JSON manifest_name ERROR_VARIABLE name_error GET
           "${fixture_json}" name)
    string(JSON capabilities_type ERROR_VARIABLE capabilities_error TYPE
           "${fixture_json}" capabilities)
    if(NOT format_error STREQUAL "NOTFOUND" OR
       NOT version_error STREQUAL "NOTFOUND" OR
       NOT id_error STREQUAL "NOTFOUND" OR
       NOT name_error STREQUAL "NOTFOUND" OR
       NOT capabilities_error STREQUAL "NOTFOUND" OR
       NOT manifest_format STREQUAL "org.archdock.theme" OR
       NOT manifest_version EQUAL 2 OR
       manifest_id STREQUAL "" OR manifest_name STREQUAL "" OR
       NOT capabilities_type STREQUAL "OBJECT")
      message(FATAL_ERROR
              "Valid fixture is missing the v2 scaffold contract: ${relative_path}")
    endif()
  else()
    math(EXPR invalid_count "${invalid_count} + 1")
    if(expected_code STREQUAL "")
      message(FATAL_ERROR
              "Invalid fixture needs an expected diagnostic: ${relative_path}")
    endif()
  endif()
endforeach()

if(NOT valid_count EQUAL 4 OR NOT invalid_count EQUAL 6)
  message(FATAL_ERROR
          "Expected 4 valid and 6 invalid fixtures; found ${valid_count} and ${invalid_count}")
endif()

message(STATUS
        "Theme v2 fixture scaffold passed: ${valid_count} valid, ${invalid_count} invalid")
