
import * as std from 'std'

goxel.registerFormat({
  name: 'Test',
  ext: 'test\0*.test\0',
  export: function(img, path) {
    try {
      console.log(`Save ${path}`)
      let out = std.open(path, 'w')
      mesh = img.getLayersMesh()
      out.close()
      console.log('done')
    } catch(e) {
      console.log('error', e)
    }
  },
})
