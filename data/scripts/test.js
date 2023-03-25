
import * as std from 'std'

goxel.registerFormat({
  name: 'Test',
  ext: 'test\0*.test\0',
  export: function(img, path) {
    try {
      console.log(`Save ${path}`)
      let out = std.open(path, 'w')
      let mesh = img.getLayersMesh()
      mesh.iter(function(p, c) {
        console.log(`${p.x} ${p.y}, ${p.z} => ${c.r}, ${c.g}, ${c.b}`)
      })
      out.close()
      console.log('done')
    } catch(e) {
      console.log('error', e)
    }
  },
})
