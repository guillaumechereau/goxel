#!/bin/bash

# Goxel macOS .app Bundle Creation Script
# This script builds Goxel and creates a proper macOS .app bundle

set -e

# Configuration
APP_NAME="Goxel"
BUNDLE_ID="io.github.guillaumechereau.Goxel"
VERSION="0.15.2"
BUILD_NUMBER="10"
DEVELOPER_NAME="Guillaume Chereau"
COPYRIGHT="Copyright © 2024 Guillaume Chereau. All rights reserved."

# Paths
PROJECT_DIR="$(pwd)"
BUILD_DIR="$PROJECT_DIR/build"
APP_DIR="$BUILD_DIR/$APP_NAME.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"

echo "Creating Goxel.app bundle..."

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Build Goxel using SCons
echo "Building Goxel..."
scons -j "$(sysctl -n hw.logicalcpu)" mode=release

# Create app bundle structure
echo "Creating app bundle structure..."
mkdir -p "$MACOS_DIR"
mkdir -p "$RESOURCES_DIR"

# Copy executable
echo "Copying executable..."
cp goxel "$MACOS_DIR/Goxel"

# Copy data files
echo "Copying data files..."
cp -R data "$RESOURCES_DIR/"

# Copy icon if available
if [ -f "data/icons/icon128.png" ]; then
    echo "Converting icon..."
    # Create iconset directory
    ICONSET_DIR="$BUILD_DIR/Goxel.iconset"
    mkdir -p "$ICONSET_DIR"
    
    # Copy icons to iconset with proper naming
    cp data/icons/icon16.png "$ICONSET_DIR/icon_16x16.png"
    cp data/icons/icon32.png "$ICONSET_DIR/icon_16x16@2x.png"
    cp data/icons/icon32.png "$ICONSET_DIR/icon_32x32.png"
    cp data/icons/icon64.png "$ICONSET_DIR/icon_32x32@2x.png"
    cp data/icons/icon128.png "$ICONSET_DIR/icon_128x128.png"
    cp data/icons/icon256.png "$ICONSET_DIR/icon_128x128@2x.png"
    cp data/icons/icon256.png "$ICONSET_DIR/icon_256x256.png"
    
    # Create icns file
    iconutil -c icns "$ICONSET_DIR" -o "$RESOURCES_DIR/Goxel.icns"
    
    # Clean up
    rm -rf "$ICONSET_DIR"
fi

# Create Info.plist
echo "Creating Info.plist..."
cat > "$CONTENTS_DIR/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>Goxel</string>
    <key>CFBundleIconFile</key>
    <string>Goxel.icns</string>
    <key>CFBundleIdentifier</key>
    <string>$BUNDLE_ID</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>$BUILD_NUMBER</string>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.productivity</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.14</string>
    <key>NSHumanReadableCopyright</key>
    <string>$COPYRIGHT</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>gox</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>Goxel Document</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>LSHandlerRank</key>
            <string>Owner</string>
        </dict>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>vox</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>MagicaVoxel Document</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>LSHandlerRank</key>
            <string>Alternate</string>
        </dict>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>qb</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>Qubicle Document</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>LSHandlerRank</key>
            <string>Alternate</string>
        </dict>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>ply</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>PLY Document</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>LSHandlerRank</key>
            <string>Alternate</string>
        </dict>
    </array>
</dict>
</plist>
EOF

# Create PkgInfo file
echo "APPL????" > "$CONTENTS_DIR/PkgInfo"

# Make executable
chmod +x "$MACOS_DIR/Goxel"

# Fix executable permissions and dependencies
echo "Fixing permissions..."
find "$APP_DIR" -type f -name "*.dylib" -exec chmod 755 {} \;
find "$APP_DIR" -type f -name "*.so" -exec chmod 755 {} \;

echo "Goxel.app created successfully at: $APP_DIR"
echo ""
echo "To install the app, run:"
echo "  cp -R \"$APP_DIR\" /Applications/"
echo ""
echo "To create a DMG, run:"
echo "  hdiutil create -volname \"Goxel\" -srcfolder \"$APP_DIR\" -ov -format UDZO \"$BUILD_DIR/Goxel-$VERSION.dmg\""
echo ""
echo "The app bundle is ready to use!" 