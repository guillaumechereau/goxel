#!/usr/bin/python

# Create the icon atlas image from all the icons svg files

import PIL.Image
from shutil import copyfile
import os
import subprocess
import re

SRC = [
    'tool_brush.svg',
    'tool_shape.svg',
    'tool_laser.svg',
    'tool_plane.svg',
    'tool_move.svg',
    'tool_pick_color.svg',
    'tool_selection.svg',
    'tool_procedural.svg',
    None,

    'mode_add.svg',
    'mode_sub.svg',
    'mode_paint.svg',
    'tool_extrude.svg',
    None,

    'shape_sphere.svg',
    'shape_cube.svg',
    'shape_cylinder.svg',
    None,

    'add.svg',
    'remove.svg',
    'arrow_downward.svg',
    'arrow_upward.svg',
    'visibility.svg',
    'visibility_off.svg',
    'edit.svg',
    'link.svg',
    None,
]

ret_img = PIL.Image.new('L', (256, 256))

x = 0
y = 0
for src in SRC:
    if src is None:
        y = y + 1
        x = 0
        continue
    path = 'svg/{}'.format(src)
    subprocess.check_output([
        'inkscape', path, '--export-area-page',
        '--export-width=24', '--export-height=24',
        '--export-png=/tmp/symbols.png'])
    img = PIL.Image.open('/tmp/symbols.png')
    img = img.split()[3]
    ret_img.paste(img, (32 * x + 4, 32 * y + 4))
    x = x + 1

white_img = PIL.Image.new('L', (256, 256), "white")
ret_img = PIL.Image.merge('LA', (white_img, ret_img))
ret_img.save('data/icons.png')

# Also create the application icons (in data/icons)
if not os.path.exists('data/icons'): os.makedirs('data/icons')
base = PIL.Image.open('icon.png').convert('RGBA')

for size in [16, 32, 48]:
    img = base.resize((size, size), PIL.Image.BILINEAR)
    img.save('data/icons/icon%d.png' % size)
