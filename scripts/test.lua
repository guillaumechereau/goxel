-- Simple example of script that fills a 64x64x64 cube from a lua function.
-- Run it from command line like that:
--   ./goxel --script ./scripts/test.lua

mesh = Mesh:new()
mesh:fill({64, 64, 64}, function(pos, size)
    x = pos[1] / size[1] - 0.5
    y = pos[2] / size[2] - 0.5
    z = pos[3] / size[3] - 0.5
    x = x * math.pi * 1.5
    y = y * math.pi * 1.5
    z = z * math.pi * 1.5
    a = math.cos(x) + math.cos(y) + math.cos(z)
    r = pos[3] / size[3]
    return {math.floor(255 * r), 0, 0, math.floor(255 * a)}
end)
mesh:save('./out.gox')
