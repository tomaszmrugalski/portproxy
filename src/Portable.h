/*
 * portproxy - NAT-PMP/uPNP proxy
 *
 * author: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * Released under GNU GPL v2 licence
 *
 * $Id$
 */	

#ifndef MAX_IFNAME_LENGTH 
#define MAX_IFNAME_LENGTH 255
#endif


#ifndef PORTABLE_H
#define PORTABLE_H

#define PORTPROXY_VERSION "2010-03-02"

#define PORTPROXY_COPYRIGHT1 "| Portproxy - NAT-PMP/UPnP proxy, version " PORTPROXY_VERSION
#define PORTPROXY_COPYRIGHT2 "| Author : Tomasz Mrugalski<tomasz.mrugalski(at)gmail.com>"
#define PORTPROXY_COPYRIGHT3 "| Licence : GNU GPL v2 only. Developed at Gdansk University of Technology."
#define PORTPROXY_COPYRIGHT4 "| Homepage: http://klub.com.pl/portproxy/"

#include "Portable.h"
#include "portproxy.h"

#include <arpa/inet.h> /* inet_ntop */
#include <netinet/in.h>

extern void Log(int level, const char* format, ... );
extern int addr_cmp(struct in_addr a, struct in_addr b);

/**********************************************************************/

#define PORTPROXY_DEFAULT_LAN_IFACE ("br-lan")
#define PORTPROXY_DEFAULT_WAN_IFACE ("eth0.1")
#define PORTPROXY_NATPMP_UDP_PORT   5351
#define PORTPROXY_UPNP_UDP_PORT     1900
#define PORTPROXY_UPNP_TCP_PORT1    2869
#define PORTPROXY_UPNP_TCP_PORT2    2857

#define PORTPROXY_DEFAULT_TIMEOUT 10

/**********************************************************************/
/*** data structures **************************************************/
/**********************************************************************/


/* Structure used for interface name reporting */
struct iface_t {
    char name[MAX_IFNAME_LENGTH];  /* interface name */
    int  id;                       /* interface ID (often called ifindex) */
    int  hardwareType;             /* type of hardware (see RFC 826) */
    unsigned char mac[255];                 /* link layer address */
    int  maclen;                   /* length of link layer address */
    int *ipaddr;                   /* assigned IPv6 link local addresses  */
    int  ipaddrcount;              /* number of assigned IPv6 link local addresses */
    unsigned int flags;            /* look IF_xxx in portable.h */
    struct iface_t* next;            /* structure describing next iface in system */
};

/**********************************************************************/
/*** file setup/default paths *****************************************/
/**********************************************************************/

#define CFGFILE "portproxy.conf"
#define LOGFILE "portproxy.log"
#define WORKDIR "/tmp"

/* ********************************************************************** */
/* *** interface flags ************************************************** */
/* ********************************************************************** */

#ifdef LINUX
#define IF_UP		      0x1
#define IF_LOOPBACK	      0x8
#define IF_RUNNING	      0x40
#define IF_MULTICAST	      0x1000
#endif

/* ********************************************************************** */
/* *** low-level error codes ******************************************** */
/* ********************************************************************** */
#define LOWLEVEL_NO_ERROR                0
#define LOWLEVEL_ERROR_UNSPEC           -1
#define LOWLEVEL_ERROR_BIND_IFACE       -2
#define LOWLEVEL_ERROR_BIND_FAILED      -4
#define LOWLEVEL_ERROR_MCAST_HOPS       -5
#define LOWLEVEL_ERROR_MCAST_MEMBERSHIP -6
#define LOWLEVEL_ERROR_GETADDRINFO      -7
#define LOWLEVEL_ERROR_SOCK_OPTS        -8
#define LOWLEVEL_ERROR_REUSE_FAILED     -9
#define LOWLEVEL_ERROR_FILE             -10
#define LOWLEVEL_ERROR_SOCKET           -11
#define LOWLEVEL_ERROR_NOT_IMPLEMENTED  -12

/* ********************************************************************** */
/* *** interface/socket low level functions ***************************** */
/* ********************************************************************** */

/* get list of interfaces */
extern struct iface_t * if_list_get();
extern void if_list_release(struct iface_t * list);
int iface_ip6_get(const char *iface, char* addr);

/* add socket to interface */
extern int sock_add(const char* ifacename, struct in_addr addr, int port, int proto, int thisifaceonly, int reuse);
extern int sock_del(int fd);
extern int sock_send(int fd, struct in_addr addr, char* buf, int buflen, int port);
extern int sock_recv(int fd, struct in_addr *my_addr, struct in_addr *peer_addr, int *peer_port, char* buf, int buflen);
extern int sock6_recv(int fd, struct in6_addr * myPackedAddr, 
		      struct in6_addr * peerPackedAddr, 
		      char * buf, int buflen);

int sock_get_port(int fd);

#endif
