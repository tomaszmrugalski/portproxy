
#include <stdio.h>
#include <arpa/inet.h> /* inet_ntop */
#include <sys/select.h> /* select, pselect, FD_CLR, FD_ISSET, FD_SET, FD_ZERO */
#include <stdlib.h> /* exit */
#include <string.h> /* memset */
#include <unistd.h> /* close */
#include <signal.h> /* signal() */
#include <time.h>   /* time() */
#include <netinet/in.h>

#include "Portable.h"
#include "portproxy.h"

int log_level = 1;
volatile int Shutdown = 0;

struct in_addr mcast; 

void signal_handler(int n) {
    Log(1, "Signal %d received. Shutting down.\n", n);
    Shutdown=1;
}

int framework_init()
{
    mcast.s_addr = htonl(4211081199u); /* 239.255.255.250 */

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    return 0;
}

void Log(int level, const char* format, ... ) {
    if (level > log_level)
        return;

    va_list args;
    va_start( args, format );
    vprintf(format, args );
    va_end( args );
}

void LogLevel(int level)
{
    log_level = level;
}

void iface_print(const struct iface_t * iface)
{
    char tmp[INET_ADDRSTRLEN];
    int i;
    char mac[18];
    memset(mac,0, 18);
    if (iface->maclen>=6)
        sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                iface->mac[0], iface->mac[1], iface->mac[2],
                iface->mac[3], iface->mac[4], iface->mac[5]);

    Log(1, "  Interface %s, %d address(es), MAC=%s\n", iface->name, iface->ipaddrcount, mac);

    for (i=0; i<iface->ipaddrcount; i++)
    {
        inet_ntop(AF_INET, &iface->ipaddr[i], tmp, INET_ADDRSTRLEN);
        Log(2, "    IPv4=%s\n", tmp);
    }
}

void ifaces_print(struct iface_t *iface)
{
    // get interfaces
    while (iface)
    {
        iface_print(iface);
        iface = iface->next;
    }
}

context_t * ctx_create()
{
    context_t * ctx = malloc(sizeof(context_t));
    memset(ctx, 0, sizeof(context_t));
    return ctx;
}

void ctx_release(context_t ** ctx)
{
    struct iface_t * ifc = (*ctx)->ifaces;
    if_list_release( ifc );
    free(*ctx);
    *ctx = NULL;
}

void socket_add2(context_t * ctx, socket_t * sock)
{
    sock->next = ctx->sockets;
    ctx->sockets = sock;
}


/**
 *
 * @param ctx - Context (socket structure will be added hre)
 * @param ifname - Interface name (ignored for now)
 * @param addr - bind socket to this address
 * @param port - port
 * @param ipproto - protocol TCP or UDP
 *
 * @return socket FD (or -1 in case of error)
 */
int socket_add(context_t * ctx, const char * ifname,
               struct in_addr addr, int port, int ipproto, int bindiface, int reuse)
{
    socket_t * s;
    char proto[10];
    char addr_plain[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,(char*)&addr,addr_plain, INET_ADDRSTRLEN);
    switch (ipproto)
    {
    case IPPROTO_UDP:
        sprintf(proto, "UDP");
        break;
    case IPPROTO_TCP:
        sprintf(proto, "TCP");
        break;
    default:
        sprintf(proto, "unknown");
    }

    int result = sock_add(ifname, addr, port, ipproto, bindiface, reuse);
    Log(1,"Creating %s socket: addr=%s, port=%d on interface %s, result=%d\n",
        proto, addr_plain, port, ifname, result);
    if (result<0)
    {
        Log(1,"Error: Unable to create socket.\n");
        return -1;
    }
    s = (socket_t*) malloc(sizeof(socket_t));
    memset(s,0,sizeof(socket_t));
    s->fd = result;
    s->ipproto = ipproto;
    s->next = ctx->sockets;
    switch (ipproto)
    {
    case IPPROTO_UDP:
        s->state = SOCKET_TYPE_UDP;
        break;
    case IPPROTO_TCP:
        s->state = SOCKET_TYPE_TCP_LISTENING;
        break;
    default:
        Log(1,"Error: unknown socket type\n");
        exit(-1);

    }

    ctx->sockets = s;

    return result;
}

/**
 *
 * @param ctx - Context (socket structure will be added hre)
 * @param ifname - Interface name (ignored for now)
 * @param addr - bind socket to this address
 * @param port - port
 * @param ipproto - protocol TCP or UDP
 *
 * @return socket FD (or -1 in case of error)
 */
int socket6_add(context_t * ctx, const char * ifname,
               struct in6_addr addr, int port, int ipproto, int bindiface, int reuse)
{
    socket_t * s;
    char proto[10];
    char addr_plain[INET_ADDRSTRLEN];
    char tmp[64];
    inet_ntop(AF_INET6,(char*)&addr,addr_plain, INET_ADDRSTRLEN);
    switch (ipproto)
    {
    case IPPROTO_UDP:
        sprintf(proto, "UDP");
        break;
    case IPPROTO_TCP:
        sprintf(proto, "TCP");
        break;
    default:
        sprintf(proto, "unknown");
    }

