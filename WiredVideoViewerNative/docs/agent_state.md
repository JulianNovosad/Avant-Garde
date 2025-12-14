# Wired Video Viewer - Agent State Documentation

## Current App Structure

### Kotlin Layer
- **MainActivity.kt**: Main entry point with SurfaceView for video rendering
- UI components for IP address/port input and stream control buttons
- JNI bindings to native layer

### Native C++ Layer
- **VideoDecoder**: MediaCodec-based H.264 decoder
- **NetworkClient**: TCP client for receiving video streams
- **H264NalParser**: Parser for extracting NAL units from stream
- JNI functions for Kotlin integration

## Improvements Made

### 1. H.264 NAL Unit Parsing
- Implemented proper start code detection (0x000001 and 0x00000001)
- Added buffer management for incomplete NAL units across network packets
- Enhanced extraction logic to handle stream fragmentation

### 2. Dynamic Video Format Detection
- Added automatic format detection from MediaCodec output
- Implemented width/height extraction from decoded stream
- Added surface geometry adjustment based on detected format

### 3. Network Client Enhancements
- Added reconnection logic with exponential backoff
- Implemented socket timeout handling
- Added connection health monitoring
- Improved error handling for various network conditions

### 4. Timestamp Management
- Implemented proper presentation timestamp calculation
- Added support for MediaCodec's native timestamp handling
- Enhanced frame synchronization

### 5. Error Handling & Recovery
- Added comprehensive error checking for MediaCodec operations
- Implemented graceful degradation and recovery mechanisms
- Added proper resource cleanup

### 6. Memory Management
- Added proper cleanup methods for all components
- Implemented reset functionality for decoder re-initialization
- Enhanced memory handling in NAL parser

## Native/C++ Integration State
- JNI bridge fully functional
- MediaCodec integration working correctly
- Network streaming with proper H.264 handling

## Removed/Refactored Files
- No files removed in this iteration
- Significant refactoring of native-lib.cpp for improved functionality

## Build Status
- Using CMake with proper linking to mediandk, android, and log libraries
- Compatible with armeabi-v7a, arm64-v8a, x86, and x86_64 architectures

## Wired Network Streaming Readiness
- ✅ TCP client implementation
- ✅ H.264 stream parsing
- ✅ Automatic format detection
- ✅ Reconnection handling
- ✅ Error recovery

## MediaCodec Integration Status
- ✅ Hardware-accelerated H.264 decoding
- ✅ SurfaceView rendering
- ✅ Dynamic format handling
- ✅ Proper timestamp management