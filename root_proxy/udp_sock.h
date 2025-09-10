/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#ifndef _INCLUDE_UDP_SOCK_H
#define _INCLUDE_UDP_SOCK_H

#include <netinet/in.h>
#include "ipall.h"

#define mksin(/*struct sockaddr_in*/ sin, /*in_addr_t*/ addr, /*u_int16_t*/ port ) { \
        memset(&(sin), 0, sizeof(sin)); \
        (sin).sin_family = AF_INET; \
        (sin).sin_addr.s_addr = (addr); \
        (sin).sin_port = htons(port); \
    }



extern int write_udp(int udp_sock, in_addr_t addr, unsigned short port,void * packet, int len);
extern int read_udp(int udp_sock, in_addr_t *from_addr,unsigned short * port,unsigned char * packet, int len);
extern int init_udp_client(in_addr_t addr,int bcast);
extern int init_udp_server(in_addr_t addr,int port,int bcast,int blocking);

extern int write_udp6(int udp_sock, struct in6_addr *addr, unsigned short port,void * packet, int len);
extern int read_udp6(int udp_sock, struct in6_addr *from_addr,unsigned short * port,unsigned char * packet, int len);
extern int init_udp6_client(struct in6_addr *addr,int bcast);
extern int init_udp6_server(struct in6_addr *addr,int port,int bcast,int blocking);

extern int udp_server_any(struct net_addr_st *ni,int bcast,int blocking);
extern int write_udp_any(int udp_sock, struct net_addr_st *ni, void * packet, int len);
extern int read_udp_any(int udp_sock, int is_type, struct net_addr_st *ni,unsigned char * packet, int len);
extern int udp_client_any(struct net_addr_st *ni,int bcast);

#endif // _INCLUDE_UDP_SOCK_H
