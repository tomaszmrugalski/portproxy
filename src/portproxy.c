#include <stdio.h>
#include <stdlib.h> /* exit */
#include <string.h> /* strcasecmp */
#include <arpa/inet.h> /* inet_ntop */
#include <ctype.h>
#include <unistd.h>
#include "Portable.h"
#include "portproxy.h"

context_t *ctx = 0;

extern volatile int Shutdown;

void help_print()
{
    printf(" portproxy %s\n"
           "----------------------\n"
           "\n"
           " --lan-iface name- defines LAN interface, (%s) by default\n"
           " --wan-iface name- defines WAN interface, (%s) by default\n"
           " -l <IPv4addr>   - defines B4 LAN IPv4 address (e.g. -l 192.168.1.1)\n"
           "                   Portproxy will listen for client's messages on this address\n"
           " -w <IPv4addr>   - defines B4 WAN IPv4 address (e.g. -w 192.0.0.2)\n"
           "                   Portproxy will use this address to forward messages to the NAT router\n"
           " -a <IPv4addr>   - defines AFTR IPv4 address (e.g. -w 192.0.0.1)\n"
           "                   Portproxy will forward incoming messages to this IPv4 address.\n"
           " --nat-pmp X     - Enables(X=1) or Disables (X=0) nat-pmp protocol (default = enabled)\n"
           " --nat-pnp-ext X - Enables (X=1) or disables (X=0) extended nat-pmp\n"
           "                   (NAT-PMP messages are extended with extra 12 bytes that\n"
           "                   hold extra information (as defined in AFTR-1.0b1 doc)\n"
           " --upnp-udp X    - enables(X=1) or disables (X=0) uPNP UDP support (default = enabled)\n"
           " --upnp-tcp X    - enables(X=1) or disables (X=0) uPNP TCP support (default = enabled)\n"
           " -g              - become a daemon\n"
           " --persistent    - be persistent. Start even when specified addresses or interface\n"
           "                   are not available yet.\n"
           " -v              - increases verbosity level to 2 (more verbose than default 1)\n"
           "                   (use twice to further increase verbosity)\n"
           "\n"
           "Following options are used for extended NAT-PMP solution only.\n"
           "Please note that extended NAT-PMP and NAT-PMP are mutually exclusive.\n"
           " -b <IPv6 addr>  - IPv6 address on the WAN side. Use as source address for\n"
           "                   extended NAT-PMP messages, forwarded to AFTR\n"
           " -s <IPv6 addr>  - AFTR IPv6 address. Extended NAT-PMP messages will be\n"
           "                   forwarded to this address.\n"
           "Following options are for testing/debugging purposes only:\n"
           " --nat-pmp-port X   - specifies UDP port for NAT-PMP protocol (default = %d)\n"
           " --upnp-udp-port X  - specifies UDP port for uPNP protocol (default = %d)\n"
           " --upnp-tcp-port1 X - specifies TCP port1 for uPNP protocol (default = %d)\n"
           " --upnp-tcp-port2 X - specifies TCP port2 for uPNP protocol (default = %d)\n"
           "\n",
           PORTPROXY_VERSION,
           PORTPROXY_DEFAULT_LAN_IFACE, PORTPROXY_DEFAULT_WAN_IFACE,
           PORTPROXY_NATPMP_UDP_PORT, PORTPROXY_UPNP_UDP_PORT,
           PORTPROXY_UPNP_TCP_PORT1, PORTPROXY_UPNP_TCP_PORT2);
}

struct in_addr default_lan_ip(context_t * ctx)
{
    return iface_ip(ctx, ctx->lan);
}

struct in_addr default_wan_ip(context_t * ctx)
{
    return iface_ip(ctx, ctx->wan);
}

