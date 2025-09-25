#!/usr/bin/env python3

"""
Create a simple Windows ICO file from a PNG icon.
This creates a basic ICO that embeds a single PNG image.
"""

import os
import struct

def create_simple_ico():
    """Create a simple ICO file from the 32x32 PNG icon."""
    png_path = "data/icons/icon32.png"
    ico_path = "goxel.ico"

    if not os.path.exists(png_path):
        print(f"Error: {png_path} not found!")
        return False

    try:
        # Read the PNG data
        with open(png_path, 'rb') as f:
            png_data = f.read()

        # ICO file format:
        # ICONDIR header (6 bytes)
        # ICONDIRENTRY entries (16 bytes each)
        # Image data

        # ICONDIR header
        ico_header = struct.pack('<HHH',
            0,      # Reserved (must be 0)
            1,      # Image type (1 = ICO)
            1       # Number of images
        )

        # ICONDIRENTRY (for the PNG)
        ico_entry = struct.pack('<BBBBHHLL',
            32,                     # Width (32 pixels)
            32,                     # Height (32 pixels)
            0,                      # Color palette count (0 = no palette)
            0,                      # Reserved (must be 0)
            1,                      # Color planes (should be 1)
            32,                     # Bits per pixel
            len(png_data),          # Size of image data
            22                      # Offset to image data (6 + 16 = 22)
        )

        # Write ICO file
        with open(ico_path, 'wb') as f:
            f.write(ico_header)
            f.write(ico_entry)
            f.write(png_data)

        print(f"Successfully created {ico_path} from {png_path}")
        return True

    except Exception as e:
        print(f"Error creating ICO file: {e}")
        return False

if __name__ == "__main__":
    success = create_simple_ico()
    exit(0 if success else 1)