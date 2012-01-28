/*
 * Dibbler - a portable DHCPv6
 *
 * author: Tomasz Mrugalski <thomson@klub.com.pl>
 * 
 * This file is a based on the lowlevel-linux.c from the Dibbler
 * project that is in turn based on ip/ipaddress.c file from iproute2
 * ipaddress.c authors (note: those folks are not involved in Dibbler development):
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 * Changes: Laszlo Valko <valko@linux.karinthy.hu>
 *
 * released under GNU GPL v2 only licence
 *
 * $Id: lowlevel-linux.c,v 1.13 2009-03-09 22:27:23 thomson Exp $
 *
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fnmatch.h>
#include <time.h>
#include <errno.h>


#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"
#include "rt_names.h"
#include "Portable.h"

/*
#define LOWLEVEL_DEBUG 1
*/

#ifndef IPV6_RECVPKTINFO
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

struct rtnl_handle rth;
char Message[1024] = {0};

int lowlevelInit()
{
    if (rtnl_open(&rth, 0) < 0) {
	sprintf(Message, "Cannot open rtnetlink\n");
	return LOWLEVEL_ERROR_SOCKET;
    }
    return 0;
}

int lowlevelExit()
{
    rtnl_close(&rth);
    return 0;
}

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

int print_linkinfo(struct nlmsghdr *n);
int print_addrinfo(struct nlmsghdr *n);
int print_selected_addrinfo(int ifindex, struct nlmsg_list *ainfo);
void ipaddr_global_get(int *count, int **buf, unsigned int ifindex, struct nlmsg_list *ainfo);
void print_link_flags( unsigned flags);
int default_scope(inet_prefix *lcl);

int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
    struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
    struct nlmsg_list *h;
    struct nlmsg_list **lp;
    
    h = malloc(n->nlmsg_len+sizeof(void*));
    if (h == NULL)
	return -1;
    
    memcpy(&h->h, n, n->nlmsg_len);
    h->next = NULL;
    
    for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
    *lp = h;
    
    ll_remember_index(who, n, NULL);
    return 0;
}

void release_nlmsg_list(struct nlmsg_list *n) {
    struct nlmsg_list *tmp;
    while (n) {
	tmp = n->next;
	free(n);
	n = tmp;
    }
}

void if_list_release(struct iface_t * list) {
    struct iface_t * tmp;
    while (list) {
        tmp = list->next;
	if (list->ipaddrcount)
	    free(list->ipaddr);
        free(list);
        list = tmp;
    }
}

/*
 * returns interface list with detailed informations
 */
struct iface_t * if_list_get()
{
    struct nlmsg_list *linfo = NULL;
    struct nlmsg_list *ainfo = NULL;
    struct nlmsg_list *l;
    struct rtnl_handle rth;
    struct iface_t * head = NULL;
    struct iface_t * tmp;
    int preferred_family = AF_PACKET;

    /* required to display information about interface */
    struct ifinfomsg *ifi;
    struct rtattr * tb[IFLA_MAX+1];
    int len;
    memset(tb, 0, sizeof(tb));
    memset(&rth,0, sizeof(rth));

    rtnl_open(&rth, 0);
    rtnl_wilddump_request(&rth, preferred_family, RTM_GETLINK);
    rtnl_dump_filter(&rth, store_nlmsg, &linfo, NULL, NULL);
    
    /* 2nd attribute: AF_UNSPEC, AF_INET, AF_INET6 */
    rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETADDR);
    rtnl_dump_filter(&rth, store_nlmsg, &ainfo, NULL, NULL);

    /* build list with interface names */
    for (l=linfo; l; l = l->next) {
	ifi = NLMSG_DATA(&l->h);
	len = (&l->h)->nlmsg_len;
	len -= NLMSG_LENGTH(sizeof(*ifi));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);

#ifdef LOWLEVEL_DEBUG
	printf("### iface %d %s (flags:%d) ###\n",ifi->ifi_index,
	       (char*)RTA_DATA(tb[IFLA_IFNAME]),ifi->ifi_flags);
