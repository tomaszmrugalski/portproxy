-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

require "luci.fs"
require "luci.model.uci"

-- returns tunnel type (softwire, DS-Lite or disabled)
local function tunnelMode()
   return luci.model.uci.cursor():get("mantun", "general", "DsliteAuto")
end

-- returns text description of tunnel type
local function tunnelModeText()
   if tunnelMode()=="0" then
      return "Softwire"
   end
   if tunnelMode()=="1" then
      return "DS-Lite"
   end
   return "Disabled"
end

-- returns current value of SMTU4
local function getSMTU4()
   x = luci.http.formvalue("cbid.misc.1.smtu4")
   if x==nil then	
   	x = luci.fs.readfile("/proc/softwire/smtu") -- "/proc/softwire/smtu"
   end	
   if x==nil then
      x = "0"
   end 
   return x
end

-- returns current value of SMTU6
local function getSMTU6()
   x = luci.http.formvalue("cbid.misc.1.smtu6")
   if x==nil then
	  x = luci.fs.readfile("/proc/softwire/ipv6smtu") 
   end	
   if x==nil then
      x = "0"
   end 
   return x
end

-- returns current status of DF bit
local function getDF()
   x = luci.http.formvalue("cbid.misc.1.dfBit")
   f.message = x
   if x==nil then
	   x = luci.fs.readfile("/proc/softwire/toggle")
   end
   if x==nil then
      x = "0"
   end
   return tostring(x)
end

f = SimpleForm("misc", "Fragmentation", 
	       "Following page allows control of packet fragmentation. "..
		  "<abbr title=\"Maximum Transmission Unit\">MTU</abbr> "..
		  "and Don't Fragment bit may be set on this page. "..
		  "Depending on the tunnel mode used, different options are "..
		  "available.")
f.message="" 
-- make sure that the field contains empty string, not nil value
-- so we can append extra strings to it.

f:field(DummyValue,"ignore1", "Tunnel mode").value = tunnelModeText()

if tunnelMode()~= "2" then
-- softwire or DS-Lite

   smtu4 = f:field(Value,"smtu4","IPv4 SMTU",
		   "If DF bit is set, size of IPv4 packets will be compared "..
		      "with SMTU, to send ICPM error messages. Depending on the "..
		      "toggle setting the packet will be forwarded or dropped. "..
		      "Maximum Value = 65535. Minimum Value = 68. "..
		      "If value is 0, system MTU will be used. ")
   
   
   function smtu4.write(self, section, value)
      if value~="0" then
	 -- for some strange reason, following code does not work
	 os.execute("echo "..value.." > /proc/softwire/smtu")
	 os.execute("echo 1 > /proc/softwire/overridesmtu") 
	 os.execute("uci set proc_vars.procentities.smtu="..value)
	 os.execute("uci set proc_vars.procentities.overridesmtu=1")

         -- luci.fs.writefile("/proc/softwire/smtu",value)
	 -- luci.fs.writefile("/proc/softwire/overridesmtu","1")
	 --luci.model.uci.cursor():set("proc_vars", "procentities", "smtu", value)
	 --luci.model.uci.cursor():set("proc_vars", "procentities", "overridesmtu", "9")
	 --luci.model.uci.cursor():commit("proc_vars")
	 f.message = f.message.." SMTU IPv4 set to "..value.."."
      else
	 -- zero value (user specified 0, let system decide on its own)
	 os.execute("echo 0 > /proc/softwire/overridesmtu") 
	 os.execute("uci set proc_vars.procentities.overridesmtu=0")

	 -- luci.fs.writefile("/proc/softwire/overridesmtu","0")
	 -- luci.model.uci.cursor():set("proc_vars", "procentities", "overridesmtu", "0")

	 f.message = f.message.." SMTU IPv4 set to 0 (system will decide)."
      end
      
      smtu4.value = value
   end
   
   function smtu4.validate(self, value)
      if tonumber(value)==nil then
	 value="0"
      end
      if value=="0" then
	 return "0"
      end
      if tonumber(value)<68 then
	 return "68"
      end
      if tonumber(value)>65535 then
	 return 65535
      end
      return tonumber(value)
   end
   
   smtu4.value = getSMTU4()

end

if tunnelMode()== "1" then
-- DS-Lite

   dfBit = f:field(ListValue,"dfBit","DF Bit Set")
   dfBit:value("0","disabled")
   dfBit:value("1","enabled")

   smtu6 = f:field(Value,"smtu6", "IPv6 SMTU", 
		   "This value is compared with the size of DS-Lite IPv6 "..
		      "packets, packet size exceeding this value are fragmented."..
		      "Maximum Value = 65535, Minimum Value = 1280."..
		      "If values is 0, operating system MTU will be used.")
   function smtu6.write(self, section, value)
      if value~="0" then
	 os.execute("echo "..value.." > /proc/softwire/ipv6smtu")
	 os.execute("echo 1 > /proc/softwire/overrideipv6smtu") 
	 os.execute("uci set proc_vars.procentities.ipv6smtu="..value)
	 os.execute("uci set proc_vars.procentities.overrideipv6smtu=1")

	 -- luci.fs.writefile("/proc/softwire/overrideipv6smtu","1")
	 -- luci.fs.writefile("/proc/softwire/ipv6smtu",tostring(value))
	 -- luci.model.uci.cursor():set("proc_vars", "procentities", "ipv6smtu", value)
	 luci.model.uci.cursor():set("proc_vars", "procentities", "overrideipv6smtu", "1")
	 f.message = f.message.." SMTU IPv6 set to "..value.."."
      else
	 os.execute("echo 0 > /proc/softwire/overrideipv6smtu") 
	 os.execute("uci set proc_vars.procentities.overrideipv6smtu=0")
	 -- luci.fs.writefile("/proc/softwire/overrideipv6smtu","0")
	 -- luci.model.uci.cursor():set("proc_vars", "procentities", "overrideipv6smtu", "0")
	 f.message = f.message.." SMTU IPv6 set to 0 (system will decide)."
      end
   end
   
   function dfBit.write(self, section, value)
      if value=="1" then
	 os.execute("echo 1 > /proc/softwire/toggle")
	 os.execute("uci set proc_vars.procentities.toggle=1")

	 -- luci.fs.writefile("/proc/softwire/toggle","1")
	 -- luci.model.uci.cursor():set("proc_vars", "procentities", "toggle", "1")
	 f.message = f.message.." DF Bit Set."
      else
	 os.execute("echo 0 > /proc/softwire/toggle")
	 os.execute("uci set proc_vars.procentities.toggle=0")

	 -- luci.fs.writefile("/proc/softwire/toggle","0")
	 -- luci.model.uci.cursor():set("proc_vars", "procentities", "toggle", "0")
	 f.message = f.message.." DF Bit reset."
      end
   end

   function smtu6.validate(self, value)
      if tonumber(value)==nil then
	 value="0"
      end
      if value=="0" then
	 return "0"
      end
      if tonumber(value)<1280 then
	 return "1280"
      end
      if tonumber(value)>65535 then
	 return "65535"
      end
      return tonumber(value)
   end

   dfBit.default = getDF()
   smtu6.value = getSMTU6()

end

return f
