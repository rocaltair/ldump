l = require "ldump"

local t = {}
local m = t
for i=1, 20 do
	m.a = {}
	m = m.a
end

print(l.dump(t))
