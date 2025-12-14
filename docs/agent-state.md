# Agent State

## Current Status
- User confirmed `/home/pc/Avant-Garde/` is the correct directory.
- Previous project was wiped/unusable.
- Task is to "start over" and build a new Android app for a "wired network video viewer" with Kotlin UI and C++ native engine via JNI.
- Discovered `npm` is installed, `yarn` is not.
- User has chosen Option 1: Create a React Native project in a sub-directory named `WiredVideoViewer`.
- React Native project `WiredVideoViewer` successfully created.
- `npm install` successfully executed within `WiredVideoViewer` directory.
- Attempted `npx react-native run-android`, which failed with "SDK location not found" error.
- User connected an ADB device.
- Found Android SDK at `/home/pc/android-sdk/` and created `WiredVideoViewer/android/local.properties` with `sdk.dir=/home/pc/android-sdk/`.
- Re-attempted `npx react-native run-android`, which failed with "[CXX1101] NDK...did not have a source.properties file".
- Successfully installed NDK version `25.2.9519653`.
- Re-attempted `npx react-native run-android`, which failed *again* with C++ compilation errors related to C++20 features (e.g., `std::regular`, `std::floating_point`, `std::identity`, `std::bit_cast`).
- Identified and modified `ndkVersion` in `WiredVideoViewer/android/build.gradle` from `"27.1.12297006"` to `"25.2.9519653"`.
- Read `WiredVideoViewer/android/app/build.gradle` and confirmed `ndkVersion` and `minSdkVersion` inheritance, but no explicit C++ standard library configuration found there.
- Read `WiredVideoViewer/android/build.gradle` and confirmed `ndkVersion` is correct, but still no `externalNativeBuild` or explicit `cppFlags`.
- Modified `WiredVideoViewer/android/app/build.gradle` to explicitly configure C++ flags and standard library (`-std=c++17`, `libc++_static`) within an `externalNativeBuild` block, which led to a new error: `"Could not find method cppFlags() for arguments ... on object of type com.android.build.gradle.internal.dsl.CmakeOptions$AgpDecorated."`
- Corrected the `externalNativeBuild` block's `cmake` configuration in `WiredVideoViewer/android/app/build.gradle` by passing all C++ flags through `arguments` to CMake.
- New error: `Could not find method arguments() for arguments [...] on object of type com.android.build.gradle.internal.dsl.CmakeOptions$AgpDecorated.` and `ANDROID_NDK=null`, `CMAKE_TOOLCHAIN_FILE=null/build/cmake/android.toolchain.cmake`. This indicated `android.ndkPath` was resolving to `null`.
- Verified NDK installation and explicitly set `ndk.dir=/home/pc/android-sdk/ndk/25.2.9519653` in `WiredVideoViewer/android/local.properties`.
- `gradlew clean` also failed with the same error, indicating the issue is in `app/build.gradle` parsing.
- Identified that `classpath("com.android.tools.build:gradle")` in `WiredVideoViewer/android/build.gradle` did not specify a version.
- Explicitly set AGP version to `8.3.0` in `WiredVideoViewer/android/build.gradle`.
- **Re-attempted build, still failing with `ANDROID_NDK=null` and `Could not find method arguments()` error.**

## Next Steps
- **Remove the `externalNativeBuild` and `packagingOptions` blocks from `WiredVideoViewer/android/app/build.gradle`.**
- **Add `ndkVersion "25.2.9519653"` directly to the `android` block in `WiredVideoViewer/android/app/build.gradle`.**
- Perform an initial build verification of the Android project (retry).
- Configure Kotlin and C++ (JNI) for the chosen project type.
