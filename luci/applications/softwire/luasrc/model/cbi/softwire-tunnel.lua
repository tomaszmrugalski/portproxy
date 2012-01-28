-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

--[[
LuCI - Lua Configuration Interface
]]--

require "luci.ip"

local returnForms = {}

mantun = Map("mantun", "Softwire configuration", "This page allows tunnel configuration. " ..
             "It is a functional equivalent of ipv6tunnel X-Wrt based configuration") 
             -- "name of the cfg file", "page title", "description"

m = mantun:section(NamedSection, "general", "general", "General tunnel configuration",
		"This section defines fundamental tunnel configuration modes. Those changes " ..
		"are saved in mantun configuration file. After you change anything is " ..
		"this section, make sure that you hit SAVE button to"..
		" display relevant part of the configuration page.")

Dsliteauto = m:option(ListValue,"DsliteAuto", "Tunnel type",
		      "This parameter defines how tunnel should be configured. "..
		      "There are 3 possible modes: Softwire, DS-Lite or Disabled. ")
Dsliteauto:value("0","Softwire")
Dsliteauto:value("1","DS-Lite")
Dsliteauto:value("2","Disable")
--Dsliteauto.default = "DS-Lite"
Dsliteauto.rmempty = false


SoftwireValueAuto = m:option(ListValue,"SoftwireValueAuto", "Softwire configuration mode",
                             "This parameter defines how Softwire tunnel should be established."..
			     "Possible modes are DHCP (automatic) or Static (manual).")
SoftwireValueAuto:value("0","DHCP")
SoftwireValueAuto:value("1","static")
--SoftwireValueAuto:depends("DsliteAuto","0")
SoftwireValueAuto.rmempty = false


DsliteValueAuto = m:option(ListValue,"DsliteValueAuto", "DS-Lite configuration mode",
                           "This parameter defines how DS-Lite tunnel should be established."..
			   "Possible modes are DHCP (automatic) or Static (manual).")
DsliteValueAuto:value("0","DHCP")
DsliteValueAuto:value("1","static")
--DsliteValueAuto:depends("DsliteAuto","1")
DsliteValueAuto.rmempty = false

table.insert(returnForms, mantun) -- make sure this form is returned

-----------------------------------------------------------------------
-- SOFTWIRE -----------------------------------------------------------
-----------------------------------------------------------------------
--if tunnelMode() == "0" then
--   if softwireMode() == "0" then 

      -- --- softwire, DHCPv4 ---

      xl2tp = Map("xl2tp", "Automatic Softwire Configuration",
		  "Following section defines softwire configuration running "..
                  "in automatic mode. Parameters will be stored in "..
		  "/etc/config/xl2tp configuration file.")

      l2tpSection = xl2tp:section(NamedSection, "general", "xl2tp", "L2TP configuration")

      l2tpauthenabled = l2tpSection:option(ListValue, "l2tpauthenabled", "L2TP Authentication",
					   "This parameter defines, if L2TP tunnel should be authenticated")
      l2tpauthenabled:value("1","Enable")
      l2tpauthenabled:value("0","Disable")
      l2tpauthenabled.rmempty = false
      
      l2tpusername = l2tpSection:option(Value,"l2tpusername", "L2TP username","Please specify your L2TP login (username) here.")
      l2tpusername:depends("l2tpauthenabled","1")
      l2tpusername.rmempty = false
      
      l2tpsecret = l2tpSection:option(Value,"l2tpsecret", "L2TP password","Please specify your L2TP secret (password) here.")
      l2tpsecret:depends("l2tpauthenabled","1")
      l2tpsecret.rmempty = false
      
      pppSection = xl2tp:section(NamedSection, "general", "general", "PPP configuration")
      
      pppauthenabled = pppSection:option(ListValue, "pppauthenabled", "PPP Authentication",
					 "This parameter defines, if PPP should be authenticated")
      pppauthenabled:value("1","Enable")
      pppauthenabled:value("0","Disable")
      pppauthenabled.rmempty = false
      
      pppusername = pppSection:option(Value,"pppusername", "PPP username","Please specify your PPP login (username) here.")
      pppusername:depends("pppauthenabled","1")
      pppusername.rmempty = false
      
      pppsecret = pppSection:option(Value,"pppsecret", "PPP password","Please specify your PPP secret (password) here.")
      pppsecret:depends("pppauthenabled","1")
      pppsecret.rmempty = false
      
      table.insert(returnForms, xl2tp) -- make sure this form is returned

