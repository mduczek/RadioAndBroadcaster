//
//  packet.c
//  Broadcaster
//
//  Created by Duczi on 11.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "packet.h"
#include <string.h>
#include <netinet/in.h>

void packet_id_request_init(packet_id_request* p){
    memset(p, 0, sizeof(packet_id_request));
    p->type = packet_type_id_request;
}

void packet_id_init(packet_id* p){
    memset(p, 0, sizeof(packet_id));
    p->type = packet_type_id;
}

void packet_data_init(packet_data_header* p){
    memset(p, 0, sizeof(packet_data_header));
    p->type = packet_type_data;
}

void packet_retrasmit_init(packet_data_header* p){
    memset(p, 0, sizeof(packet_data_header));
    p->type= packet_type_retransmit;
}

bool packet_id_cmp(packet_id* one, packet_id* two){
    return (strncmp((char*)(&one->app), (char*)(&two->app), sizeof(one->app)) == 0) &&
    (strncmp(one->broadcaster_name, two->broadcaster_name, sizeof(one->broadcaster_name)) == 0);
}

void packet_id_ntoh(packet_id* p){
    p->max_packet_size = ntohs(p->max_packet_size);
}
void packet_data_header_ntoh(packet_data_header* p){
    p->serial = ntohs(p->serial);
}
void packet_id_request_ntoh(packet_id_request* p){
    return;
}
void packet_id_hton(packet_id* p){
    p->max_packet_size = htons(p->max_packet_size);
}
void packet_data_header_hton(packet_data_header* p){
    p->serial = htons(p->serial);
}
void packet_id_request_hton(packet_id_request* p){
    return;
}