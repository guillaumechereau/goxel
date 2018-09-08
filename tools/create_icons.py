#!/usr/bin/python

# Goxel 3D voxels editor
#
# copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
#
# Goxel is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# goxel.  If not, see <http://www.gnu.org/licenses/>.

# ************************************************************************
# Create the icon atlas image from all the icons svg files

import itertools
import PIL.Image
from shutil import copyfile
import os
import subprocess
import re

# Use inkscape to convert the svg into one big png file.
subprocess.check_output([
    'inkscape', './svg/icons.svg', '--export-area-page',
    '--export-dpi=192', '--export-png=/tmp/icons.png'])

src_img = PIL.Image.open('/tmp/icons.png')
ret_img = PIL.Image.new('RGBA', (512, 512))

for x, y in itertools.product(range(8), range(8)):
    img = src_img.crop((x * 46 + 2, y * 46 + 2, x * 46 + 46, y * 46 + 46))
    # Make the image white in the special range.
    if y >= 2 and y < 5:
        tmp = PIL.Image.new('RGBA', (44, 44), (255, 255, 255, 255))
        tmp.putalpha(img.split()[3])
        img = tmp
    ret_img.paste(img, (64 * x + 10, 64 * y + 10))

ret_img.save('data/images/icons.png')

# Also create the application icons (in data/icons)
if not os.path.exists('data/icons'): os.makedirs('data/icons')
base = PIL.Image.open('icon.png').convert('RGBA')

for size in [16, 24, 32, 48, 64, 128, 256]:
    img = base.resize((size, size), PIL.Image.BILINEAR)
    img.save('data/icons/icon%d.png' % size)
