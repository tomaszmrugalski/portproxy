-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2
require "luci.http"
require "luci.fs"

luci.http.header("refresh","10") -- refesh this page every 10 seconds

-- button styles
local stopStyle="reset"
local startStyle="reload"
local enableStyle="apply"
local disableStyle="remove"

-- returns if specifid process is running or not
function started(process)
   if pid(process) then
      return true
   else
      return false
   end
end

-- returns if specified service is enabled or not
function enabled(service)
   return luci.sys.init.enabled(service)
end

-- returns PID of specified service
function pid(process)
   processes = luci.sys.process.list() -- get list of processes
   -- processes is a table of tables
   -- each entry specifies sigle process
   -- each process is described with list of parameters
   
   for pid,procinfo in pairs(processes) do
      if (string.find(procinfo["COMMAND"], process)) then
	 return pid
      end
   end
   return nil
end

-- returns description if service is started/stopped, enabled/disabled
function status(service,process)
   local s
   if enabled(service) then
      s = "enabled, "
   else
      s = "disabled, "
   end
   
   if pid(process) then
      s = s.."started"
   else
      s = s.."stopped"
   end 

   return s
end

-----------------------------------------------------------------------
-- DHCP ---------------------------------------------------------------
-----------------------------------------------------------------------
dhcpForm = SimpleForm("dhcp", "DHCPv4 client service", 
		      "This section provides control for DHCPv4 client, used to obtain "..
			 "configuration parameters from "..
			 "<abbr title=\"Internet Service Provider\">ISP</abbr>"..
		         " network required "..
			 "for device configuration.")
dhcpForm.reset = false  -- don't display reset button below this form
dhcpForm.submit = false -- don't display reset button below this form

dhcpForm:field(DummyValue, "ignore1", "DHCPv4 Client status").value = status("dhcp","dhclient")
dhcpForm:field(DummyValue, "ignore2","PID").value = pid("dhclient")

if (started("dhclient")) then

   dhcpStop=dhcpForm:field(Button, "dhcpstop", "Stop DHCP", 
			   "Click this button to stop DHCP service immediately.")
   dhcpStop.inputstyle = stopStyle
   function dhcpStop.write(self, value)
      os.execute("/etc/init.d/dhclient stop > /dev/null")
      dhcpForm.message = "DHCP client stopped."
   end

else

   dhcpStart=dhcpForm:field(Button, "dhcpstart", "Start DHCP", 
			    "Click this button to start DHCP (dhclient) service.")
   dhcpStart.inputstyle = startStyle
   function dhcpStart.write(self, value)
      os.execute("/etc/init.d/dhclient start > /dev/null")
      dhcpForm.message = "DHCP client started."
   end

end

if (enabled("dhclient")) then
   dhcpDisable = dhcpForm:field(Button,"dhcpdisable", "Disable DHCP",
				"Click this button to disable DHCP service. "..
                                "Disabled service will not be started again.")
   dhcpDisable.inputstyle = disableStyle
   function dhcpDisable.write(self, value)
      luci.sys.init.disable("dhcp")
      dhcpForm.message = "DHCP client disabled."
   end
else
   dhcpEnable  = dhcpForm:field(Button,"dhcpdisable", "Enable DHCP",
				"Click this button to enable DHCP service. "..
				"Enabled service will be started after reboot,"..
				" during certain actions or started from this page.")
   dhcpEnable.inputstyle = enableStyle
   function dhcpEnable.write(self, value)
      luci.sys.init.enable("dhcp")
      dhcpForm.message = "DHCP client enabled."
   end

end

-----------------------------------------------------------------------
-- RADVD --------------------------------------------------------------
-----------------------------------------------------------------------
radvd = SimpleForm("radvd", "Router Advertisements service",
		   "This section page provides control for RADVD - a daemon "..
		   "that is responsible for sending "..
		   "<abbr title=\"Router Advertisement\">RA</abbr> "..
                   "messages that are used to configure hosts in local network.")
radvd.reset = false
radvd.submit = false
radvd:field(DummyValue, "ignore3", "RADVD daemon status").value = status("radvd","radvd")
radvd:field(DummyValue, "ignore4","PID").value = pid("radvd")

if (started("radvd")) then

   radvdStop=radvd:field(Button, "radvdstop", "Stop RADVD", 
			 "Click this button to stop RADVD daemon immediately.")
   radvdStop.inputstyle = stopStyle
   function radvdStop.write(self, value)
      os.execute("/etc/init.d/radvd stop > /dev/null")
      radvd.message = "RADVD daemon stopped."
   end
else
   radvdStart=radvd:field(Button, "radvdstart", "Start RADVD", 
                          "Click this button to start RADVD service.")
   radvdStart.inputstyle = startStyle
   function radvdStart.write(self, value)
      os.execute("/etc/init.d/radvd start > /dev/null")
      radvd.message = "RADVD daemon started."
   end

end

