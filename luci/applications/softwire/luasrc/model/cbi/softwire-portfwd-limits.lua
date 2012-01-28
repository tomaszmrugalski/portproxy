-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

require("luci.sys")
m = Map("softwire", "Manual port forwarding administration", "This section allows system administrators to "
.. " define, how many ports end-user is allowed to forward.")


s = m:section(TypedSection, "limits")
s.anonymous = true

port = s:option(Value, "maxports", "Maximum number of ports user is allowed to forward.")
port.isinteger = true



return m
