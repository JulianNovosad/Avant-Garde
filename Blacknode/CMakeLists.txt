cmake_minimum_required(VERSION 3.14)
project(BlacknodeNative LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Raylib (platform will be Android when building with ANDROID ABI)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(raylib)

# libjpeg-turbo for MJPEG decode
FetchContent_Declare(
  libjpeg_turbo
  GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(libjpeg_turbo)

# rapidjson (header-only)
FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(rapidjson)

file(GLOB_RECURSE BLACKNODE_SOURCES app/src/main/cpp/*.cpp app/src/main/cpp/*.c)
add_library(blacknode_native SHARED ${BLACKNODE_SOURCES})

target_include_directories(blacknode_native PRIVATE
  ${PROJECT_SOURCE_DIR}/app/src/main/cpp
  ${rapidjson_SOURCE_DIR}/include
  ${libjpeg_turbo_SOURCE_DIR}/
)

# Link against raylib and turbojpeg. On Android, raylib should build for target platform.
target_link_libraries(blacknode_native PUBLIC raylib turbojpeg log android)

set_target_properties(blacknode_native PROPERTIES OUTPUT_NAME "blacknode_native")

message(STATUS "Blacknode native library configured.")
