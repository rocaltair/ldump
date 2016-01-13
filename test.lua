ldump = require "ldump"

local tables = {
	t1 = {
		a = 1,
		x = "5",
		list = {1,2,3,4,5},
		["quote\""] = "here is the quote [[here]]",
	},
	t2 = {
		function () end,
	},
	t3 = {
		[{}] = 5,
	},
}

table.foreach(tables, function(tname, tbl)
	local ok = ldump.check(tbl)
	if not ok then
		print(string.format("tname=%s check failed", tname))
	else
		print(string.format("tname=%s check ok", tname))
		local s, len = ldump.dump(tables.t1)
		print("len of " .. tname, len)
		print("dump of " .. tname, s)
		print(tname .. " in line", ldump.dump_in_line(tbl))
	end
end)

print("check nil", ldump.check()) -- this will emit an error for non args in ldump.check

