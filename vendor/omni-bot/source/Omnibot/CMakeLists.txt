cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)

project(Omnibot)

# Zig's compiler driver also supplies the archive tools when cross-building.
if(TCE_ZIG_EXECUTABLE)
    set(CMAKE_C_ARCHIVE_CREATE "\"${TCE_ZIG_EXECUTABLE}\" ar qc <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "\"${TCE_ZIG_EXECUTABLE}\" ar qc <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "\"${TCE_ZIG_EXECUTABLE}\" ranlib <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "\"${TCE_ZIG_EXECUTABLE}\" ranlib <TARGET>")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

macro(add_cxx_flag flag)
    message(STATUS "Configure with flag '${flag}'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endmacro()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MSVC static runtime linkage
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(OMNIBOT_ET "Build the Enemy Territory bots module" ON)
option(OMNIBOT_RTCW "Build the Return to Castle Wolfenstein bots module" OFF)
option(OMNIBOT_STATIC_BOOST "Use static linked boost libraries" ON)
option(FEATURE_NAVSTUDIO "Compile the local waypoint and goal authoring tools" OFF)

# Boost.Filesystem and Boost.Regex are the only Boost libraries here that have
# to be compiled; everything else is header only. Replacing them with
# <filesystem> and <regex> means a cross build - the WebAssembly one above all -
# only needs Boost *headers*, which are architecture independent, instead of a
# full cross-compiled Boost. Requires C++17, so it stays opt-in for the native
# builds that are known to work with the C++11/Boost combination.
if (EMSCRIPTEN)
    set(OMNIBOT_STD_FILESYSTEM_DEFAULT ON)
else ()
    set(OMNIBOT_STD_FILESYSTEM_DEFAULT OFF)
endif ()
option(OMNIBOT_STD_FILESYSTEM "Use std::filesystem/std::regex instead of the compiled Boost libraries" ${OMNIBOT_STD_FILESYSTEM_DEFAULT})

if (OMNIBOT_STD_FILESYSTEM)
    set(CMAKE_CXX_STANDARD 17)
endif ()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR_LOWER)
set(MODULE_SUFFIX "")
if(EMSCRIPTEN)
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
    set(OMNIBOT_TARGET_ARCH wasm32)
    set(MODULE_SUFFIX ".wasm32")
elseif(ANDROID)
    set(OMNIBOT_TARGET_ARCH "${CMAKE_ANDROID_ARCH_ABI}")
    if(NOT OMNIBOT_TARGET_ARCH MATCHES "^(armeabi-v7a|arm64-v8a)$")
        message(FATAL_ERROR "Unsupported Omni-bot Android ABI: ${OMNIBOT_TARGET_ARCH}")
    endif()
    set(MODULE_SUFFIX ".android.${OMNIBOT_TARGET_ARCH}")
elseif(APPLE)
    set(OMNIBOT_TARGET_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    if(OMNIBOT_TARGET_ARCH MATCHES ";")
        set(OMNIBOT_TARGET_ARCH universal)
    elseif(NOT OMNIBOT_TARGET_ARCH)
        set(OMNIBOT_TARGET_ARCH "${SYSTEM_PROCESSOR_LOWER}")
    endif()
    set(MODULE_SUFFIX "_mac")
elseif(SYSTEM_PROCESSOR_LOWER MATCHES "^(x86|i[3-6]86|x86_64|amd64)$")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OMNIBOT_TARGET_ARCH x86_64)
        if(WIN32)
            set(MODULE_SUFFIX "_x64")
        else()
            set(MODULE_SUFFIX ".x86_64")
        endif()
    else()
        set(OMNIBOT_TARGET_ARCH i386)
    endif()
elseif(SYSTEM_PROCESSOR_LOWER MATCHES "^(aarch64|arm64)$")
    set(OMNIBOT_TARGET_ARCH aarch64)
    set(MODULE_SUFFIX ".aarch64")
elseif(SYSTEM_PROCESSOR_LOWER MATCHES "^arm")
    set(OMNIBOT_TARGET_ARCH armv7)
    set(MODULE_SUFFIX ".armv7")
else()
    message(FATAL_ERROR "Unsupported Omni-bot target: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(DEPENDENCIES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")

add_library(omni-shared INTERFACE)
if(FEATURE_NAVSTUDIO)
    target_compile_definitions(omni-shared INTERFACE FEATURE_NAVSTUDIO)
    file(WRITE "${CMAKE_BINARY_DIR}/tce-build-variant.txt" "navstudio\n")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/tce-build-variant.txt" "game\n")
endif()

set(Boost_USE_STATIC_LIBS        ${OMNIBOT_STATIC_BOOST})  # only find static libs
set(Boost_USE_DEBUG_LIBS         OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)

if (WIN32)
    set(Boost_USE_STATIC_RUNTIME    ON)
else ()
    set(Boost_USE_STATIC_RUNTIME    OFF)
endif ()

