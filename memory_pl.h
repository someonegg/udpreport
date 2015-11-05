
/*
 *  Copyright (C) someonegg .
 *
 *  PL stand for production line
 */

#ifndef __MEMORY_PL__
#define __MEMORY_PL__


typedef struct mempl_s mempl_t;

typedef struct {

    void* context;

    /*
     *  Called in mempl_destroy() when we will discard a half-finished memory.
     */
    void (*discard_memory) (void* context, mempl_t* mempl, int i_stage, void* mem);

    /*
     *  Called in mempl_stage_pop() when memory list will be empty.
     */
    void (*stage_enter_empty) (void* context, mempl_t* mempl, int i_stage);

    /*
     *  Called in mempl_stage_push() when memory list is empty.
     */
    void (*stage_leave_empty) (void* context, mempl_t* mempl, int i_stage);

} mempl_callback_t;


/*
 *  nstage : total processes on this production line
 *
 *  RETURN VALUE
 *    mempl_create() return a mempl_t pointer on success. Otherwise, NULL is returned.
 */
mempl_t* mempl_create(int n_stage, mempl_callback_t* cb);

/*
 *  Always successful.
 */
void mempl_destroy(mempl_t* mempl);

/*
 *  RETURN VALUE
 *    mempl_stage_push() returns zero on success. Otherwise, -1 is returned.
 */
int mempl_stage_push(mempl_t* mempl, int i_stage_to, void* memory);

/*
 *  RETURN VALUE
 *    mempl_stage_pop() returns zero on success. Otherwise, -1 is returned.
 *    When memory list is empty, zero returned and memory is set to NULL.
 */
int mempl_stage_pop(mempl_t* mempl, int i_stage_from, void** memory);

/*
 *  RETURN VALUE
 *    mempl_stage_count() returns >=0 on success. Otherwise, -1 is returned.
 */
int mempl_stage_count(mempl_t* mempl, int i_stage);

#endif // __MEMORY_PL__
