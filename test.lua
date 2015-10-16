ldump = require "ldump"

t1 = {
	a = 1,
	x = "5",
	list = {1,2,3,4,5},
}

local s, len = ldump.dump(t1)
print("len", len)
print("t1", s)
print("t1 in line", ldump.dump_in_line(t1))


t2 = {
	function () end,
}

t3 = {
	[{}] = 5,
}

print("check t1", ldump.check(t1))
print("check t2", ldump.check(t2))
print("check t3", ldump.check(t3))
print("check nil", ldump.check())