#endif
	if (!head) 
	{
	    tmp = malloc(sizeof(struct iface_t));
	    head = tmp; // it's the first interface
	} else
	{
	    tmp->next = malloc(sizeof(struct iface_t));
	    tmp = tmp->next;
	}

	snprintf(tmp->name,MAX_IFNAME_LENGTH,"%s",(char*)RTA_DATA(tb[IFLA_IFNAME]));
	tmp->id=ifi->ifi_index;
	tmp->flags=ifi->ifi_flags;
	tmp->hardwareType = ifi->ifi_type;

        memset(tmp->mac,0,255);

	/* This stuff reads MAC addr */
	/* Does inetface has LL_ADDR? */
	if (tb[IFLA_ADDRESS]) {
  	    tmp->maclen = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
	    memcpy(tmp->mac,RTA_DATA(tb[IFLA_ADDRESS]), tmp->maclen);
	}
	/* Tunnels can have no LL_ADDR. RTA_PAYLOAD doesn't check it and try to
	 * dereference it in this manner
	 * #define RTA_PAYLOAD(rta) ((int)((rta)->rta_len) - RTA_LENGTH(0))
	 * */
	else {
	    tmp->maclen=0;
	}

	ipaddr_global_get(&tmp->ipaddrcount, &tmp->ipaddr, tmp->id, ainfo);
    }

    release_nlmsg_list(linfo);
    release_nlmsg_list(ainfo);

    return head;
}

/**
 * returns first non-local IPv6 address for specified interface
 */
int ipaddr6_global_get(char *globalAddr, int ifindex, struct nlmsg_list *ainfo) {
    char addr[16];
    struct rtattr * rta_tb[IFA_MAX+1];

    for ( ;ainfo ;  ainfo = ainfo->next) {
	struct nlmsghdr *n = &ainfo->h;
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	if ( (ifa->ifa_family == AF_INET6) && (ifa->ifa_index == ifindex) ) {
	    memset(rta_tb, 0, sizeof(rta_tb));
	    parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));
	    if (!rta_tb[IFA_LOCAL])   rta_tb[IFA_LOCAL]   = rta_tb[IFA_ADDRESS];
	    if (!rta_tb[IFA_ADDRESS]) rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];
	    
	    memcpy(addr,(char*)RTA_DATA(rta_tb[IFLA_ADDRESS]),16);
	    if ( (addr[0]==0xfe && addr[1]==0x80) || /* link local */
		 (addr[0]==0xff) ) { /* multicast */
		continue; /* ignore non link-scoped addrs */
	    }
	    
	    memcpy(globalAddr, addr,16);
	    return 0;
	}
    }
    
    return -1;
}

int iface_ip6_get(const char *iface, char * addr)
{
    struct nlmsg_list *linfo = NULL;
    struct nlmsg_list *ainfo = NULL;
    struct nlmsg_list *l;
    struct rtnl_handle rth;
    int foundIface = 0;
    int foundAddr = 0;
    int preferred_family = AF_PACKET;

    /* required to display information about interface */
    struct ifinfomsg *ifi;
    struct rtattr * tb[IFLA_MAX+1];
    int len;
    memset(tb, 0, sizeof(tb));
    memset(&rth,0, sizeof(rth));

    rtnl_open(&rth, 0);
    rtnl_wilddump_request(&rth, preferred_family, RTM_GETLINK);
    rtnl_dump_filter(&rth, store_nlmsg, &linfo, NULL, NULL);
    
    /* 2nd attribute: AF_UNSPEC, AF_INET, AF_INET6 */
    rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETADDR);
    rtnl_dump_filter(&rth, store_nlmsg, &ainfo, NULL, NULL);

    /* build list with interface names */
    for (l=linfo; l; l = l->next) {
	ifi = NLMSG_DATA(&l->h);
	len = (&l->h)->nlmsg_len;
	len -= NLMSG_LENGTH(sizeof(*ifi));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);

	const char *ifname = (char*)RTA_DATA(tb[IFLA_IFNAME]);
	if (strcmp(iface,ifname))
	    continue;
	foundIface = 1;

	if (ipaddr6_global_get(addr, ifi->ifi_index, ainfo)==0)
	{
	    foundAddr = 1;
	}
    }

    release_nlmsg_list(linfo);
    release_nlmsg_list(ainfo);

    if (!foundIface)
	Log(1,"Error: Failed to find %s interface.\n", iface);
    else
	if (!foundAddr)
	    Log(1,"Error: Interface %s does not seem to have global address\n", iface);

    if (foundAddr)
    {
	return 0;
    }
    else
	return -1;
}