    int result = socket(AF_INET6, SOCK_DGRAM, ipproto);
    Log(1,"Creating %s socket: addr=%s, port=%d on interface %s, result=%d\n",
        proto, addr_plain, port, ifname, result);
    if (result<0)
    {
        Log(1,"Error: IPv6 socket creation failed.\n");
        return -1;
    }

    s = (socket_t*) malloc(sizeof(socket_t));
    memset(s,0,sizeof(socket_t));
    s->fd = result;
    s->ipproto = ipproto;
    s->next = ctx->sockets;
    switch (ipproto)
    {
    case IPPROTO_UDP:
        s->state = SOCKET_TYPE_UDP6;
        break;
    case IPPROTO_TCP:
        s->state = SOCKET_TYPE_TCP_LISTENING;
        break;
    default:
        Log(1,"Error: unknown socket type\n");
        exit(-1);

    }
    ctx->sockets = s;

    struct sockaddr_in6 srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin6_family = AF_INET6;
    srv.sin6_addr = addr;
    if (ctx->bind_ext_nat_pmp_to_natpmp_port)
        srv.sin6_port = htons(ctx->natpmp_udp_port);
    if (bind(s->fd, (struct sockaddr *) &srv, sizeof(srv)) < 0)
    {
        Log(1, "Error: failed to bind IPv6 socket.\n");
        return -1;
    }

    memset(&srv, 0, sizeof(srv));
    srv.sin6_family = AF_INET6;
    srv.sin6_addr = ctx->aftrIPv6;
    srv.sin6_port = htons(ctx->natpmp_udp_port);

    inet_ntop(AF_INET6,(char*)&srv.sin6_addr, tmp, INET_ADDRSTRLEN);
    Log(3,"Connecting UDP socket %d to address %s/port %d\n", s->fd, tmp, htons(srv.sin6_port));
    if (connect(s->fd, (struct sockaddr *) &srv, sizeof(srv)) < 0)
    {
        Log(1,"Error: connect() failed.\n");
        return -1;
    }

    return s->fd;
}

int socket_del(context_t * ctx, int fd)
{
    socket_t * s = ctx->sockets, *prev = NULL;

    while (s)
    {
        if (s->fd == fd)
        {
            if (prev)
                prev->next = s->next;
            else
                ctx->sockets = s->next;
            sock_del(s->fd);
            Log(2,"Socket %d deleted\n", fd);
            free(s);
            return 0;
        }
        prev = s;
        s = s->next;
    }

    Log(1,"Error: Unable to delete socket: fd=%d not found.\n", fd);
    return -1;
}

int ctx_mappings_cnt(context_t * ctx)
{
    mapping_t * m = ctx->mappings;
    int i = 0;
    while (m)
    {
        i++;
        m = m->next;
    }
    return i;
}

void ctx_mappings_remove_expired(context_t *ctx)
{
    int i = 0;
    time_t now = time(NULL);
    mapping_t * m = ctx->mappings;
    while (m)
    {
        if (m->timeout <= now)
        { /* mapping has expired */
            mapping_delete(ctx, m);
            m = ctx->mappings;
            i++;
            continue;
        }

        m = m->next;
    }
    if (i>0)
        Log(2,"%d expired mappings removed.\n", i);
}


unsigned int ctx_timeout_get(context_t *ctx)
{
    unsigned int timeout = 3600;
    unsigned int now = time(NULL);

    mapping_t * m = ctx->mappings;
    while (m)
    {
        if (m->timeout < now)
        {
            /* Log(3,"#### expired mapping found timemout=%d now=%d\n", m->timeout, now); */
            timeout = 0;
            break;
        }
        if (timeout > m->timeout-now)
            timeout = m->timeout-now;
        m = m->next;
    }

    if (timeout == 0)
        timeout = 1;

    return timeout;
}

pkt_t * wait_for_traffic(context_t * ctx)
{
    struct timeval timeout;
    socket_t * sock;
    int result;

    timeout.tv_sec = ctx_timeout_get(ctx);
    timeout.tv_usec = 0;

    fd_set fds;
    FD_ZERO(&fds);
    sock = ctx->sockets;
    while (sock)
    {
        FD_SET(sock->fd, &fds);
        sock = sock->next;
    }

    // sockets_print(ctx->sockets); // DEBUG

    Log(2,"\nWaiting for incoming packets (timeout: %ds, %dus), %d mappings (%d pkts handled)\n",
        timeout.tv_sec, timeout.tv_usec, ctx_mappings_cnt(ctx), ctx->pktCnt);
    result = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

    if (result <=0)
    {
        /* timeout reached, nothing received */
        Log(3,"Timeout reached, nothing received (result=%d)\n",
            result);
        return NULL;
    }

    sock = ctx->sockets;
    while (sock)
    {
        if (FD_ISSET(sock->fd,&fds))
        {
            switch (sock->state)
            {
            case SOCKET_TYPE_UDP:
                return sock_udp_receive(ctx, sock);
            case SOCKET_TYPE_UDP6:
                return sock_udp6_receive(ctx, sock);
            case SOCKET_TYPE_TCP_LISTENING:
                return sock_tcp_accept(ctx, sock);
            case SOCKET_TYPE_TCP_CONNECTED:
                return sock_tcp_receive(ctx, sock);
            }
        }
        sock = sock->next;
    }
    Log(1,"Error: socket not found.\n");
    return NULL;
}