struct in_addr default_router_ip(context_t *ctx)
{
    struct in_addr x = {0};
    long mask, addr;
    char buf[256];
    int line = 0;
    FILE * f;
    char * c;
    f = fopen("/proc/net/route", "r");
    if(!f)
        return x; /* failed to detect default GW */
    while(fgets(buf, sizeof(buf), f)) {
        if(line > 0) {
            c = buf;
            while(*c && !isspace(*c))
                c++; /* ignore interface name */
            while(*c && isspace(*c))
                c++; /* ignore space after it */
            if(sscanf(c, "%lx%lx", &mask, &addr)==2) {
                if(mask == 0) { /* default */
                    fclose(f);
                    x.s_addr = addr;
                    return x; /* found */
                }
            }
        }
        line++;
    }
    if(f)
        fclose(f);
    return x;
}

struct in_addr iface_ip(context_t * ctx, const char *name)

{
    struct in_addr x = {0};
    struct iface_t * iface = ctx->ifaces;
    while (iface)
    {
        if (!strcmp(iface->name,name))
            break;
        iface = iface->next;
    }
    if (iface)
    {
        if (iface->ipaddrcount)
        {
            x.s_addr = iface->ipaddr[0];
            return x;
        }
    }
    return x;
}

void ctx_defaults(context_t *ctx)
{
    sprintf(ctx->lan, PORTPROXY_DEFAULT_LAN_IFACE);
    sprintf(ctx->wan, PORTPROXY_DEFAULT_WAN_IFACE);

    ctx->lanIP = default_lan_ip(ctx);
    ctx->wanIP = default_wan_ip(ctx);
    ctx->aftrIPv4 = default_router_ip(ctx); // gateway

    ctx->natpmp_udp_port = PORTPROXY_NATPMP_UDP_PORT;
    ctx->upnp_udp_port   = PORTPROXY_UPNP_UDP_PORT;
    ctx->upnp_tcp_port1  = PORTPROXY_UPNP_TCP_PORT1;
    ctx->upnp_tcp_port2  = PORTPROXY_UPNP_TCP_PORT2;
    ctx->shared_udp_sock = 1;

    ctx->fwd_tcp_port1 = PORTPROXY_UPNP_TCP_PORT1;
    ctx->fwd_tcp_port2 = PORTPROXY_UPNP_TCP_PORT2;

    ctx->enable_nat_pmp = 0;
    ctx->enable_nat_pmp_ext = 1;
    ctx->enable_upnp_udp = 0;
    ctx->enable_upnp_tcp = 0; /* TCP traffic forwarding is not required for now */

    ctx->loglevel = 0;

    ctx->bind_ext_nat_pmp_to_natpmp_port = 0;
    ctx->timeout = PORTPROXY_DEFAULT_TIMEOUT;

    ctx->daemon = 0;
}

/**
 * parses command line parameters
 *
 * @param ctx global context
 * @param argc
 * @param argv
 *
 * @return
 */
