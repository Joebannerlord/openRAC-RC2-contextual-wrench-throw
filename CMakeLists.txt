# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(rac2_contextual_wrench VERSION 0.1.2 LANGUAGES CXX)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" sdk_lock)
string(JSON pinned_sdk GET "${sdk_lock}" version)
set(OPENRAC_SDK "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${pinned_sdk}"
    CACHE PATH "Unpacked OpenRAC Mod SDK")
if(NOT EXISTS "${OPENRAC_SDK}/VERSION")
  message(FATAL_ERROR "Run python tools/fetch_sdk.py to fetch OpenRAC Mod SDK ${pinned_sdk}")
endif()
file(STRINGS "${OPENRAC_SDK}/VERSION" actual_sdk LIMIT_COUNT 1)
if(NOT actual_sdk STREQUAL pinned_sdk)
  message(FATAL_ERROR "Expected OpenRAC Mod SDK ${pinned_sdk}, found ${actual_sdk}")
endif()
set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
find_package(OpenRACMod 1.20 EXACT CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(mod_target rac2_contextual_wrench_012)
openrac_add_mod(${mod_target} MANIFEST openrac-mod.json SOURCES src/mod.cpp)
set_property(TARGET ${mod_target} PROPERTY CXX_STANDARD 20)
get_target_property(package_dir ${mod_target} OPENRAC_MOD_PACKAGE_DIR)

add_custom_target(release_zip
  COMMAND "${Python3_EXECUTABLE}" "${OPENRAC_SDK}/tools/package_mod.py"
          "${package_dir}" -o "${CMAKE_CURRENT_BINARY_DIR}/dist"
  DEPENDS ${mod_target}
  VERBATIM)

include(CTest)
if(BUILD_TESTING)
  add_executable(input_test tests/input_test.cpp)
  add_test(NAME contextual_input COMMAND input_test)
  add_executable(plugin_test tests/plugin_test.cpp)
  target_include_directories(plugin_test PRIVATE "${OPENRAC_SDK}/include")
  target_link_libraries(plugin_test PRIVATE ${CMAKE_DL_LIBS})
  add_test(NAME contextual_plugin COMMAND plugin_test
    "${package_dir}/lib/${OPENRAC_MOD_PLATFORM}/$<TARGET_FILE_NAME:${mod_target}>")
endif()
