//
//  stations.c
//  Broadcaster
//
//  Created by Duczi on 16.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "stations.h"
#include <string.h>
#include <stdio.h>

void sta_init(stations* st){
    pthread_mutex_init(&st->mutex, NULL);

    st->current_i = st_not_active;
    
    for (size_t i=0; i<st_max_stations; i++){
        packet_id_init(&st->list[i].id_packet);
        st->list[i].expires_in = 0;
    }
    
    sta_update_print_buff(st);
}


void sta_destroy(stations* st){
    pthread_mutex_destroy(&st->mutex);
}

packet_id sta_current(stations* st){
    return st->list[st->current_i].id_packet;
}

bool sta_previous(stations* st){
    if(st->current_i == st_not_active && st->list[0].expires_in ==0) return false;
	if(st->current_i == st_not_active ){
    	st->current_i = 0;
    	sta_update_print_buff(st);
    	return true;
    }else if(st->current_i > 0){
    	st->current_i--;
    	sta_update_print_buff(st);
    	return true;
	}else return false;
}
bool sta_next(stations* st){
    if(st->current_i == st_not_active && st->list[0].expires_in ==0) return false;
    if (st->current_i == st_not_active-1 ||
        st->list[st->current_i+1].expires_in==0) return false;
	if(st->current_i == st_not_active) st->current_i = 0;
	else st->current_i++;    

    sta_update_print_buff(st);
    return true;
}


size_t sta_add(stations* st, packet_id* p, struct sockaddr_in* ctrl_addr){
    bool found = false;
    size_t i;
    for (i=0; i<st_max_stations && !found && st->list[i].expires_in > 0; i++) {
        if (packet_id_cmp(&st->list[i].id_packet, p)) {
            st->list[i].expires_in = st_expires_in;
            found = true;
        }
    }
    if (!found && i < st_max_stations) {
        stations_item* it = &st->list[i];
        it->id_packet = *p;
        it->expires_in = st_expires_in;
        it->ctrl_addr = *ctrl_addr;
    }else i=st_not_active;

    sta_update_print_buff(st);
	return i;
}

void sta_time_unit_passed(stations* st){
    stations_item temp [st_max_stations];
    memset(temp, '\0', sizeof(temp));
    size_t i, j;

    for (i=0; i<st_max_stations && st->list[i].expires_in > 0; i++) {
        st->list[i].expires_in--;
    }
    
    if (st->list[st->current_i].expires_in==0) {
        //the station ended switch to not_active
        st->current_i = st_not_active;
    }
    
    packet_id current = st->list[st->current_i].id_packet;
    
    for (j=0, i=0; i<st_max_stations; i++) {
        if (st->list[i].expires_in>0) {
            temp[j] = st->list[i];
            ++j;
        }
    }
    
    memcpy(st->list, temp, sizeof(temp));
    if (st->current_i!=st_not_active) {
        for (i=0; i<st_max_stations; i++) {
            if(packet_id_cmp(&st->list[i].id_packet, &current)){
                st->current_i = i;
                break;
            }
        }
    }

    sta_update_print_buff(st);
}


const char print_line [] = "=======================\r\n";
const char print_welcome[] = "< Odbiornik >\r\n";

void sta_update_print_buff(stations* st){
    char* ptr = st->print_buff;
    memset(st->print_buff, '\0', sizeof(st->print_buff));
    
    ptr += sprintf(ptr, "%s%s",print_line, print_welcome);
    
    for (size_t i=0; i<st_max_stations && st->list[i].expires_in > 0; i++) {
        
        if (st->current_i == i) ptr += sprintf(ptr, "> ");
        else ptr += sprintf(ptr, "  ");
        ptr += sprintf(ptr, "%zd %s\r\n",i, st->list[i].id_packet.broadcaster_name);
    }
    
    sprintf(ptr, "%s", print_line);
}