int cmd_line_parse(context_t * ctx, int argc, char *argv[])
{
    ctx_defaults(ctx);
    argc--; /* ignore app name */
    argv++;

    if (argc==0)
    {
        help_print();
        exit(0);
    }

    while (argc)
    {
        if (!strcasecmp(argv[0],"-h"))
        {
            help_print();
            exit(0);
        } else
        if (!strcasecmp(argv[0],"--persistent"))
        {
            ctx->persistent = 1;
        } else
        if (!strcasecmp(argv[0],"--lan-iface"))
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --lan-iface option requires extra parameter (LAN interface name).\n");
                return -1;
            }
            strncpy(ctx->lan, argv[0], MAX_IFNAME_LENGTH);
            ctx->lanIP = default_lan_ip(ctx);
        } else
        if (!strcasecmp(argv[0],"--wan-iface"))
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --wan-iface option requires extra parameter (WAN interface name).\n");
                return -1;
            }
            strncpy(ctx->wan, argv[0], MAX_IFNAME_LENGTH);
            ctx->wanIP = default_wan_ip(ctx);
        } else
        if (!strcasecmp(argv[0],"-l")) // B4 lanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: -l option requires extra parameter (an IPv4 address).\n");
                return -1;
            }
            inet_pton(AF_INET, argv[0], &ctx->lanIP);
        } else
        if (!strcasecmp(argv[0], "-a")) // AFTR address
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: -n option requires extra parameter (an IPv4 address).\n");
                return -1;
            }
            inet_pton(AF_INET, argv[0], &ctx->aftrIPv4);
        } else
        if (!strcasecmp(argv[0], "-w")) // B4 wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: -w option requires extra parameter (an IPv4 address).\n");
                return -1;
            }
            inet_pton(AF_INET, argv[0], &ctx->wanIP);
        } else

        if (!strcasecmp(argv[0], "-b")) // B4 wanIPv6
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: -b option requires extra parameter (an IPv6 address).\n");
                return -1;
            }
            if (!inet_pton(AF_INET6, argv[0], &ctx->wanIPv6))
            {
                Log(1,"Failed to parse %s as B4 WAN IPv6 address.\n", argv[0]);
                return -1;
            }
        } else

        if (!strcasecmp(argv[0], "-s")) // AFTR wanIPv6
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: -s option requires extra parameter (an IPv6 address).\n");
                return -1;
            }
            if (!inet_pton(AF_INET6, argv[0], &ctx->aftrIPv6))
            {
                Log(1,"Failed to parse %s as AFTR IPv6 address.", argv[0]);
                return -1;
            }
        } else

        /* enable/disable */
        if (!strcasecmp(argv[0],"--nat-pmp")) /* NAT-PMP protocol */
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --nat-pmp option requires extra parameter (1-enable, 0-disable).\n");
                return -1;
            }
            ctx->enable_nat_pmp = atoi(argv[0]);
        } else
        if (!strcasecmp(argv[0],"--nat-pmp-ext")) /* NAT-PMP protocol + SHIM */
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --nat-pmp option requires extra parameter (1-enable, 0-disable).\n");
                return -1;
            }
            ctx->enable_nat_pmp_ext = atoi(argv[0]);
        } else
        if (!strcasecmp(argv[0],"--upnp-udp")) /* uPNP (UDP) */
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --upnp-udp option requires extra parameter (1-enable, 0-disable).\n");
                return -1;
            }
            ctx->enable_upnp_udp = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"--upnp-tcp")) /* uPNP (TCP) */
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --upnp-tcp option requires extra parameter (1-enable, 0-disable).\n");
                return -1;
            }
            ctx->enable_upnp_tcp = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"--timeout")) /* mapping timeout */
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --timeout option requires extra parameter (mapping timeout, in seconds).\n");
                return -1;
            }
            ctx->timeout = atoi(argv[0]);
        } else

        /* ports */
        if (!strcasecmp(argv[0],"--nat-pmp-port")) // wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --nat-pmp-port requires extra parameter (port number).\n");
                return -1;
            }
            ctx->natpmp_udp_port = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"--upnp-udp-port")) // wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --upnp-udp-port requires extra parameter (port number).\n");
                return -1;
            }
            ctx->upnp_udp_port = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"--upnp-tcp-port1")) // wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --upnp-tcp-port1 option requires extra parameter (port number).\n");
                return -1;
            }
            ctx->upnp_tcp_port1 = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"--upnp-tcp-port2")) // wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --upnp-tcp-port2 option requires extra parameter (port number).\n");
                return -1;
            }
            ctx->upnp_tcp_port2 = atoi(argv[0]);
        } else
        if (!strcasecmp(argv[0],"--shared-udp-sock")) // wanIP
        {
            argc--;
            argv++;
            if (!argc)
            {
                Log(1,"Error: --shared-udp-sock requires extra parameter (0-disabled;1-enabled).\n");
                return -1;
            }
            ctx->shared_udp_sock = atoi(argv[0]);
        } else

        if (!strcasecmp(argv[0],"-v")) // verbose level
        {
            ctx->loglevel++;
        } else
        if (!strcasecmp(argv[0],"-g")) // verbose level
        {
            ctx->daemon = 1;
        } else
        {
            Log(1,"Error: Invalid option: %s\n", argv[0]);
            return -1;
        }

        argc--;
        argv++;
    }

    LogLevel(ctx->loglevel);
    Log(1,"%s\n", (PORTPROXY_COPYRIGHT1));
    Log(1,"%s\n", (PORTPROXY_COPYRIGHT2));
    Log(1,"%s\n", (PORTPROXY_COPYRIGHT3));
    Log(1,"%s\n", (PORTPROXY_COPYRIGHT4));

    context_print(ctx);

    if (ctx->daemon)
        daemon(0,0);

    return 0; // all ok
}

