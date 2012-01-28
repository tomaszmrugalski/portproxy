-- author: tomek mrugalski <thomson(at)klub(dot)com(dot)pl>
-- license: GNU GPLv2

require("luci.sys")
f = Map("softwire", "Portproxy - permanent configuration", 
  "Portproxy is an automatic port forwarding proxy. It supports NAT-PMP and "..
  "UPnP protocols. This page provides interface for changing permanent" ..
  "portproxy configuration. Changes made here will be preserved after " ..
  "device reboot.")

s = f:section(TypedSection,"portproxy","Features")
s.anonymous = true

natpmp = s:option(ListValue,"natpmp", "NAT-PMP", 
  "Classic NAT-PMP. Incoming NAT-PMP requests will be forwarded as IPv4 "..
  "packets (or IPv4-over-IPv6, if DS-lite tunnel is established). Note that "..
  "classic and extended NAT-PMP support is mutually exclusive.")
natpmp:value("1","enabled")
natpmp:value("0","disabled")

extnatpmp = s:option(ListValue,"natpmpext", "Extended NAT-PMP", 
  "Extended NAT-PMP. Incoming NAT-PMP requests will be forwarded as IPv6 "..
  "packets with extra information (shim) added. Note that classic and "..
  "extended NAT-PMP support is mutually exclusive.")
extnatpmp:value("1","enabled")
extnatpmp:value("0","disabled")

upnpudp = s:option(ListValue,"upnpudp","UPnP (UDP traffic)", 
  "UDP traffic of UPnP protocol. Defines if portproxy should forward UDP "..
  "queries between LAN and WAN iterfaces. This also covers retransmitting "..
  "received multicast annoucenments/discovery attempts. This support may "..
  "be considered ALG functionality.")
upnpudp:value("1","enabled")
upnpudp:value("0","disabled")

upnptcp = s:option(ListValue,"upnptcp","UPnP (TCP traffic)", 
  "TCP traffic of UPnP protocol. After dicovery is complete, clients "..
  "(control points) are able to communicate with IGD (AFTR) directly. "..
  "With this flag enabled, B4 (this box) will act as a relay. Clients "..
  "will estabilish TCP connections with B4 and B4 will establish second "..
  "TCP connection to AFTR. This functionality is considered experimental "..
  "and is not expected to be actively used.")
upnptcp:value("1","enabled")
upnptcp:value("0","disabled")

-- ------------- EXTENDED NAT-PMP configuration -----------------------------
s = f:section(TypedSection,"portproxy","Extended NAT-PMP parameters")
s.anonymous = true

aftripv6 = s:option(Value,"aftripv6","AFTR IPv6 address",
  "This address is used in extended NAT-PMP support only. Incoming "..
  "client queries will be redirected to this IPv6 address. ('-s' "..
  "command-line option equivalent). This value is ignored if extended "..
  "NAT-PMP is disabled. Use :: if you want portproxy to autodetect it."..
  "If autodetection is used, make sure that proper WAN interface is "..
  "specified (see --wan-iface option).")
aftripv6.rmempty = false

wanipv6 = s:option(Value,"wanipv6","B4 WAN IPv6 address", 
  "This address is used in extended NAT-PMP support only. " ..
  "Forwarded queries, sent do AFTR will be sent from this address. " ..
  "('-b' command-line option equivalent). This value is ignored if " ..
  "extended NAT-PMP is disabled.")
wanipv6.rmempty = false

persistent = s:option(ListValue, "persistent", "Persistent",
  "When enabled, option allows portproxy to be started, before actual "..
  "tunnel is established. If enabled, portproxy will not exit if "..
  "specified interface or IPv6 address is not present, but rather "..
  "will keep trying to bind sockets with 10 seconds intervals.")
persistent:value("0","disabled (exit if no addr is available)")
persistent:value("1","enabled (keep trying)")

s = f:section(TypedSection,"portproxy","NAT-PMP/UPnP parameters")
s.anonymous = true

lanipv4 = s:option(Value,"lanipv4","B4: LAN IPv4 address", 
  "This address is used on LAN interface to listen to incoming "..
  "client's requests. Default value is 192.168.1.1 ('-l' "..
  "command-line equivalent)")
lanipv4.rmempty = false

wanipv4 = s:option(Value,"wanipv4","B4: WAN IPv4 address", 
  "This address is used on WAN interface to communicate with AFTR" ..
  " in NAT-PMP and UPnP modes. Default value is 192.0.0.2 ('-w' " ..
  "command-line equivalent)")
wanipv4.rmempty = false

aftripv4 = s:option(Value,"aftripv4","AFTR IPv4 address", 
  "This address is used to send forwarded requests to. " ..
  "Default value is 192.0.0.1 ('-a' command-line equivalent).")
aftripv4.rmempty = false

laniface = s:option(Value,"laniface", "LAN interface", 
  "This is the interface used for communication with clients. "..
  "Default value is 'br-lan'. ('--lan-iface' command-line equivalent).")
laniface.rmempty = false

waniface = s:option(Value,"waniface", "WAN interface", 
  "This is the interface used for communication with AFTR "..
  "('softwire' or 'eth0.1' by default)")
waniface.rmempty = false

return f
