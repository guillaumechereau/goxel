#!/usr/bin/python3

# Goxel 3D voxels editor
#
# copyright (c) 2024-present Guillaume Chereau <guillaume@noctua-software.com>
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

# Script that generates a font file from the svg images in svg/font/

import fontforge
import glob
import sys, os, re

START_CODE = 0xE660

def make_font_from_svgs(sources, output):
    font = fontforge.font()
    font.encoding = "UnicodeFull"
    font.fontname = "Goxel"
    font.familyname = "Goxel"
    font.fullname = "Goxel"
    font.version = "1.0.0"
    font.weight = "Regular"
    font.upos = -100
    font.uwidth = 50
    font.ascent = 850
    font.descent = 150
    font.em = 1000
    for idx, file in enumerate(sorted(sources)):
        assert file.endswith(".svg")
        code = START_CODE + idx
        glyph_name = file.replace(".svg", "")
        glyph = font.createChar(code, glyph_name)
        glyph.importOutlines(file)
        glyph.width = 1000
    font.generate(output)

sources = glob.glob('svg/glyphs/*.svg')
make_font_from_svgs(sources, "data/fonts/goxel-font.ttf")