void help()
{
    exit(0);
}

/**
 * opens required sockets
 *
 * @param ctx
 *
 * @return 0 if successful, -1 when error is detected
 */
int sockets_bind(context_t * ctx)
{
    int result;
    int port;
    socket_t * sock;

    if (ctx->enable_upnp_udp)
    {
        int lan_sock, wan_sock;
        struct in_addr anyaddr;
        anyaddr.s_addr = htonl(INADDR_ANY);
        Log(1,"Enabling uPNP (UDP)\n");

        /* LAN side socket */
        lan_sock = socket_add(ctx, ctx->lan, anyaddr, ctx->upnp_udp_port, IPPROTO_UDP,
                              1/*don't bind to iface*/, 1/* allow reuse*/ );
        port = sock_get_port(lan_sock);
        Log(2,"UDP(SSDP) socket created: sockid=%d, port=%d\n", lan_sock, port);
        if (lan_sock<0)
            return -1;
        //struct in_addr mcast;
        //inet_pton(AF_INET, SSDP_MCAST_ADDRESS, &mcast);
        sock = socket_find(ctx, lan_sock);
        result = socket_mcast_join(sock, mcast, ctx->lanIP);
        if (result<0)
            return -1;
        ctx->lan_ssdp_sock = lan_sock;


        wan_sock = socket_add(ctx, ctx->wan, anyaddr, ctx->upnp_udp_port, IPPROTO_UDP,
                              1/*don't bind to iface*/, 1 /*allow reuse*/);
        port = sock_get_port(wan_sock);
        Log(2,"UDP(SSDP) socket created: sockid=%d, port=%d\n", wan_sock, port);
        if (wan_sock<0)
            return -1;
        sock = socket_find(ctx, wan_sock);
        result = socket_mcast_join(sock, mcast, ctx->wanIP);
        if (result<0)
            return -1;
        ctx->wan_ssdp_sock = wan_sock;

        socket_t * lans = socket_find(ctx, lan_sock);
        socket_t * wans = socket_find(ctx, wan_sock);

        lans->peer = wans->fd;
        wans->peer = lans->fd;

        Log(2, "Sockets %d(LAN:%s) and %d(WAN:%s) paired.\n", lans->fd, ctx->lan, wans->fd, ctx->wan);
    }

    if (ctx->enable_nat_pmp_ext)
    {
        /* open IPv4 socket */
        Log(1,"Enabling NAT-PMP extended.\n");
        result = socket_add(ctx, ctx->lan, ctx->lanIP, ctx->natpmp_udp_port, IPPROTO_UDP, 0/*don't bind to iface*/, 0/*don't allow reuse*/);
        port = sock_get_port(result);
        Log(2,"UDP: sockid=%d, port=%d\n", result, port);
        if (result<0)
            return -1;
        ctx->nat_pmp_lan_sock = result;

        /* open IPv6 socket */
        struct in6_addr bindme;
        bindme = ctx->wanIPv6;
        if (!memcmp(&bindme,&in6addr_any,16))
        {
            Log(1,"B4 WAN IPv6 address not defined. Detecting from %s interface.\n", ctx->wan);
            if (iface_ip6_get(ctx->wan, (char*)&bindme)<0)
            {
                Log(1,"Failed to detect global IPv6 address on %s interface.\n", ctx->wan);
                return -1;
            }
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, (void*)&bindme, buf, INET6_ADDRSTRLEN);
            Log(1,"Detected %s IPv6 address on %s interface.\n", buf, ctx->wan);
        }

        result = socket6_add(ctx, ctx->wan, bindme, ctx->natpmp_udp_port, IPPROTO_UDP, 0, 0);
        port = sock_get_port(result);
        ctx->nat_pmp_ext_sock = result;
        Log(2,"UDP6: sockid=%d, port=%d\n", result, port);
        if (result<0)
            return -1;
    }

    if (ctx->enable_nat_pmp)
    {
        Log(1,"Enabling NAT-PMP\n");
        result = socket_add(ctx, ctx->lan, ctx->lanIP, ctx->natpmp_udp_port, IPPROTO_UDP,
                            0/*don't bind to iface*/, 0/*don't allow reuse*/);
        port = sock_get_port(result);
        Log(2,"UDP: sockid=%d, port=%d\n", result, port);
        if (result<0)
            return -1;
    }

    if (ctx->enable_upnp_tcp)
    {
        Log(1,"Enabling uPNP (TCP)\n");
        result = socket_add(ctx, ctx->lan, ctx->lanIP, ctx->upnp_tcp_port1, IPPROTO_TCP,
                            0/*don't bind to iface*/, 0/*don't allow reuse*/);
        port = sock_get_port(result);
        Log(2,"TCP: sockid=%d, port=%d\n", result, port);
        if (result<0)
            return -1;

        result = socket_add(ctx, ctx->lan, ctx->lanIP, ctx->upnp_tcp_port2, IPPROTO_TCP,
                            0/*don't bind to iface*/, 0/*don't allow reuse*/);
        port = sock_get_port(result);
        Log(2,"TCP: sockid=%d, port=%d\n", result, port);
        if (result<0)
            return -1;
    }

    if (ctx->shared_udp_sock && (ctx->enable_upnp_udp || ctx->enable_nat_pmp))
    {
        Log(1,"Opening single UDP transmission socket (random port)\n");
        result = socket_add(ctx, ctx->wan, ctx->wanIP, 0, IPPROTO_UDP, 0/*don't bind to iface*/, 0/*don't allow reuse*/);
        port = sock_get_port(result);
        Log(2,"UDP: sockid=%d, port=%d\n", result, port);
        ctx->shared_udp_sock_fd = result;
        if (result<0)
            return -1;
        /* TCP sockets will be opened as needed */
    }

    return 1;
}