--   else -- softwire == 1

      -- --- softwire, static ---
      xl2tp_static = Map("xl2tp_static", "Static Softwire configuration",
                         "Following section defines Sotfwire configuration running"..
			 " in manual (static) mode. Parameters will be stored "..
                         "in /etc/config/xl2tp_static configuration file.")

      swSection = xl2tp_static:section(NamedSection, "general", "xl2tp", "Manual Softwire Server Address")

      lnsaddress_static = swSection:option(Value,"lns", "Softwire Server (IPv4)", "Specified IPv4 address of Softwire server. Switch configuration to "..
				    " DHCPv4 if you want to obtain this information automatically.")

      l2tpSection_static = xl2tp_static:section(NamedSection, "general", "xl2tp", "L2TP configuration")


      l2tpauthenabled_static = l2tpSection_static:option(ListValue, "l2tpauthenabled", "L2TP Authentication",
					   "This parameter defines, if L2TP tunnel should be authenticated")
      l2tpauthenabled_static:value("1","Enable")
      l2tpauthenabled_static:value("0","Disable")
      l2tpauthenabled_static.rmempty = false
      
      l2tpusername_static = l2tpSection_static:option(Value,"l2tpusername", "L2TP username","Please specify your L2TP login (username) here.")
      l2tpusername_static:depends("l2tpauthenabled","1")
      l2tpusername_static.rmempty = false
      
      l2tpsecret_static = l2tpSection_static:option(Value,"l2tpsecret", "L2TP password","Please specify your L2TP secret (password) here.")
      l2tpsecret_static:depends("l2tpauthenabled","1")
      l2tpsecret_static.rmempty = false
      
      pppSection_static = xl2tp_static:section(NamedSection, "general", "general", "PPP configuration")
      
      pppauthenabled_static = pppSection_static:option(ListValue, "pppauthenabled", "PPP Authentication",
					 "This parameter defines, if PPP should be authenticated")
      pppauthenabled_static:value("1","Enable")
      pppauthenabled_static:value("0","Disable")
      
      pppusername_static = pppSection_static:option(Value,"pppusername", "PPP username","Please specify your PPP login (username) here.")
      pppusername_static:depends("pppauthenabled","1")
      pppusername_static.rmempty = false
      
      pppsecret_static = pppSection_static:option(Value,"pppsecret", "PPP password","Please specify your PPP secret (password) here.")
      pppsecret_static:depends("pppauthenabled","1")
      pppsecret_static.rmempty = false

      table.insert(returnForms, xl2tp_static) -- make sure this form is returned
--   end
--end -- tunnelMode()==0 (softwire)

-----------------------------------------------------------------------
-- DS-LITE ------------------------------------------------------------
-----------------------------------------------------------------------
--if tunnelMode() == "1" then

   --if dsliteMode()=="0" then
   -- --- DS-Lite, DHCPv6 ---
      overridetunnel = Map("overridetunnel", "DS-Lite automatic configuration",
			   "Following section defines DS-Lite configuration "..
			   "running in automatic (DHCP) mode. Parameters "..
                           "will be stored in /etc/config/overridetunnel configuration file.")
      overSection = overridetunnel:section(NamedSection, "general", "overridetunnel", 
					   "DS-Lite parameters","IPv6 parameters are specified here.")

      dhcpv6_name_servers = overSection:option(Value,"dhcp6_name_servers", "Primary DNS (IPv6)",
					       "e.g. 2001::2")
      dhcpv6_name_servers.rmempty = false

      ip6gateway = overSection:option(Value, "ip6gateway", "IPv6 WAN default gateway", "e.g. 2001::1")
      ip6gateway.rmempty = false

      aftr6addr = overSection:option(Value, "remoteendpointipv6addr", "AFTR IPv6 Address")
      aftr6addr.rmempty = false

      table.insert(returnForms, overridetunnel)
   --else
   -- --- DS-Lite, DHCPv6 ---
      mantunDsLite = Map("mantun", "DS-Lite manual configuration",
			 "This section defines DS-Lite configuration "..
			 "running in static (manual) mode. Parameters "..
			 "will be stored in /etc/config/mantun configuration file.")
      mantunDsLite.template="softwire/map"

      dsStaticSection = mantunDsLite:section(NamedSection, "general", "general", 
					     "DS-Lite parameters","Those parameters define static (manual) configuration of DS-Lite tunnel.")
      localip6addr  = dsStaticSection:option(Value,"Localip6addr", "B4 WAN IPv6 Address", "e.g. 2002::1/64")
      localip6addr.rmempty = false
      remoteip6addr = dsStaticSection:option(Value,"Remoteip6addr", "AFTR IPv6 Address", "e.g. 2002::8")
      remoteip6addr.rmempty = false
      lanipv6addr   = dsStaticSection:option(Value,"LANip6addr", "B4 LAN IPv6 Address", "e.g. 2000::1")
      lanipv6addr.rmempty = false
      pd            = dsStaticSection:option(Value,"PrefixDelegation", "IPv6 Prefix Delegation")
      pd.rmempty = false
      ip6gw         = dsStaticSection:option(Value,"Ipv6gate", "IPv6 default gateway")
      ip6gw.rmempty = false
      ip6dns        = dsStaticSection:option(Value,"Ipv6dns", "Primary DNS")
      ip6dns.rmempty = false

      table.insert(returnForms, mantunDsLite)
      --end (DS-Lite, DHCPv6)

--end -- tunnelMode() == "1" (DS-Lite)

-----------------------------------------------------------------------
-- DISABLED -----------------------------------------------------------
-----------------------------------------------------------------------


return unpack(returnForms)
