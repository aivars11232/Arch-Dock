# The icon helper with pictures it cannot use, and no AI models: the dock's
# own spoilt picture (--remove-unreadable) is removed, for the dock to write
# again, without loading the network and without a mark that would keep it
# from being made later; anyone else's is left alone; a good picture with no
# models, or with too little free memory, is the helper's own failure, never
# marked either.
# cmake -DHELPER=<arch-dock-icon-object> -DWORK=<folder> -DPICTURE=<a PNG> -P icon-helper-pictures.cmake
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(WRITE "${WORK}/spoilt.png" "not a picture, as a picture cut short would read")
execute_process(COMMAND "${HELPER}" "${WORK}/spoilt.png" "${WORK}/spoilt.adicon"
                RESULT_VARIABLE keptStatus ERROR_VARIABLE keptError)
if(NOT keptStatus EQUAL 4)
  message(FATAL_ERROR "someone else's spoilt picture should be refused (exit 4), not ${keptStatus}: ${keptError}")
endif()
if(NOT EXISTS "${WORK}/spoilt.png")
  message(FATAL_ERROR "a picture the dock did not ask to remove was removed")
endif()

execute_process(COMMAND "${HELPER}" --remove-unreadable "${WORK}/spoilt.png" "${WORK}/spoilt.adicon"
                RESULT_VARIABLE spoiltStatus ERROR_VARIABLE spoiltError)
if(NOT spoiltStatus EQUAL 0)
  message(FATAL_ERROR "the dock's spoilt picture alone should leave nothing to make (exit ${spoiltStatus}): ${spoiltError}")
endif()
if(EXISTS "${WORK}/spoilt.png")
  message(FATAL_ERROR "the dock's spoilt picture was not removed for it to write again")
endif()
if(EXISTS "${WORK}/spoilt.failed" OR EXISTS "${WORK}/spoilt.adicon")
  message(FATAL_ERROR "a spoilt picture was marked or made")
endif()

file(COPY_FILE "${PICTURE}" "${WORK}/good.png")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ARCHDOCK_ICON_OBJECT_MEMORY_MIN_MB=999999999
                        "${HELPER}" --remove-unreadable "${WORK}/good.png" "${WORK}/good.adicon"
                RESULT_VARIABLE shortStatus ERROR_VARIABLE shortError)
if(NOT shortStatus EQUAL 5)
  message(FATAL_ERROR "with too little free memory the helper should wait (exit 5), not ${shortStatus}: ${shortError}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ARCHDOCK_ICON_OBJECT_MEMORY_MIN_MB=0
                        "${HELPER}" --remove-unreadable "${WORK}/good.png" "${WORK}/good.adicon"
                RESULT_VARIABLE goodStatus ERROR_VARIABLE goodError)
if(NOT goodStatus EQUAL 2)
  message(FATAL_ERROR "with no models the helper should fail on its own (exit 2), not ${goodStatus}: ${goodError}")
endif()
if(EXISTS "${WORK}/good.failed")
  message(FATAL_ERROR "the helper's own failure marked a good picture for good")
endif()
if(NOT EXISTS "${WORK}/good.png")
  message(FATAL_ERROR "a good picture was removed")
endif()
message(STATUS "icon helper pictures: only the dock's spoilt picture removed, own failures unmarked")
