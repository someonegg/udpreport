
/*
 *  Copyright (C) someonegg .
 *
 *  MT stand for multithread
 */

#ifndef __MEMPL_MT__
#define __MEMPL_MT__

#include "memory_pl.h"
#include <stdlib.h>
#include <pthread.h>


typedef struct {
    pthread_mutex_t lock;
    mempl_t* mempl;
} mempl_mt_t;


static inline
mempl_mt_t* mempl_mt_create(int n_stage, mempl_callback_t* cb)
{
    mempl_mt_t* mempl_mt;
    mempl_mt = (mempl_mt_t*)malloc(sizeof(mempl_mt_t));
    if (mempl_mt == NULL)
        return NULL;
    mempl_mt->mempl = mempl_create(n_stage, cb);
    if (mempl_mt->mempl == NULL){
        free(mempl_mt);
        return NULL;
    }
    if (pthread_mutex_init(&(mempl_mt->lock), NULL) != 0) {
        mempl_destroy(mempl_mt->mempl);
        free(mempl_mt);
        return NULL;
    }
    return mempl_mt;
}

static inline
void mempl_mt_destroy(mempl_mt_t* mempl_mt)
{
    mempl_destroy(mempl_mt->mempl);
    pthread_mutex_destroy(&(mempl_mt->lock));
    free(mempl_mt);
}

static inline
int mempl_mt_stage_push(mempl_mt_t* mempl_mt, int i_stage_to, void* memory)
{
    int ret;
    pthread_mutex_lock(&(mempl_mt->lock));
    ret = mempl_stage_push(mempl_mt->mempl, i_stage_to, memory);
    pthread_mutex_unlock(&(mempl_mt->lock));
    return ret;
}

static inline
int mempl_mt_stage_pop(mempl_mt_t* mempl_mt, int i_stage_from, void** memory)
{
    int ret;
    pthread_mutex_lock(&(mempl_mt->lock));
    ret = mempl_stage_pop(mempl_mt->mempl, i_stage_from, memory);
    pthread_mutex_unlock(&(mempl_mt->lock));
    return ret;
}

static inline
int mempl_mt_stage_count(mempl_mt_t* mempl_mt, int i_stage)
{
    int ret;
    pthread_mutex_lock(&(mempl_mt->lock));
    ret = mempl_stage_count(mempl_mt->mempl, i_stage);
    pthread_mutex_unlock(&(mempl_mt->lock));
    return ret;
}


#endif // __MEMPL_MT__
