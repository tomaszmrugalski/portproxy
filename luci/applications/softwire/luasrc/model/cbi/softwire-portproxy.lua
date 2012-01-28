-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

f = SimpleForm("processes", "Portproxy", 
  "Portproxy is an automatic port forwarding proxy. It supports NAT-PMP, "..
  "extended NAT-PMP and UPnP protocols. This page is intended for temporary "..
  "portproxy modifications and monitoring. Changes made to this page will "..
  "be lost after device reboot!")
f.reset = "Reset"
f.submit = "Submit"


procs = luci.sys.process.list() -- get list of processes
proxy = nil -- to be filled, if portproxy daemon is running

function searchForPortproxy(pid,tbl)
   if (string.find(tbl["COMMAND"],"/usr/sbin/portproxy") ) then
      proxy = tbl
      f:field(DummyValue,"parameters","Parameters used").value = tbl["COMMAND"]
   end
end
table.foreach(procs,searchForPortproxy) -- for each process, run searchForPortproxy function

state = nil

if proxy then
    state = "RUNNING"
    --table.foreach(proxy,print) -- uncomment to get all process info on console
else
    -- portproxy is NOT running --
    state = "NOT RUNNING"
    proxy = {} -- just create empty table
end

f:field(DummyValue,"status","status").value = state
f:field(DummyValue,"pid","PID").value = proxy["PID"]
f:field(DummyValue,"user","User").value = proxy["USER"]
f:field(DummyValue,"cpu","CPU usage(%)").value = proxy["%CPU"]
f:field(DummyValue,"mem","Memory usage(%)").value = proxy["%MEM"]

if state == "RUNNING" then

hup = f:field(Button, "_hup", "Hang up")
hup.inputstyle = "reload"
hup.value = proxy["PID"]

function hup.write(self, pid)
    -- print("Sending HUP to process " .. self.value) -- print
    null, self.tag_error[pid] = luci.sys.process.signal(self.value, 1)
end

term = f:field(Button, "_term", "Terminate")
term.inputstyle = "remove"
term.value = proxy["PID"]
function term.write(self, pid)
    -- print("Sending TERM to process " .. self.value)
    null, self.tag_error[pid] = luci.sys.process.signal(self.value, 15)
end

kill = f:field(Button, "_kill", "Kill")
kill.inputstyle = "reset"
kill.value = proxy["PID"]
function kill.write(self, section)
    -- print("#### Sending KILL to process " .. self.value)
    null, self.tag_error[section] = luci.sys.process.signal(self.value, 9)
end

else

    params = f:field(Value,"params","Portproxy parameters")

    function params.write(self, section, value)
	-- print(value)
	params.value = tostring(value)
    end

    start = f:field(Button, "_start", "Start")
    start.inputstyle = "start"
    function start.write(self, section)
	local par = params.value
	local cmd = "portproxy"
	if type(par) ~= "function" then
	    cmd = cmd .. " " ..par .. " -g"
	end
	
	cmd = cmd .. " &"
    	-- print("#### Starting portproxy, cmd=" .. cmd)
	self.tag_error[section] = os.execute(cmd)
    end

end

reload = f:field(Button, "_refresh", "Refresh status")
reload.inputstyle = "reload"

return f