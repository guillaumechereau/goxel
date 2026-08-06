#!/usr/bin/env python3
"""
Convert Goxel TXT export to Minecraft .schematic file.
Uses the MinecraftComplete palette mapping for accurate block conversion.
"""

import json
import struct
import gzip
import os
import sys
from collections import defaultdict

def write_nbt_tag(tag_type, name, value, file):
    """Write an NBT tag to file."""
    # Tag type
    file.write(struct.pack('>B', tag_type))
    # Name length and name
    if name is not None:
        file.write(struct.pack('>H', len(name)))
        file.write(name.encode('utf-8'))
    
    # Value based on type
    if tag_type == 1:  # TAG_Byte
        file.write(struct.pack('>b', value))
    elif tag_type == 2:  # TAG_Short
        file.write(struct.pack('>h', value))
    elif tag_type == 3:  # TAG_Int
        file.write(struct.pack('>i', value))
    elif tag_type == 7:  # TAG_Byte_Array
        file.write(struct.pack('>i', len(value)))
        file.write(bytes(value))
    elif tag_type == 8:  # TAG_String
        value_bytes = value.encode('utf-8')
        file.write(struct.pack('>H', len(value_bytes)))
        file.write(value_bytes)
    elif tag_type == 10:  # TAG_Compound
        # Value should be a list of (tag_type, name, value) tuples
        for tag in value:
            write_nbt_tag(tag[0], tag[1], tag[2], file)
        # End tag
        file.write(struct.pack('>B', 0))

def load_block_mapping():
    """Load the RGB to block ID mapping."""
    mapping_path = os.path.expanduser("~/.config/goxel/palettes/minecraft_block_mapping.json")
    
    if not os.path.exists(mapping_path):
        print(f"Error: Block mapping not found at {mapping_path}")
        print("Please run create_minecraft_palette.py first to generate the mapping.")
        return None
    
    with open(mapping_path, 'r') as f:
        data = json.load(f)
    
    # Create RGB tuple to block data mapping
    rgb_to_block = {}
    for index, block_data in data['mapping'].items():
        rgb_tuple = tuple(block_data['rgb'])
        rgb_to_block[rgb_tuple] = block_data
    
    print(f"Loaded mapping for {len(rgb_to_block)} blocks")
    return rgb_to_block

def parse_block_id(block_id_str):
    """Parse block ID string like '1:3' into (id, metadata)."""
    if ':' in block_id_str:
        parts = block_id_str.split(':')
        return int(parts[0]), int(parts[1])
    else:
        return int(block_id_str), 0

