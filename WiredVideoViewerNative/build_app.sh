#!/bin/bash

# Set paths
PROJECT_DIR="/home/pc/Avant-Garde/WiredVideoViewerNative"
BUILD_DIR="$PROJECT_DIR/app/build"
SRC_DIR="$PROJECT_DIR/app/src/main/java"
RES_DIR="$PROJECT_DIR/app/src/main/res"
MANIFEST_FILE="$PROJECT_DIR/app/src/main/AndroidManifest.xml"
OUTPUT_DIR="$BUILD_DIR/outputs/apk/debug"
CLASSES_DIR="$BUILD_DIR/intermediates/classes/debug"
DEX_DIR="$BUILD_DIR/intermediates/dex/debug"
SDK_DIR="/home/pc/android-sdk"
BUILD_TOOLS_DIR="$SDK_DIR/build-tools/36.0.0"
PLATFORM_DIR="$SDK_DIR/platforms/android-36"

# Create directories
mkdir -p "$CLASSES_DIR" "$DEX_DIR" "$OUTPUT_DIR"

# Compile Java/Kotlin sources
echo "Compiling sources..."
# Note: This is a simplified example. In reality, you would need to compile all sources and dependencies.

# Convert classes to dex format using d8
echo "Converting to dex format..."
"$BUILD_TOOLS_DIR/d8" --output "$DEX_DIR" "$CLASSES_DIR"/*.class

# Package resources
echo "Packaging resources..."
"$BUILD_TOOLS_DIR/aapt" package -f -m \
  -J "$SRC_DIR" \
  -M "$MANIFEST_FILE" \
  -S "$RES_DIR" \
  -I "$PLATFORM_DIR/android.jar" \
  -F "$OUTPUT_DIR/resources.apk"

# Create unsigned APK
echo "Creating unsigned APK..."
cd "$OUTPUT_DIR"
mkdir -p temp_apk
cd temp_apk
unzip ../resources.apk
mkdir -p classes.dex
cp "$DEX_DIR/classes.dex" classes.dex/
zip -r ../unsigned-app-debug.apk .

# Clean up
cd ..
rm -rf temp_apk resources.apk

echo "Build completed. APK located at: $OUTPUT_DIR/unsigned-app-debug.apk"