pkt_t * sock_tcp_accept(context_t *ctx, socket_t * sock)
{
    char tmp[INET_ADDRSTRLEN];
    int port;
    int result;

    struct sockaddr_in clientaddr;
    unsigned int addrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, addrlen);
    int newfd = accept (sock->fd, (struct sockaddr *)&clientaddr, &addrlen);
    if (newfd < 0 )
    {
        Log(1,"Error: Failed to accept incoming connection on TCP socket %d\n", sock->fd);
        return 0;
    }

    port = htons(clientaddr.sin_port);
    inet_ntop(AF_INET, &clientaddr.sin_addr, tmp, INET_ADDRSTRLEN);

    Log(1,"Accepted incoming TCP connection from %s,port %d\n", tmp, port);

    socket_t * lansock = malloc(sizeof(socket_t));
    memset(lansock, 0, sizeof(socket_t));
    lansock->fd = newfd;
    lansock->ipproto = IPPROTO_TCP;
    lansock->state = SOCKET_TYPE_TCP_CONNECTED;
    lansock->next = ctx->sockets;
    lansock->peer_addr = clientaddr.sin_addr;
    lansock->peer_port = port;


    /* create connection to destination */
    socket_t * wansock = malloc(sizeof(socket_t));
    memset(wansock, 0, sizeof(socket_t));
    wansock->ipproto = IPPROTO_TCP;
    wansock->state = SOCKET_TYPE_TCP_CONNECTED;

    wansock->fd = socket(AF_INET, SOCK_STREAM,0);
    if (wansock->fd<0)
    {
        Log(1,"Error: Unable to create TCP relay socket.\n");
        close(lansock->fd);
        free(lansock);
        return 0;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr = ctx->aftrIPv4;
    port = sock_get_port(sock->fd);
    server.sin_port = htons(port);
    inet_ntop(AF_INET, &server.sin_addr, tmp, INET_ADDRSTRLEN);

    Log(1,"Establishing connection to %s/%d\n", tmp, port);

    result = connect(wansock->fd, (struct sockaddr*)&server, sizeof(server));
    if (result<0)
    {
        Log(1,"Error failed to establish connection to %s/port %d\n",
            tmp, ntohs(port));
        close(lansock->fd);
        free(lansock);
        close(wansock->fd);
        free(wansock);
        return 0;
    }
    wansock->peer_addr = server.sin_addr;
    wansock->peer_port = ntohs(server.sin_port);

    /* ok, connection established */
    wansock->peer = lansock->fd;
    lansock->peer = wansock->fd;

    Log(2,"Connection established. Ready to relay data.\n");
    socket_add2(ctx, lansock);
    socket_add2(ctx, wansock);

    return 0;
}

pkt_t * sock_tcp_receive(context_t *ctx, socket_t * sock)
{
    struct in_addr my_addr, peer_addr;
    int peer_port = 0;
    int result;
    char tmp[INET_ADDRSTRLEN], tmp2[INET_ADDRSTRLEN];

    pkt_t * pkt = (pkt_t*)malloc(sizeof(pkt_t));
    memset(pkt, 0, sizeof(pkt_t));
    int len = 1500;
    pkt->data = malloc(len);
    pkt->proto = sock->ipproto;
    pkt->datalen = len;
    pkt->sock = sock->fd;

    Log(3,"Reading from TCP socket %d\n", sock->fd);
    result = sock_recv(sock->fd,&my_addr, &peer_addr, &peer_port, pkt->data, pkt->datalen);
    if (result<0)
    {
        Log(1,"Error: unable to read from TCP socket %d, error code %d\n", result);
        return NULL;
    }
    pkt->datalen  = result;

    socket_t * fwdto = socket_find(ctx,sock->peer);
    if (!fwdto)
    {
        Log(1,"Error: unable to find corresponding socket.\n");
        return pkt;
    }
    //pkt->srcIP    = 0;
    //pkt->dstIP    = 0;
    //pkt->srcPort  = 0;
    //pkt->dstPort  = 0;

    inet_ntop(AF_INET, &sock->peer_addr, tmp, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &fwdto->peer_addr, tmp2,INET_ADDRSTRLEN);
    if (pkt->datalen)
        Log(1,"Received TCP packet: %d bytes (sock-id=%d): from %s/port %d, fwd to %s/port %d\n",
            pkt->datalen, sock->fd, tmp, sock->peer_port, tmp2, fwdto->peer_port);
    else
        Log(1,"Received TCP packet: FIN (sock-id=%d)\n", sock->fd);
    return pkt;
}