/**
 * returns non-local addresses for specified interface
 */
void ipaddr_global_get(int *count, int **bufPtr, unsigned int ifindex, struct nlmsg_list *ainfo) {
    int cnt=0;
    int * buf=0, * tmpbuf=0;
    struct rtattr * rta_tb[IFA_MAX+1];

    for ( ;ainfo ;  ainfo = ainfo->next) {
	struct nlmsghdr *n = &ainfo->h;
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	if ( (ifa->ifa_family == AF_INET) 
	    && (ifa->ifa_index == ifindex) ) {
	    memset(rta_tb, 0, sizeof(rta_tb));
	    parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));
	    if (!rta_tb[IFA_LOCAL])   rta_tb[IFA_LOCAL]   = rta_tb[IFA_ADDRESS];
	    if (!rta_tb[IFA_ADDRESS]) rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];

	    buf = (int*) malloc( (cnt+1)*4);
	    memcpy(buf+4, tmpbuf, cnt*4); /* copy old addrs */
	    memcpy(buf,(char*)RTA_DATA(rta_tb[IFLA_ADDRESS]),4);
	    free(tmpbuf);
	    tmpbuf = buf;
	    cnt++;
	}
    }
    *count = cnt;
    *bufPtr = buf;
}


void setnonblocking(int sock)
{
    int opts;
    
    opts = fcntl(sock,F_GETFL);
    if (opts < 0) {
	Log(1,"Unable to call F_GETFL)\n");
	exit(EXIT_FAILURE);
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(sock,F_SETFL,opts) < 0) {
	perror("fcntl(F_SETFL)");
	exit(EXIT_FAILURE);
    }
    return;
}


