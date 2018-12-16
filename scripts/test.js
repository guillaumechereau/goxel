
/*
 * For the moment we have this code to create a proxy for the Mesh object.
 * Of course this should be hidden in the future.
 */
function Mesh() {
    this.data = goxel_call('mesh_new');
    Duktape.fin(this, function(obj) {
        goxel_call('mesh_delete', obj.data);
    });
    return new Proxy(this, {
        get: function(targ, key, recv) {
            return function() {
                const args = Array.prototype.slice.call(arguments);
                args = ['mesh_' + key, targ.data].concat(args);
                goxel_call.apply(null, args);
            };
        },
    });
}

const mesh = new Mesh();
mesh.fill([64, 64, 64], function(pos, size) {
    x = pos[0] / size[0] - 0.5;
    y = pos[1] / size[1] - 0.5;
    z = pos[2] / size[2] - 0.5;
    x = x * Math.PI * 1.5;
    y = y * Math.PI * 1.5;
    z = z * Math.PI * 1.5;
    a = Math.cos(x) + Math.cos(y) + Math.cos(z);
    r = pos[2] / size[2];
    return [Math.floor(255 * r), 0, 0, Math.floor(255 * a)];
});
mesh.save('./out.gox');