pkt_t * sock_udp_receive(context_t * ctx, socket_t * sock)
{
    struct in_addr my_addr, peer_addr;
    int peer_port = 0;
    int result;
    char tmp[INET_ADDRSTRLEN], tmp2[INET_ADDRSTRLEN];

    pkt_t * pkt = (pkt_t*)malloc(sizeof(pkt_t));
    memset(pkt, 0, sizeof(pkt_t));
    int len = 1500;
    pkt->data = malloc(len);
    pkt->proto = sock->ipproto;
    pkt->datalen = len;
    pkt->sock = sock->fd;

    Log(3,"Reading from UDP socket %d\n", sock->fd);
    result = sock_recv(sock->fd,&my_addr, &peer_addr, &peer_port, pkt->data, pkt->datalen);
    if (result<0)
    {
        Log(1,"Error: unable to read from UDP socket %d, error code %d\n", result);
        return NULL;
    }
    pkt->datalen  = result;
    pkt->srcIP    = peer_addr;
    pkt->dstIP    = my_addr;
    pkt->srcPort  = peer_port;
    pkt->dstPort  = sock_get_port(sock->fd);

    inet_ntop(AF_INET, &pkt->srcIP, tmp, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &pkt->dstIP, tmp2,INET_ADDRSTRLEN);
    Log(1,"Received UDP packet: %d bytes (sock-id=%d): %s/port %d -> %s/port %d\n",
        pkt->datalen, sock->fd, tmp, pkt->srcPort, tmp2, pkt->dstPort);

#if 0
    /* simple echo */
    sock_send(sock->fd, pkt->srcIP, pkt->data, pkt->datalen, pkt->srcPort);
#endif

    return pkt;
}


pkt_t * sock_udp6_receive(context_t * ctx, socket_t * sock)
{
    struct in6_addr my_addr, peer_addr;
    int peer_port = 0;
    int result;
    char tmp[INET6_ADDRSTRLEN], tmp2[INET6_ADDRSTRLEN];

    pkt_t * pkt = (pkt_t*)malloc(sizeof(pkt_t));
    memset(pkt, 0, sizeof(pkt_t));
    int len = 1500;
    pkt->data = malloc(len);
    pkt->proto = sock->ipproto;
    pkt->datalen = len;
    pkt->sock = sock->fd;

    Log(3,"Reading from UDP6 socket %d\n", sock->fd);
    result = sock6_recv(sock->fd,&my_addr, &peer_addr, pkt->data, pkt->datalen);
    if (result<0)
    {
        Log(1,"Error: unable to read from UDP socket %d, error code %d\n", result);
        return NULL;
    }
    pkt->datalen  = result;
    //pkt->srcIP    = 0;
    //pkt->dstIP    = 0;
    pkt->srcPort  = peer_port;
    pkt->dstPort  = sock_get_port(sock->fd);

    inet_ntop(AF_INET6, &peer_addr, tmp, INET_ADDRSTRLEN);
    inet_ntop(AF_INET6, &my_addr, tmp2,INET_ADDRSTRLEN);

    Log(1,"Received UDP6 packet: %d bytes (sock-id=%d): %s/port %d -> %s/port %d\n",
        pkt->datalen, sock->fd, tmp, pkt->srcPort, tmp2, pkt->dstPort);

#if 0
    /* simple echo */
    sock_send(sock->fd, pkt->srcIP, pkt->data, pkt->datalen, pkt->srcPort);
#endif

    return pkt;
}




/**
 * handles incoming traffic, forwards packets and does NAT
 *
 * @param ctx
 *
 * @return
 */
int run(context_t * ctx)
{
    pkt_t * pkt = NULL;

    while (!Shutdown)
    {
        pkt = wait_for_traffic(ctx);
        if (pkt)
        {
            ctx->pktCnt++;
            handle_packet(ctx, pkt);
        }

        ctx_mappings_remove_expired(ctx);
    }
    return 0;
}


/**
 * returns socket fd that is going to be used to forward traffic
 * creates required socket if necessary. uses existing socket if possible
 *
 * @param ctx
 * @param pkt
 *
 * @return socket fd
 */
int query_socket_get(context_t * ctx, pkt_t * pkt)
{
    int fd;
    if (pkt->proto == IPPROTO_UDP)
    {
        /* use single socket for all traffic */
        return ctx->shared_udp_sock_fd;
    } else {
        /* create extra socket for handling this traffic */
        Log(2,"Creating new socket for handling received packet.\n");
        fd = socket_add(ctx, ctx->wan, ctx->wanIP, 0/*random port*/, pkt->proto,
                        0/*don't bind to iface*/, 0/*don't allow reuse*/);
        if (fd<0)
        {
            Log(1,"Error: Unable to create socket.\n");
            return -1;
        }
        return fd;
    }
}

