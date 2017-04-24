
Goxel
=====

Version 0.6.0

By Guillaume Chereau <guillaume@noctua-software.com>

[![Build Status](
    https://travis-ci.org/guillaumechereau/goxel.svg?branch=master)](
    https://travis-ci.org/guillaumechereau/goxel)

Webpage: https://guillaumechereau.github.io/goxel

About
-----

You can use goxel to create voxel graphics (3D images formed of cubes).  It
works on Linux, Windows and OSX

![goxel screenshot 0](/screenshots/screenshot-castle.png?raw=true)
![goxel screenshot 1](/screenshots/screenshot-procedural-city.png?raw=true)
![goxel screenshot 2](/screenshots/screenshot-dicom.png?raw=true)


Licence
-------

Goxel is released under the GPL3 licence.


Features
--------

- 24 bits RGB colors.
- Unlimited scene size.
- Unlimited undo buffer.
- Layers.
- Marching Cube rendering.
- Procedural rendering.
- Export to obj, pyl, png, magica voxel, qubicle.


Usage
-----

- Left click: apply selected tool operation.
- Middle click: rotate the view.
- right click: pan the view.
- Left/Right arrow: rotate the view.
- Mouse wheel: zoom in and out.


Building
--------

The building system uses scons.  You can compile in debug with 'scons', and in
release with 'scons debug=0'.  On Windows, I only tried to build with msys2.
The code is in C99, using some gnu extensions, so it does not compile with
msvc.

# Linux

Install dependencies: scons pkg-config libglfw3-dev libgtk-3-dev

Then to build:

    make release

# Windows

You need to install msys2 mingw, and the following packages:

    pacman -S mingw-w64-x86_64-gcc
    pacman -S mingw-w64-x86_64-glfw
    pacman -S mingw-w64-x86_64-libtre-git
    pacman -S scons
    pacman -S make

Then to build:

    make release


Download
--------

The last release files can be downloaded from [there](
https://github.com/guillaumechereau/goxel/releases/latest).
