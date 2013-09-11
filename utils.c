//
//  utils.c
//  Rdio
//
//  Created by Duczi on 23.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "utils.h"

void debug(const char *fmt, ...)
{
    if(_debug_flag ==1){
        va_list fmt_args;
        
        fprintf(stderr, "D: ");
        va_start(fmt_args, fmt);
        vfprintf(stderr, fmt, fmt_args);
        va_end (fmt_args);
        fprintf(stderr, "\n");
    }
}

void syserr(const char *fmt, ...)
{
    va_list fmt_args;
    
    fprintf(stderr, "ERROR: ");
    
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);
    perror(" ");
    exit(EXIT_FAILURE);
}


void hexdump(void *ptr, size_t buflen) {
    unsigned char *buf = (unsigned char*)ptr;
    size_t i, j;
    for (i=0; i<buflen; i+=16) {
        fprintf(stderr,"%06zu: ", i);
        for (j=0; j<16; j++)
            if (i+j < buflen)
                fprintf(stderr,"%02x ", buf[i+j]);
            else
                fprintf(stderr,"   ");
        fprintf(stderr," ");
        for (j=0; j<16; j++)
            if (i+j < buflen)
                fprintf(stderr,"%c", isprint(buf[i+j]) ? buf[i+j] : '.');
        fprintf(stderr,"\n");
    }
}


void addr_struct_from(char* ip_str, uint16_t port, struct sockaddr_in* addr_in){
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    
    (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    if (getaddrinfo(ip_str, NULL, &addr_hints, &addr_result) != 0)
        syserr("invalid ip_str");
    
    addr_in->sin_family = AF_INET; // IPv4
    addr_in->sin_addr.s_addr =
    ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
    addr_in->sin_port = htons(port);

    freeaddrinfo(addr_result);
}

int udp_socket_and_bind(uint16_t port){
    int sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    if (sock < 0) syserr("error creating crtl socket");
    
    struct sockaddr_in port_addr;
    port_addr.sin_family = AF_INET; // IPv4
    port_addr.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    port_addr.sin_port = htons(port);
    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *) &port_addr, (socklen_t) sizeof(port_addr)) < 0)
        syserr("error binding ctrl socket");
    
    return sock;
}
//the function will fill membership request, so later you can drop the membership with the same mreq
void add_multicast_membership(int socket, struct in_addr * multiaddr, struct ip_mreqn* mreq ){
    mreq->imr_multiaddr = *multiaddr;
    mreq->imr_ifindex = 0;
    mreq->imr_address.s_addr = htonl(INADDR_ANY);
    
    setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)mreq, sizeof(*mreq));
}

void drop_multicast_membership(int socket,  struct ip_mreqn* mreq){
    setsockopt(socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)mreq, sizeof(*mreq));
}

bool sockaddr_in_cmp(struct sockaddr_in* s1, struct sockaddr_in* s2){
    char s1_str [INET_ADDRSTRLEN];
    char s2_str [INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(s1->sin_addr), s1_str, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(s2->sin_addr), s2_str, INET_ADDRSTRLEN);
    debug("cmp %s %s",s1_str, s2_str);
    return s1->sin_port == s2->sin_port && strncmp(s1_str, s2_str, INET_ADDRSTRLEN)==0;
}