void mapping_add(context_t * ctx, mapping_t *m)
{
    m->next = ctx->mappings;
    m->timeout = time(NULL) + ctx->timeout;
    ctx->mappings = m;
    Log(3,"Added mapping. %d mapping(s).\n", ctx_mappings_cnt(ctx));
}

socket_t * socket_find(context_t * ctx, int fd)
{
    socket_t * sock = ctx->sockets;
    while (sock)
    {
        if (sock->fd==fd)
            return sock;
        sock = sock->next;
    }

    return 0;
}


int handle_tcp_packet(context_t * ctx, pkt_t * pkt)
{
    int fd = 0;
    /* find corresponding socket */
    socket_t * sock = socket_find(ctx, pkt->sock);
    if (!sock)
    {
        Log(1,"Error: Unable to find socket %d\n", pkt->sock);
        return -1;
    }

    fd = sock->peer;
    sock = socket_find(ctx, fd);
    if (!sock)
    {
        Log(1,"Error: Unable to find correspodning socket %d for socket %d\n",
            fd, pkt->sock);
        return -1;
    }

    Log(2,"Sending %d bytes to TCP socket %d\n", pkt->datalen, sock->fd);
    int result = send(sock->fd, pkt->data, pkt->datalen, 0);
    if (result<0)
    {
        Log(1,"Failed to send data.\n");
    }

    packet_delete(pkt);

    return result<0?result:0;
}


/**
 * performs required actions when TCP connection is closed
 *
 * @param ctx
 * @param pkt
 *
 * @return 0 if ok, -1 otherwise
 */
int handle_tcp_close(context_t *ctx, pkt_t *pkt)
{
    int fd, peer;
    socket_t * sock;
    if (pkt->datalen)
    {
        Log(1,"Error: attempt to close socket, but there are data to read.\n");
        return -1;
    }
    fd = pkt->sock;
    sock = socket_find(ctx, fd);
    if (!sock)
    {
        Log(1,"Error: Unable to find socket %d\n", fd);
        return -1;
    }
    peer = sock->peer;

    socket_del(ctx, fd);
    socket_del(ctx, peer);
    Log(1,"TCP connection closed: sockets %d and %d closed.\n", fd, peer);

    return 0;
}

int handle_ssdp(context_t * ctx, pkt_t *pkt)
{
    socket_t * sock = 0;
    int result = 0;
    sock = socket_find(ctx, pkt->sock);

    Log(2, "SSDP packet (%d bytes) received via socket %d, sending via %d\n",
        pkt->datalen, sock->fd, sock->peer);

    result = sock_send(sock->peer, mcast, pkt->data, pkt->datalen, ctx->upnp_udp_port);

    if (result<0)
    {
        Log(1,"Error: Failed to send SSDP packet.\n");
    }
    return result;
}

void packet_delete(pkt_t * pkt)
{
    if (!pkt)
        return;
    if (pkt->data)
        free(pkt->data);
    free(pkt);
}


/**
 * checks if packet is a multicast received on WAN interface
 *
 * @param ctx
 * @param pkt
 *
 * @return
 */int packet_wan_mcast(context_t *ctx, pkt_t *pkt)
{
    if (addr_cmp(pkt->dstIP, mcast) &&
        pkt->sock == ctx->wan_ssdp_sock)
    {
        Log(2,"Mcast pkt received on WAN interface (pkt->sock=%d)\n", pkt->sock);
        return 1;
    }

    return 0;
}


int handle_nat_pmp_ext_fromclient(context_t *ctx, pkt_t *pkt)
{
    Log(2,"Received %d bytes from client, forwarding to AFTR (over IPv6)\n", pkt->datalen);
    char buf[128];
    if (pkt->datalen > 100)
    {
        Log(1,"Error: overrun: packet from client (%d) larger than 100 bytes.\n", pkt->datalen);
        return -1;
    }

    buf[0] = 12;
    buf[1] = 'P';
    memcpy(buf+2, &pkt->srcPort, 2);
    memcpy(buf+4, &ctx->lanIP, 4);
    memcpy(buf+8, &pkt->srcIP, 4);
    memmove(buf+12, pkt->data, pkt->datalen);

    Log(3,"Sending %d bytes over sockid=%d\n", pkt->datalen+12, ctx->nat_pmp_ext_sock);
    if (send(ctx->nat_pmp_ext_sock, buf, pkt->datalen+12, 0) < 0)
    {
        Log(1,"Error: failed to send data to AFTR over IPv6 (NAT-PMP ext)\n");
        return -1;
    }

    return 0;
}

