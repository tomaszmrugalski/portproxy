module("luci.controller.softwire.portproxy", package.seeall)

function index()

    local page = entry({"admin","softwire"}, template("softwire/overview"), "Softwire", 80)
    page.dependent=false

    local page = entry({"admin", "softwire", "tunnel"}, cbi("softwire-tunnel"), "Tunnel configuration", 10)
    page.dependent=false

    local page = entry({"admin", "softwire", "tunnel-frag"}, cbi("softwire-misc"), "Tunnel Fragmentation", 15)
    page.dependent=false

    local page = entry({"admin", "softwire", "tunnel-adv"}, cbi("softwire-adv"), "Advanced Settings and status", 20)
    page.dependent=false

    local page = entry({"admin","softwire", "portfwd"}, cbi("softwire-portfwd"), "Manual Port Forwarding", 30)
    page.dependent=false

    local page = entry({"admin","softwire", "portfwd-limits"}, cbi("softwire-portfwd-limits"), "Manual Port Forwarding Limits", 40)
    page.dependent=false

    local page = entry({"admin","softwire", "portproxy-perm"}, cbi("softwire-portproxy-perm"), "Portproxy - permanent configuration", 50)
    page.dependent=false

    local page = entry({"admin","softwire", "portproxy"}, cbi("softwire-portproxy"), "Portproxy - temporary configuration", 60)
    page.dependent=false

        
--[[
    local page = entry({"admin","softwire", "test1"}, call("action_test1"), "Script test", 10)
    page.dependent=false

    local page=entry({"admin","softwire", "test2"}, template("softwire/portproxy"), "Template test", 20)
    page.dependent=false

    local page=entry({"admin","softwire", "test3"}, template("softwire/portproxy"), "Form test", 30)
    page.dependent=false
    
    local page = entry({"admin","softwire", "template"}, template("softwire/portproxy"), "Softwire cfg (Template)", 40)
    page.dependent=false ]]--

    
end
     
function action_test1()
     luci.http.prepare_content("text/plain")
     luci.http.write("Portproxy configuration.")
end

