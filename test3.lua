l = require "ldump"

local t = {
	[function() end] = 1,
}

print(l.dump(t))