int handle_nat_pmp_ext_fromserver(context_t *ctx, pkt_t *pkt)
{
    struct sockaddr_in client;

    Log(2,"Received %d bytes from server, returning reponse to clients (over IPv4)\n", pkt->datalen);
    if (pkt->datalen < 12)
    {
        Log(1, "Error: received truncated data (%d bytes, less than required 12)\n", pkt->datalen);
        return -1;
    }
    if ( (pkt->data[0]!=12) || (pkt->data[1]!='P') )
    {
        Log(1, "Error: incorrect AFTR response. byte[0]!=12 or byte[1]!='P'\n");
        return -1;
    }

    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    //memcpy(&client.sin_port, pkt->data+2, 2);
    client.sin_port = ntohs( *(short*)(pkt->data+2));
    memcpy(&client.sin_addr, pkt->data+8, 4);

    char tmp[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &client.sin_addr, tmp, INET_ADDRSTRLEN);

    Log(3,"Forwarding response to %s/%d over socket %d\n",
        tmp, htons(client.sin_port), ctx->nat_pmp_lan_sock);

    if (sendto(ctx->nat_pmp_lan_sock, pkt->data + 12, pkt->datalen-12, 0, (struct sockaddr *)&client, sizeof(client)) <0)
    {
        Log(1,"Error: Failed to send data to client.\n");
        return -1;
    }

    return 0;
}

/**
 * generic packet handling procedure
 *
 * @param ctx context
 * @param pkt received packet to be handled
 *
 * @return 0 - handled ok, -1 - error
 */
int handle_packet(context_t * ctx, pkt_t * pkt)
{
    mapping_t * m = 0;
    int s;

    /* handle TCP traffic */
    if (pkt->proto==IPPROTO_TCP)
    {
        if (pkt->datalen)
            return handle_tcp_packet(ctx, pkt); /* incoming data */
        else
            return handle_tcp_close(ctx, pkt); /* TCP connection is closed */
    }

    /* UDP (NAT-PMP/SSDP) traffic */

    /* multicast traffic from WAN (SSDP NOTIFY announcements, forward to LAN) */
    if (packet_wan_mcast(ctx, pkt))
    {
        /* multicast packet received on WAN interface, re-multicast it on LAN */
        Log(1,"Forwarding SSDP multicast to LAN\n");
        if (sock_send(ctx->lan_ssdp_sock, mcast, pkt->data, pkt->datalen, pkt->dstPort)<0)
            Log(1, "Error: Failed to forward SSDP packet to LAN.\n");
        return 0;
    }

    if (ctx->enable_nat_pmp_ext)
    {
        Log(3,"###: ctx->nat_pmp_ext_sock=%d pkt->sock=%d\n", ctx->nat_pmp_ext_sock,
            pkt->sock);
        if (pkt->sock == ctx->nat_pmp_ext_sock)
        {
            return handle_nat_pmp_ext_fromserver(ctx, pkt);
        }
        return handle_nat_pmp_ext_fromclient(ctx, pkt);
    }

    if ((m=mapping_find(ctx, pkt)))
    {
        /* there's mapping for this packet, so it is an expected response */
        response_send(ctx, m, pkt);
        mapping_delete(ctx, m);
        return 0;
    }

    if (packet_valid(ctx, pkt))
    {
        /* new packet, forward it and create mapping for response */
        s = query_socket_get(ctx, pkt);
        m = mapping_create(ctx, pkt, s);
        mapping_add(ctx, m);
        mapping_print(m);
        if (query_send(ctx, m, pkt)<0)
        {
            Log(1,"Error: failed to send packet.\n");
            return -1;
        }
        return 0;
    }

    return -1;
}

/**
 * compares 2 addresses
 *
 * @param a first address
 * @param b second address
 *
 * @return 1 if addresses match, 0 otherwise
 */
int addr_cmp(struct in_addr a, struct in_addr b)
{
    if (!memcmp(&a,&b,sizeof(struct in_addr)))
        return 1;
    return 0;
}

/**
 * tried to find mapping matching received packet
 *
 * @param ctx
 * @param pkt
 *
 * @return mapping, if found (NULL otherwise)
 */
mapping_t * mapping_find(context_t *ctx, pkt_t *pkt)
{
    char tmp1[INET_ADDRSTRLEN], tmp2[INET_ADDRSTRLEN];
    mapping_t * m = ctx->mappings;
    inet_ntop(AF_INET, &pkt->srcIP, tmp1, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &pkt->dstIP, tmp2, INET_ADDRSTRLEN);

    Log(3,"Trying to match: pkt:[src=%s,dst=%s,sPort=%d,dPort=%d,proto=%d]\n",
        tmp1, tmp2, pkt->srcPort, pkt->dstPort, pkt->proto);
    while (m)
    {
        inet_ntop(AF_INET, &m->aftrIPv4, tmp1, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &m->wanIP, tmp2, INET_ADDRSTRLEN);
        Log(3,"mapping: aftrIPv4=%s, wanIP=%s, routerPort=%d, wanPort=%d, proto=%d timeout=%d\n",
            tmp1, tmp2, m->routerPort, m->wanPort, m->proto, m->timeout);
        if ( (addr_cmp(m->aftrIPv4, mcast) || addr_cmp(pkt->srcIP, m->aftrIPv4)) &&
             addr_cmp(pkt->dstIP, m->wanIP) &&
             (pkt->srcPort == m->routerPort) &&
             (pkt->dstPort == m->wanPort) &&
             (pkt->proto   == m->proto) )
        {
            return m;
        }

        m = m->next;
    }

    return NULL;
}

