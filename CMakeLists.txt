cmake_minimum_required(VERSION 3.18)
project(blacknode_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use FetchContent for dependencies
include(FetchContent)

# Raylib (master)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG master
)
FetchContent_MakeAvailable(raylib)

# libjpeg-turbo (only for Android builds - desktop uses placeholder)
if(ANDROID)
  FetchContent_Declare(
    libjpeg_turbo
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG main
  )
  FetchContent_MakeAvailable(libjpeg_turbo)
endif()

# rapidjson
FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG master
)
FetchContent_MakeAvailable(rapidjson)

# glm
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

if(NOT ANDROID)
  message(STATUS "Configuring for desktop build (libjpeg-turbo will be skipped).")
endif()

if(ANDROID)
  add_library(blacknode SHARED
    src/PacketDefs.h
    src/Network/UdpReceiver.cpp
    src/Video/Decoder.cpp
    src/Render/Scene.cpp
    src/main.cpp
  )

  target_include_directories(blacknode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${rapidjson_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${libjpeg_turbo_SOURCE_DIR}/simd
  )

  target_link_libraries(blacknode PRIVATE
    raylib
    jpegturbo-static
    android
    log
    GLESv3
  )

  set_target_properties(blacknode PROPERTIES
    OUTPUT_NAME "blacknode"
  )

  install(TARGETS blacknode DESTINATION lib)
endif()
cmake_minimum_required(VERSION 3.18)
project(Blacknode1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT ANDROID)
  message(STATUS "Configuring for desktop/debug mode")
endif()

# Compiler flags: LTO, NEON when targeting arm
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB_RECURSE ALL_SOURCES
  ${CORE_DIR}/core/*.cpp ${CORE_DIR}/core/*.hpp ${CORE_DIR}/core/*.h
  ${CORE_DIR}/net/*.cpp ${CORE_DIR}/net/*.h
  ${CORE_DIR}/ui/*.cpp ${CORE_DIR}/ui/*.h
  ${CORE_DIR}/render/*.cpp ${CORE_DIR}/render/*.h
  ${CORE_DIR}/*.cpp ${CORE_DIR}/*.h
)

# Exclude optional Raylib renderer when building the desktop GL harness
list(REMOVE_ITEM ALL_SOURCES ${CORE_DIR}/Renderer.cpp ${CORE_DIR}/Renderer.h)
list(REMOVE_ITEM ALL_SOURCES ${CORE_DIR}/VideoDec.cpp ${CORE_DIR}/VideoDec.h)
# Exclude Android-only JNI sources from desktop build
file(GLOB ANDROID_SOURCES ${CORE_DIR}/android/*.cpp ${CORE_DIR}/android/*.h)
list(REMOVE_ITEM ALL_SOURCES ${ANDROID_SOURCES})
# Exclude application entry points from the library; they are built into the
# desktop test executable `blacknode_app` instead of the shared library.
list(REMOVE_ITEM ALL_SOURCES ${CORE_DIR}/main.cpp ${CORE_DIR}/main_app.cpp)

add_library(blacknode SHARED ${ALL_SOURCES})

target_include_directories(blacknode PRIVATE
  ${CORE_DIR}
  ${CORE_DIR}/core
  ${CORE_DIR}/net
  ${CORE_DIR}/ui
  ${CORE_DIR}/render
)

# RapidJSON (fetch for parsing TCP JSON)
include(FetchContent)
FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(rapidjson)
target_include_directories(blacknode PRIVATE ${rapidjson_SOURCE_DIR}/include)

# GLM for math
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        0.9.9.8
)
FetchContent_MakeAvailable(glm)
include_directories(${glm_SOURCE_DIR})
target_include_directories(blacknode PRIVATE ${glm_SOURCE_DIR})

# GLFW + glad for desktop GL context (only if not already provided by raylib)
if(NOT TARGET glfw)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
  )
  FetchContent_MakeAvailable(glfw)
endif()

if(NOT TARGET glad)
  FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
  )
  FetchContent_MakeAvailable(glad)
  ## Use the `glad` target provided by the glad FetchContent project.
  ## The glad project generates its sources at configure time; link against the
  ## `glad` target instead of referencing generated files directly.
  target_link_libraries(blacknode PRIVATE glad)
endif()


if(ANDROID)
  # Android NDK specific flags
  set_target_properties(blacknode PROPERTIES
    ANDROID_ARCH "arm64-v8a"
  )
  target_compile_options(blacknode PRIVATE -fno-exceptions -fomit-frame-pointer -funwind-tables)
  target_compile_options(blacknode PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(blacknode PRIVATE -Wl,--gc-sections)
  # enable NEON and LTO
  target_compile_options(blacknode PRIVATE -mfpu=neon)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(blacknode PRIVATE -O3 -flto)
    target_link_options(blacknode PRIVATE -flto)
  endif()

  find_library(log-lib log)
  find_library(android-lib android)
  target_link_libraries(blacknode PRIVATE ${log-lib} ${android-lib} GLESv3 EGL jnigraphics)
else()
  find_package(OpenGL REQUIRED)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(blacknode PRIVATE OpenGL::GL dl pthread OpenSSL::SSL OpenSSL::Crypto raylib)
  # Desktop executable for testing (only when not building for Android)
  add_executable(blacknode_app ${CORE_DIR}/main_app.cpp)
  target_include_directories(blacknode_app PRIVATE ${CORE_DIR})
  # rapidjson is used by the desktop harness as well
  target_include_directories(blacknode_app PRIVATE ${rapidjson_SOURCE_DIR}/include)
  # Ensure generated glad headers are available for the executable target
  target_include_directories(blacknode_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/_deps/glad-build/include ${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw-src/include)
  target_link_libraries(blacknode_app PRIVATE blacknode glfw glad OpenGL::GL dl pthread OpenSSL::SSL OpenSSL::Crypto)
endif()

set_target_properties(blacknode PROPERTIES OUTPUT_NAME "blacknode")

install(TARGETS blacknode DESTINATION lib)
