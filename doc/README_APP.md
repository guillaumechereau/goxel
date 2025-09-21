# Goxel macOS App Creation

This directory contains scripts to create a macOS .app bundle and DMG installer for Goxel.

## Quick Start

1. **Create the .app bundle:**
   ```bash
   ./create_app.sh
   ```

2. **Create a DMG installer:**
   ```bash
   ./create_dmg.sh
   ```

## What's Created

After running the scripts, you'll have:

- `build/Goxel.app` - A complete macOS application bundle
- `build/Goxel-0.15.2.dmg` - A distributable DMG installer

## Installation

### Install from .app bundle:
```bash
cp -R build/Goxel.app /Applications/
```

### Install from DMG:
Double-click the DMG file and drag Goxel to Applications.

## Features

- ✅ Proper .app bundle structure
- ✅ Custom icon (converted from PNG to .icns)
- ✅ File associations (.gox, .vox, .qb, .ply)
- ✅ Retina display support
- ✅ All necessary resources included
- ✅ DMG installer with Applications symlink
- ✅ Compressed DMG for distribution

## File Associations

The app supports opening these file types:
- `.gox` - Goxel native format
- `.vox` - MagicaVoxel format  
- `.qb` - Qubicle format
- `.ply` - PLY format

## Technical Details

- **Bundle ID:** `io.github.guillaumechereau.Goxel`
- **Version:** 0.15.2
- **Minimum macOS:** 10.14 (Mojave)
- **Architecture:** Universal (depends on build system)

For detailed build instructions and troubleshooting, see `BUILD_MACOS.md`. 