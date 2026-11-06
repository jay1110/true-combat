cmake_minimum_required(VERSION 3.20)
# Compatibility entry point: editor resources are part of the one variant PK3.
# A separate addon would reintroduce resource-ordering and stale-menu problems.
if(NOT MODULE_DIRECTORY)
  message(FATAL_ERROR "Editor addons were replaced by the unified mod package. Set MODULE_DIRECTORY to collected Editor binaries, or use tools/release/local_bundle.py with --variant navstudio.")
endif()
set(BUILD_VARIANT navstudio)
include("${CMAKE_CURRENT_LIST_DIR}/package_modules.cmake")
