//
//  cyclic_buff.h
//  Broadcaster
//
//  Created by Duczi on 08.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#ifndef Broadcaster_cyclic_buff_h
#define Broadcaster_cyclic_buff_h

#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

typedef struct _cyclic_buff{
    char* data;
    size_t* item_sizes;
    size_t offset;
    size_t length;
    size_t item_count;
    
    size_t alloc_size;
    size_t max_length;
    size_t max_item_size;
} cbuff;

typedef struct _cbuff_item_container{
    char* const data;
    size_t size;
    const size_t alloc_size;
} cbuff_item_container;

cbuff_item_container* cbuff_item_container_new(const cbuff* cb);
void cbuff_item_container_del(cbuff_item_container* cont);

const cbuff * cbuff_new(size_t size);

void cbuff_format(const cbuff * cb, size_t max_item_size);

void cbuff_del(const cbuff * cb);

bool cbuff_push_back(const cbuff * cb, char* data, size_t size);

bool cbuff_pop_front(const cbuff* cb, cbuff_item_container* cont);

void cbuff_item_get(const cbuff* cb, size_t i, cbuff_item_container* cont);

void cbuff_item_set(const cbuff* cb, size_t i, const char* buff, size_t size);


#endif
