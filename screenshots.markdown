---
layout: default
images:
  - path: marching-cube.png
    title: Marching Cube Rendering
    text: Support for smooth rendering of voxels using the marching cube
          algorithm.
  - path: procedural.png
    title: Procedural rendering tool
    text: It is possible to programatically generate shapes using goxel
          procedural language, inspired by [ContextFree].
          <br>
          [More informations >][procedural]
  - path: laser.png
    title: Laser tool
    text: Carve in real time into the volume.
  - path: layers.png
    title: Layers panel
    text: You can stack as many layers as you want.
          Layers can be edited independently, so that you can use
          one per part of your image.
  - path: image-plane.png
    title: Plane
    text: You can import a 2D image into Goxel, to use as a reference for
          your model.
  - path: palettes.png
    title: Palettes panel
    text: Goxel comes with a set of predefined color palettes.
  - path: selection.png
    title: Selection tool
    text: Allow to operate on a portion of the image.
---

### Screenshots

{::options parse_block_html="true" /}

{% for image in page.images %}
<div class="row z-depth-2">
  <div class="col m6 s12">
   <img class="materialboxed responsive-img"
        src="{{site.baseurl}}/images/screenshots/{{ image.path }}"
        alt="{{image.title}}" >
  </div>
  <div class="col m6 s12 card-content flow-text">
   <h5>{{image.title}}</h5>

   {{image.text}}

  </div>
</div>
{% endfor %}


[ContextFree]: https://www.contextfreeart.org/
[procedural]: https://blog.noctua-software.com/goxel-procedural.html