int sanity_check(context_t * ctx)
{
    if (!ctx->enable_nat_pmp && !ctx->enable_upnp_tcp && !ctx->enable_upnp_udp && !ctx->enable_nat_pmp_ext)
    {
        Log(1,"Error: all protocols (NAT-PMP, uPNP) disabled. No traffic left.\n");
        return 0;
    }

    if (ctx->enable_nat_pmp && ctx->enable_nat_pmp_ext)
    {
        Log(1,"Error: enable NAT-PMP and enable NAT-PMP (extended) are mutually exclusive. Can't enable both\n");
        return -1;
    }

    return 1;
}

int main(int argc, char *argv[])

{
    int status;

    /* init */
    framework_init(); /* install signal handlers */

    ctx = ctx_create();

    ctx->ifaces = if_list_get();

    if (cmd_line_parse(ctx, argc, argv)<0)
    {
        Log(1, "Command-line parsing failed.\n");
        help_print();
        return -1;
    }

    if (!sanity_check(ctx))
    {
        Log(1, "Error: Unable to continue, sanity check failed.\n");
        return -1;
    }

    // bind sockets
    if (!ctx->persistent)
    {
        if (sockets_bind(ctx)<0)
        {
            Log(1,"Error: Unable to bind sockets.\n");
            return -1;
        }
    } else
    {
        do
        {
            status = sockets_bind(ctx);
            if (status<0)
            {
                Log(1,"Persistent mode: binding failed. Retrying in 10 seconds.\n");
                sockets_close(ctx);
                sleep(10);
            } else
            {
                Log(1,"Persistent mode: binding successful. Starting operation\n");
                break;
            }
        } while (Shutdown!=1);

    }

    run(ctx);

    sockets_close(ctx);

    ctx_release(&ctx);

    return 0;
}
