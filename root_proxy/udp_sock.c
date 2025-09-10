/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/nameser.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <resolv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>


#include "log_message.h"
#include "udp_sock.h"
#include "liball.h"



int write_udp(int udp_sock, in_addr_t addr, unsigned short port,void * packet, int len)
{
struct sockaddr_in sin;
int ret;

	mksin(sin,addr,port);
	if ((ret = sendto(udp_sock, packet, len, 0,(struct sockaddr *) &sin, sizeof(sin))) <= 0)
		log_message(MSG_NORMAL,"sendto error on %d, %d bytes to %08X:%d (%s)\n",udp_sock,len,addr,port,ERRMSG);
	return ret;
}



int read_udp(int udp_sock, in_addr_t *from_addr,unsigned short * from_port,unsigned char * packet, int len)
{
struct sockaddr_in from_addr_i;
int ret;
socklen_t from_len_i = sizeof(struct sockaddr);

	if ((!packet)||(len <= 0)||(udp_sock <= 0)) return -1;
	ret = recvfrom(udp_sock,packet,len,0,(struct sockaddr *)&from_addr_i,&from_len_i);
	if (from_addr) *from_addr = from_addr_i.sin_addr.s_addr;
	if (from_port) *from_port = ntohs(from_addr_i.sin_port);
	return ret;
}



int init_udp_client(in_addr_t addr,int bcast)
{
int one = 1;
int sock = 0;
struct sockaddr_in sa;

    mksin(sa,addr,0); // port 0 = assign me a client port

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) return -1;
    if (bind(sock, (struct sockaddr *)&sa, sizeof sa)) { log_message(MSG_NORMAL,"init_udp_client UDP bind: %s - %s\n",ipchar(addr),ERRMSG); return -1; }
    if (bcast)
		{
		if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&one,sizeof(one)) < 0) { log_message(MSG_NORMAL,"UDP SETsockopt-SO_SNDBUF: %s\n",ERRMSG); return -1; }
		}
	return sock;
}



int init_udp_server(in_addr_t addr,int port,int bcast,int blocking)
{
int sock,sock_opt;
struct sockaddr_in sa;

    mksin(sa,addr,port);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) { log_message(MSG_NORMAL,"UDP socket: %s\n",ERRMSG); return -1; }
    if (bind(sock, (struct sockaddr *)&sa, sizeof sa)) { log_message(MSG_NORMAL,"init_udp_server UDP bind: %s:%d - %s\n",ipchar(addr),port,ERRMSG); return -1; }
    if (bcast)
        {
        sock_opt = 1;
        if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&sock_opt,sizeof(sock_opt)) < 0) log_message(MSG_NORMAL,"UDP SETsockopt-SO_SNDBUF: %s\n",ERRMSG);
        }
    set_blocking(sock,blocking);
    return sock;
}


// IPv6  :)



int write_udp6(int udp_sock, struct in6_addr *addr, unsigned short port,void * packet, int len)
{
struct sockaddr_in6 sin;
int ret;

	mksin6(&sin,addr,port);
	if ((ret = sendto(udp_sock, packet, len, 0,(struct sockaddr *) &sin, sizeof(sin))) <= 0)
		{
		char ip6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6,addr,ip6,INET6_ADDRSTRLEN);
		log_message(MSG_NORMAL,"sendto6 error on %d, %d bytes to %s:%d (%s)\n",udp_sock,len,ip6,port,ERRMSG);
		}
	return ret;
}



int read_udp6(int udp_sock, struct in6_addr *from_addr,unsigned short * from_port,unsigned char * packet, int len)
{
struct sockaddr_in6 from_addr_i;
int ret;
socklen_t from_len_i = sizeof(struct sockaddr_in6);

	if ((!packet)||(len <= 0)||(udp_sock <= 0)) return -1;
	ret = recvfrom(udp_sock,packet,len,0,(struct sockaddr *)&from_addr_i,&from_len_i);
	if (from_addr) MEMCPY(from_addr,&from_addr_i.sin6_addr,sizeof(struct in6_addr));
	if (from_port) *from_port = ntohs(from_addr_i.sin6_port);
	return ret;
}



int init_udp6_client(struct in6_addr *addr,int bcast)
{
int one = 1;
int sock = 0;
struct sockaddr_in6 sa;

    mksin6(&sa,addr,0); // port 0 = assign me a client port

	if ((sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) return -1;
    if (bind(sock, (struct sockaddr *)&sa, sizeof sa)) { log_message(MSG_NORMAL,"UDP6 bind: %s\n",ERRMSG); return -1; }
    if (bcast)
		{
		if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&one,sizeof(one)) < 0) { log_message(MSG_NORMAL,"UDP6 SETsockopt-SO_SNDBUF: %s\n",ERRMSG); return -1; }
		}
	return sock;
}



int init_udp6_server(struct in6_addr *addr,int port,int bcast,int blocking)
{
int sock,sock_opt;
struct sockaddr_in6 sa;

    mksin6(&sa,addr,port);

    if ((sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) { log_message(MSG_NORMAL,"UDP6 socket: %s\n",ERRMSG); return -1; }
    if (bind(sock, (struct sockaddr *)&sa, sizeof sa)) { log_message(MSG_NORMAL,"init_udp6_server UDP bind: %s\n",ERRMSG); return -1; }
    if (bcast) {
        sock_opt = 1;
        if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&sock_opt,sizeof(sock_opt)) < 0) log_message(MSG_NORMAL,"UDP6 SETsockopt-SO_SNDBUF: %s\n",ERRMSG);
        }
    set_blocking(sock,blocking);
    return sock;
}



