cmake_minimum_required(VERSION 3.20)
project(tce_reconstruction C)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Use Visual Studio with -A Win32; the ET module ABI is 32-bit.")
endif()

# This is initially the ET SDK baseline, not a completed TC:E reconstruction.
# Keep outputs away from the reference binaries in tcetest/.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(cmake/sdk_sources.cmake)

function(et_module target folder define)
  add_library(${target} SHARED ${${target}_SOURCES} game/tce_bg.c
    game/tce_weapon_parse.c game/tce_weapon_init.c game/tce_gear_parse.c ${folder}/${folder}.def)
  target_compile_definitions(${target} PRIVATE WIN32 _WINDOWS ${define}
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  target_compile_options(${target} PRIVATE /W3 /Zc:preprocessor-)
  target_link_libraries(${target} PRIVATE winmm)
  set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "${target}_mp_x86"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

et_module(qagame game GAMEDLL)
et_module(cgame cgame CGAMEDLL)
et_module(ui ui UIDLL)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME reconstruction_ledger COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/reconstruction_tracker.py" check)
  add_executable(tce_shared_test tests/tce_shared_test.c tests/tce_parser_test.c
    tests/tce_weapon_init_test.c game/tce_bg.c game/tce_weapon_parse.c
    game/tce_weapon_init.c game/tce_gear_parse.c game/q_shared.c)
  target_compile_definitions(tce_shared_test PRIVATE WIN32 _CRT_SECURE_NO_WARNINGS)
  set(reference_dll "${CMAKE_CURRENT_SOURCE_DIR}/../tcetest/cgame_mp_x86.dll")
  file(SHA256 "${reference_dll}" reference_sha256)
  if(reference_sha256 STREQUAL "d85a8e94da7c94956eff560e4cc406b4116f5a6ffd820b59accd9ad7e1e2414c")
    add_test(NAME tce_shared_binary_comparison COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_recovery.py" $<TARGET_FILE:tce_shared_test>)
  else()
    message(FATAL_ERROR "Reference DLL hash changed; re-verify test function addresses first.")
  endif()
endif()
