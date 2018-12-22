-- Goxel 3D voxels editor
--
-- copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
--
-- Goxel is free software: you can redistribute it and/or modify it under the
-- terms of the GNU General Public License as published by the Free Software
-- Foundation, either version 3 of the License, or (at your option) any later
-- version.
--
-- Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
-- WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
-- FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
-- details.
-- 
-- You should have received a copy of the GNU General Public License along with
-- goxel.  If not, see <http://www.gnu.org/licenses/>.


-- Code that is executed before any lua script, to setup the global classes.

function make_class(name)
    local meta = {}
    function meta.new(self, ...)
        self.data = goxel_call(name .. '_new', ...)
        return setmetatable(self, meta)
    end
    function meta.__index(self, attr)
        return function(self, ...)
            return goxel_call(name .. '_' .. attr, self.data, ...)
        end
    end
    function meta.__gc(self)
        goxel_call(name .. '_delete', self.data)
    end
    return meta
end

Mesh = make_class('mesh')
Proc = make_class('proc')
