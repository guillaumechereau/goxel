print('Test script')

mesh = Mesh:new()

cubeSize = 64
local function positionsGenerator()
    local x, y, z
    return coroutine.wrap(function()
        for x = 0, cubeSize - 1 do
            for y = 0, cubeSize - 1 do
                for z = 0, cubeSize - 1 do
                    coroutine.yield(x, y, z)
                end
            end
        end
    end)
end

function computeColor(x, y, z)
    r = z / cubeSize
    x = x / cubeSize - 0.5
    y = y / cubeSize - 0.5
    z = z / cubeSize - 0.5
    x = x * math.pi * 1.5
    y = y * math.pi * 1.5
    z = z * math.pi * 1.5
    a = math.cos(x) + math.cos(y) + math.cos(z)
    return math.floor(255 * r), 0, 0, math.floor(255 * a)
end

for x, y, z in positionsGenerator() do
    r, g, b, a = computeColor(x, y, z)
    mesh:set_at({x, y, z}, {r, g, b, a})
end

mesh:save('/tmp/out.gox')
