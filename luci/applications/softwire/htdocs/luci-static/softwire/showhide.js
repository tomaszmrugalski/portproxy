
window.onload=showhide_init();

function showhide_init()
{
   var mode = document.getElementById("cbid.mantun.general.DsliteAuto");
   var swMode = document.getElementById("cbid.mantun.general.SoftwireValueAuto");
   var dsMode = document.getElementById("cbid.mantun.general.DsliteValueAuto");

   mode.onchange=new Function("showhide(this)");
   swMode.onchange=new Function("showhide(this)");
   dsMode.onchange=new Function("showhide(this)");

   showhide("");
}

function showhide(id)
{
   // cbi_d_update(id.id);

   var mode = document.getElementById("cbid.mantun.general.DsliteAuto").value;
   var swMode = document.getElementById("cbid.mantun.general.SoftwireValueAuto").value;
   var dsMode = document.getElementById("cbid.mantun.general.DsliteValueAuto").value;

   var xl2tp = false;
   var xl2tp_static = false;
   var overridetunnel = false;
   var mantun = false;
   

   if (mode==0 && swMode==0)
   {
     // alert("Softwire, DHCPv4");
     xl2tp = true;
   } else
   if (mode==0 && swMode==1)
   {
     // alert("Softwire, manual");
     xl2tp_static = true;
   } else
   if (mode==1 && dsMode==0)
   {
     // alert("DS-Lite, DHCPv6");
     overridetunnel = true;
   } else
   if (mode==1 && dsMode==1)
   {
     // alert("DS-Lite, DHCPv4");
     mantun = true;
   } else
   if (mode==2)
   {
     // alert("Tunnel disabled");
   }

   showhide_map("cbi-xl2tp",xl2tp);
   showhide_map("cbi-xl2tp_static", xl2tp_static);
   showhide_map("cbi-overridetunnel", overridetunnel);
   showhide_map("cbi-mantun2", mantun);
}

// show or hide specified element (a map section)
function showhide_map(id, status)
{
  var obj = document.getElementById(id);
  if (status==true)
     obj.style.display = '';
  else
     obj.style.display = 'none';
}
