# Building Goxel for macOS

This document describes how to build Goxel as a macOS .app bundle and create a DMG installer.

## Prerequisites

Before building, ensure you have the following installed:

1. **Xcode Command Line Tools**:
   ```bash
   xcode-select --install
   ```

2. **SCons** (Python build system):
   ```bash
   pip install scons
   ```

3. **GLFW** (OpenGL framework):
   ```bash
   brew install glfw
   ```

4. **pkg-config**:
   ```bash
   brew install pkg-config
   ```

5. **ImageMagick** (optional, for DMG background image):
   ```bash
   brew install imagemagick
   ```

## Building the .app Bundle

### Method 1: Using the Automated Script (Recommended)

1. Run the app creation script:
   ```bash
   ./create_app.sh
   ```

This will:
- Build Goxel using SCons in release mode
- Create a proper macOS .app bundle structure
- Convert PNG icons to an .icns file
- Copy all necessary resources and data files
- Create a proper Info.plist with file associations
- Set correct permissions

The resulting app will be located at `build/Goxel.app`.

### Method 2: Manual Build

1. Build Goxel:
   ```bash
   scons -j $(sysctl -n hw.logicalcpu) mode=release
   ```

2. Create the app bundle manually following the structure in `create_app.sh`.

## Creating a DMG Installer

After creating the .app bundle, you can create a DMG installer:

```bash
./create_dmg.sh
```

This will:
- Create a DMG staging area
- Add an Applications folder symlink
- Create a background image (if ImageMagick is installed)
- Customize the DMG appearance with proper icon layout
- Create a compressed DMG file

The resulting DMG will be located at `build/Goxel-[VERSION].dmg`.

## Installing the App

### From .app Bundle
```bash
cp -R build/Goxel.app /Applications/
```

### From DMG
1. Open the DMG file
2. Drag Goxel.app to the Applications folder

## File Associations

The app bundle includes support for the following file types:
- `.gox` - Goxel native format (primary)
- `.vox` - MagicaVoxel format
- `.qb` - Qubicle format
- `.ply` - PLY format

## Troubleshooting

### Build Issues

1. **GLFW not found**: Make sure GLFW is installed via Homebrew
2. **pkg-config not found**: Install pkg-config via Homebrew
3. **SCons not found**: Install SCons via pip

### Runtime Issues

1. **App won't launch**: Check that the executable has proper permissions
2. **Missing resources**: Ensure all data files are copied to the Resources folder
3. **Gatekeeper issues**: You may need to right-click the app and select "Open" the first time

### Code Signing (Optional)

For distribution, you may want to code sign the app:

```bash
codesign --deep --force --verify --verbose --sign "Developer ID Application: Your Name" build/Goxel.app
```

## Directory Structure

The created .app bundle follows this structure:
```
Goxel.app/
├── Contents/
│   ├── Info.plist          # App metadata and file associations
│   ├── PkgInfo             # Package type identifier
│   ├── MacOS/
│   │   └── Goxel           # Main executable
│   └── Resources/
│       ├── Goxel.icns      # App icon
│       └── data/           # Goxel data files
│           ├── fonts/
│           ├── icons/
│           ├── palettes/
│           ├── shaders/
│           └── ...
```

## Notes

- The build scripts are configured for Goxel version 0.15.2
- The minimum macOS version is set to 10.14 (Mojave)
- The app includes proper Retina display support
- All necessary frameworks and libraries are linked during the build process 