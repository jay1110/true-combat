include_guard(GLOBAL)
if(TCE_BUILD_TCE_HELPERS)
  add_custom_target(tce_helpers SOURCES
    "${TCE_ROOT_DIR}/tce/launch-legacy.sh" "${TCE_ROOT_DIR}/tce/README.md")
endif()
if(TCE_BUILD_OMNIBOT_CORE)
  set(OMNIBOT_ET ON CACHE BOOL "Build ET bot runtime" FORCE)
  set(OMNIBOT_RTCW OFF CACHE BOOL "Build RTCW runtime" FORCE)
  set(OMNIBOT_STD_FILESYSTEM ON CACHE BOOL "Use modern header-only Boost dependencies" FORCE)
  add_subdirectory("${TCE_VENDOR_DIR}/omni-bot/source/Omnibot" "${CMAKE_BINARY_DIR}/omnibot" EXCLUDE_FROM_ALL)
  add_custom_target(omnibot ALL DEPENDS omnibot-et)
  add_custom_target(tce_game_runtime DEPENDS tce_modules omnibot-et)
endif()
if(TCE_BUILD_NAVMESH_TOOL)
  enable_language(CXX)
  add_subdirectory("${TCE_NAVSTUDIO_DIR}/navmesh" "${CMAKE_BINARY_DIR}/navmesh" EXCLUDE_FROM_ALL)
endif()
if(TCE_BUILD_NAVSTUDIO)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_custom_target(navstudio SOURCES "${TCE_NAVSTUDIO_DIR}/entry.py" "${TCE_NAVSTUDIO_DIR}/README.md")
  add_custom_target(navstudio-run
    COMMAND "${Python3_EXECUTABLE}" "${TCE_NAVSTUDIO_DIR}/entry.py"
    WORKING_DIRECTORY "${TCE_ROOT_DIR}" USES_TERMINAL)
  if(WIN32)
    find_program(TCE_POWERSHELL NAMES powershell pwsh REQUIRED)
    set(tce_recast_package_args -RecastBuild "${CMAKE_BINARY_DIR}/navmesh")
    if(TARGET tce_navmesh_preview)
      include(FetchContent)
      FetchContent_GetProperties(recast SOURCE_DIR tce_recast_source_dir)
      set(tce_recast_package_args
        -RecastExecutable "$<TARGET_FILE:tce_navmesh_preview>"
        -RecastLicense "${tce_recast_source_dir}/License.txt")
    endif()
    add_custom_target(navstudio-package
      COMMAND "${TCE_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File
        "${TCE_NAVSTUDIO_DIR}/build-windows.ps1"
        -Python "${Python3_EXECUTABLE}"
        ${tce_recast_package_args}
        -Destination "${CMAKE_BINARY_DIR}/navstudio-package"
      WORKING_DIRECTORY "${TCE_ROOT_DIR}" USES_TERMINAL VERBATIM)
    if(TARGET tce_navmesh_preview)
      add_dependencies(navstudio-package tce_navmesh_preview)
    endif()
  endif()
endif()
