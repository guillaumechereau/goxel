
Small explanation of the internals of goxel code
================================================

The voxels data are stored as blocks of 16^3 voxels (`block_t`).  The blocks
implement a copy on write mechanism with references counting, so that it is
very fast to copy blocks, the actual data (`block_data_t`) is copied only when
we make change to a block.

Several blocks together form a mesh (`mesh_t`), the meshes also use a copy on
write mechanism to make copy basically free.

An `image_t` contains several `layer_t`, which is basically a mesh plus a few
attributes.  The image also keeps snapshots of the layers at every changes for
undo history (since we use copy on write on individual blocks, this does not
require much memory).

The basic function to operate on a mesh is `mesh_op`, we give it a `painter_t`
pointer that defines the operation: shape, color, mode, etc.

All the rendering functions are differed.  The `render_xxx` calls just build a
list of operations, that is executed when we call `render_render`.

The assets are stored directly in the C code (`src/assets.inl`), a python
script (`tools/create_assets`) takes care of generating this file.
We can then use `assets_get` to retrieve them.

The gui is using Ocornut imgui library, with a few custom widgets defined in
`src/imgui_user.inl`.
