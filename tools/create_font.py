#!/usr/bin/python
#encoding: utf-8

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
