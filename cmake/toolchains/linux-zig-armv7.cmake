if(NOT TCE_ZIG_EXECUTABLE)
  message(FATAL_ERROR "Set TCE_ZIG_EXECUTABLE to the Zig compiler executable")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/linux-armv7.cmake")
