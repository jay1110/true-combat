if(NOT TCE_EMSDK)
  set(TCE_EMSDK "$ENV{EMSDK}" CACHE PATH "Emscripten SDK installation")
endif()
if(NOT EXISTS "${TCE_EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  message(FATAL_ERROR "Set TCE_EMSDK or EMSDK to an installed Emscripten SDK")
endif()
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TCE_EMSDK)
include("${TCE_EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

# Ask the active SDK: older Emscripten calls this compiler_rt, newer SDKs
# clang_rt.builtins. Link by name so emcc selects its matching PIC variant.
execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-libgcc-file-name
  RESULT_VARIABLE _tce_builtin_status OUTPUT_VARIABLE _tce_builtin_path
  OUTPUT_STRIP_TRAILING_WHITESPACE)
get_filename_component(_tce_builtin_file "${_tce_builtin_path}" NAME)
if(NOT _tce_builtin_status EQUAL 0 OR NOT _tce_builtin_file MATCHES "^lib([A-Za-z0-9_.-]+)\\.a$")
  message(FATAL_ERROR "Cannot identify the Emscripten compiler builtins library")
endif()
set(TCE_WASM_BUILTINS_LIBRARY "${CMAKE_MATCH_1}" CACHE INTERNAL "Active Emscripten compiler builtins" FORCE)
