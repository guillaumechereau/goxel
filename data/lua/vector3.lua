--------------------------------------------------------------------------------
--   Copyright (c) 2015 , 蒙占志(topameng) topameng@gmail.com
--   All rights reserved.
--
--   Use, modification and distribution are subject to the "New BSD License"
--   as listed at <url: http://www.opensource.org/licenses/bsd-license.php>.
--------------------------------------------------------------------------------

local acos	= math.acos
local sqrt 	= math.sqrt
local max 	= math.max
local min 	= math.min
local clamp = math.clamp
local cos	= math.cos
local sin	= math.sin
local abs	= math.abs
local sign	= math.sign
local setmetatable = setmetatable
local rawset = rawset
local rawget = rawget

local rad2Deg = math.rad2Deg
local deg2Rad = math.deg2Rad

Vector3 = {	
	class = "Vector3",
}

local fields = {}

setmetatable(Vector3, Vector3)

Vector3.__index = function(t,k)
	local var = rawget(Vector3, k)
	
	if var == nil then							
		var = rawget(fields, k)
		
		if var ~= nil then
			return var(t)				
		end		
	end
	
	return var
end

Vector3.__call = function(t,x,y,z)
	return Vector3.New(x,y,z)
end

function Vector3.New(x, y, z)
	local v = {x = x or 0, y = y or 0, z = z or 0}		
	setmetatable(v, Vector3)		
	return v
end
	
function Vector3:Set(x,y,z)	
	self.x = x or 0
	self.y = y or 0
	self.z = z or 0
end

function Vector3:Get()	
	return self.x, self.y, self.z	
end

function Vector3:Clone()
	return Vector3.New(self.x, self.y, self.z)
end

function Vector3.Distance(va, vb)
	return sqrt((va.x - vb.x)^2 + (va.y - vb.y)^2 + (va.z - vb.z)^2)
end

function Vector3.Max(lhs, rhs)
	return Vector3.New(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z))
end

function Vector3.Min(lhs, rhs)
	return Vector3.New(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z))
end

function Vector3.Cross(lhs, rhs)
	local x = lhs.y * rhs.z - lhs.z * rhs.y
	local y = lhs.z * rhs.x - lhs.x * rhs.z
	local z = lhs.x * rhs.y - lhs.y * rhs.x
	return Vector3.New(x,y,z)	
end

function Vector3:Equals(other)
	return self.x == other.x and self.y == other.y and self.z == other.z
end

function Vector3:Mul(q)
	if type(q) == "number" then
		self.x = self.x * q
		self.y = self.y * q
		self.z = self.z * q
	else
		self:MulQuat(q)
	end
	
	return self
end

function Vector3:Div(d)
	self.x = self.x / d
	self.y = self.y / d
	self.z = self.z / d
	
	return self
end

function Vector3:Add(vb)
	self.x = self.x + vb.x
	self.y = self.y + vb.y
	self.z = self.z + vb.z
	
	return self
end

function Vector3:Sub(vb)
	self.x = self.x - vb.x
	self.y = self.y - vb.y
	self.z = self.z - vb.z
	
	return self
end

function Vector3:MulQuat(quat)	   
	local num 	= quat.x * 2
	local num2 	= quat.y * 2
	local num3 	= quat.z * 2
	local num4 	= quat.x * num
	local num5 	= quat.y * num2
	local num6 	= quat.z * num3
	local num7 	= quat.x * num2
	local num8 	= quat.x * num3
	local num9 	= quat.y * num3
	local num10 = quat.w * num
	local num11 = quat.w * num2
	local num12 = quat.w * num3
	
	local x = (((1 - (num5 + num6)) * self.x) + ((num7 - num12) * self.y)) + ((num8 + num11) * self.z)
	local y = (((num7 + num12) * self.x) + ((1 - (num4 + num6)) * self.y)) + ((num9 - num10) * self.z)
	local z = (((num8 - num11) * self.x) + ((num9 + num10) * self.y)) + ((1 - (num4 + num5)) * self.z)
	
	self:Set(x, y, z)	
	return self
end

Vector3.__tostring = function(self)
	return "["..self.x..","..self.y..","..self.z.."]"
end

Vector3.__div = function(va, d)
	return Vector3.New(va.x / d, va.y / d, va.z / d)
end

Vector3.__mul = function(va, d)
	if type(d) == "number" then
		return Vector3.New(va.x * d, va.y * d, va.z * d)
	else
		local vec = va:Clone()
		vec:MulQuat(d)
		return vec
	end	
end

Vector3.__add = function(va, vb)
	return Vector3.New(va.x + vb.x, va.y + vb.y, va.z + vb.z)
end

Vector3.__sub = function(va, vb)
	return Vector3.New(va.x - vb.x, va.y - vb.y, va.z - vb.z)
end

Vector3.__unm = function(va)
	return Vector3.New(-va.x, -va.y, -va.z)
end

Vector3.__eq = function(a,b)
	local v = a - b
	local delta = v:SqrMagnitude()
	return delta < 1e-10
end

-- Direction                                     x  y  z
fields.up 		= function() return Vector3.New( 0, 0, 1) end
fields.down 	= function() return Vector3.New( 0, 0, 0) end
fields.right	= function() return Vector3.New( 1, 0, 0) end
fields.left		= function() return Vector3.New(-1, 0, 0) end
fields.forward 	= function() return Vector3.New( 0, 1, 0) end
fields.back		= function() return Vector3.New( 0,-1, 0) end
fields.zero		= function() return Vector3.New( 0, 0, 0) end

fields.magnitude 	= Vector3.Magnitude
fields.normalized 	= Vector3.Normalize
fields.sqrMagnitude = Vector3.SqrMagnitude