/**
 * deletes mapping
 *
 * @param ctx
 * @param m
 *
 * @return
 */
int mapping_delete(context_t * ctx, mapping_t * m)
{
    mapping_t * tmp = ctx->mappings;
    if (tmp==m)
    {
        /* removing first entry */
        ctx->mappings = tmp->next;
        // Log(3,"Removed mapping (first on list).\n");
        return 0;
    }
    while (tmp->next)
    {
        if (tmp->next==m)
        {
            tmp->next = tmp->next->next;
            free(m);
            // Log(3,"Removed mapping.\n");
            return 0;
        }
        tmp = tmp->next;
    }
    Log(3,"Error: Unable to delete mapping\n");
    return -1;
}

/**
 * checks if packet is valid (i.e. has proper addresses, src/dst ports etc.)
 *
 * @param ctx
 * @param pkt
 *
 * @return
 */
int packet_valid(context_t *ctx, pkt_t * pkt)
{
    return 1;
}

void mapping_print(mapping_t *m)
{
   char tmp1[INET_ADDRSTRLEN], tmp2[INET_ADDRSTRLEN], tmp3[INET_ADDRSTRLEN], tmp4[INET_ADDRSTRLEN];
   inet_ntop(AF_INET, &m->clientIP, tmp1, INET_ADDRSTRLEN);
   inet_ntop(AF_INET, &m->lanIP, tmp2, INET_ADDRSTRLEN);
   inet_ntop(AF_INET, &m->wanIP, tmp3, INET_ADDRSTRLEN);
   inet_ntop(AF_INET, &m->aftrIPv4, tmp4, INET_ADDRSTRLEN);

   Log(2, "Mapping %s: src=%s/port %d,dst=%s/port %d => src=%s/port %d,dst=%s/port %d (sock=%d/%d)\n",
       (m->proto==IPPROTO_UDP?"UDP":"TCP"), tmp1, m->clientPort, tmp2, m->lanPort, tmp3, m->wanPort,
       tmp4, m->routerPort, m->lanSock, m->wanSock );
}

/**
 * adds mapping for a newly received packet
 *
 * @param ctx
 * @param pkt
 *
 * @return
 */
mapping_t * mapping_create(context_t *ctx, pkt_t * pkt, int sock)
{
    mapping_t * m = (mapping_t*)malloc(sizeof(mapping_t));
    memset(m,0, sizeof(mapping_t));

    char tmp1[INET_ADDRSTRLEN],tmp2[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &pkt->dstIP, tmp1, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &mcast, tmp2, INET_ADDRSTRLEN);

    if (!addr_cmp(pkt->dstIP,mcast))
    {
        /* add normal mapping (NAT-PMP) */
        m->clientIP   = pkt->srcIP;
        m->lanIP      = pkt->dstIP;
        m->wanIP      = ctx->wanIP;
        m->aftrIPv4   = ctx->aftrIPv4;
        m->proto      = pkt->proto;
        m->clientPort = pkt->srcPort;
        m->lanPort    = pkt->dstPort;
        m->wanPort    = sock_get_port(sock);
        m->routerPort = pkt->dstPort; /* use the same port as on this machine */
        m->lanSock    = pkt->sock;
        m->wanSock    = sock;
        Log(3,"Added unicast mapping.\n");
        mapping_print(m);
    } else
    {
        /* add mapping for packet sent to multicast address */
        m->clientIP   = pkt->srcIP;
        m->lanIP      = pkt->dstIP;
        m->wanIP      = ctx->wanIP;
        m->aftrIPv4   = mcast;
        m->proto      = pkt->proto;
        m->clientPort = pkt->srcPort;
        m->lanPort    = pkt->dstPort;
        m->wanPort    = sock_get_port(sock);
        m->routerPort = pkt->dstPort;
        m->lanSock    = pkt->sock;
        m->wanSock    = sock;
        Log(3,"Added mcast mapping.\n");
        mapping_print(m);
    }

    return m;
}

int query_send(context_t * ctx, mapping_t *m, pkt_t * pkt)
{
    char tmp[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &m->aftrIPv4, tmp, INET_ADDRSTRLEN);
    Log(1,"Forwarding %s query (%d bytes) to %s/port %d\n",
        (pkt->proto==IPPROTO_UDP?"UDP":"TCP"),
        pkt->datalen, tmp, m->routerPort);
    return sock_send(m->wanSock, m->aftrIPv4, pkt->data, pkt->datalen, m->routerPort);
}

/**
 * forwards response to client
 *
 * @param ctx
 * @param m
 * @param pkt
 *
 * @return
 */
