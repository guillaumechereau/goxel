
Goxel++
=====

[![Build Status](https://github.com/tatelax/goxelplusplus/actions/workflows/ci.yml/badge.svg)](https://github.com/tatelax/goxelplusplus/actions/workflows/ci.yml)

Goxel++ is a fork of [Goxel](https://github.com/guillaumechereau/goxel). It includes several QOL improvements such as improved camera controls and additional settings which makes working with Goxel much less cumbersome for more complex models.

Original webpage: https://goxel.xyz

Original author: Guillaume Chereau <guillaume@noctua-software.com>

About
-----

You can use Goxel++ to create voxel graphics (3D images formed of cubes).  It
works on Linux, BSD, Windows and macOS.

Goxel++ features over Goxel
---------------------------

* Adds camera "fly mode" with controls similar to Unity.
  * Hold right mouse button and press W, A, S, or D to enter fly mode.
  * WASD to control camera, Q/E to move up and down or Ctrl/Space.
  * Left shift to double fly speed
* Adds ability to set snapping size to match current brush size
* Adds adjustable camera FOV
* Adds zoom to cursor position instead of zoom to center of screen
* Adds zoom collision so zooming stops when close to a voxel
* Unified two ways of exporting into one


Download
--------

The last release files can be downloaded from [here](
https://github.com/tatelax/goxelplusplus/releases/latest).

![goxel screenshot 0](https://goxel.xyz/gallery/thibault-fisherman-house.jpg)
Fisherman house, made with Goxel by
[Thibault Simar](https://www.artstation.com/exm)


License
-------

Goxel++ is released under the GNU GPL3 license.

Features
--------

- 24 bits RGB colors.
- Unlimited scene size.
- Unlimited undo buffer.
- Layers.
- Marching Cube rendering.
- Procedural rendering.
- Export to obj, pyl, png, magica voxel, qubicle.
- Ray tracing.


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
release with 'scons mode=release'.  On Windows, currently possible to build
with [msys2](https://www.msys2.org/) or try prebuilt
[goxel](https://packages.msys2.org/base/mingw-w64-goxel) package directly.
The code is in C99, using some gnu extensions, so it does not compile
with msvc.

# Linux/BSD

Install dependencies using your package manager.  On Debian/Ubuntu:

    - scons
    - pkg-config
    - libglfw3-dev
    - libgtk-3-dev

Then to build, run the command:

    make release

# Windows

You need to install MSYS2 and use the **MSYS2 MINGW64** terminal to install the following packages:

    pacman -S mingw-w64-x86_64-gcc
    pacman -S mingw-w64-x86_64-glfw
    pacman -S mingw-w64-x86_64-libtre-git
    pacman -S scons
    pacman -S make

Then to build:

    make release

Contributing
------------

Please read the [contributing document](CONTRIBUTING.md).