if (OMNIBOT_STD_FILESYSTEM)
    # These headers are architecture independent. Avoid FindBoost's removed
    # CMake policy and stage only boost/, never the host libc include root.
    if(NOT OMNIBOT_BOOST_INCLUDEDIR)
        find_path(OMNIBOT_BOOST_INCLUDEDIR boost/version.hpp
            HINTS "${Boost_ROOT}" "$ENV{BOOST_ROOT}" PATHS "C:/boost"
            NO_CMAKE_FIND_ROOT_PATH)
        if(NOT OMNIBOT_BOOST_INCLUDEDIR)
            message(FATAL_ERROR "Boost headers missing; set OMNIBOT_BOOST_INCLUDEDIR to their parent directory")
        endif()
    endif()
    # What is left of Boost here is header only (dynamic_bitset, multi_array,
    # pool, lexical_cast, algorithm/string, bind, smart pointers, ...) and
    # therefore architecture independent. Emscripten's toolchain file restricts
    # find_package()/find_path() to its own sysroot, which has no Boost, so
    # allow pointing straight at any host header tree instead:
    #   -DOMNIBOT_BOOST_INCLUDEDIR=/usr/include
    if (OMNIBOT_BOOST_INCLUDEDIR)
        if (NOT EXISTS "${OMNIBOT_BOOST_INCLUDEDIR}/boost/version.hpp")
            message(FATAL_ERROR
                "OMNIBOT_BOOST_INCLUDEDIR=${OMNIBOT_BOOST_INCLUDEDIR} does not contain boost/version.hpp")
        endif ()
        message(STATUS "Using Boost headers from ${OMNIBOT_BOOST_INCLUDEDIR}")
        # Do NOT put that directory on the include path directly. The usual value
        # is a system include root such as /usr/include, and adding it makes the
        # *host* libc headers visible to the cross compiler: <stdio.h> then
        # resolves to glibc's, which immediately fails with
        #   fatal error: 'bits/libc-header-start.h' file not found
        # because that glibc-internal header does not exist in the Emscripten
        # sysroot. Expose a directory that contains nothing but "boost" instead,
        # so only the header-only Boost tree is added and the toolchain keeps its
        # own libc/libc++.
        set(OMNIBOT_BOOST_INCLUDE_STAGE "${CMAKE_BINARY_DIR}/boost-include")
        file(MAKE_DIRECTORY "${OMNIBOT_BOOST_INCLUDE_STAGE}")
        file(CREATE_LINK "${OMNIBOT_BOOST_INCLUDEDIR}/boost"
             "${OMNIBOT_BOOST_INCLUDE_STAGE}/boost"
             RESULT OMNIBOT_BOOST_LINK_RESULT SYMBOLIC)
        if (NOT OMNIBOT_BOOST_LINK_RESULT EQUAL 0)
            # Filesystems without symlink support (e.g. Windows without developer
            # mode): fall back to copying the header tree into the build dir.
            file(COPY "${OMNIBOT_BOOST_INCLUDEDIR}/boost"
                 DESTINATION "${OMNIBOT_BOOST_INCLUDE_STAGE}")
        endif ()
        target_include_directories(omni-shared SYSTEM INTERFACE "${OMNIBOT_BOOST_INCLUDE_STAGE}")
    else ()
        find_package(Boost REQUIRED)
        target_link_libraries(omni-shared INTERFACE Boost::boost)
    endif ()
    target_compile_definitions(omni-shared INTERFACE OMNIBOT_USE_STD_FILESYSTEM)
    # Permit the older header-only Boost shipped with native Omni-bot setups
    # to use its pre-C++17 adapters with modern standard libraries.
    if(MSVC)
        target_compile_definitions(omni-shared INTERFACE $<$<COMPILE_LANGUAGE:CXX>:_HAS_AUTO_PTR_ETC=1>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(omni-shared INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR>
            $<$<COMPILE_LANGUAGE:CXX>:_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION>)
    endif()
    if (NOT MSVC)
        # C++17 removed the (long ignored) "register" storage class, which the
        # vendored gmscript sources still use - a hard error instead of the
        # warning it is in C++11.
        target_compile_options(omni-shared INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
    endif ()
else ()
    find_package(Boost COMPONENTS system filesystem regex date_time REQUIRED)
    target_link_libraries(omni-shared INTERFACE Boost::boost Boost::filesystem Boost::regex Boost::date_time)
endif ()

target_include_directories(omni-shared INTERFACE "${DEPENDENCIES_PATH}/WildMagic5/SDK/include")

if (NOT WIN32)
    add_compile_options(-Wall -Wno-deprecated -fno-strict-aliasing)
endif ()
add_compile_options("$<$<CONFIG:DEBUG>:-D_DEBUG>")

add_subdirectory(Common)

if (OMNIBOT_ET)
    add_subdirectory(ET)
    # Set ET as the startup project in Visual Studio
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT omnibot-et)
endif ()

if (OMNIBOT_RTCW)
    add_subdirectory(RTCW)
    if (NOT OMNIBOT_ET)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT omnibot-rtcw)
    endif ()
endif ()
