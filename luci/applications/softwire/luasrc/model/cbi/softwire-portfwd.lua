-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

require("luci.sys")
m = Map("firewall", "Manual port forwarding", "This section allows you to change destination address of forwarded packets.")

s = m:section(TypedSection, "redirect", "")
s.template  = "softwire/customtbl"
--#s.addremove = true
s.add = true -- don't allow adding more port forwarding
--#s.remove = true
s.anonymous = true
s.rowcolors = true
s.maxentries = 5
s.extedit   = false -- luci.dispatcher.build_url("admin", "network", "firewall", "redirect", "%s")

local maxentries = luci.model.uci.cursor():get("softwire", "limits", "maxports")
s.maxentries = tonumber(maxentries)

-- name = s:option(Value, "_name", translate("name"), translate("cbi_optional"))
-- name.size = 10

--iface = s:option(Value,"iface","Interface")
--iface = "wan"
--[[iface = s:option(ListValue, "src", translate("fw_zone"))
iface.default = "wan"
luci.model.uci.cursor():foreach("firewall", "zone",
	function (section)
		iface:value(section.name)
	end)
]]--
proto = s:option(ListValue, "proto", translate("protocol"))
proto:value("tcp", "TCP")
proto:value("udp", "UDP")
proto:value("tcpudp", "TCP+UDP")

dport = s:option(Value, "src_dport")
dport.size = 5

to = s:option(Value, "dest_ip")
for i, dataset in ipairs(luci.sys.net.arptable()) do
	to:value(dataset["IP address"])
end

toport = s:option(Value, "dest_port")
toport.size = 5

return m
