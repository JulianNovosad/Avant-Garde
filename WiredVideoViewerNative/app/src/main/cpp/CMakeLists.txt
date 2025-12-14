# For more information about using CMake with Android Studio, read the
# documentation: https://developer.android.com/studio/projects/add-native-code#cmake

cmake_minimum_required(VERSION 3.18.1)
project(WiredVideoViewerNative)

# Declares and configures the native shared library.
# wiredvideoviewer-native is the name of the module that will be used for System.loadLibrary()
add_library( # Sets the name of the library.
             wiredvideoviewer-native

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             native-lib.cpp )

# Searches for a JNI library and specifies the path to the located library.
find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies targets for debugging.
target_link_libraries( # Specifies the target library.
                       wiredvideoviewer-native

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                       android
                       mediandk
                       )