int sock_add(const char * ifacename, struct in_addr addr, int port, int proto, int thisifaceonly, int reuse)
{
    int error;
    int on = 1;
    struct addrinfo hints;
    struct addrinfo *res, *res2;
    struct ip_mreq mreq; 
    int Insock;
    int multicast;
    char port_char[6];
    struct sockaddr_in bindme;
    sprintf(port_char,"%d",port);

    if ( (addr.s_addr >> 24) >= 239)
	multicast = 1;
    else
	multicast = 0;

#ifdef LOWLEVEL_DEBUG
    printf("### iface: %s(id=%d), addr=%x, port=%d, ifaceonly=%d reuse=%d###\n",
    ifacename,ifaceid, addr, port, thisifaceonly, reuse);
    fflush(stdout);
#endif
    
    /* Open a socket for inbound traffic */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    switch (proto)
    {
    case IPPROTO_TCP:
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	break;
    case IPPROTO_UDP:
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	break;
    case IPPROTO_RAW:
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_RAW;
	break;
    default:
	Log(1, "Error: invalid socket type: %d\n", proto);
	return LOWLEVEL_ERROR_UNSPEC;
    }
    //hints.ai_flags = AI_PASSIVE; 
   if( (error = getaddrinfo(NULL,  port_char, &hints, &res)) ){
	sprintf(Message, "getaddrinfo failed. Is IPv4 protocol supported by kernel?");
	return LOWLEVEL_ERROR_GETADDRINFO;
    }
    if( (Insock = socket(AF_INET, hints.ai_socktype,0 )) < 0){
	sprintf(Message, "socket creation failed. Is IPv4 protocol supported by kernel?");
	return LOWLEVEL_ERROR_UNSPEC;
    }
	
    /* Set the options  to receivce ipv6 traffic */
    if (setsockopt(Insock, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on)) < 0) {
	sprintf(Message, "Unable to set up socket option IP_RECVPKTINFO.");
	return LOWLEVEL_ERROR_SOCK_OPTS;
    }

    if (thisifaceonly) {
	if (setsockopt(Insock, SOL_SOCKET, SO_BINDTODEVICE, ifacename, strlen(ifacename)+1) <0) {
	    sprintf(Message, "Unable to bind socket to interface %s.", ifacename);
	    return LOWLEVEL_ERROR_BIND_IFACE;
	}
    }

    /* allow address reuse (this option sucks - why allow running multiple servers?) */
    if (reuse!=0) {
	if (setsockopt(Insock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)  {
	    sprintf(Message, "Unable to set up socket option SO_REUSEADDR.");
	    return LOWLEVEL_ERROR_REUSE_FAILED;
	}
    }

    /* bind socket to a specified port */
    bzero(&bindme, sizeof(struct sockaddr_in));
    bindme.sin_family = AF_INET;
    bindme.sin_port   = htons(port);
    bindme.sin_addr   = addr;
    if (bind(Insock, (struct sockaddr*)&bindme, sizeof(bindme)) < 0) {
	sprintf(Message, "Unable to bind socket: %s", strerror(errno) );
	return LOWLEVEL_ERROR_BIND_FAILED;
    }
    
    if (proto == IPPROTO_TCP)
    {
	setnonblocking(Insock);
	listen(Insock, 5); // maximum number of connections
    }

    freeaddrinfo(res);

    /* multicast server stuff */
    if (multicast) {
	char plain[INET_ADDRSTRLEN];
	hints.ai_flags = 0;
	inet_ntop(AF_INET, (void*)&addr, plain, INET_ADDRSTRLEN);

	if((error = getaddrinfo(plain, port_char, &hints, &res2))){
	    sprintf(Message, "Failed to obtain getaddrinfo");
	    return LOWLEVEL_ERROR_GETADDRINFO;
	}
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr = addr; // IP multicast address of group
	mreq.imr_interface = addr; // Local IP address of interface
	
	/* Add to the all agent multicast address */
	if (setsockopt(Insock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
	    sprintf(Message, "error joining IPv4 group");
	    return LOWLEVEL_ERROR_MCAST_MEMBERSHIP;
	}
	freeaddrinfo(res2);
    }
    
    return Insock;
}

int sock_get_port(int fd)
{
    socklen_t len;
    struct sockaddr_in bindme;
    bzero(&bindme, sizeof(struct sockaddr_in));
    len = sizeof(bindme);
    if (getsockname(fd, (struct sockaddr *)&bindme, &len)<0)
    {
	Log(1,"Error: Unable to determine local port of the socket %d\n", fd);
	return -1;
    }
    return htons(bindme.sin_port);
}

int sock_del(int fd)
{
    return close(fd);
}

int sock_send(int sock, struct in_addr addr, char *data, int dataLen, int port)
{
    struct sockaddr_in to;
    int result;
    char plain[INET_ADDRSTRLEN];
    socklen_t len;

    len = sizeof(to);

        memset(&to, 0, len);
    to.sin_family = AF_INET;
    to.sin_port   = htons(port);
    to.sin_addr   = addr;

    result = sendto(sock, data, dataLen, 0, &to, len);

    if (result<0) {
	inet_ntop(AF_INET, (void*)&addr, plain, INET_ADDRSTRLEN);
	Log(1, "Unable to send data (socket=%d, dst addr=%s, %d bytes)\n", 
	    sock, plain, dataLen);
	return LOWLEVEL_ERROR_SOCKET;
    }
    return LOWLEVEL_NO_ERROR;
}

int sock_recv(int fd, struct in_addr * my_addr, struct in_addr * peer_addr, int * peer_port, char * buf, int buflen)
{
    struct msghdr msg;            /* message received by recvmsg */
    struct sockaddr_in peerAddr; /* sender address */
    struct sockaddr_in myAddr;   /* my address */
    struct iovec iov;             /* simple structure containing buffer address and length */

    struct cmsghdr *cm;           /* control message */
    struct in_pktinfo *pktinfo; 

    char control[CMSG_SPACE(sizeof(struct in_pktinfo))];
    char controlLen = CMSG_SPACE(sizeof(struct in_pktinfo));
    int result = 0;
    bzero(&msg, sizeof(msg));
    bzero(&peerAddr, sizeof(peerAddr));
    bzero(&myAddr, sizeof(myAddr));
    bzero(&control, sizeof(control));
    iov.iov_base = buf;
    iov.iov_len  = buflen;

    msg.msg_name       = &peerAddr;
    msg.msg_namelen    = sizeof(peerAddr);
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = control;
    msg.msg_controllen = controlLen;

    result = recvmsg(fd, &msg, 0);

    if (result==-1) {
	return LOWLEVEL_ERROR_UNSPEC;
    }

    /* get source address */
    *peer_addr = peerAddr.sin_addr;
    *peer_port = ntohs(peerAddr.sin_port);

    /* get destination address */
    for(cm = (struct cmsghdr *) CMSG_FIRSTHDR(&msg); cm; cm = (struct cmsghdr *) CMSG_NXTHDR(&msg, cm)){
	if (cm->cmsg_level != IPPROTO_IP || cm->cmsg_type != IP_PKTINFO)
	    continue;
	pktinfo= (struct in_pktinfo *) (CMSG_DATA(cm));
	*my_addr = pktinfo->ipi_addr;
    }
    return result;
}

int sock6_recv(int fd, struct in6_addr * myPackedAddr, 
	       struct in6_addr * peerPackedAddr, 
	       char * buf, int buflen)
{
    struct msghdr msg;            /* message received by recvmsg */
    struct sockaddr_in6 peerAddr; /* sender address */
    struct sockaddr_in6 myAddr;   /* my address */
    struct iovec iov;             /* simple structure containing buffer address and length */

    struct cmsghdr *cm;           /* control message */
    struct in6_pktinfo *pktinfo; 

    char control[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    char controlLen = CMSG_SPACE(sizeof(struct in6_pktinfo));
    int result = 0;
    bzero(&msg, sizeof(msg));
    bzero(&peerAddr, sizeof(peerAddr));
    bzero(&myAddr, sizeof(myAddr));
    bzero(&control, sizeof(control));
    iov.iov_base = buf;
    iov.iov_len  = buflen;

    msg.msg_name       = &peerAddr;
    msg.msg_namelen    = sizeof(peerAddr);
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = control;
    msg.msg_controllen = controlLen;

    result = recvmsg(fd, &msg, 0);

    if (result==-1) {
	return LOWLEVEL_ERROR_UNSPEC;
    }

    /* get destination address */
    for(cm = (struct cmsghdr *) CMSG_FIRSTHDR(&msg); cm; cm = (struct cmsghdr *) CMSG_NXTHDR(&msg, cm)){
	if (cm->cmsg_level != IPPROTO_IPV6 || cm->cmsg_type != IPV6_PKTINFO)
	    continue;
	pktinfo= (struct in6_pktinfo *) (CMSG_DATA(cm));
	memmove(myPackedAddr, (void*)&pktinfo->ipi6_addr, 16);
    }
    return result;
}


void microsleep(int microsecs)
{
    struct timespec x,y;

    x.tv_sec  = (int) microsecs / 1000000;
    x.tv_nsec = ( microsecs - x.tv_sec*1000000) * 1000;
    nanosleep(&x,&y);
}
