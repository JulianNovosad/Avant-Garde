# AGENTS.md

This file provides guidance to Qoder (qoder.com) when working with code in this repository.

## Project Overview

This is an Android application called "Wired Video Viewer" that receives H.264 video streams over a wired network connection and decodes them using Android's MediaCodec for hardware-accelerated playback. The app also sends device orientation data to the video source via UDP/ZeroMQ.

## Code Architecture

### High-Level Structure
1. **Kotlin Layer** (`app/src/main/java/com/wiredvideoviewer/MainActivity.kt`):
   - Main UI with SurfaceView for video rendering
   - Sensor handling for device orientation (rotation vector sensor)
   - ZeroMQ networking for sending orientation data via TCP
   - Device discovery on local network using network interface enumeration
   - JNI bindings to native layer

2. **Native C++ Layer** (`app/src/main/cpp/native-lib.cpp`):
   - VideoDecoder: MediaCodec-based H.264 decoder
   - H264NalParser: Parser for extracting NAL units from RTP stream
   - RtpReceiver: UDP receiver for RTP video streams
   - JNI functions for Kotlin integration

3. **Build System**:
   - Gradle build system with CMake for native code
   - Supports armeabi-v7a, arm64-v8a, x86, and x86_64 architectures
   - NDK version 23.1.7779620

## Common Development Commands

### Building the Project
```bash
# Standard Gradle build
./gradlew build

# Build debug APK
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease
```

### Running Tests
```bash
# Run unit tests
./gradlew test

# Run instrumented tests
./gradlew connectedAndroidTest
```

### Cleaning the Project
```bash
# Clean build artifacts
./gradlew clean
```

### Installing and Running on Device
```bash
# Install debug APK
./gradlew installDebug

# Run the app
adb shell am start -n com.wiredvideoviewer/.MainActivity
```

## Key Implementation Details

### Video Processing Pipeline
1. RTP packets received via UDP on port 1001
2. H264NalParser extracts complete NAL units from RTP packet stream
3. VideoDecoder feeds NAL units to Android MediaCodec
4. Decoded frames rendered to SurfaceView

### Orientation Data Flow
1. Rotation vector sensor data captured
2. Converted to yaw/pitch/roll angles with adjustments for landscape orientation
3. Sent via ZeroMQ/TCP to ports 2001/2002/2003 respectively

### Device Discovery
1. Enumerates network interfaces to find wired connections
2. Scans subnet for reachable devices
3. Populates device selection spinner in UI

### Error Handling & Recovery
- Automatic decoder re-initialization
- Network reconnection logic with timeouts
- Proper resource cleanup on errors
- Comprehensive logging for debugging

## Important Constants
- Video Stream Port: 1001
- Orientation Data Ports: Yaw(2001), Pitch(2002), Roll(2003)
- Supported Architectures: armeabi-v7a, arm64-v8a, x86, x86_64
- NDK Version: 23.1.7779620
- Compile SDK Version: 30
- Min SDK Version: 21