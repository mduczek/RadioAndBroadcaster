//
//  rdbuff.h
//  Broadcaster
//
//  Created by Duczi on 16.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#ifndef Broadcaster_rdbuff_h
#define Broadcaster_rdbuff_h

#include "cyclic_buff.h"
#include "basic_libs.h"

typedef struct _rdbuff{
    const cbuff* buff;
    uint16_t begin_serial;
} rdbuff;

//it's like iterable->end pointer, valid serial nums are [begin_serial, end_serial-1 ]
uint16_t rdbuff_end_serial(rdbuff* rd);

static const char rdbuff_retransmit_mark = 'R';

bool rdbuff_full(rdbuff* rd);

bool rdbuff_ready_to_read(rdbuff* rd);

void rdbuff_init(rdbuff* rd, size_t size);

void rdbuff_format(rdbuff* rd, size_t max_item_size);

void rdbuff_destroy(rdbuff* rd);

bool rdbuff_pop_front(rdbuff* rd, cbuff_item_container* cont);

bool rdbuff_push_back(rdbuff* rd, char* data, size_t size, uint16_t* serial);

bool rdbuff_item_get(rdbuff* rd, uint16_t serial, cbuff_item_container* cont);

bool rdbuff_item_set(rdbuff* rd, uint16_t serial, const char* buff, size_t size);


#endif
