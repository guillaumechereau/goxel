#!/usr/bin/python

# Requires Python Version 3 Interpreter

# A Simple Script Which Uses FontForge to Read Font Files
# Remove All The Characters Except The Given in "CHARS" Variable
# And Write it To A File

# Font Forge Module is not Available on Pip
# the module get installed with FontForge Software itself
# Font-Forge: https://fontforge.org/en-US/

import fontforge
import unicodedata

PATH = "./data/fonts/Montserrat-Regular.ttf"

CHARS = (
	u"abcdefghijklmnopqrstuvwxyz"
	u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	u"0123456789"
	u" ?!\"#$%&'()*+,-./°¯[]^:<>{}@_=±"
	u"◀▶▲▼▴▾●©"
)

font = fontforge.open(PATH)
for g in font:
	u = font[g].unicode
	if u == -1: continue
	u = chr(u)
	if u not in CHARS: continue
	font.selection[ord(u)] = True

font.selection.invert()

for i in font.selection.byGlyphs:
	font.removeGlyph(i)

font.generate("data/fonts/Montserrat-Minified.ttf")
