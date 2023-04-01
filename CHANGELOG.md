#Changelog

## [0.12.0] - 2023-04-01

- Added basic support for Minetest file format import.
- Added a new tool to select from a 2d rectangle.
- Fixed issues with high density screens.
- Added some (very basic) support for scripting.

The biggest new feature is probably the scripting support.  For the moment it
is very limited, see the example in data/scripts/test.js.  If you would like to
make your own scripts but some features are missing (and they will), please
open an issue about it.


## [0.10.6] - 2020-07-09

Minor update that improves glTF color export: we can now export the models
with the colors put into a texture instead of vertices attributes.


## [0.10.5] - 2020-01-06

Minor update to attempt to fix a crash with AMD cards.

### Fixed
- Reintroduced layer bounding box edit widget.


## [0.10.4] - 2019-07-29

### Added
- Line tool (same as brush tool with shift pressed).
### Changed
- Better plane tool: on click the plane move just before the clicked voxel
  for quick editing.  The plane rendering has also been improved.
- Slightly better looking grid effect.
### Fixed
- Fixed the behavior of snapping to always select the closest snapped point.
- Fixed bug with selections on top of planes.


## [0.10.0]

### Added
- 'Magic Wand' tool.  Allows to select adjacent voxels.
- Show mirror axis on image box.

### Fixed
- Fixed bug in KVX export.
- Fixed crash with undo when we change materials or cameras.


## [0.9.0] - 2019-06-04

This major release brings proper material support, and better pathtracing
rendering.  The code has changed a lot, so expect a few bugs!

### Added
- Layer materials: each layer can now have its own material.
- Transparent materials.
- Emission materials.
- Support for png palettes.
- Add new view settings.
- Allow to scale a layer (only by factors of two).

### Changed
- Marching cube rendering default to 'flat' colors.
- Layer visibility is saved.
- Materials now use metallic/roughness settings.

### Fixed
- Bug with retina display on OSX.


## [0.8.3] - 2017-03-30

Minor release.

### Added
- Shape layers, for non destructible shapes creation.
- New path tracer, based on yocto-gl.  Totally remove the old one based on
  cycles.
- Support for exporting to KVX.
- Support for build engine palettes.

### Changed
- New default material used: lower the specular reflection.
