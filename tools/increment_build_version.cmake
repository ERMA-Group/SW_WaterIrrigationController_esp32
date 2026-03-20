if(NOT DEFINED BUILD_VERSION_FILE)
    message(FATAL_ERROR "BUILD_VERSION_FILE is required")
endif()

if(NOT EXISTS "${BUILD_VERSION_FILE}")
    message(FATAL_ERROR "Build version header not found: ${BUILD_VERSION_FILE}")
endif()

file(READ "${BUILD_VERSION_FILE}" content)
string(REGEX MATCH "kBuild[ \t]*=[ \t]*([0-9]+)" match "${content}")

if(NOT match)
    message(FATAL_ERROR "kBuild entry was not found in ${BUILD_VERSION_FILE}")
endif()

set(current "${CMAKE_MATCH_1}")
math(EXPR next "${current} + 1")

string(REGEX REPLACE "kBuild[ \t]*=[ \t]*[0-9]+" "kBuild = ${next}" updated "${content}")
file(WRITE "${BUILD_VERSION_FILE}" "${updated}")

message(STATUS "Incremented build version: ${current} -> ${next}")
