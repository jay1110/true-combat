cmake_minimum_required(VERSION 3.20)
project(tce_omnibot LANGUAGES C CXX)

# Standalone entry point for the gameplay runtime. The root Game build also
# includes it; waypoint/goal authoring remains an explicit separate variant.
option(FEATURE_NAVSTUDIO "Compile local waypoint/goal authoring code" OFF)
option(OMNIBOT_ET "Build the Enemy Territory bots module" ON)
option(OMNIBOT_RTCW "Build unsupported RTCW module" OFF)
get_filename_component(TCE_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_subdirectory("${TCE_REPOSITORY_ROOT}/vendor/omni-bot/source/Omnibot" core)
