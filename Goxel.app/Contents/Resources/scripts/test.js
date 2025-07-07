
import * as std from 'std'

/*
 * Example of file format support in js:
 */
/*
goxel.registerFormat({
  name: 'Test',
  exts: ['*.test'],
  exts_desc: 'test',
  import: function(img, path) {
    let layer = img.addLayer()
    let volume = layer.volume
    volume.setAt([0, 0, 0], [255, 0, 0, 255])
  },
  export: function(img, path) {
    try {
      console.log(`Save ${path}`)
      let out = std.open(path, 'w')
      let volume = img.getLayersVolume()
      volume.iter(function(p, c) {
        out.printf(`${p.x} ${p.y}, ${p.z} => ${c.r}, ${c.g}, ${c.b}\n`)
      })
      out.close()
      console.log('done')
    } catch(e) {
      console.log('error', e)
    }
  },
})
*/

function getRandomColor() {
  return [
    Math.floor(Math.random() * 256),
    Math.floor(Math.random() * 256),
    Math.floor(Math.random() * 256),
    255
  ]
}

goxel.registerScript({
  name: 'FillRandom',
  description: 'Fill selection with random voxels',
  onExecute: function() {
    let box = goxel.image.selectionBox
    let volume = goxel.image.activeLayer.volume
    box.iterVoxels(function(pos) {
      volume.setAt(pos, getRandomColor())
    })
  }
})

goxel.registerScript({
  name: 'Dilate',
  onExecute: function() {
    let volume = goxel.image.activeLayer.volume
    volume.copy().iter(function(pos, color) {
      for (let z = -1; z < 2; z++) {
        for (let y = -1; y < 2; y++) {
          for (let x = -1; x < 2; x++) {
            volume.setAt([pos.x + x, pos.y + y, pos.z + z], color)
          }
        }
      }
    })
  }
})