if (enabled("radvd")) then
   radvdDisable = radvd:field(Button,"radvddisable", "Disable RADVD",
			      "Click this button to disable RADVD service. "..
			      "Disabled service will not be started again.")
   radvdDisable.inputstyle = disableStyle
   function radvdDisable.write(self, value)
      luci.sys.init.disable("radvd")
      radvd.message = "RADVD service disabled."
   end
else
   radvdEnable  = radvd:field(Button,"radvddisable", "Enable RADVD",
			      "Click this button to enable RADVD service. "..
		              "Enabled service will be started after reboot,"..
			      " during certain actions or started from this page.")
   radvdEnable.inputstyle = enableStyle
   function radvdEnable.write(self, value)
      luci.sys.init.enable("radvd")
      radvd.message = "RADVD service enabled."
   end

end

-----------------------------------------------------------------------
-- DNS MASQ -----------------------------------------------------------
-----------------------------------------------------------------------
dnsmasq = SimpleForm("dnsmasq", "DNSMASQ service",
		   "This section page provides control for DNSMASQ - a daemon "..
		   "that is responsible for providing "..
		   "<abbr title=\"Network Address Translation\">NAT</abbr> "..
                   "and DNS service for your local network.")
dnsmasq.reset = false
dnsmasq.submit = false
dnsmasq:field(DummyValue, "ignore5", "DNSMASQ daemon status").value = status("dnsmasq","dnsmasq")
dnsmasq:field(DummyValue, "ignore6","PID").value = pid("dnsmasq")

if (started("dnsmasq")) then
   dnsmasqStop=dnsmasq:field(Button, "dnsmasqstop", "Stop DNSMASQ", 
			 "Click this button to stop DNSMASQ daemon immediately.")
   dnsmasqStop.inputstyle = stopStyle
   function dnsmasqStop.write(self, value)
      os.execute("/etc/init.d/dnsmasq stop > /dev/null")
      dnsmasq.message = "DNSMASQ service stopped."
   end

else
   dnsmasqStart=dnsmasq:field(Button, "dnsmasqstart", "Start DNSMASQ", 
                          "Click this button to start DNSMASQ service.")
   dnsmasqStart.inputstyle = startStyle
   function dnsmasqStart.write(self, value)
      os.execute("/etc/init.d/dnsmasq start > /dev/null")
      dnsmasq.message = "DNSMASQ service started."
   end

end

if (enabled("dnsmasq")) then
   dnsmasqDisable = dnsmasq:field(Button,"dnsmasqdisable", "Disable DNSMASQ",
			      "Click this button to disable DNSMASQ service. "..
			      "Disabled service will not be started again.")
   dnsmasqDisable.inputstyle = disableStyle
   function dnsmasqDisable.write(self, value)
      luci.sys.init.disable("dnsmasq")
      dnsmasq.message = "DNSMASQ service disabled."
   end
else
   dnsmasqEnable  = dnsmasq:field(Button,"dnsmasqdisable", "Enable DNSMASQ",
			      "Click this button to enable DNSMASQ service. "..
		              "Enabled service will be started after reboot,"..
			      " during certain actions or started from this page.")
   dnsmasqEnable.inputstyle = enableStyle
   function dnsmasqEnable.write(self, value)
      luci.sys.init.enable("dnsmasq")
      dnsmasq.message = "DNSMASQ service enabled."
   end

end

-----------------------------------------------------------------------
-- TUNNEL -------------------------------------------------------------
-----------------------------------------------------------------------
tunnel = SimpleForm("tunnel", "Tunnel service",
		   "This section page provides control for tunnel - a set "..
		   "of scripts that manage tunnel creation and configuration.")
tunnel.reset = false
tunnel.submit = false

tunnelStat = tunnel:field(DummyValue, "ignore7", "TUNNEL daemon status")


if enabled("tunnelscript") then
   tunnelStat.value = "enabled"
else
   tunnelStat.value = "disabled"
end

tunnelStop=tunnel:field(Button, "tunnelstop", "Stop Tunnel", 
			"Click this button to tear down tunnel.")
tunnelStop.inputstyle = stopStyle

tunnelStart=tunnel:field(Button, "tunnelstart", "Start Tunnel", 
			 "Click this button to begin tunnel "..
                         "initialization process.")
tunnelStart.inputstyle = startStyle

function tunnelStop.write(self, value)
   os.execute("/etc/init.d/tunnelscript stop > /dev/null")
   tunnel.message = "Tunnel stopped. (/etc/init.d/tunnelscript stop command executed)"
end

function tunnelStart.write(self, value)
   os.execute("/etc/init.d/tunnelscript start > /dev/null")
   tunnel.message = "Tunnel configuration process started "..
      "(/etc/init.d/tunnelscript stop command executed)."
end

if (enabled("tunnelscript")) then
   tunnelDisable = tunnel:field(Button,"tunneldisable", "Disable Tunnel",
			      "Click this button to disable Tunnel service. "..
			      "Disabled service will not be started again.")
   tunnelDisable.inputstyle = disableStyle
   function tunnelDisable.write(self, value)
      luci.sys.init.disable("tunnelscript")
      tunnel.message = "Tunnel service disabled."
   end
