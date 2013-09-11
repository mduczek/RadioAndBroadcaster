//
//  cyclic_buff.c
//  Broadcaster
//
//  Created by Duczi on 08.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "cyclic_buff.h"
#include <assert.h>
#include <string.h>

typedef struct _item_container{
    char* data;
    size_t size;
    size_t alloc_size;
} _item_container;
bool* _checkEq [ sizeof(_item_container) == sizeof(cbuff_item_container) ? 1: -2];

cbuff const * cbuff_new(size_t size){
    cbuff* buff = malloc(sizeof(cbuff));
    buff->data = malloc(size);
    buff->item_sizes = NULL;
    buff->length = 0;
    buff->max_item_size = 0;
    buff->max_length = 0;
    buff->offset = 0;
    buff->alloc_size = size;

    return buff;
}

void cbuff_format(cbuff const * cbc, size_t max_item_size){
    cbuff* cb = (cbuff*) cbc;
    if (cb->item_sizes != NULL)
        free(cb->item_sizes);
    cb->item_count = 0;
    cb->length = 0;
    cb->offset = 0;
    cb->max_length = cb->alloc_size / max_item_size;
    cb->max_item_size = max_item_size;
    cb->item_sizes = calloc(cb->max_length, sizeof(cb->item_sizes[0]));
}

void cbuff_del(cbuff const * cbc){
    cbuff* cb = (cbuff*) cbc;
    free(cb->data);
    free(cb->item_sizes);
    free(cb);
}

char* offset(cbuff* cb, size_t i){
    return cb->data + i * cb->max_item_size;
}

bool cbuff_push_back(const cbuff* cbc, char* data, size_t size){
    cbuff* cb = (cbuff*) cbc;
    if(cb->length == cb->max_length) return false;
    
    cbuff_item_set(cb, cb->length, data, size);
    return true;
}

bool cbuff_pop_front(const cbuff* cbc, cbuff_item_container* cont){
    cbuff* cb = (cbuff*) cbc;
    if (cb->length == 0) return false;
    
    if (cont) cbuff_item_get(cb, 0, cont);
    if (cb->item_sizes[cb->offset] > 0)
        cb->item_count--;
    cb->item_sizes[cb->offset] = 0;
    cb->offset = (cb->offset + 1) % cb->max_length;
    cb->length--;
    return true;
}

void cbuff_item_get(const cbuff* cbc, size_t i, cbuff_item_container* cont){
    cbuff* cb = (cbuff*) cbc;
    
    size_t index = (cb->offset + i) % cb->max_length;
    _item_container* cnt = (_item_container*) cont;
    if (cnt->alloc_size < cb->max_item_size) {
        cnt->data = realloc(cnt->data, cb->max_item_size);
        cnt->alloc_size = cb->max_item_size;
    }
    
    cnt->size = cb->item_sizes[index];
    memset(cnt->data, 0, cnt->alloc_size);
    memcpy(cnt->data, offset(cb, index) , cont->size);
}

void cbuff_item_set(const cbuff* cbc, size_t i, const char* buff, size_t size){
    assert(size <= cbc->max_item_size);
    cbuff* cb = (cbuff*) cbc;
    size_t index = (cb->offset + i) % cb->max_length;
    
    if(cb->item_sizes[index]==0 && size > 0)
        cb->item_count++;
    else if(cb->item_sizes[index] > 0 && size ==0)
        cb->item_count--;
    
    cb->item_sizes[index] = size;
    memcpy( offset(cb, index) , buff, size);
    
    
    if(cb->length < (i % cb->max_length) +1 && size > 0)
        cb->length = (i % cb->max_length) +1;
}

cbuff_item_container* cbuff_item_container_new(const cbuff* cb){
    _item_container* cont = malloc(sizeof(_item_container));
    cont->alloc_size = cb->max_item_size;
    cont->data = calloc(1, cont->alloc_size);
    cont->size = 0;
    return (cbuff_item_container*) cont;
}
void cbuff_item_container_del(cbuff_item_container* cont){
    free(cont->data);
    free(cont);
}

