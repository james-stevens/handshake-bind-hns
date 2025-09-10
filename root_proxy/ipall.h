/*****************************************************************
*                                                                *
*    (c) Copyright 2009-2020 JRCS Ltd  - All Rights Reserved     *
*                                                                *
*****************************************************************/
#ifndef _INCLUDE_IPALL_H_
#define _INCLUDE_IPALL_H_

#include <stdint.h>
#include <netinet/in.h>

struct net_addr_st {
    uint16_t port;
    uint8_t is_type;
    union addr_u {
		in_addr_t v4;
		struct in6_addr v6;
		} addr;
    };

#endif // _INCLUDE_LIBALL_H_
