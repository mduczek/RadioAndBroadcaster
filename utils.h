//
//  utils.h
//  Broadcaster
//
//  Created by Duczi on 12.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#ifndef Broadcaster_utils_h
#define Broadcaster_utils_h

#include "basic_libs.h"
#include <netdb.h>

const static int _debug_flag = 1;

//prints to stderr
void debug(const char *fmt, ...);

void syserr(const char *fmt, ...);

//prints to stderr
void hexdump(void *ptr, size_t buflen);


void addr_struct_from(char* ip_str, uint16_t port, struct sockaddr_in* addr_in);

int udp_socket_and_bind(uint16_t port);

void add_multicast_membership(int socket, struct in_addr * multiaddr, struct ip_mreqn* mreq );

void drop_multicast_membership(int socket,  struct ip_mreqn* mreq);

bool sockaddr_in_cmp(struct sockaddr_in* s1, struct sockaddr_in* s2);


#endif