int response_send(context_t *ctx, mapping_t * m, pkt_t *pkt)
{
    char tmp[INET_ADDRSTRLEN];
    int result;
    Log(2,"Handling response using following mapping:\n");
    mapping_print(m);

    inet_ntop(AF_INET, &m->aftrIPv4, tmp, INET_ADDRSTRLEN);
    Log(1,"Forwarding %s response (%d bytes) to %s/port %d\n",
        (pkt->proto==IPPROTO_UDP?"UDP":"TCP"),
        pkt->datalen, tmp, m->clientPort);

    result = sock_send(m->lanSock, m->clientIP, pkt->data, pkt->datalen, m->clientPort);

    return result;
}

void sockets_close(context_t * ctx)
{
    while (ctx->sockets)
    {
        Log(2, "Closing socket %d\n", ctx->sockets->fd);
        socket_del(ctx, ctx->sockets->fd); /* close socket and remove socket structure */
    }
}


/**
 * joins multicast group
 *
 * @param fd
 * @param mcast
 * @param local_addr
 *
 * @return
 */
int socket_mcast_join(socket_t * sock, struct in_addr mcast, struct in_addr local_addr)
{
    char tmp1[INET_ADDRSTRLEN], tmp2[INET_ADDRSTRLEN];
    struct ip_mreq imr;
    memset(&imr, 0, sizeof(struct ip_mreq));
    imr.imr_multiaddr = mcast; /* multicast address, e.g. 239.255.255.250 */
    imr.imr_interface = local_addr; /* address on local interface */

    inet_ntop(AF_INET, &imr.imr_multiaddr.s_addr, tmp1, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &imr.imr_interface.s_addr, tmp2, INET_ADDRSTRLEN);

    if (setsockopt(sock->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (void *)&imr, sizeof(struct ip_mreq)) < 0)
    {
        Log(1,"Error: Failed to join multicast group %s (local IP=%s)\n",
            tmp1, tmp2);
        return -1;
    }

    Log(1,"Successfully joint multicast group %s (local IP=%s)\n", tmp1, tmp2);
    sock->mcast = 1;

    int opt=1;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
    {
        Log(1,"Error: Failed to define SO_REUSEADDR\n");
        return -1;
    }

    /* disable loopback (I don't want to receive my own traffic) */
    opt = 0;
    if (setsockopt(sock->fd, IPPROTO_IP, IP_MULTICAST_LOOP, &opt, sizeof(opt)) <0)
        Log(1,"Failed to disable multicast loopback on socket %d\n", sock->fd);

    return 0;
}

void sockets_print(socket_t * s)
{
    char tmp[INET_ADDRSTRLEN];
    while (s)
    {
        inet_ntop(AF_INET, &s->peer_addr, tmp, INET_ADDRSTRLEN);
        Log(3,"  fd=%d, proto=%d, state=%d, peer-sock=%d, peer_addr=%s, peer_port=%d, mcast=%d\n",
            s->fd, s->ipproto, s->state, s->peer, tmp, s->peer_port, s->mcast);
        s = s->next;
    }
}

void context_print(context_t *ctx)
{
    char tmp[INET_ADDRSTRLEN];
    mapping_t *m;

    Log(2,"Detected interfaces:\n");
    ifaces_print(ctx->ifaces);
    Log(2,"Configuration:\n");

    Log(2,"  lan=%s\n", ctx->lan);
    Log(2,"  wan=%s\n", ctx->wan);

    inet_ntop(AF_INET, &ctx->lanIP, tmp, INET_ADDRSTRLEN);
    Log(2,"  lanIP=%s\n", tmp);
    inet_ntop(AF_INET, &ctx->wanIP, tmp, INET_ADDRSTRLEN);
    Log(2,"  wanIP=%s\n", tmp);
    inet_ntop(AF_INET, &ctx->aftrIPv4, tmp, INET_ADDRSTRLEN);
    Log(2,"  aftrIPv4=%s\n", tmp);

    sockets_print(ctx->sockets);

    Log(2,"  natpmp_udp_port=%d\n", ctx->natpmp_udp_port);
    Log(2,"  upnp_udp_port=%d\n", ctx->upnp_udp_port);
    Log(2,"  upnp_tcp_port1=%d\n", ctx->upnp_tcp_port1);
    Log(2,"  upnp_tcp_port2=%d\n", ctx->upnp_tcp_port2);
    Log(2,"  fwd_tcp_port1=%d\n", ctx->fwd_tcp_port1);
    Log(2,"  fwd_tcp_port2=%d\n", ctx->fwd_tcp_port2);

    Log(2,"  shared_udp_sock=%s\n", (ctx->shared_udp_sock?"YES":"NO"));
    Log(2,"  enable_natpmp=%s\n", (ctx->enable_nat_pmp?"YES":"NO"));
    Log(2,"  enable_upnp_tcp=%s\n", (ctx->enable_upnp_tcp?"YES":"NO"));
    Log(2,"  enable_upnp_udp=%s\n", (ctx->enable_upnp_udp?"YES":"NO"));

    m = ctx->mappings;
    while (m)
    {
        mapping_print(m);
        m = m->next;
    }
}
