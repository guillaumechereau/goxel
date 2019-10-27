+++
title = "Live"
+++

<style>

  #goxel-canvas-parent {
    position: relative;
    width: 100%;
    height: 100%;
  }

  #goxel-canvas {
    position: absolute;
    width: 100%;
    height: 100%;
  }

</style>

<script src="goxel.js"></script>

<div id="goxel-canvas-parent">
  <canvas id='goxel-canvas'></canvas>
</div>

<script>
  let canvas = document.getElementById('goxel-canvas')
  canvas.oncontextmenu = function (e) {
    e.preventDefault();
  };
  Goxel({
    canvas: canvas
  })
  .then(function(Goxel) {
    let contextAttributes = {};
    contextAttributes.alpha = false;
    contextAttributes.depth = true;
    contextAttributes.stencil = true;
    contextAttributes.antialias = true;
    contextAttributes.premultipliedAlpha = true;
    contextAttributes.preserveDrawingBuffer = false;
    contextAttributes.preferLowPowerToHighPerformance = false;
    contextAttributes.failIfMajorPerformanceCaveat = false;
    let ctx = Goxel.GL.createContext(canvas, contextAttributes);
    Goxel.GL.makeContextCurrent(ctx);

    onResize = function() {
      let w = canvas.offsetWidth
      let h = canvas.offsetHeight
      Goxel.setCanvasSize(w, h)
    }
    window.addEventListener("resize", onResize, true)
    Goxel.callMain()
    onResize()

    let onWheelEvent = function(e) {
      e.preventDefault()
      let delta = e.deltaY > 0 ? 1 : -1
      Goxel._on_scroll(0, 0, -delta * 2)
      return false
    }

    canvas.addEventListener('wheel', onWheelEvent, {passive: false});
  })
</script>

