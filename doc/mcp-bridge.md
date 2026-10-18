# Goxel command bridge

The command bridge lets an external program inspect and edit the scene that is
open in the Goxel editor while you keep working on it.  It was written for
[Goxel-MCP](https://github.com/yberudev/Goxel-MCP), a Model Context Protocol
server, but any program that can open a Unix socket can use it.

The bridge is available on Linux, BSD and macOS, in the GLFW build (`scons`).
It is not compiled in on Windows and in the web build, and the macOS Xcode
project (`osx/`) does not use it.


## Enabling the bridge

The bridge is disabled by default.  Enable it by giving a socket path:

    goxel --mcp-socket /tmp/goxel.sock

or with an environment variable:

    GOXEL_MCP_SOCKET=/tmp/goxel.sock goxel

- The socket is created with mode `0600`, so only the current user can connect.
- A stale socket file at the same path is replaced.  Any other kind of file
  there is left untouched, and the bridge stays disabled.
- On macOS, socket paths are limited to 103 bytes.
- The socket file is removed when Goxel exits normally.  If Goxel is killed,
  the file is left behind and is replaced the next time Goxel starts.
- One client is served at a time.  Other clients wait until it disconnects.


## Protocol

The client sends one JSON object per line and receives one JSON object per
line in response:

    -> {"id": 1, "cmd": "info", "args": {}}
    <- {"id": 1, "ok": true, "result": {...}}

    -> {"id": 2, "cmd": "open", "args": {"path": "/missing.gox"}}
    <- {"id": 2, "ok": false, "error": "cannot load gox file"}

`id` is optional.  When present, it is copied into the response.

Commands run on the main thread, between two frames, so the editor shows the
changes immediately.  Each command that modifies the scene adds one step to
the undo history.  Ctrl+Z in the editor (or the `undo` command) reverts the
whole command.

Voxel colors are packed as unsigned 32 bits integers `0xRRGGBBAA`: for example
`4278190335` is opaque red (`0xff0000ff`).


### Commands

| Command | Arguments | Result |
|---|---|---|
| `ping`, `info` | | `goxel_version`, `bridge_version`, `path` of the open file, `modified`, and `layers` (`id`, `name`, `visible`, `active`, `editable`, `voxels` count) |
| `get_scene` | `layer_id` (optional) | `layers` as for `info`, but with `data`: a flat array `[x, y, z, rgba, x, y, z, rgba, ...]` instead of the voxel count |
| `apply` | `ops`: array of operations, see below | `results`: one entry per operation |
| `new_scene` | | Replaces the open image with an empty one |
| `open` | `path` | Opens a `.gox` file, or imports any other supported format into a new image |
| `save` | `path` | Saves the image as `.gox` and makes it the current file |
| `export` | `path`, `format` (optional, for example `"vox"`, `"gltf"`, `"obj"`, `"png"`) | Uses Goxel's own exporters |
| `undo`, `redo` | | |


### Operations of the `apply` command

Operations are applied in order, and a single `apply` is a single undo step.
On error, processing stops: the operations already applied are kept, and they
can be undone.

| Operation | Fields | Result |
|---|---|---|
| `add_layer` | `name` (optional) | `{"id": <new layer id>}` |
| `delete_layer` | layer | |
| `rename_layer` | layer, `name` | |
| `set_visible` | layer, `visible` (boolean) | |
| `set_active` | layer | |
| `clear_layer` | layer | |
| `set_voxels` | layer, `data`: `[x, y, z, rgba, ...]` | `{"set": <count>}`.  A voxel with alpha `0` is removed |

A layer is given either by `"id"`, or by `"new_index"`: the index of a layer
created by a previous `add_layer` of the same request.  This creates a layer
and fills it in one undo step:

```json
{"id": 3, "cmd": "apply", "args": {"ops": [
    {"op": "add_layer", "name": "wheels"},
    {"op": "set_voxels", "new_index": 0,
     "data": [0, 0, 0, 255, 1, 0, 0, 255]},
    {"op": "set_active", "new_index": 0}
]}}
```

Clone layers and shape layers cannot be edited with `set_voxels` and
`clear_layer`: the `editable` flag returned by `info` tells which layers can
be.


## Example

With a Python client:

```python
import json, socket

s = socket.socket(socket.AF_UNIX)
s.connect("/tmp/goxel.sock")
f = s.makefile("rw")

def call(cmd, **args):
    f.write(json.dumps({"cmd": cmd, "args": args}) + "\n")
    f.flush()
    r = json.loads(f.readline())
    if not r["ok"]:
        raise RuntimeError(r["error"])
    return r["result"]

red = 0xff0000ff
voxels = []
for x in range(10):
    for y in range(10):
        voxels += [x, y, 0, red]

layer = call("info")["layers"][0]["id"]
call("apply", ops=[{"op": "set_voxels", "id": layer, "data": voxels}])
call("save", path="/tmp/floor.gox")
```
