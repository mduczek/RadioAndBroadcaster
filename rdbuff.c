//
//  rdbuff.c
//  Rdio
//
//  Created by Duczi on 19.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "rdbuff.h"

void rdbuff_init(rdbuff* rd, size_t size){
    rd->buff = cbuff_new(size);
    rd->begin_serial = 0;
}

void rdbuff_format(rdbuff* rd, size_t max_item_size){
    cbuff_format(rd->buff, max_item_size);
	rd->begin_serial = 0;
}

void rdbuff_destroy(rdbuff* rd){
    cbuff_del(rd->buff);
}

bool rdbuff_pop_front(rdbuff* rd, cbuff_item_container* cont){
    cbuff_pop_front(rd->buff, cont);
    rd->begin_serial++;
    return true;
}

bool rdbuff_push_back(rdbuff* rd, char* data, size_t size, uint16_t* serial){
    if(cbuff_push_back(rd->buff, data, size)){
        *serial = rd->begin_serial + rd->buff->length-1;
        return true;
    }else return false;
}


bool rdbuff_item_get(rdbuff* rd, uint16_t serial, cbuff_item_container* cont){
    if (serial < rd->begin_serial || serial >= rdbuff_end_serial(rd)) return false;
    
    cbuff_item_get(rd->buff, serial - rd->begin_serial, cont);
    return true;
}

bool rdbuff_item_set(rdbuff* rd, uint16_t serial, const char* buff, size_t size){
    if (serial < rd->begin_serial || serial >= rdbuff_end_serial(rd)) return false;
    if (rd->buff->length == 0) {//#############think about it
        rd->begin_serial = serial;
    }
    cbuff_item_set(rd->buff, serial - rd->begin_serial, buff, size);
    return true;
}

uint16_t rdbuff_end_serial(rdbuff* rd){
    return rd->begin_serial + rd->buff->max_length;
}
bool rdbuff_full(rdbuff* rd){
    return rd->buff->length == rd->buff->max_length;
}
