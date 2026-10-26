# Configure on a macOS host with an Xcode/CommandLineTools SDK.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Universal mod slices")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS host")
