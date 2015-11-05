#ifndef _LIST_H
#define _LIST_H

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};


static inline void INIT_LIST_HEAD(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

static inline void __list_add(struct list_head *new , struct list_head *prev , struct list_head *next)
{
    new->next = next;
    new->prev = prev;
    prev->next = new;
    next->prev = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new , struct list_head *head)
{
    __list_add(new , head , head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new , struct list_head *head)
{
    __list_add(new , head->prev , head);
}

static inline void __list_del(struct list_head *prev , struct list_head *next)
{
    prev->next = next;
    next->prev = prev;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev , entry->next);
}

/**
 * list_move - delete from one list and add as another's head
 * @entry: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *entry, struct list_head *head)
{
    list_del(entry);
    list_add(entry, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @entry: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *entry, struct list_head *head)
{
    list_del(entry);
    list_add_tail(entry, head);
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @entry: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const struct list_head *entry, const struct list_head *head)
{
    return entry->next == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
    return (head == head->next);
}


#ifndef offset_of
#define offset_of(type , member) (size_t)(&((type *)0)->member)
#endif

#ifndef container_of
#define container_of(ptr , type , member) \
({ \
    (type *)( (char *)ptr - offset_of(type , member)); \
})
#endif

#define list_entry(ptr , type , member) \
   container_of(ptr , type , member)

#define list_first_entry(ptr , type , member) \
   list_entry((ptr)->next , type , member)

#define list_for_each(pos, head) \
   for (pos = (head)->next; pos != (head); pos = pos->next)

#endif// _LIST_H

