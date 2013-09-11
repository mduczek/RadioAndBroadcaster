//
//  packet.h
//  Broadcaster
//
//  Created by Duczi on 06.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#ifndef Broadcaster_packet_h
#define Broadcaster_packet_h

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#define packet_type_data 'd'
#define packet_type_id 'i'
#define packet_type_id_request 'w'
#define packet_type_retransmit 't'
#define packet_max_size 10000 //10kb
#define packet_max_broadcaster_name 16

typedef struct _app_id{
    char app_name [12];
    char version;
} app_id;

typedef struct _packet_id {
    char type;
    app_id app;
    char broadcaster_name[packet_max_broadcaster_name];
    uint16_t max_packet_size;
    char multicast[INET_ADDRSTRLEN];
    in_port_t data_port;
} packet_id;


void packet_id_init(packet_id* p);

bool packet_id_cmp(packet_id* one, packet_id* two);


typedef struct _packet_data {
    char type;
    uint16_t serial;
} packet_data_header;

void packet_data_init(packet_data_header* p);
void packet_retrasmit_init(packet_data_header* p);

typedef struct _packet_id_request{
    char type;
    app_id app;
} packet_id_request;

void packet_id_request_init(packet_id_request* p);

typedef char packet_header_buff[sizeof(packet_id) > sizeof(packet_id_request)
                                ? sizeof(packet_id) : sizeof(packet_id_request)];

//ntoh and hton for packets
void packet_id_ntoh(packet_id* p);
void packet_data_header_ntoh(packet_data_header* p);
void packet_id_request_ntoh(packet_id_request* p);
void packet_id_hton(packet_id* p);
void packet_data_header_hton(packet_data_header* p);
void packet_id_request_hton(packet_id_request* p);

#endif
