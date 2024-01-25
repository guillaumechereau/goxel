#!/usr/bin/python
#encoding: utf-8

# Goxel 3D voxels editor
#
# copyright (c) 2018-present Guillaume Chereau <guillaume@noctua-software.com>
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

import contextlib
import os
import subprocess

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

CHARS = (u"abcdefghijklmnopqrstuvwxyz"
         u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
         u"0123456789"
         u" ?!\"#$%&'()*+,-./°¯[]^:<>{}@_=±\\"
         u"◀▶▲▼▴▾●©")

def run():
    dst = "data/fonts/DejaVuSans-light.ttf"
    subprocess.call(['pyftsubset', FONT_PATH,
                     '--text=%s' % CHARS,
                     '--no-hinting', '--output-file=%s' % dst])

# font.generate("data/fonts/DejaVuSans-light.ttf")
if __name__ == '__main__':
    with contextlib.chdir(os.path.join(os.path.dirname(__file__), '../')):
        run()