def parse_txt_file(txt_path):
    """Parse Goxel TXT file and return list of (x, y, z, r, g, b) tuples."""
    voxels = []
    
    with open(txt_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            try:
                parts = line.split()
                if len(parts) != 4:
                    print(f"Warning: Invalid line {line_num}: {line}")
                    continue
                
                x = int(parts[0])
                y = int(parts[1]) 
                z = int(parts[2])
                
                # Parse RRGGBB color
                color_hex = parts[3]
                if len(color_hex) != 6:
                    print(f"Warning: Invalid color {line_num}: {color_hex}")
                    continue
                
                r = int(color_hex[0:2], 16)
                g = int(color_hex[2:4], 16)
                b = int(color_hex[4:6], 16)
                
                voxels.append((x, y, z, r, g, b))
                
            except ValueError as e:
                print(f"Warning: Could not parse line {line_num}: {line} - {e}")
                continue
    
    print(f"Parsed {len(voxels)} voxels from {txt_path}")
    return voxels

def convert_to_schematic(voxels, rgb_to_block, output_path, transform='none'):
    """Convert voxels to .schematic format.
    
    Args:
        transform: 'none', 'z_to_y', 'rotate_x90', 'rotate_x180', 'rotate_x270'
    """
    
    if not voxels:
        print("Error: No voxels to convert")
        return False
    
    # Apply coordinate transformation
    transformed_voxels = []
    for x, y, z, r, g, b in voxels:
        if transform == 'z_to_y':
            # Goxel Z-up to Minecraft Y-up: (x,y,z) -> (x,z,-y)
            new_x, new_y, new_z = x, z, -y
        elif transform == 'rotate_x90':
            # Rotate 90° around X-axis: (x,y,z) -> (x,-z,y)  
            new_x, new_y, new_z = x, -z, y
        elif transform == 'rotate_x180':
            # Rotate 180° around X-axis: (x,y,z) -> (x,-y,-z)
            new_x, new_y, new_z = x, -y, -z
        elif transform == 'rotate_x270':
            # Rotate 270° around X-axis: (x,y,z) -> (x,z,-y)
            new_x, new_y, new_z = x, z, -y
        else:  # 'none'
            new_x, new_y, new_z = x, y, z
        
        transformed_voxels.append((new_x, new_y, new_z, r, g, b))
    
    voxels = transformed_voxels
    print(f"Applied transformation: {transform}")
    
    # Find bounding box
    min_x = min(v[0] for v in voxels)
    max_x = max(v[0] for v in voxels)
    min_y = min(v[1] for v in voxels)
    max_y = max(v[1] for v in voxels)
    min_z = min(v[2] for v in voxels)
    max_z = max(v[2] for v in voxels)
    
    width = max_x - min_x + 1
    height = max_y - min_y + 1
    length = max_z - min_z + 1
    
    print(f"Schematic dimensions: {width}x{height}x{length}")
    print(f"Bounding box: X[{min_x}..{max_x}] Y[{min_y}..{max_y}] Z[{min_z}..{max_z}]")
    
    # Create block and data arrays (YZX ordering)
    total_blocks = width * height * length
    blocks = [0] * total_blocks  # Default to air (block ID 0)
    data = [0] * total_blocks    # Default metadata 0
    
    conversion_stats = defaultdict(int)
    unknown_colors = set()
    
    # Convert each voxel
    for x, y, z, r, g, b in voxels:
        rgb_tuple = (r, g, b)
        
        if rgb_tuple not in rgb_to_block:
            unknown_colors.add(rgb_tuple)
            continue
        
        block_info = rgb_to_block[rgb_tuple]
        block_id, metadata = parse_block_id(block_info['block_id'])
        
        # Convert to schematic coordinates (relative to min bounds)
        rel_x = x - min_x
        rel_y = y - min_y
        rel_z = z - min_z
        
        # Calculate YZX index
        index = rel_y * width * length + rel_z * width + rel_x
        
        blocks[index] = block_id
        data[index] = metadata
        
        conversion_stats[block_info['display_name']] += 1
    
    # Report conversion statistics
    if unknown_colors:
        print(f"Warning: {len(unknown_colors)} unknown colors found:")
        for rgb in list(unknown_colors)[:5]:  # Show first 5
            print(f"  RGB{rgb}")
        if len(unknown_colors) > 5:
            print(f"  ... and {len(unknown_colors) - 5} more")
    
    print(f"Block conversion summary:")
    for block_name, count in sorted(conversion_stats.items(), key=lambda x: x[1], reverse=True)[:10]:
        print(f"  {block_name}: {count} blocks")
    
    # Write .schematic file
    with gzip.open(output_path, 'wb') as f:
        # Root compound tag
        write_nbt_tag(10, "Schematic", [
            (2, "Width", width),
            (2, "Height", height),
            (2, "Length", length),
            (8, "Materials", "Alpha"),
            (7, "Blocks", blocks),
            (7, "Data", data),
        ], f)
    
    print(f"Successfully created {output_path}")
    return True

def main():
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print("Usage: python3 txt_to_schematic.py <input.txt> <output.schematic> [transform]")
        print("Transforms:")
        print("  none      - No transformation (default)")
        print("  z_to_y    - Convert Z-up (Goxel) to Y-up (Minecraft)")
        print("  rotate_x90  - Rotate 90° around X-axis")
        print("  rotate_x180 - Rotate 180° around X-axis") 
        print("  rotate_x270 - Rotate 270° around X-axis")
        print("Example: python3 txt_to_schematic.py my_build.txt my_build.schematic z_to_y")
        sys.exit(1)
    
    txt_path = sys.argv[1]
    output_path = sys.argv[2]
    
    if not os.path.exists(txt_path):
        print(f"Error: Input file {txt_path} not found")
        sys.exit(1)
    
    # Load block mapping
    rgb_to_block = load_block_mapping()
    if rgb_to_block is None:
        sys.exit(1)
    
    # Parse TXT file
    voxels = parse_txt_file(txt_path)
    if not voxels:
        print("Error: No valid voxels found in TXT file")
        sys.exit(1)
    
    # Get transform option
    transform = 'none'
    if len(sys.argv) > 3:
        transform = sys.argv[3]
    
    # Convert to schematic
    success = convert_to_schematic(voxels, rgb_to_block, output_path, transform)
    
    if success:
        print(f"\n✅ Conversion complete!")
        print(f"Import {output_path} into Minecraft using:")
        print(f"  • WorldEdit: //schem load {os.path.basename(output_path).replace('.schematic', '')}")
        print(f"  • MCEdit: File → Import → {output_path}")
        print(f"  • Schematica mod: Load schematic from file")
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()