// Uses "struct net_addr_st" (IPv4 or IPv6)


int udp_server_any(struct net_addr_st *ni,int bcast,int blocking)
{
struct sockaddr_in sa;
struct sockaddr_in6 sa6;
int domain = -1,sock=-1;
socklen_t sz=0;
void * addr=NULL;

	if (ni->is_type==4) { domain = PF_INET; mksin(sa,ni->addr.v4,ni->port); addr=&sa; sz=sizeof(sa); }
	if (ni->is_type==6) { domain = PF_INET6; mksin6(&sa6,&ni->addr.v6,ni->port); addr=&sa6; sz=sizeof(sa6); }
	if (!addr) { logmsg(MSG_ERROR,"ERROR: udp_server_any invalid transport type '%d'\n",ni->is_type); return -1; }

	if ((sock = socket(domain, SOCK_DGRAM, IPPROTO_UDP)) < 0) { log_message(MSG_NORMAL,"udp_server_any socket: %s\n",ERRMSG); return -1; }
    if (bind(sock,addr,sz)) { log_message(MSG_NORMAL,"udp_server_any UDP bind: %s\n",ERRMSG); return -1; }
    if (bcast) {
        int sock_opt = 1;
        if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&sock_opt,sizeof(sock_opt)) < 0) log_message(MSG_NORMAL,"udp_server_any SETsockopt-SO_SNDBUF: %s\n",ERRMSG);
        }
    set_blocking(sock,blocking);
    return sock;
}



int write_udp_any(int udp_sock, struct net_addr_st *ni, void * packet, int len)
{
struct sockaddr_in sa;
struct sockaddr_in6 sa6;
int ret;
socklen_t sz=0;
void * addr=NULL;

    if (ni->is_type==4) { mksin(sa,ni->addr.v4,ni->port); addr = &sa; sz = sizeof(sa); }
    if (ni->is_type==6) { mksin6(&sa6,&ni->addr.v6,ni->port); addr = &sa6; sz = sizeof(sa6); }
	if (!addr) { logmsg(MSG_ERROR,"ERROR: write_udp_any invalid transport type '%d'\n",ni->is_type); return -1; }

	if (!sz) return -1;

	ret = sendto(udp_sock, packet, len, 0,addr,sz);

	if (ret!=len)
		log_message(MSG_DEBUG,"write_udp_any: sendto error fd=%d, %d bytes to %s:%d (%s)\n",udp_sock,len,IPADDRCHAR(*ni),ni->port,ERRMSG);
		// src-ip may be forged & we don't want manic syslogging

	return ret;
}



int read_udp_any(int udp_sock, int is_type, struct net_addr_st *ni,unsigned char * packet, int len)
{
struct sockaddr_in sa;
struct sockaddr_in6 sa6;
socklen_t sz;
void * addr=NULL;
int ret;

	if ((!packet)||(len <= 0)||(udp_sock <= 0)) return -1;

    if (is_type==4) { addr = &sa; sz = sizeof(sa); }
    if (is_type==6) { addr = &sa6; sz = sizeof(sa6); }
	if (!addr) { logmsg(MSG_ERROR,"ERROR: read_udp_any invalid transport type '%d'\n",ni->is_type); return -1; }

	ret = recvfrom(udp_sock,packet,len,0,addr,&sz);

	if (!ni) return ret;

	memset(ni,0,sizeof(struct net_addr_st));

	ni->is_type = is_type;

	if (is_type==4) {
		ni->addr.v4 = sa.sin_addr.s_addr;
		ni->port = ntohs(sa.sin_port);
		}

	if (is_type==6) {
		MEMCPY(&ni->addr.v6,&sa6.sin6_addr,sizeof(struct in6_addr));
		ni->port = ntohs(sa6.sin6_port);
		}

	return ret;
}



int udp_client_any(struct net_addr_st *ni,int bcast)
{
int domain = -1;
struct sockaddr_in sa;
struct sockaddr_in6 sa6;
socklen_t sz=0;
void * addr=NULL;
int sock = 0;

	if (ni->is_type==4) { mksin(sa,ni->addr.v4,ni->port); domain = PF_INET; addr=&sa; sz=sizeof(sa); }
	if (ni->is_type==6) { mksin6(&sa6,&ni->addr.v6,ni->port); domain = PF_INET6; addr=&sa6; sz=sizeof(sa6); }
	if (!addr) { logmsg(MSG_ERROR,"ERROR: udp_client_any invalid transport type '%d'\n",ni->is_type); return -1; }
	if (!sz) return -1;

	if ((sock = socket(domain, SOCK_DGRAM, IPPROTO_UDP)) < 0) return -1;
    if (bind(sock, addr,sz)) { log_message(MSG_NORMAL,"UDP bind: %s\n",ERRMSG); return -1; }
    if (bcast) {
		int one = 1;
		if (setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&one,sizeof(one)) < 0) { log_message(MSG_NORMAL,"UDP6 SETsockopt-SO_SNDBUF: %s\n",ERRMSG); return -1; }
		}
	return sock;
}
