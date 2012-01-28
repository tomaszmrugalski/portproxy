

#include <netinet/in.h>
#include <stdarg.h>

#ifndef PORTPROXY_H
#define PORTPROXY_H


#define SSDP_MCAST_ADDRESS ("239.255.255.250")

typedef struct Mapping_t
{
    struct in_addr clientIP;
    struct in_addr lanIP;
    struct in_addr wanIP;
    struct in_addr aftrIPv4; /* not really necessary, will allways == ctx->aftrIPv4 */
    int proto;
    int clientPort;
    int lanPort; /* not necessary, will use the same socket anyway */
    int wanPort;
    int routerPort;
    int lanSock; /* socket the original packet was receive from */
    int wanSock; /* socket to be used for forwarding data to router */
    int timeout;
    struct Mapping_t *next;
} mapping_t;

typedef enum
{
    SOCKET_TYPE_UDP = IPPROTO_UDP,
    SOCKET_TYPE_UDP6,
    SOCKET_TYPE_TCP_LISTENING = IPPROTO_TCP,
    SOCKET_TYPE_TCP_CONNECTED
} socket_type_t;

typedef struct Socket_t
{
    int fd;
    int ipproto; /* TCP state: listening socket, connected socket */
    socket_type_t state;
    int peer; /* fd of the paired socket, used for paired TCP sockets */
    struct in_addr peer_addr; /* used for TCP sockets, just for printing purposes */
    int peer_port; /* used for TCP sockets, just for printing purposes */
    int mcast; /* is this multicast socket? */
    struct Socket_t * next;
} socket_t;

typedef struct Pkt_t
{
    struct in_addr srcIP;
    struct in_addr dstIP;
    int srcPort;
    int dstPort;
    int proto;
    char * data;
    int datalen;
    int sock;
} pkt_t;

typedef struct Context_t
{
    struct iface_t * ifaces;
    char lan[MAX_IFNAME_LENGTH];
    char wan[MAX_IFNAME_LENGTH];
    int persistent;           /* should portproxy continue even when interface or address is not available */
    struct in_addr lanIP;     /* local IPv4 address (bind sockets here) */
    struct in_addr wanIP;     /* external IPv4 address (send fwd packets from here) */
    struct in_addr aftrIPv4;  /* next hop. Send fwd packets there */
    struct in6_addr wanIPv6;  /* used for extended NAT-PMP */
    struct in6_addr aftrIPv6; /* used for extended NAT-PMP */
    socket_t * sockets; /* socket structures */
    mapping_t * mappings;

    unsigned int natpmp_udp_port;
    unsigned int upnp_udp_port;
    unsigned int upnp_tcp_port1;
    unsigned int upnp_tcp_port2;

    unsigned int fwd_tcp_port1;
    unsigned int fwd_tcp_port2;

    int bind_ext_nat_pmp_to_natpmp_port;

    unsigned int shared_udp_sock; /* should one UDP socket be shared for all forwarded queries? */

    /* sockets */
    int shared_udp_sock_fd; /* share udp socket (fd) */
    int wan_ssdp_sock; /* SSDP socket on WAN side */
    int lan_ssdp_sock; /* SSDP socket on LAN side */
    int nat_pmp_ext_sock; /* IPv6 socket for sending requests from clients to AFTR in extended NAT-PMP */
    int nat_pmp_lan_sock; /* IPv4 socket for sending responses from AFTR to clients in extended NAT-PMP */

    /* features */
    int enable_nat_pmp;
    int enable_nat_pmp_ext;
    int enable_upnp_tcp;
    int enable_upnp_udp;

    int loglevel;
    int timeout; /* mapping expiration time, specified in seconds */
    int daemon;

    /* statistics */
    int pktCnt;
    int ssdpCnt;
    int natpmpCnt;
} context_t;

extern context_t *ctx;
extern const struct in_addr mcast;

void Log(int level, const char* format, ... );
void LogLevel(int level);

void iface_print(const struct iface_t * iface);
void ifaces_print(struct iface_t *iface);
void ctx_print(context_t * ctx);
context_t * ctx_create();
void ctx_release(context_t ** ctx);

int socket_add(context_t * ctx, const char * ifname, struct in_addr addr, 
	       int port, int ipproto, int bindiface, int reuse);
void socket_add2(context_t * ctx, socket_t * sock);
int socket6_add(context_t * ctx, const char * ifname, struct in6_addr addr,
		int port, int ipproto, int bindiface, int reuse);
int socket_del(context_t * ctx, int fd);
int socket_mcast_join(socket_t *sock, struct in_addr mcast, struct in_addr local_addr);
context_t * ctx_create();

int run(context_t * ctx);

struct in_addr iface_ip(context_t * ctx, const char *name);


/* framework */
int framework_init();
pkt_t * wait_for_traffic(context_t * ctx);
int handle_packet(context_t * ctx, pkt_t * pkt);
mapping_t * mapping_find(context_t *ctx, pkt_t *pkt);
int response_send(context_t *ctx, mapping_t * m, pkt_t *pkt);
int mapping_delete(context_t * ctx, mapping_t * m);
int packet_valid(context_t *ctx, pkt_t * pkt);
void mapping_add(context_t * ctx, mapping_t *m);
mapping_t * mapping_create(context_t *ctx, pkt_t * pkt, int sock);
int query_send(context_t * ctx, mapping_t *m, pkt_t * pkt);
void mapping_print(mapping_t *m);
socket_t * socket_find(context_t * ctx, int fd);
void ctx_mappings_remove_expired(context_t *ctx);

void sockets_close(context_t * ctx);
pkt_t * sock_tcp_accept(context_t *ctx, socket_t * sock);
pkt_t * sock_tcp_receive(context_t *ctx, socket_t * sock);
pkt_t * sock_udp_receive(context_t * ctx, socket_t * sock);
pkt_t * sock_udp6_receive(context_t * ctx, socket_t * sock);

void packet_delete(pkt_t * pkt);

void sockets_print(socket_t * s);
void context_print(context_t *ctx);

#endif
