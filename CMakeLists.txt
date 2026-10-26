cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/game/tce_version.h" tce_version_line REGEX "^#define TCE_VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" TCE_MOD_VERSION "${tce_version_line}")
project(tce2_mod VERSION "${TCE_MOD_VERSION}" LANGUAGES C)

# Mod libraries only. The external ET/ETLegacy host is not part of this build.
include(cmake/TCEOptions.cmake)
include(cmake/TCEPlatform.cmake)
include(CTest)
add_subdirectory(src)
include(cmake/TCETools.cmake)
