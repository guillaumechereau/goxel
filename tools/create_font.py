#!/usr/bin/python
#encoding: utf-8

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
# Generate a small font file by removing all unwanted characters from a base
# font.

import fontforge
import unicodedata

PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

CHARS = (u"abcdefghijklmnopqrstuvwxyz"
         u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
         u"0123456789"
         u" ?!\"#$%&'()*+,-./°¯[]^:<>{}@_=±"
         u"◀▶▲▼▴▾●©"
         )

font = fontforge.open(PATH)
for g in font:
    u = font[g].unicode
    if u == -1: continue
    u = unichr(u)
    if u not in CHARS: continue
    font.selection[ord(u)] = True

font.selection.invert()
for i in font.selection.byGlyphs:
    font.removeGlyph(i)

font.generate("data/fonts/DejaVuSans-light.ttf")
