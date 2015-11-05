
/*
 *  Copyright (C) someonegg .
 *
 *  PL stand for production line
 */

#include "memory_pl.h"
#include "list.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define ASSERT assert


typedef struct {
    struct list_head head;
    void* memory;
} mem_head_t;

typedef struct {
    struct list_head head;
    int count;
} stage_head_t;

struct mempl_s {
    int n_stage;
    stage_head_t* stages;

    mempl_callback_t cb;

    struct list_head memh_pool;
};


mempl_t* mempl_create(int n_stage, mempl_callback_t* cb)
{
    mempl_t* mempl;
    int i = 0;

    ASSERT(n_stage > 0);
    ASSERT(cb != NULL);

    mempl = (mempl_t*)malloc(sizeof(mempl_t));
    if (mempl == NULL)
        return NULL;

    mempl->n_stage = n_stage;

    mempl->stages = (stage_head_t*)malloc(n_stage * sizeof(stage_head_t));
    if (mempl->stages == NULL) {
        free(mempl);
        return NULL;
    }

    for (i = 0; i < mempl->n_stage; ++i) {
        INIT_LIST_HEAD(&(mempl->stages[i].head));
        mempl->stages[i].count = 0;
    }

    mempl->cb = *cb;
    INIT_LIST_HEAD(&(mempl->memh_pool));

    return mempl;
}

void mempl_destroy(mempl_t* mempl)
{
    struct list_head* pos;
    mem_head_t* memh;
    int i = 0;

    ASSERT(mempl != NULL);

    for (i = 0; i < mempl->n_stage; ++i) {
        while (mempl->stages[i].count != 0) {
            pos = mempl->stages[i].head.next;
            list_del(pos);
            --(mempl->stages[i].count);
            memh = list_entry(pos, mem_head_t, head);
            if (mempl->cb.discard_memory) {
                mempl->cb.discard_memory(mempl->cb.context, mempl, i, memh->memory);
            }
            free(memh);
        }
    }

    while (!list_empty(&(mempl->memh_pool))) {
        pos = mempl->memh_pool.next;
        list_del(pos);
        memh = list_entry(pos, mem_head_t, head);
        free(memh);
    }

    free(mempl->stages);
    free(mempl);
}

int mempl_stage_push(mempl_t* mempl, int i_stage_to, void* memory)
{
    struct list_head* pos;
    mem_head_t* memh;
    int stage_empty = 0;

    ASSERT(mempl != NULL);

    if (!(i_stage_to >= 0 && i_stage_to < mempl->n_stage))
        return -1;

    if (memory == NULL)
        return -1;

    stage_empty = (mempl->stages[i_stage_to].count == 0);

    if (!list_empty(&(mempl->memh_pool))) {
        pos = mempl->memh_pool.next;
        list_del(pos);
        memh = list_entry(pos, mem_head_t, head);
    } else {
        memh = (mem_head_t*)malloc(sizeof(mem_head_t));
        if (memh == NULL)
            return -1;
    }

    memh->memory = memory;
    list_add_tail(&(memh->head), &(mempl->stages[i_stage_to].head));
    ++(mempl->stages[i_stage_to].count);

    if (stage_empty) {
        if (mempl->cb.stage_leave_empty) {
            mempl->cb.stage_leave_empty(mempl->cb.context, mempl, i_stage_to);
        }
    }

    return 0;
}

int mempl_stage_pop(mempl_t* mempl, int i_stage_from, void** memory)
{
    struct list_head* pos;
    mem_head_t* memh;

    ASSERT(mempl != NULL);

    if (!(i_stage_from >= 0 && i_stage_from < mempl->n_stage))
        return -1;

    if (memory == NULL)
        return -1;

    if (mempl->stages[i_stage_from].count == 0) {
        *memory = NULL;
        return 0;
    }

    pos = mempl->stages[i_stage_from].head.next;
    memh = list_entry(pos, mem_head_t, head);
    *memory = memh->memory;

    list_move_tail(pos, &(mempl->memh_pool));
    --(mempl->stages[i_stage_from].count);

    if (mempl->stages[i_stage_from].count == 0) {
        if (mempl->cb.stage_enter_empty) {
            mempl->cb.stage_enter_empty(mempl->cb.context, mempl, i_stage_from);
        }
    }

    return 0;
}

int mempl_stage_count(mempl_t* mempl, int i_stage)
{
    ASSERT(mempl != NULL);

    if (!(i_stage >= 0 && i_stage < mempl->n_stage))
        return -1;

    return mempl->stages[i_stage].count;
}

