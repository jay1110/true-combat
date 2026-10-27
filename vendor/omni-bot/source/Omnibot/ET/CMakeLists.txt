# Omnibot ET build

add_library(omnibot-et MODULE ${CMAKE_CURRENT_SOURCE_DIR}/ET_BatchBuild.cpp)
target_link_libraries(omnibot-et omni-shared omnibot-common)
if(MSVC)
    target_link_options(omnibot-et PRIVATE "/MAP:$<TARGET_FILE_DIR:omnibot-et>/$<TARGET_FILE_BASE_NAME:omnibot-et>.map")
elseif(NOT APPLE AND NOT TCE_ZIG_EXECUTABLE)
    # Keep defined local callbacks observable as well as exported C++ methods.
    # Final WASM exports alone do not name internal GM drawing callbacks.
    target_link_options(omnibot-et PRIVATE
        "LINKER:-Map,$<TARGET_FILE_DIR:omnibot-et>/$<TARGET_FILE_BASE_NAME:omnibot-et>.map")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(omnibot-et PRIVATE -Wl,--no-undefined)
    if(TCE_ZIG_EXECUTABLE)
        # The Zig C++ shared link does not pull integer helpers referenced only
        # by a static C archive. Supply its matching PIC compiler runtime.
        get_filename_component(OMNI_ZIG_DIRECTORY "${TCE_ZIG_EXECUTABLE}" DIRECTORY)
        set(OMNI_COMPILER_RT_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/libomnibot_compiler_rt.a")
        set(OMNI_ZIG_CPU_ARGS)
        if(OMNIBOT_TARGET_ARCH STREQUAL aarch64)
            set(OMNI_ZIG_TARGET aarch64-linux-gnu.2.17)
        elseif(OMNIBOT_TARGET_ARCH STREQUAL armv7)
            set(OMNI_ZIG_TARGET arm-linux-gnueabihf.2.17)
            set(OMNI_ZIG_CPU_ARGS -mcpu=cortex_a7)
        elseif(OMNIBOT_TARGET_ARCH STREQUAL x86_64)
            set(OMNI_ZIG_TARGET x86_64-linux-gnu.2.17)
        else()
            set(OMNI_ZIG_TARGET x86-linux-gnu.2.17)
        endif()
        add_custom_command(OUTPUT "${OMNI_COMPILER_RT_OBJECT}"
            COMMAND "${TCE_ZIG_EXECUTABLE}" build-lib
                "${OMNI_ZIG_DIRECTORY}/lib/compiler_rt.zig"
                -target ${OMNI_ZIG_TARGET} ${OMNI_ZIG_CPU_ARGS} -O ReleaseFast -fPIC -fno-compiler-rt
                "-femit-bin=${OMNI_COMPILER_RT_OBJECT}"
            DEPENDS "${OMNI_ZIG_DIRECTORY}/lib/compiler_rt.zig"
            VERBATIM)
        add_custom_target(omnibot-compiler-runtime DEPENDS "${OMNI_COMPILER_RT_OBJECT}")
        add_dependencies(omnibot-et omnibot-compiler-runtime)
        target_link_libraries(omnibot-et "${OMNI_COMPILER_RT_OBJECT}")
        target_link_options(omnibot-et PRIVATE -Wl,-Bsymbolic)
    endif()
endif()

set_target_properties(omnibot-et PROPERTIES
    PREFIX ""
    OUTPUT_NAME "omnibot_et${MODULE_SUFFIX}"
    # POSITION_INDEPENDENT_CODE ON
    # MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

if (EMSCRIPTEN)
    # A dlopen()-able wasm side module, named like the engine's other ones
    # (omnibot_et.wasm32.so) so BotLoadLibrary.cpp finds it. The web shell has
    # to compile it up front (FS.createPreloadedFile) just like cgame/ui/qagame,
    # because the browser will not compile a module synchronously on the main
    # thread and dlopen() would fail otherwise.
    set_target_properties(omnibot-et PROPERTIES SUFFIX ".so")
    target_link_options(omnibot-et PRIVATE -sSIDE_MODULE=1)
endif ()

if (WIN32)
    set_target_properties(omnibot-et PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ""
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ""
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ""
    )
    target_sources(omnibot-et PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../projects/msvc12/Common/Common.rc)
endif ()

# A generator-independent contract for release tooling; never guess VS output
# directories or copy a core built with the other authoring variant.
get_filename_component(OMNIBOT_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
if(FEATURE_NAVSTUDIO)
    set(OMNIBOT_BUILD_VARIANT navstudio)
else()
    set(OMNIBOT_BUILD_VARIANT game)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/omnibot-runtime-$<CONFIG>.json"
    CONTENT "{\n  \"schema\": 1,\n  \"variant\": \"${OMNIBOT_BUILD_VARIANT}\",\n  \"system\": \"${CMAKE_SYSTEM_NAME}\",\n  \"architecture\": \"${OMNIBOT_TARGET_ARCH}\",\n  \"library\": \"$<TARGET_FILE:omnibot-et>\",\n  \"runtime_scripts\": \"${OMNIBOT_VENDOR_ROOT}/runtime\"\n}\n")
