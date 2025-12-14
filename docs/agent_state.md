# Agent State

## Current App Structure
- **Root:** `Avant-Garde/WiredVideoViewer/` (React Native project)
- **Android:** `Avant-Garde/WiredVideoViewer/android/`
  - `app/build.gradle`: Android application build configuration. Uses Kotlin, links React Native, and crucially, uses `externalNativeBuild` with CMake. `ndkVersion` and `kotlinVersion` defined at project level. `compileSdk 33`, `minSdkVersion 21`, `targetSdkVersion 33`.
  - `app/src/main/AndroidManifest.xml`: Defines app components, `MainActivity` as launcher, and `INTERNET` permission. `usesCleartextTraffic` is set to `true`.
  - `app/src/main/java/`: Java/Kotlin source code.
    - `com/wiredvideoviewer/VideoEngineModule.kt`: React Native native module, exposing JNI methods for `initDecoder`, `decodeNalUnit`, `releaseDecoder`, `startNetworkClient`, and `stopNetworkClient`.
    - `com/wiredvideoviewer/VideoEnginePackage.kt`: React Native package, which likely registers `VideoEngineModule`.
  - `app/src/main/cpp/`: C++ native source code.
    - `CMakeLists.txt`: Defines how the C++ code is built. Creates `wiredvideoviewer-native` library from `native-lib.cpp`, linking `log`, `android`, and `mediandk`.
    - `native-lib.cpp`: Implements `VideoDecoder` class for MediaCodec H.264 decoding and `NetworkClient` class for TCP video reception. Exposes JNI methods for `initDecoder`, `decodeNalUnit`, `releaseDecoder`, `startNetworkClient`, and `stopNetworkClient`.

## Native/C++ Integration State
- The project uses CMake for building native C++ code (`wiredvideoviewer-native`).
- `native-lib.cpp` directly implements `VideoDecoder` using NDK MediaCodec for H.264, and `NetworkClient` for TCP socket communication and streaming.
- `mediandk` is correctly linked in `CMakeLists.txt`.
- JNI functions for network client (`startNetworkClient`, `stopNetworkClient`) are now exposed to React Native via `VideoEngineModule.kt`.

## Removed or Refactored Files
- None in this session so far.

## Build Status and Errors
- Android build (`./gradlew assembleDebug`) succeeded. There were deprecation warnings about `ndk.dir` but they did not prevent a successful build.

## Wired Network Streaming Readiness
- `INTERNET` permission is present.
- `NetworkClient` is implemented in C++ to receive TCP streams.
- The `NetworkClient` directly feeds data to the `VideoDecoder`.
- React Native bridge methods for `startVideoStream` and `stopVideoStream` (which call native `startNetworkClient` and `stopNetworkClient` respectively) are now available.

## MediaCodec Integration Status
- NDK MediaCodec is used in `native-lib.cpp` via `VideoDecoder` class.
- H.264 MIME type (`video/avc`) is specified.
- `initDecoder`, `decodeNalUnit`, `releaseDecoder` JNI methods are exposed to React Native.

## Identified Discrepancies/Bugs
- None currently, the previous discrepancy has been addressed.

## Next Steps:
- The next step would typically involve integrating these new React Native methods into the JavaScript/TypeScript frontend to control the native video streaming. This would involve modifying `App.tsx` or related React Native components to call `VideoEngineModule.startVideoStream()` and `VideoEngineModule.stopVideoStream()`, likely passing an IP address and port.
- Also, further verification of the H.264 NAL unit parsing and framing mechanism in `NetworkClient` would be beneficial for robustness, as noted in the C++ code.