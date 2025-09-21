#!/bin/bash

# Goxel macOS DMG Creation Script
# This script creates a DMG installer for Goxel.app

set -e

# Configuration
APP_NAME="Goxel"
VERSION="0.15.2"
DMG_NAME="Goxel-$VERSION"
BACKGROUND_COLOR="#2E3440"

# Paths
PROJECT_DIR="$(pwd)"
BUILD_DIR="$PROJECT_DIR/build"
APP_DIR="$BUILD_DIR/$APP_NAME.app"
DMG_DIR="$BUILD_DIR/dmg"
DMG_PATH="$BUILD_DIR/$DMG_NAME.dmg"

echo "Creating DMG installer for Goxel..."

# Check if .app exists
if [ ! -d "$APP_DIR" ]; then
    echo "Error: $APP_DIR not found. Please run ./create_app.sh first."
    exit 1
fi

# Clean previous DMG build
rm -rf "$DMG_DIR"
rm -f "$DMG_PATH"
mkdir -p "$DMG_DIR"

# Copy app to DMG directory
echo "Copying Goxel.app to DMG staging area..."
cp -R "$APP_DIR" "$DMG_DIR/"

# Create Applications symlink
echo "Creating Applications symlink..."
ln -sf /Applications "$DMG_DIR/Applications"

# Create a background image if we have ImageMagick
if command -v convert >/dev/null 2>&1; then
    echo "Creating background image..."
    # Create a simple background with the app name
    convert -size 600x400 xc:"$BACKGROUND_COLOR" \
        -font "Arial-Bold" -pointsize 48 -fill white \
        -gravity center -annotate +0-50 "$APP_NAME" \
        -font "Arial" -pointsize 16 -fill "#D8DEE9" \
        -gravity center -annotate +0+20 "Drag $APP_NAME to Applications to install" \
        "$DMG_DIR/.background.png"
    BACKGROUND_IMAGE=".background.png"
else
    echo "ImageMagick not found, skipping background image creation"
    BACKGROUND_IMAGE=""
fi

# Create DMG
echo "Creating DMG..."
hdiutil create -volname "$APP_NAME" -srcfolder "$DMG_DIR" -ov -format UDRW "$DMG_PATH.temp.dmg"

# Mount the DMG for customization
echo "Mounting DMG for customization..."
DEVICE=$(hdiutil attach -readwrite -noverify -noautoopen "$DMG_PATH.temp.dmg" | grep -E '^/dev/' | sed 1q | awk '{print $1}')
MOUNT_POINT="/Volumes/$APP_NAME"

# Wait for mount and verify it exists
sleep 3
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: DMG mount point $MOUNT_POINT not found"
    exit 1
fi

# Customize the DMG appearance using AppleScript
echo "Customizing DMG appearance..."
osascript << EOF
tell application "Finder"
    tell disk "$APP_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {100, 100, 700, 500}
        set theViewOptions to the icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 128
        set text size of theViewOptions to 16
        if "$BACKGROUND_IMAGE" is not "" then
            set background picture of theViewOptions to file "$BACKGROUND_IMAGE"
        end if
        set position of item "$APP_NAME.app" of container window to {150, 200}
        set position of item "Applications" of container window to {450, 200}
        update without registering applications
        delay 2
        close
    end tell
end tell
EOF

# Unmount the DMG
echo "Unmounting DMG..."
hdiutil detach "$DEVICE"

# Convert to final compressed format
echo "Converting to final DMG format..."
hdiutil convert "$DMG_PATH.temp.dmg" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH"

# Clean up
rm -f "$DMG_PATH.temp.dmg"
rm -rf "$DMG_DIR"

echo "DMG created successfully at: $DMG_PATH"
echo ""
echo "File size: $(du -h "$DMG_PATH" | cut -f1)"
echo ""
echo "The DMG installer is ready for distribution!" 