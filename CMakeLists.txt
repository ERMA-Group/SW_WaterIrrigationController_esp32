# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/M_BSW_ESP32
    # add other submodules here
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(SW_WaterIrrigationContoller)

# Include submodules
add_subdirectory(src)
add_subdirectory(M_ESTP)
add_subdirectory(M_ShiftRegister)