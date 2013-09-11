//
//  ui.h
//  Broadcaster
//
//  Created by Duczi on 16.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#ifndef Broadcaster_ui_h
#define Broadcaster_ui_h

#include <pthread.h>
#include "../packet.h"
#include <netinet/in.h>

#define st_max_stations 11
#define st_not_active 10
#define st_expires_in 4

typedef struct _stations_item{
    packet_id id_packet;
    struct sockaddr_in ctrl_addr;
    char expires_in;
} stations_item;

typedef struct _stations{
    pthread_mutex_t mutex;
    stations_item list [st_max_stations];
    size_t current_i;
    char print_buff [(st_max_stations+7) * (packet_max_broadcaster_name+9) ];
    
} stations;

packet_id sta_current(stations* st);

void sta_init(stations* st);
void sta_destroy(stations* st);
bool sta_previous(stations* st);
bool sta_next(stations* st);

size_t sta_add(stations* st, packet_id* p, struct sockaddr_in* ctrl_addr );
void sta_time_unit_passed(stations* st);

void sta_update_print_buff(stations* st);

#endif
