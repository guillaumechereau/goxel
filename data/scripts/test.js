
import * as std from 'std'

goxel.registerFormat({
  name: 'Test',
  ext: 'test\0*.test\0',
  export: function(img, path) {
    try {
      console.log(`Save ${path}`)
      let out = std.open(path, 'w')
      let volume = img.getLayersVolume()
      volume.iter(function(p, c) {
        console.log(`${p.x} ${p.y}, ${p.z} => ${c.r}, ${c.g}, ${c.b}`)
      })
      out.close()
      console.log('done')
    } catch(e) {
      console.log('error', e)
    }
  },
})

goxel.registerScript({
  name: 'Test',
  onExecute: function() {
    console.log('test')
    let box = goxel.selection
    if (!box) {
      console.log('Need a selection')
      return
    }
    let volume = goxel.image.layer.volume
    box.iterVoxels(function(pos) {
      pos = box.wordToLocal(pos)
    })
  }
})
