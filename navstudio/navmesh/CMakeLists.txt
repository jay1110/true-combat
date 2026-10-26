cmake_minimum_required(VERSION 3.20)
project(tce_navmesh_preview LANGUAGES CXX)
include(FetchContent)
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
# Pinned upstream v1.6.0. Only Recast's CPU geometry builder is linked.
FetchContent_Declare(recast
  GIT_REPOSITORY https://github.com/recastnavigation/recastnavigation.git
  GIT_TAG b4554541b658630816dba41466eb1cefb624519e)
FetchContent_MakeAvailable(recast)
add_executable(tce_navmesh_preview preview.cpp)
target_compile_features(tce_navmesh_preview PRIVATE cxx_std_17)
target_link_libraries(tce_navmesh_preview PRIVATE Recast)