else
   tunnelEnable  = tunnel:field(Button,"tunneldisable", "Enable Tunnel",
			      "Click this button to enable Tunnel service. "..
		              "Enabled service will be started after reboot,"..
			      " during certain actions or started from this page.")
   tunnelEnable.inputstyle = enableStyle
   function tunnelEnable.write(self, value)
      luci.sys.init.enable("tunnelscript")
      tunnel.message = "Tunnel service enabled."
   end

end

-----------------------------------------------------------------------
-- PORTPROXY ----------------------------------------------------------
-----------------------------------------------------------------------
proxy = SimpleForm("proxy", "Portproxy service",
		   "This section page provides control for portproxy - a "..
		   "daemon that works as an itermediary between hosts "..
		   "located in LAN and ISP regarding automatic port"..
		   "configuration.")
proxy.reset = false
proxy.submit = false
proxy:field(DummyValue, "ignore9", "Porproxy daemon status").value = status("portproxy","portproxy")
proxy:field(DummyValue, "ignore10","PID").value = pid("portproxy")

if (started("portproxy")) then
   proxyStop=proxy:field(Button, "proxystop", "Stop Portproxy", 
			 "Click this button to stop Portproxy daemon immediately.")
   proxyStop.inputstyle = stopStyle
   function proxyStop.write(self, value)
      os.execute("/etc/init.d/portproxy stop > /dev/null")
      proxy.message = "Portproxy daemon stopped."
   end

else
   proxyStart=proxy:field(Button, "proxystart", "Start Portproxy", 
                          "Click this button to start Portproxy service.")
   proxyStart.inputstyle = startStyle
   function proxyStart.write(self, value)
      os.execute("/etc/init.d/portproxy start > /dev/null")
      proxy.message = "Portproxy daemon started."
   end

end

if (enabled("portproxy")) then
   proxyDisable = proxy:field(Button,"proxydisable", "Disable Portproxy",
			      "Click this button to disable Portproxy service. "..
			      "Disabled service will not be started again.")
   proxyDisable.inputstyle = disableStyle
   function proxyDisable.write(self, value)
      luci.sys.init.disable("proxy")
      proxy.message = "Portproxy service disabled."
   end
else
   proxyEnable  = proxy:field(Button,"proxydisable", "Enable Portproxy",
			      "Click this button to enable Portproxy service. "..
		              "Enabled service will be started after reboot,"..
			      " during certain actions or started from this page.")
   proxyEnable.inputstyle = enableStyle
   function proxyEnable.write(self, value)
      luci.sys.init.enable("proxy")
      proxy.message = "Portproxy service enabled."
   end
end

-- executes command and gets its stdout
-- local ps = luci.util.execi("top -bn1")

-----------------------------------------------------------------------
-- SOFTWIRE STATUS ----------------------------------------------------
-----------------------------------------------------------------------
statusInfo = SimpleForm("softwire","Tunnel status",
			"This section provides information about "..
			   "established tunnels (softwire or DS-Lite).")

-- returns tunnel mode (softwire, DS-Lite, disabled)
local function tunnelMode()
   return luci.model.uci.cursor():get("mantun", "general", "DsliteAuto")
end

-- returns text version of tunnel mode
local function tunnelModeText()
   if tunnelMode()=="0" then
      return "Softwire"
   end
   if tunnelMode()=="1" then
      return "DS-Lite"
   end
   return "Disabled"
end

-- returns text description of tunnel softwire status
local function softwireStatus()
   if tunnelMode()~="0" then 
      return "disabled"
   end

   devices = luci.sys.net.devices()
   local s
   for id,device in pairs(devices) do
      if device == "ppp0" then
	 s = "Softwire established (ppp0 interface available)"
      end
   end
   if s==nil then
      return "Softwire failed to establish (ppp0 interface missing)"
   end

   if luci.fs.readfile("/tmp/gotvaluefrmdhcp") then
      s = s.. ", DHCP successful. Softwire operational."
   else
      s = s..", but DHCP failed."
   end

   return s
end

-- returns text description of DS-Lite tunnel status
local function dsliteStatus()
   if tunnelMode()~="1" then 
      return "disabled"
   end

   devices = luci.sys.net.devices()
   local s
   for id,device in pairs(devices) do
      if device == "dslite" then
	 s = "DS-Lite tunnel configured (dslite interface available)"
      end
   end
   if s==nil then
      return "DS-Lite tunnel not configured (dslite interface missing)"
   end
   return s
end

statusInfo:field(DummyValue,"ignore11", "Tunnel mode").value = tunnelModeText()

statusInfo:field(DummyValue,"ignore12", "Softwire status").value = softwireStatus()

statusInfo:field(DummyValue,"ignore13", "DS-Lite status").value = dsliteStatus()

statusInfo.reset = false
statusInfo.submit = false
statusInfo.errmessage = "This page reloads every 10 seconds."
-- using errmessage as this message is printed in red.

return dhcpForm, radvd, dnsmasq, tunnel, proxy, statusInfo
